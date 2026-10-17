#include "pe_to_elf.hpp"

#include "common/logging.hpp"

#include <LIEF/PE.hpp>
#include <LIEF/Abstract.hpp>

#include <elf.h>
#include <sys/stat.h>

#ifdef __x86_64__
#include <cpuid.h>
#endif

#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace win2lin::relinker {

namespace {

// ---------------------------------------------------------------------------
// Windows PE constants (kept local so we do not depend on windows.h)
// ---------------------------------------------------------------------------
namespace pe {
constexpr uint32_t SCN_CNT_CODE               = 0x00000020;
constexpr uint32_t SCN_CNT_INITIALIZED_DATA   = 0x00000040;
constexpr uint32_t SCN_CNT_UNINITIALIZED_DATA = 0x00000080;
constexpr uint32_t SCN_MEM_EXECUTE            = 0x20000000;
constexpr uint32_t SCN_MEM_READ               = 0x40000000;
constexpr uint32_t SCN_MEM_WRITE              = 0x80000000;
} // namespace pe

// ---------------------------------------------------------------------------
// CPUID feature validation
// ---------------------------------------------------------------------------
struct MissingFeatures {
    std::vector<std::string> names;
    bool empty() const { return names.empty(); }
};

MissingFeatures check_cpu_features() {
    MissingFeatures missing;
#ifdef __x86_64__
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;

    // Leaf 1: FMA (ECX bit 12), LZCNT/ABM (ECX bit 29)
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        if (!(ecx & (1u << 12))) missing.names.emplace_back("FMA");
        // LZCNT (ABM) is officially leaf 0x80000001 ECX bit 5, but Intel also
        // reports it in leaf 1 ECX bit 29; we check the AMD leaf below.
    }

    // Leaf 7 subleaf 0: AVX2 (EBX bit 5), BMI1 (EBX bit 3), BMI2 (EBX bit 8)
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        if (!(ebx & (1u << 5))) missing.names.emplace_back("AVX2");
        if (!(ebx & (1u << 3))) missing.names.emplace_back("BMI1");
        if (!(ebx & (1u << 8))) missing.names.emplace_back("BMI2");
    } else {
        missing.names.emplace_back("AVX2");
        missing.names.emplace_back("BMI1");
        missing.names.emplace_back("BMI2");
    }

    // Extended leaf 0x80000001: LZCNT/ABM (ECX bit 5)
    unsigned int max_ext = 0;
    if (__get_cpuid(0x80000000, &eax, &ebx, &ecx, &edx)) {
        max_ext = eax;
    }
    bool lzcnt_ok = false;
    if (max_ext >= 0x80000001 &&
        __get_cpuid(0x80000001, &eax, &ebx, &ecx, &edx)) {
        lzcnt_ok = (ecx & (1u << 5)) != 0;
    }
    if (!lzcnt_ok) missing.names.emplace_back("LZCNT");
#else
    missing.names.emplace_back("x86-64 (host is not x86-64)");
#endif
    return missing;
}

// ---------------------------------------------------------------------------
// DLL -> shim .so mapping
// ---------------------------------------------------------------------------
std::string dll_to_shim(const std::string& dll, const std::string& prefix,
                        const std::string& suffix) {
    std::string stem = dll;
    // Strip directory components and extension.
    const auto slash = stem.find_last_of("/\\");
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    for (char& c : stem) c = static_cast<char>(std::tolower((unsigned char)c));
    const auto dot = stem.find_last_of('.');
    if (dot != std::string::npos) stem = stem.substr(0, dot);
    return prefix + stem + suffix;
}

// Sanitise an ordinal import into a deterministic exportable symbol name.
std::string ordinal_symbol(const std::string& dll_stem, uint16_t ordinal) {
    return "win2lin.ord." + dll_stem + "." + std::to_string(ordinal);
}

std::string dll_stem(const std::string& dll) {
    std::string stem = dll;
    const auto slash = stem.find_last_of("/\\");
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    for (char& c : stem) c = static_cast<char>(std::tolower((unsigned char)c));
    const auto dot = stem.find_last_of('.');
    if (dot != std::string::npos) stem = stem.substr(0, dot);
    return stem;
}

// ---------------------------------------------------------------------------
// SYSV ELF hash (used for DT_HASH)
// ---------------------------------------------------------------------------
uint32_t elf_hash(const char* name) {
    uint32_t h = 0;
    for (; *name; ++name) {
        h = (h << 4) + static_cast<uint8_t>(*name);
        const uint32_t g = h & 0xf0000000u;
        if (g) h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

inline uint64_t align_up(uint64_t v, uint64_t a) {
    return (v + a - 1) & ~(a - 1);
}

// Small endian-safe byte buffer writer.
class ByteBuffer {
public:
    explicit ByteBuffer(size_t reserve = 0) { bytes_.reserve(reserve); }

    uint64_t size() const { return bytes_.size(); }

    void grow_to(uint64_t offset) {
        if (bytes_.size() < offset) bytes_.resize(offset, 0);
    }

    template <typename T>
    void write_at(uint64_t offset, const T& value) {
        grow_to(offset + sizeof(T));
        std::memcpy(bytes_.data() + offset, &value, sizeof(T));
    }

    void write_bytes_at(uint64_t offset, const void* data, size_t len) {
        grow_to(offset + len);
        std::memcpy(bytes_.data() + offset, data, len);
    }

    void append(const void* data, size_t len) {
        const auto* p = static_cast<const uint8_t*>(data);
        bytes_.insert(bytes_.end(), p, p + len);
    }

    std::vector<uint8_t>& data() { return bytes_; }

private:
    std::vector<uint8_t> bytes_;
};

// Metadata block describing where each synthetic ELF section lives.
struct MetaLayout {
    uint64_t base_rva = 0;    // RVA where the metadata PT_LOAD begins
    uint64_t total_size = 0;

    uint64_t dynstr_rva = 0, dynstr_size = 0;
    uint64_t dynsym_rva = 0, dynsym_size = 0;
    uint64_t hash_rva = 0, hash_size = 0;
    uint64_t rela_rva = 0, rela_size = 0;
    uint64_t dynamic_rva = 0, dynamic_size = 0;
};

} // namespace

// ---------------------------------------------------------------------------
// Construction / entry point
// ---------------------------------------------------------------------------
PeToElfConverter::PeToElfConverter(RelinkOptions opts) : opts_(std::move(opts)) {}

bool PeToElfConverter::fail(const std::string& msg) {
    last_error_ = msg;
    LOG_ERROR("%s", msg.c_str());
    return false;
}

bool PeToElfConverter::convert() {
    LOG_INFO("Relinking '%s' -> '%s'", opts_.input_pe.c_str(),
             opts_.output_elf.c_str());
    if (opts_.validate_features && !validate_system_features()) return false;
    if (!parse_pe()) return false;
    if (!rewrite_imports()) return false;
    if (!generate_elf()) return false;
    LOG_INFO("Relink completed successfully");
    return true;
}

bool PeToElfConverter::validate_system_features() {
    // Why: the PE may contain AVX2/FMA/BMI/LZCNT instructions. Instead of
    // disassembling megabytes of code we validate the *host* CPU, which is the
    // only environment the relinked binary can legally run on (native, no
    // emulation). A missing feature means guaranteed SIGILL at runtime.
    MissingFeatures missing = check_cpu_features();
    if (!missing.empty()) {
        std::ostringstream oss;
        oss << "Host CPU is missing required x86-64 features: ";
        for (size_t i = 0; i < missing.names.size(); ++i) {
            if (i) oss << ", ";
            oss << missing.names[i];
        }
        oss << ". Refusing to emit a binary that would SIGILL. "
               "Re-run with --no-feature-check to override.";
        return fail(oss.str());
    }
    LOG_DEBUG("CPU feature validation passed (AVX2/FMA/BMI1/BMI2/LZCNT)");
    return true;
}

// ---------------------------------------------------------------------------
// Parse
// ---------------------------------------------------------------------------
bool PeToElfConverter::parse_pe() {
    LOG_INFO("Parsing PE: %s", opts_.input_pe.c_str());
    std::unique_ptr<LIEF::PE::Binary> pe;
    try {
        pe = LIEF::PE::Parser::parse(opts_.input_pe);
    } catch (const std::exception& e) {
        return fail(std::string("LIEF threw while parsing PE: ") + e.what());
    }
    if (!pe) return fail("LIEF could not parse '" + opts_.input_pe + "'");

    if (pe->type() != LIEF::PE::PE_TYPE::PE32_PLUS) {
        return fail("Only PE32+ (64-bit) images are supported in Phase 1");
    }
    const auto machine = pe->header().machine();
    if (machine != LIEF::PE::Header::MACHINE_TYPES::AMD64 &&
        machine != LIEF::PE::Header::MACHINE_TYPES::ARM64) {
        return fail("Unsupported PE machine type (need AMD64)");
    }
    if (machine == LIEF::PE::Header::MACHINE_TYPES::ARM64) {
        return fail("ARM64 PE images are out of scope for Phase 1");
    }

    imagebase_ = pe->optional_header().imagebase();
    entry_rva_ = pe->optional_header().addressof_entrypoint();
    section_align_ = pe->optional_header().section_alignment();
    file_align_ = pe->optional_header().file_alignment();

    if (entry_rva_ == 0) {
        LOG_WARN("PE has no entry point (DLL-like image)");
    }

    // Sections. We keep a memory image zero-padded to virtual size so that
    // PE-bss semantics survive the PE -> ELF section mapping.
    sections_.clear();
    for (const LIEF::PE::Section& s : pe->sections()) {
        PeSection out;
        out.name = s.name();
        out.rva = s.virtual_address();
        out.vsize = s.virtual_size();
        out.raw_size = s.sizeof_raw_data();
        out.characteristics = s.characteristics();

        const auto content = s.content();
        size_t mem_size = static_cast<size_t>(std::max<uint64_t>(out.vsize, out.raw_size));
        out.content.assign(mem_size, 0);
        std::memcpy(out.content.data(), content.data(),
                    std::min(content.size(), mem_size));
        sections_.push_back(std::move(out));
    }
    if (sections_.empty()) return fail("PE contains no sections");

    // Imports: every entry gives us the RVA of the IAT slot the code calls
    // through. That slot becomes a GLOB_DAT relocation target in the ELF.
    imports_.clear();
    for (const LIEF::PE::Import& imp : pe->imports()) {
        const std::string dll(imp.name());
        for (const LIEF::PE::ImportEntry& e : imp.entries()) {
            PeImportSlot slot;
            slot.dll = dll;
            slot.iat_rva = e.iat_address();
            if (e.is_ordinal()) {
                slot.ordinal = e.ordinal();
            } else {
                slot.func = e.name();
            }
            imports_.push_back(std::move(slot));
        }
    }
    LOG_INFO("Parsed %zu sections and %zu imported symbols", sections_.size(),
             imports_.size());

    // Base relocations (needed for PIE/ET_DYN). DIR64 entries become
    // R_X86_64_RELATIVE records with addend = value - imagebase.
    base_relocs_.clear();
    has_relocs_ = false;
    if (pe->has_relocations()) {
        for (const LIEF::PE::Relocation& block : pe->relocations()) {
            for (const LIEF::PE::RelocationEntry& e : block.entries()) {
                if (e.type() == LIEF::PE::RelocationEntry::BASE_TYPES::ABS) {
                    continue; // padding
                }
                if (e.type() != LIEF::PE::RelocationEntry::BASE_TYPES::DIR64) {
                    LOG_WARN("Skipping unsupported base relocation type %d at RVA 0x%llx",
                             (int)e.type(),
                             (unsigned long long)(block.virtual_address() + e.position()));
                    continue;
                }
                const uint64_t rva = uint64_t(block.virtual_address()) + e.position();
                auto value = pe->get_int_from_virtual_address<uint64_t>(
                    rva, LIEF::Binary::VA_TYPES::RVA);
                if (!value) {
                    LOG_WARN("Could not read base relocation value at RVA 0x%llx",
                             (unsigned long long)rva);
                    continue;
                }
                base_relocs_.push_back({rva, *value});
                has_relocs_ = true;
            }
        }
    }
    LOG_INFO("Collected %zu base relocations (reloc table present: %s)",
             base_relocs_.size(), has_relocs_ ? "yes" : "no");
    if (!has_relocs_ && !opts_.linux_fixed_base) {
        LOG_WARN("PE has no base relocation table; forcing fixed-base ELF "
                 "(ET_EXEC @ 0x%llx) to preserve embedded absolute addresses",
                 (unsigned long long)imagebase_);
        opts_.linux_fixed_base = true;
    }

    // Delay imports are resolved at runtime through the delay-load helper,
    // which itself calls LoadLibraryA/GetProcAddress. We leave those slots
    // alone so the helper populates them with our shim's HMODULE.
    if (pe->has_delay_imports()) {
        for (const LIEF::PE::DelayImport& d : pe->delay_imports()) {
            LOG_INFO("Delay import present: %s (%zu funcs) - resolved at runtime",
                     std::string(d.name()).c_str(), (size_t)d.entries().size());
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Import table rewrite: strip Windows DLLs, bind to our shims
// ---------------------------------------------------------------------------
bool PeToElfConverter::rewrite_imports() {
    LOG_INFO("Rewriting imports: mapping Windows DLLs to native shims");
    std::map<std::string, ShimBinding> by_shim; // soname -> binding

    for (const PeImportSlot& slot : imports_) {
        const std::string soname = dll_to_shim(slot.dll, opts_.shim_prefix,
                                               opts_.shim_suffix);
        ShimBinding& b = by_shim[soname];
        b.pe_dll = dll_stem(slot.dll);
        b.shim_soname = soname;

        std::string sym;
        if (!slot.func.empty()) {
            sym = slot.func;
        } else {
            sym = ordinal_symbol(dll_stem(slot.dll), slot.ordinal);
            LOG_WARN("Ordinal import %s#%u has no name; expecting shim export '%s'",
                     slot.dll.c_str(), slot.ordinal, sym.c_str());
        }
        // Deduplicate function names within a shim.
        auto& fns = b.functions;
        if (std::find(fns.begin(), fns.end(), sym) == fns.end()) {
            fns.push_back(sym);
        }
    }

    if (by_shim.empty()) {
        LOG_WARN("PE imports no DLLs (fully self-contained image)");
    }

    bindings_.clear();
    for (auto& [soname, b] : by_shim) {
        LOG_INFO("  %-24s -> %-28s (%zu functions)", b.pe_dll.c_str(),
                 b.shim_soname.c_str(), b.functions.size());
        bindings_.push_back(std::move(b));
    }
    return true;
}

// ---------------------------------------------------------------------------
// ELF generation
// ---------------------------------------------------------------------------
bool PeToElfConverter::generate_elf() {
    constexpr uint64_t PAGE = 0x1000;
    const bool fixed_base = opts_.linux_fixed_base;
    const uint16_t e_type = fixed_base ? ET_EXEC : ET_DYN;
    const uint64_t base = fixed_base ? imagebase_ : 0;

    ByteBuffer buf(1 << 20);

    // ---- section virtual address sanity -----------------------------------
    uint64_t min_sec_rva = UINT64_MAX;
    uint64_t max_sec_end = 0;
    for (const PeSection& s : sections_) {
        min_sec_rva = std::min(min_sec_rva, s.rva);
        max_sec_end = std::max(max_sec_end, s.rva + std::max<uint64_t>(s.vsize, s.raw_size));
    }
    if (min_sec_rva == UINT64_MAX) min_sec_rva = PAGE;

    // ---- figure out which segments need to be writable --------------------
    // Relocation targets (IAT slots + base-reloc sites) must live in a
    // writable PT_LOAD because ld.so patches them in memory at load time.
    std::vector<uint64_t> patch_rvas;
    patch_rvas.reserve(imports_.size() + base_relocs_.size());
    for (const PeImportSlot& i : imports_) patch_rvas.push_back(i.iat_rva);
    if (!fixed_base) {
        for (const PeBaseReloc& r : base_relocs_) patch_rvas.push_back(r.rva);
    }
    std::sort(patch_rvas.begin(), patch_rvas.end());

    auto section_needs_write = [&](const PeSection& s) {
        const uint64_t lo = s.rva, hi = s.rva + std::max<uint64_t>(s.vsize, s.raw_size);
        auto it = std::lower_bound(patch_rvas.begin(), patch_rvas.end(), lo);
        return it != patch_rvas.end() && *it < hi;
    };

    // ---- zero out IAT slots in the copied section bytes -------------------
    for (const PeImportSlot& imp : imports_) {
        for (PeSection& s : sections_) {
            const uint64_t lo = s.rva;
            const uint64_t hi = s.rva + s.content.size();
            if (imp.iat_rva >= lo && imp.iat_rva + 8 <= hi) {
                std::memset(s.content.data() + (imp.iat_rva - lo), 0, 8);
            }
        }
    }

    // ---- map each PE section to a PT_LOAD ---------------------------------
    struct Segment {
        uint32_t type = PT_LOAD;
        uint32_t flags = PF_R;
        uint64_t offset = 0, vaddr = 0, filesz = 0, memsz = 0, align = PAGE;
    };
    std::vector<Segment> segments;
    // (segment -> section index, -1 for synthetic)
    std::vector<int> seg_section;

    // Header PT_LOAD covering the ELF header + program headers + .interp.
    // We place it at RVA 0 and require the first PE section to start at or
    // after min_sec_rva so we never clobber PE code.
    const uint64_t header_span = std::max<uint64_t>(min_sec_rva, PAGE);

    // Reserve the number of program headers: PHDR, INTERP, header LOAD,
    // N section LOADs, metadata LOAD, trampoline LOAD, DYNAMIC, GNU_STACK.
    size_t seg_count = 1 /*PHDR*/ + 1 /*INTERP*/ + 1 /*hdr LOAD*/ +
                       sections_.size() + 1 /*meta LOAD*/ +
                       1 /*tramp LOAD*/ + 1 /*DYNAMIC*/ + 1 /*GNU_STACK*/;
    const uint64_t phnum = seg_count;
    const uint64_t header_size = sizeof(Elf64_Ehdr) + phnum * sizeof(Elf64_Phdr);

    if (header_size > header_span) {
        return fail("PE sections start too early (RVA 0x" +
                    [&] { std::ostringstream o; o << std::hex << min_sec_rva; return o.str(); }() +
                    "); cannot fit ELF headers. Refusing to clobber code.");
    }

    // .interp string lives right after the program headers.
    const std::string interp = "/lib64/ld-linux-x86-64.so.2";
    const uint64_t interp_off = sizeof(Elf64_Ehdr) + phnum * sizeof(Elf64_Phdr);

    // Copy PE section contents into the output at file offset == RVA.
    // This keeps p_offset == p_vaddr(RVA) which trivially satisfies the ELF
    // congruence requirement p_offset % p_align == p_vaddr % p_align for any
    // base that is page aligned (all PE imagebases are).
    for (const PeSection& s : sections_) {
        if (s.raw_size > 0) {
            buf.write_bytes_at(s.rva, s.content.data(),
                               std::min<size_t>(s.raw_size, s.content.size()));
        }
    }
    buf.write_bytes_at(interp_off, interp.c_str(), interp.size() + 1);

    // ---- build import symbol table ----------------------------------------
    // One undefined symbol per unique import name.
    std::vector<std::string> sym_names;
    std::map<std::string, uint32_t> sym_index;
    auto intern_symbol = [&](const std::string& n) -> uint32_t {
        auto it = sym_index.find(n);
        if (it != sym_index.end()) return it->second;
        const uint32_t idx = static_cast<uint32_t>(sym_names.size()) + 1; // +1 for null
        sym_names.push_back(n);
        sym_index[n] = idx;
        return idx;
    };

    struct GlobDat {
        uint64_t rva;
        uint32_t sym;
    };
    std::vector<GlobDat> glob_dats;
    for (const PeImportSlot& imp : imports_) {
        std::string name = imp.func.empty()
                               ? ordinal_symbol(dll_stem(imp.dll), imp.ordinal)
                               : imp.func;
        const uint32_t idx = intern_symbol(name);
        glob_dats.push_back({imp.iat_rva, idx});
    }

    // ---- build dynamic string table ---------------------------------------
    // Layout: [0]=NUL, then shim sonames, runpath, then symbol names. Offsets
    // recorded so DT_NEEDED / st_name can reference them.
    std::string dynstr(1, '\0');
    std::map<std::string, uint64_t> dynstr_off;
    auto add_dynstr = [&](const std::string& s) -> uint64_t {
        auto it = dynstr_off.find(s);
        if (it != dynstr_off.end()) return it->second;
        const uint64_t off = dynstr.size();
        dynstr += s;
        dynstr.push_back('\0');
        dynstr_off[s] = off;
        return off;
    };

    std::vector<uint64_t> needed_offsets;
    for (const ShimBinding& b : bindings_) {
        needed_offsets.push_back(add_dynstr(b.shim_soname));
    }
    uint64_t runpath_off = 0;
    if (!opts_.runpath.empty()) runpath_off = add_dynstr(opts_.runpath);
    std::vector<uint64_t> sym_name_offsets(sym_names.size());
    for (size_t i = 0; i < sym_names.size(); ++i) {
        sym_name_offsets[i] = add_dynstr(sym_names[i]);
    }

    // ---- build .rela.dyn ---------------------------------------------------
    struct Rela {
        uint64_t offset;
        uint64_t info;
        int64_t addend;
    };
    std::vector<Rela> relas;
    if (!fixed_base) {
        for (const PeBaseReloc& r : base_relocs_) {
            const int64_t addend =
                static_cast<int64_t>(r.value) - static_cast<int64_t>(imagebase_);
            relas.push_back({base + r.rva, ELF64_R_INFO(0, R_X86_64_RELATIVE), addend});
        }
    }
    for (const GlobDat& g : glob_dats) {
        relas.push_back({base + g.rva, ELF64_R_INFO(g.sym, R_X86_64_GLOB_DAT), 0});
    }

    // ---- lay out the metadata block ---------------------------------------
    MetaLayout meta;
    meta.base_rva = align_up(max_sec_end, PAGE);

    uint64_t cursor = 0; // offset within the metadata block
    auto place = [&](uint64_t size, uint64_t align) -> uint64_t {
        cursor = align_up(cursor, align);
        const uint64_t at = cursor;
        cursor += size;
        return at;
    };

    const uint64_t nsyms = sym_names.size() + 1; // + null
    const uint64_t dynstr_size = dynstr.size();
    const uint64_t dynsym_size = nsyms * sizeof(Elf64_Sym);
    const uint64_t nbucket = std::max<uint64_t>(1, nsyms);
    const uint64_t hash_size = (2 + nbucket + nsyms) * sizeof(uint32_t);
    const uint64_t rela_size = relas.size() * sizeof(Elf64_Rela);
    // Dynamic entry count: NEEDED*N, HASH, STRTAB, STRSZ, SYMTAB, SYMENT,
    // RELA, RELASZ, RELAENT, RUNPATH, FLAGS, FLAGS_1, NULL
    const uint64_t dyn_count = bindings_.size() + 13 + (runpath_off ? 0 : 0);
    const uint64_t dynamic_size = dyn_count * sizeof(Elf64_Dyn);

    meta.dynstr_rva = meta.base_rva + place(dynstr_size, 1);
    meta.dynsym_rva = meta.base_rva + place(dynsym_size, 8);
    meta.hash_rva   = meta.base_rva + place(hash_size, 4);
    meta.rela_rva   = meta.base_rva + place(rela_size, 8);
    meta.dynamic_rva = meta.base_rva + place(dynamic_size, 8);
    meta.total_size = cursor;
    meta.dynstr_size = dynstr_size;
    meta.dynsym_size = dynsym_size;
    meta.hash_size = hash_size;
    meta.rela_size = rela_size;
    meta.dynamic_size = dynamic_size;

    // ---- entry trampoline --------------------------------------------------
    // The Linux kernel jumps to e_entry with rsp 16-byte aligned (rsp % 16 ==
    // 0). Windows calls a PE entry point, so the PE's own prologue assumes rsp
    // % 16 == 8. Feeding it a 16-aligned stack makes MS-ABI callees (our
    // shims) spill SSE registers with `movaps` to a misaligned address ->
    // SIGSEGV. This trampoline re-establishes the expected alignment via a
    // `call` (which pushes a return address) and provides a clean exit stub in
    // case the PE entry ever returns.
    const uint64_t tramp_rva = align_up(meta.base_rva + meta.total_size, PAGE);
    std::vector<uint8_t> tramp;
    tramp.reserve(32);
    // and rsp, -16
    tramp.insert(tramp.end(), {0x48, 0x83, 0xe4, 0xf0});
    // call rel32 (target = PE entry RVA)
    {
        const uint64_t call_end = tramp_rva + tramp.size() + 5;
        const int64_t rel = static_cast<int64_t>(entry_rva_) -
                            static_cast<int64_t>(call_end);
        tramp.push_back(0xE8);
        for (int i = 0; i < 4; ++i)
            tramp.push_back(static_cast<uint8_t>((rel >> (8 * i)) & 0xFF));
    }
    // return stub: xor edi,edi; mov eax,60; syscall  (exit(0))
    tramp.insert(tramp.end(), {0x31, 0xff});
    tramp.insert(tramp.end(), {0xb8, 0x3c, 0x00, 0x00, 0x00});
    tramp.insert(tramp.end(), {0x0f, 0x05});
    buf.write_bytes_at(tramp_rva, tramp.data(), tramp.size());

    // ---- emit .dynstr ------------------------------------------------------
    buf.write_bytes_at(meta.dynstr_rva, dynstr.data(), dynstr.size());

    // ---- emit .dynsym ------------------------------------------------------
    {
        uint64_t off = meta.dynsym_rva;
        Elf64_Sym null_sym{};
        buf.write_at(off, null_sym);
        off += sizeof(Elf64_Sym);
        for (size_t i = 0; i < sym_names.size(); ++i) {
            Elf64_Sym sym{};
            sym.st_name = static_cast<uint32_t>(sym_name_offsets[i]);
            sym.st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE);
            sym.st_other = STV_DEFAULT;
            sym.st_shndx = SHN_UNDEF;
            sym.st_value = 0;
            sym.st_size = 0;
            buf.write_at(off, sym);
            off += sizeof(Elf64_Sym);
        }
    }

    // ---- emit .hash (SYSV) -------------------------------------------------
    {
        std::vector<uint32_t> hash(2 + nbucket + nsyms, 0);
        hash[0] = static_cast<uint32_t>(nbucket);
        hash[1] = static_cast<uint32_t>(nsyms);
        auto* buckets = hash.data() + 2;
        auto* chains = buckets + nbucket;
        for (uint64_t i = 1; i < nsyms; ++i) {
            const std::string& nm = sym_names[i - 1];
            const uint32_t b = elf_hash(nm.c_str()) % nbucket;
            chains[i] = buckets[b];
            buckets[b] = static_cast<uint32_t>(i);
        }
        buf.write_bytes_at(meta.hash_rva, hash.data(), hash.size() * sizeof(uint32_t));
    }

    // ---- emit .rela.dyn ----------------------------------------------------
    {
        uint64_t off = meta.rela_rva;
        for (const Rela& r : relas) {
            Elf64_Rela er{};
            er.r_offset = r.offset;
            er.r_info = r.info;
            er.r_addend = r.addend;
            buf.write_at(off, er);
            off += sizeof(Elf64_Rela);
        }
    }

    // ---- emit .dynamic -----------------------------------------------------
    {
        std::vector<Elf64_Dyn> dyn;
        auto put = [&](int64_t tag, uint64_t val) {
            Elf64_Dyn d{};
            d.d_tag = tag;
            d.d_un.d_val = val;
            dyn.push_back(d);
        };
        for (uint64_t o : needed_offsets) put(DT_NEEDED, o);
        // Pointer tags are virtual addresses: absolute for ET_EXEC, image-
        // relative (base 0) for ET_DYN. `base` already encodes the difference.
        put(DT_HASH, base + meta.hash_rva);
        put(DT_STRTAB, base + meta.dynstr_rva);
        put(DT_SYMTAB, base + meta.dynsym_rva);
        put(DT_STRSZ, meta.dynstr_size);
        put(DT_SYMENT, sizeof(Elf64_Sym));
        put(DT_RELA, base + meta.rela_rva);
        put(DT_RELASZ, meta.rela_size);
        put(DT_RELAENT, sizeof(Elf64_Rela));
        if (runpath_off) put(DT_RUNPATH, runpath_off);
        put(DT_FLAGS, DF_BIND_NOW);      // resolve GLOB_DAT eagerly
        put(DT_FLAGS_1, DF_1_NOW);
        put(DT_NULL, 0);
        uint64_t off = meta.dynamic_rva;
        for (const Elf64_Dyn& d : dyn) {
            buf.write_at(off, d);
            off += sizeof(Elf64_Dyn);
        }
        meta.dynamic_size = dyn.size() * sizeof(Elf64_Dyn);
    }

    // ---- program headers ---------------------------------------------------
    std::vector<Segment> phdrs;
    // PT_PHDR (points at phdrs inside the header LOAD)
    {
        Segment s;
        s.type = PT_PHDR;
        s.flags = PF_R;
        s.offset = sizeof(Elf64_Ehdr);
        s.vaddr = base + sizeof(Elf64_Ehdr);
        s.filesz = phnum * sizeof(Elf64_Phdr);
        s.memsz = s.filesz;
        s.align = 8;
        phdrs.push_back(s);
    }
    // PT_INTERP
    {
        Segment s;
        s.type = PT_INTERP;
        s.flags = PF_R;
        s.offset = interp_off;
        s.vaddr = base + interp_off;
        s.filesz = interp.size() + 1;
        s.memsz = s.filesz;
        s.align = 1;
        phdrs.push_back(s);
    }
    // Header LOAD [0, header_span)
    {
        Segment s;
        s.type = PT_LOAD;
        s.flags = PF_R;
        s.offset = 0;
        s.vaddr = base + 0;
        s.filesz = header_span;
        s.memsz = header_span;
        s.align = PAGE;
        phdrs.push_back(s);
    }
    // One LOAD per PE section
    for (const PeSection& sec : sections_) {
        Segment s;
        s.type = PT_LOAD;
        s.flags = PF_R;
        if (sec.characteristics & pe::SCN_MEM_EXECUTE) s.flags |= PF_X;
        if ((sec.characteristics & pe::SCN_MEM_WRITE) || section_needs_write(sec)) {
            s.flags |= PF_W;
        }
        s.offset = sec.rva;
        s.vaddr = base + sec.rva;
        s.filesz = std::min<uint64_t>(sec.raw_size, sec.content.size());
        s.memsz = std::max<uint64_t>(sec.vsize, s.filesz);
        s.align = PAGE;
        phdrs.push_back(s);
    }
    // Metadata LOAD
    {
        Segment s;
        s.type = PT_LOAD;
        s.flags = PF_R;
        s.offset = meta.base_rva;
        s.vaddr = base + meta.base_rva;
        s.filesz = meta.total_size;
        s.memsz = meta.total_size;
        s.align = PAGE;
        phdrs.push_back(s);
    }
    // Entry trampoline LOAD (executable)
    {
        Segment s;
        s.type = PT_LOAD;
        s.flags = PF_R | PF_X;
        s.offset = tramp_rva;
        s.vaddr = base + tramp_rva;
        s.filesz = tramp.size();
        s.memsz = tramp.size();
        s.align = PAGE;
        phdrs.push_back(s);
    }
    // PT_DYNAMIC
    {
        Segment s;
        s.type = PT_DYNAMIC;
        s.flags = PF_R;
        s.offset = meta.dynamic_rva;
        s.vaddr = base + meta.dynamic_rva;
        s.filesz = meta.dynamic_size;
        s.memsz = meta.dynamic_size;
        s.align = 8;
        phdrs.push_back(s);
    }
    // PT_GNU_STACK (non-executable stack)
    {
        Segment s;
        s.type = PT_GNU_STACK;
        s.flags = PF_R | PF_W;
        s.offset = 0;
        s.vaddr = 0;
        s.filesz = 0;
        s.memsz = 0;
        s.align = 0x10;
        phdrs.push_back(s);
    }

    if (phdrs.size() != phnum) {
        return fail("Internal error: program header count mismatch");
    }

    // ---- emit program headers ---------------------------------------------
    {
        uint64_t off = sizeof(Elf64_Ehdr);
        for (const Segment& s : phdrs) {
            Elf64_Phdr ph{};
            ph.p_type = s.type;
            ph.p_flags = s.flags;
            ph.p_offset = s.offset;
            ph.p_vaddr = s.vaddr;
            ph.p_paddr = s.vaddr;
            ph.p_filesz = s.filesz;
            ph.p_memsz = s.memsz;
            ph.p_align = s.align;
            buf.write_at(off, ph);
            off += sizeof(Elf64_Phdr);
        }
    }

    // ---- section headers (for tooling / test introspection) ---------------
    struct Shdr {
        std::string name;
        uint32_t type = SHT_PROGBITS;
        uint64_t flags = 0;
        uint64_t addr = 0, offset = 0, size = 0, addralign = 1, entsize = 0;
    };
    std::vector<Shdr> shdrs;
    shdrs.push_back({"", SHT_NULL, 0, 0, 0, 0, 0, 0});
    shdrs.push_back({".interp", SHT_PROGBITS, SHF_ALLOC, base + interp_off,
                     interp_off, interp.size() + 1, 1, 0});
    shdrs.push_back({".dynstr", SHT_STRTAB, SHF_ALLOC, base + meta.dynstr_rva,
                     meta.dynstr_rva, meta.dynstr_size, 1, 0});
    shdrs.push_back({".dynsym", SHT_DYNSYM, SHF_ALLOC, base + meta.dynsym_rva,
                     meta.dynsym_rva, meta.dynsym_size, 8, sizeof(Elf64_Sym)});
    shdrs.push_back({".hash", SHT_HASH, SHF_ALLOC, base + meta.hash_rva,
                     meta.hash_rva, meta.hash_size, 4, sizeof(uint32_t)});
    shdrs.push_back({".rela.dyn", SHT_RELA, SHF_ALLOC, base + meta.rela_rva,
                     meta.rela_rva, meta.rela_size, 8, sizeof(Elf64_Rela)});
    shdrs.push_back({".dynamic", SHT_DYNAMIC, SHF_ALLOC | SHF_WRITE,
                     base + meta.dynamic_rva, meta.dynamic_rva,
                     meta.dynamic_size, 8, sizeof(Elf64_Dyn)});
    shdrs.push_back({".win2lin.tramp", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR,
                     base + tramp_rva, tramp_rva, tramp.size(), 16, 0});
    // PE sections, preserving original names so tooling can correlate them.
    for (const PeSection& sec : sections_) {
        Shdr sh;
        sh.name = sec.name.empty() ? ".raw" : sec.name;
        const bool bss = sec.raw_size == 0 && sec.vsize > 0;
        sh.type = bss ? SHT_NOBITS : SHT_PROGBITS;
        sh.flags = SHF_ALLOC;
        if (sec.characteristics & pe::SCN_MEM_EXECUTE) sh.flags |= SHF_EXECINSTR;
        if ((sec.characteristics & pe::SCN_MEM_WRITE) || section_needs_write(sec)) {
            sh.flags |= SHF_WRITE;
        }
        sh.addr = base + sec.rva;
        sh.offset = sec.rva;
        sh.size = bss ? sec.vsize : sec.raw_size;
        sh.addralign = std::max<uint64_t>(1, sec.rva ? PAGE : 1);
        sh.entsize = 0;
        shdrs.push_back(sh);
    }

    // .shstrtab at the very end.
    std::string shstr(1, '\0');
    std::vector<uint32_t> sh_name_off(shdrs.size());
    for (size_t i = 1; i < shdrs.size(); ++i) {
        sh_name_off[i] = static_cast<uint32_t>(shstr.size());
        shstr += shdrs[i].name;
        shstr.push_back('\0');
    }
    const uint32_t shstrtab_idx = static_cast<uint32_t>(shdrs.size());
    {
        Shdr sh;
        sh.name = ".shstrtab";
        sh.type = SHT_STRTAB;
        sh.flags = 0;
        sh.addr = 0;
        sh.addralign = 1;
        shdrs.push_back(sh);
        sh_name_off.push_back(static_cast<uint32_t>(shstr.size()));
        shstr += ".shstrtab";
        shstr.push_back('\0');
    }

    // Section header table placed after all mapped content (not loaded).
    uint64_t file_end =
        std::max<uint64_t>(meta.base_rva + meta.total_size,
                           max_sec_end);
    file_end = std::max<uint64_t>(file_end, interp_off + interp.size() + 1);
    file_end = std::max<uint64_t>(file_end, tramp_rva + tramp.size());
    uint64_t shoff = align_up(file_end, 8);
    const uint64_t shstr_off = shoff + shdrs.size() * sizeof(Elf64_Shdr);
    shdrs[shstrtab_idx].offset = shstr_off;
    shdrs[shstrtab_idx].size = shstr.size();
    buf.write_bytes_at(shstr_off, shstr.data(), shstr.size());

    {
        uint64_t off = shoff;
        for (size_t i = 0; i < shdrs.size(); ++i) {
            Elf64_Shdr sh{};
            sh.sh_name = (i == 0) ? 0 : sh_name_off[i];
            sh.sh_type = shdrs[i].type;
            sh.sh_flags = shdrs[i].flags;
            sh.sh_addr = shdrs[i].addr;
            sh.sh_offset = shdrs[i].offset;
            sh.sh_size = shdrs[i].size;
            // Symbol/relocation sections must point at their companion
            // tables or the dynamic loader / readelf cannot interpret them.
            switch (shdrs[i].type) {
                case SHT_DYNSYM: sh.sh_link = 2 /* .dynstr */; sh.sh_info = 1; break;
                case SHT_HASH:   sh.sh_link = 2 /* .dynstr */; break;
                case SHT_DYNAMIC: sh.sh_link = 2 /* .dynstr */; break;
                case SHT_RELA:   sh.sh_link = 3 /* .dynsym */; break;
                default: sh.sh_link = 0; break;
            }
            sh.sh_info = (shdrs[i].type == SHT_DYNSYM) ? 1 : 0;
            sh.sh_addralign = shdrs[i].addralign;
            sh.sh_entsize = shdrs[i].entsize;
            buf.write_at(off, sh);
            off += sizeof(Elf64_Shdr);
        }
    }

    // ---- ELF header --------------------------------------------------------
    {
        Elf64_Ehdr eh{};
        eh.e_ident[EI_MAG0] = 0x7f;
        eh.e_ident[EI_MAG1] = 'E';
        eh.e_ident[EI_MAG2] = 'L';
        eh.e_ident[EI_MAG3] = 'F';
        eh.e_ident[EI_CLASS] = ELFCLASS64;
        eh.e_ident[EI_DATA] = ELFDATA2LSB;
        eh.e_ident[EI_VERSION] = EV_CURRENT;
        eh.e_ident[EI_OSABI] = ELFOSABI_SYSV;
        eh.e_type = e_type;
        eh.e_machine = EM_X86_64;
        eh.e_version = EV_CURRENT;
        eh.e_entry = base + tramp_rva;
        eh.e_phoff = sizeof(Elf64_Ehdr);
        eh.e_shoff = shoff;
        eh.e_flags = 0;
        eh.e_ehsize = sizeof(Elf64_Ehdr);
        eh.e_phentsize = sizeof(Elf64_Phdr);
        eh.e_phnum = static_cast<uint16_t>(phdrs.size());
        eh.e_shentsize = sizeof(Elf64_Shdr);
        eh.e_shnum = static_cast<uint16_t>(shdrs.size());
        eh.e_shstrndx = shstrtab_idx;
        buf.write_at(0, eh);
    }

    // ---- write to disk -----------------------------------------------------
    const std::vector<uint8_t>& out = buf.data();
    std::ofstream ofs(opts_.output_elf, std::ios::binary | std::ios::trunc);
    if (!ofs) return fail("Cannot open output file: " + opts_.output_elf);
    ofs.write(reinterpret_cast<const char*>(out.data()),
              static_cast<std::streamsize>(out.size()));
    if (!ofs) return fail("Write failed: " + opts_.output_elf);
    ofs.close();

    ::chmod(opts_.output_elf.c_str(), 0755);

    LOG_INFO("Wrote ELF: %s (%zu bytes, %s, %zu PT_LOAD-ish phdrs, %zu symbols)",
             opts_.output_elf.c_str(), out.size(),
             fixed_base ? "ET_EXEC" : "ET_DYN", phdrs.size(), sym_names.size());
    for (const ShimBinding& b : bindings_) {
        LOG_INFO("  NEEDED %s", b.shim_soname.c_str());
    }
    return true;
}

} // namespace win2lin::relinker
