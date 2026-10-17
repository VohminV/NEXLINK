// Unit + integration tests for the PE -> ELF relinker.
//
// Strategy:
//   * Convert a MinGW-built fixture PE using the relinker library directly.
//   * Validate the produced ELF by parsing its raw bytes (ELF header, program
//     headers, .dynamic, .dynsym) - independent of the writer's own structs.
//   * Execute the relinked binary with the shim directory on LD_LIBRARY_PATH
//     and verify it prints the expected output through libwin_kernel32.so.
//   * Cover negative paths (missing file, non-PE input).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <elf.h>
#include <sys/wait.h>
#include <unistd.h>

#include "relinker/pe_to_elf.hpp"

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool cond, const std::string& what) {
    ++g_checks;
    if (cond) {
        std::cout << "  [ OK ] " << what << "\n";
    } else {
        ++g_failures;
        std::cout << "  [FAIL] " << what << "\n";
    }
}

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
}

template <typename T>
const T* at(const std::vector<uint8_t>& buf, uint64_t off) {
    if (off + sizeof(T) > buf.size()) return nullptr;
    return reinterpret_cast<const T*>(buf.data() + off);
}

std::string cstr_at(const std::vector<uint8_t>& buf, uint64_t off) {
    std::string s;
    while (off < buf.size() && buf[off] != '\0') s.push_back(char(buf[off++]));
    return s;
}

// Runs `path` with LD_LIBRARY_PATH=shim_dir and returns (exit_code, stdout).
struct RunResult {
    int code = -1;
    std::string out;
};

RunResult run_captured(const std::string& path, const std::string& shim_dir) {
    RunResult r;
    int pipefd[2];
    if (pipe(pipefd) != 0) return r;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return r;
    }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        std::string lp = shim_dir;
        setenv("LD_LIBRARY_PATH", lp.c_str(), 1);
        execl(path.c_str(), path.c_str(), (char*)nullptr);
        _exit(127);
    }
    close(pipefd[1]);
    char tmp[4096];
    ssize_t n;
    while ((n = read(pipefd[0], tmp, sizeof(tmp))) > 0) {
        r.out.append(tmp, size_t(n));
    }
    close(pipefd[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) r.code = WEXITSTATUS(status);
    return r;
}

struct ElfFacts {
    bool valid = false;
    uint16_t type = 0;
    uint16_t machine = 0;
    uint64_t entry = 0;
    std::vector<std::string> needed;
    std::vector<std::string> dynsyms;
    bool entry_in_exec_segment = false;
    bool has_trampoline = false;
};

ElfFacts parse_elf(const std::vector<uint8_t>& buf) {
    ElfFacts f;
    const Elf64_Ehdr* eh = at<Elf64_Ehdr>(buf, 0);
    if (!eh) return f;
    if (std::memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0) return f;
    if (eh->e_ident[EI_CLASS] != ELFCLASS64) return f;
    if (eh->e_ident[EI_DATA] != ELFDATA2LSB) return f;
    f.valid = true;
    f.type = eh->e_type;
    f.machine = eh->e_machine;
    f.entry = eh->e_entry;

    // Executable LOAD segments (to check the entry lands in one).
    struct ExecRange { uint64_t lo, hi; };
    std::vector<ExecRange> exec_ranges;

    uint64_t dyn_off = 0, dyn_size = 0;
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        const Elf64_Phdr* ph = at<Elf64_Phdr>(buf, eh->e_phoff +
                                              uint64_t(i) * eh->e_phentsize);
        if (!ph) break;
        if (ph->p_type == PT_LOAD && (ph->p_flags & PF_X)) {
            exec_ranges.push_back({ph->p_vaddr, ph->p_vaddr + ph->p_memsz});
        }
        if (ph->p_type == PT_DYNAMIC) {
            dyn_off = ph->p_offset;
            dyn_size = ph->p_filesz;
        }
    }
    for (const auto& er : exec_ranges) {
        if (f.entry >= er.lo && f.entry < er.hi) f.entry_in_exec_segment = true;
    }

    // Section headers give us .dynsym / .dynstr to read symbols.
    uint64_t symtab_off = 0, strtab_off = 0;
    uint64_t sym_count = 0;
    for (uint16_t i = 0; i < eh->e_shnum; ++i) {
        const Elf64_Shdr* sh = at<Elf64_Shdr>(buf, eh->e_shoff +
                                              uint64_t(i) * eh->e_shentsize);
        if (!sh) break;
        if (sh->sh_type == SHT_DYNSYM) {
            symtab_off = sh->sh_offset;
            sym_count = sh->sh_size / sizeof(Elf64_Sym);
            const Elf64_Shdr* str = at<Elf64_Shdr>(
                buf, eh->e_shoff + uint64_t(sh->sh_link) * eh->e_shentsize);
            if (str) strtab_off = str->sh_offset;
        }
    }
    for (uint64_t i = 1; i < sym_count; ++i) {
        const Elf64_Sym* sym = at<Elf64_Sym>(buf, symtab_off +
                                             i * sizeof(Elf64_Sym));
        if (!sym) break;
        f.dynsyms.push_back(cstr_at(buf, strtab_off + sym->st_name));
    }

    // Parse .dynamic for DT_NEEDED strings.
    for (uint64_t off = 0; off + sizeof(Elf64_Dyn) <= dyn_size;
         off += sizeof(Elf64_Dyn)) {
        const Elf64_Dyn* d = at<Elf64_Dyn>(buf, dyn_off + off);
        if (!d) break;
        if (d->d_tag == DT_NULL) break;
        if (d->d_tag == DT_NEEDED) {
            // d_val is an offset into .dynstr; find it via the dynsym strtab.
            f.needed.push_back(cstr_at(buf, strtab_off + d->d_un.d_val));
        }
    }

    // Trampoline section presence (proves the alignment shim was emitted).
    for (uint16_t i = 0; i < eh->e_shnum; ++i) {
        const Elf64_Shdr* sh = at<Elf64_Shdr>(buf, eh->e_shoff +
                                              uint64_t(i) * eh->e_shentsize);
        if (!sh) break;
        const Elf64_Shdr* str = at<Elf64_Shdr>(
            buf, eh->e_shoff + uint64_t(eh->e_shstrndx) * eh->e_shentsize);
        if (!str) break;
        std::string name = cstr_at(buf, str->sh_offset + sh->sh_name);
        if (name == ".win2lin.tramp") f.has_trampoline = true;
    }
    return f;
}

bool contains(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& e : v) if (e == s) return true;
    return false;
}

} // namespace

int main() {
    const std::string pe_path = WIN2LIN_TEST_PE;
    const std::string shim_dir = WIN2LIN_SHIM_DIR;
    const std::string build_dir = WIN2LIN_TEST_BUILD_DIR;

    std::cout << "== relinker tests ==\n";
    std::cout << "  fixture PE : " << (pe_path.empty() ? "(none)" : pe_path) << "\n";
    std::cout << "  shim dir   : " << shim_dir << "\n";

    // ---- negative: missing input -----------------------------------------
    {
        win2lin::relinker::RelinkOptions o;
        o.input_pe = build_dir + "/does_not_exist.exe";
        o.output_elf = build_dir + "/unused.elf";
        win2lin::relinker::PeToElfConverter c(o);
        check(!c.convert(), "convert() fails on missing input");
        check(!c.last_error().empty(), "last_error() populated on failure");
    }

    // ---- negative: non-PE input ------------------------------------------
    {
        // Feed the test executable itself (an ELF), not a PE.
        win2lin::relinker::RelinkOptions o;
        o.input_pe = "/bin/ls";
        o.output_elf = build_dir + "/nope.elf";
        win2lin::relinker::PeToElfConverter c(o);
        check(!c.convert(), "convert() rejects a non-PE input");
    }

    if (pe_path.empty()) {
        std::cout << "  (MinGW not found; skipping PE conversion tests)\n";
        std::cout << g_checks - g_failures << "/" << g_checks << " checks passed\n";
        return g_failures == 0 ? 0 : 1;
    }

    // ---- convert fixture --------------------------------------------------
    const std::string out_elf = build_dir + "/hello_win.elf";
    {
        win2lin::relinker::RelinkOptions o;
        o.input_pe = pe_path;
        o.output_elf = out_elf;
        win2lin::relinker::PeToElfConverter c(o);
        bool ok = c.convert();
        check(ok, "convert() succeeds on fixture PE");
        if (!ok) std::cout << "       error: " << c.last_error() << "\n";
        check(c.bindings().size() == 1, "one shim binding produced");
        if (!c.bindings().empty()) {
            check(c.bindings()[0].shim_soname == "libwin_kernel32.so",
                  "kernel32 mapped to libwin_kernel32.so");
        }
    }

    // ---- validate ELF bytes ----------------------------------------------
    std::vector<uint8_t> buf = read_file(out_elf);
    check(!buf.empty(), "output ELF file exists and is non-empty");
    ElfFacts f = parse_elf(buf);
    check(f.valid, "output is a valid little-endian ELF64");
    check(f.machine == EM_X86_64, "machine is EM_X86_64");
    check(f.type == ET_DYN || f.type == ET_EXEC, "type is ET_DYN or ET_EXEC");
    check(f.entry != 0, "entry point is non-zero");
    check(f.entry_in_exec_segment, "entry point lies in an executable segment");
    check(f.has_trampoline, "entry trampoline section present");
    check(contains(f.needed, "libwin_kernel32.so"),
          "DT_NEEDED lists libwin_kernel32.so");
    check(contains(f.dynsyms, "GetStdHandle"), "dynsym has GetStdHandle");
    check(contains(f.dynsyms, "WriteFile"), "dynsym has WriteFile");
    check(contains(f.dynsyms, "ExitProcess"), "dynsym has ExitProcess");

    // ---- integration: actually run it ------------------------------------
    RunResult r = run_captured(out_elf, shim_dir);
    check(r.code == 0, "relinked PE exits 0");
    check(r.out.find("Hello from relinked PE!") != std::string::npos,
          "relinked PE prints expected message via shim");
    if (r.out.find("Hello from relinked PE!") == std::string::npos) {
        std::cout << "       stdout: " << r.out << "\n";
    }

    std::cout << (g_checks - g_failures) << "/" << g_checks
              << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
