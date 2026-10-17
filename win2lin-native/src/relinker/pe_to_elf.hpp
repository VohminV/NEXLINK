#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace win2lin::relinker {

// Options controlling PE->ELF static relinking.
struct RelinkOptions {
    std::string input_pe;
    std::string output_elf;
    // Disable ASLR (PIE): emit ET_EXEC anchored at the PE imagebase.
    // Required when the PE was linked without base relocations but still
    // embeds absolute addresses.
    bool linux_fixed_base = false;
    // Abort if the host CPU lacks AVX2/FMA/BMI1/BMI2/LZCNT (PE may use them).
    bool validate_features = true;
    // Shim .so name pattern: "<prefix><dll_stem><suffix>", e.g.
    // "libwin_" + "kernel32" + ".so" -> libwin_kernel32.so
    std::string shim_prefix = "libwin_";
    std::string shim_suffix = ".so";
    // DT_RUNPATH baked into the output so shims are found next to the binary.
    std::string runpath = "$ORIGIN";
};

// Per-DLL resolution report for the import rewrite stage.
struct ShimBinding {
    std::string pe_dll;      // original Windows DLL name (lowercased)
    std::string shim_soname; // injected DT_NEEDED entry
    std::vector<std::string> functions;
};

class PeToElfConverter {
public:
    explicit PeToElfConverter(RelinkOptions opts);
    bool convert();
    const std::vector<ShimBinding>& bindings() const { return bindings_; }
    const std::string& last_error() const { return last_error_; }

private:
    bool validate_system_features();
    bool parse_pe();
    bool rewrite_imports();
    bool generate_elf();

    bool fail(const std::string& msg);

    RelinkOptions opts_;
    std::vector<ShimBinding> bindings_;
    std::string last_error_;

    // Populated by parse_pe()
    struct PeSection {
        std::string name;
        uint64_t rva;        // virtual address relative to imagebase
        uint64_t vsize;      // virtual size in memory
        uint32_t raw_size;   // size of initialized data on disk
        uint32_t characteristics;
        std::vector<uint8_t> content; // raw content (zero-padded to vsize)
    };
    struct PeImportSlot {
        std::string dll;
        std::string func;    // empty if ordinal-only
        uint16_t ordinal;    // valid if func.empty()
        uint64_t iat_rva;    // RVA of the IAT slot that must hold the resolved address
    };
    struct PeBaseReloc {
        uint64_t rva;        // RVA of the 8-byte slot to patch
        uint64_t value;      // current absolute value (based at imagebase)
    };

    uint64_t imagebase_ = 0;
    uint64_t entry_rva_ = 0;
    uint32_t section_align_ = 0x1000;
    uint32_t file_align_ = 0x200;
    std::vector<PeSection> sections_;
    std::vector<PeImportSlot> imports_;
    std::vector<PeBaseReloc> base_relocs_;
    bool has_relocs_ = false;
};

} // namespace win2lin::relinker
