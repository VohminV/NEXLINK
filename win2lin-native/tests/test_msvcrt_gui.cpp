// Integration tests for Phase 1.5: MSVCRT bootstrap + mock user32 + DXGI
// skeleton.
//
// Strategy:
//   * Convert the MinGW MSVCRT-linked fixture (hello_window.exe) with the
//     relinker library directly.
//   * Validate the produced ELF: DT_NEEDED covers the four shims (kernel32,
//     msvcrt, user32, dxgi), dynsym contains the windowing/DXGI entry points
//     plus the injected nexlink_crt_bootstrap hook.
//   * Execute the relinked binary with the shim directory on LD_LIBRARY_PATH
//     and verify the full pipeline: CRT bootstrap -> global C++ constructor ->
//     mock window creation -> DXGI factory + adapter enumeration -> exit 0.
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

struct RunResult {
    int code = -1;
    std::string out;
};

// Runs `path` with LD_LIBRARY_PATH=shim_dir and captures stdout+stderr.
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
        setenv("LD_LIBRARY_PATH", shim_dir.c_str(), 1);
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
    std::vector<std::string> needed;
    std::vector<std::string> dynsyms;
    bool has_crt_boot_reloc = false;
};

ElfFacts parse_elf(const std::vector<uint8_t>& buf) {
    ElfFacts f;
    const Elf64_Ehdr* eh = at<Elf64_Ehdr>(buf, 0);
    if (!eh) return f;
    if (std::memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0) return f;
    f.valid = true;
    f.type = eh->e_type;

    uint64_t dyn_off = 0, dyn_size = 0;
    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        const Elf64_Phdr* ph = at<Elf64_Phdr>(buf, eh->e_phoff +
                                              uint64_t(i) * eh->e_phentsize);
        if (!ph) break;
        if (ph->p_type == PT_DYNAMIC) {
            dyn_off = ph->p_offset;
            dyn_size = ph->p_filesz;
        }
    }

    uint64_t symtab_off = 0, strtab_off = 0, sym_count = 0;
    uint64_t rela_off = 0, rela_size = 0;
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
        if (sh->sh_type == SHT_RELA) {
            rela_off = sh->sh_offset;
            rela_size = sh->sh_size;
        }
    }

    for (uint64_t i = 1; i < sym_count; ++i) {
        const Elf64_Sym* sym = at<Elf64_Sym>(buf, symtab_off +
                                             i * sizeof(Elf64_Sym));
        if (!sym) break;
        f.dynsyms.push_back(cstr_at(buf, strtab_off + sym->st_name));
    }

    for (uint64_t off = 0; off + sizeof(Elf64_Dyn) <= dyn_size;
         off += sizeof(Elf64_Dyn)) {
        const Elf64_Dyn* d = at<Elf64_Dyn>(buf, dyn_off + off);
        if (!d) break;
        if (d->d_tag == DT_NULL) break;
        if (d->d_tag == DT_NEEDED) {
            f.needed.push_back(cstr_at(buf, strtab_off + d->d_un.d_val));
        }
    }

    // An R_X86_64_64 relocation against nexlink_crt_bootstrap proves the entry
    // trampoline was routed through the CRT bootstrap hook.
    for (uint64_t off = 0; off + sizeof(Elf64_Rela) <= rela_size;
         off += sizeof(Elf64_Rela)) {
        const Elf64_Rela* r = at<Elf64_Rela>(buf, rela_off + off);
        if (!r) break;
        const uint32_t sym_idx = ELF64_R_SYM(r->r_info);
        const uint32_t type = ELF64_R_TYPE(r->r_info);
        if (type != R_X86_64_64) continue;
        const Elf64_Sym* sym = at<Elf64_Sym>(buf, symtab_off +
                                             uint64_t(sym_idx) * sizeof(Elf64_Sym));
        if (!sym) continue;
        if (cstr_at(buf, strtab_off + sym->st_name) == "nexlink_crt_bootstrap") {
            f.has_crt_boot_reloc = true;
        }
    }
    return f;
}

bool contains(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& e : v)
        if (e == s) return true;
    return false;
}

} // namespace

int main() {
    const std::string pe_path = WIN2LIN_GUI_TEST_PE;
    const std::string shim_dir = WIN2LIN_SHIM_DIR;
    const std::string build_dir = WIN2LIN_TEST_BUILD_DIR;

    std::cout << "== msvcrt/user32/dxgi integration tests ==\n";
    std::cout << "  fixture PE : " << (pe_path.empty() ? "(none)" : pe_path) << "\n";
    std::cout << "  shim dir   : " << shim_dir << "\n";

    if (pe_path.empty()) {
        std::cout << "  (MinGW C++ not found; skipping GUI fixture tests)\n";
        std::cout << g_checks - g_failures << "/" << g_checks << " checks passed\n";
        return g_failures == 0 ? 0 : 1;
    }

    // ---- convert the MSVCRT GUI fixture ----------------------------------
    const std::string out_elf = build_dir + "/hello_window.elf";
    {
        win2lin::relinker::RelinkOptions o;
        o.input_pe = pe_path;
        o.output_elf = out_elf;
        win2lin::relinker::PeToElfConverter c(o);
        const bool ok = c.convert();
        check(ok, "convert() succeeds on MSVCRT GUI fixture");
        if (!ok) std::cout << "       error: " << c.last_error() << "\n";

        bool msvcrt_binding = false, user32_binding = false, dxgi_binding = false,
             kernel32_binding = false;
        for (const auto& b : c.bindings()) {
            if (b.shim_soname == "libwin_msvcrt.so") msvcrt_binding = true;
            if (b.shim_soname == "libwin_user32.so") user32_binding = true;
            if (b.shim_soname == "libwin_dxgi.so") dxgi_binding = true;
            if (b.shim_soname == "libwin_kernel32.so") kernel32_binding = true;
        }
        check(msvcrt_binding, "msvcrt.dll mapped to libwin_msvcrt.so");
        check(user32_binding, "user32.dll mapped to libwin_user32.so");
        check(dxgi_binding, "dxgi.dll mapped to libwin_dxgi.so");
        check(kernel32_binding, "KERNEL32.dll mapped to libwin_kernel32.so");
    }

    // ---- validate ELF dynamic tables -------------------------------------
    std::vector<uint8_t> buf = read_file(out_elf);
    check(!buf.empty(), "output ELF exists and is non-empty");
    ElfFacts f = parse_elf(buf);
    check(f.valid, "output is a valid little-endian ELF64");
    check(f.type == ET_DYN || f.type == ET_EXEC, "type is ET_DYN or ET_EXEC");
    check(contains(f.needed, "libwin_msvcrt.so"), "DT_NEEDED lists libwin_msvcrt.so");
    check(contains(f.needed, "libwin_user32.so"), "DT_NEEDED lists libwin_user32.so");
    check(contains(f.needed, "libwin_dxgi.so"), "DT_NEEDED lists libwin_dxgi.so");
    check(contains(f.needed, "libwin_kernel32.so"), "DT_NEEDED lists libwin_kernel32.so");
    check(contains(f.dynsyms, "CreateWindowExW"), "dynsym has CreateWindowExW");
    check(contains(f.dynsyms, "RegisterClassExW"), "dynsym has RegisterClassExW");
    check(contains(f.dynsyms, "CreateDXGIFactory1"), "dynsym has CreateDXGIFactory1");
    check(contains(f.dynsyms, "_initterm"), "dynsym has _initterm");
    check(contains(f.dynsyms, "__getmainargs"), "dynsym has __getmainargs");
    check(contains(f.dynsyms, "nexlink_crt_bootstrap"),
          "dynsym has injected nexlink_crt_bootstrap hook");
    check(f.has_crt_boot_reloc,
          "R_X86_64_64 relocation routes entry through nexlink_crt_bootstrap");

    // ---- integration: actually run it ------------------------------------
    RunResult r = run_captured(out_elf, shim_dir);

    // CRT shim announced itself before any PE code ran.
    check(r.out.find("[NEXLINK MSVCRT] bootstrap: CRT initialized") != std::string::npos,
          "msvcrt bootstrap banner printed");
    // Global C++ constructor ran (via CRT startup / _initterm-equivalent).
    check(r.out.find("[gui-test] msvcrt printf alive, g_probe=42") != std::string::npos,
          "msvcrt printf works and global constructor ran");
    // Mock windowing. MinGW %p prints 16 hex digits with no 0x prefix.
    check(r.out.find("RegisterClassExW -> atom=1") != std::string::npos,
          "mock RegisterClassExW returned ATOM 1");
    // Phase 4: HWND is a real SDL-backed handle (non-null heap pointer).
    {
        const auto pos = r.out.find("CreateWindowExW -> hwnd=");
        check(pos != std::string::npos && pos + 24 < r.out.size() &&
                  r.out.substr(pos + 24, 16) != "0000000000000000",
              "CreateWindowExW returned non-null HWND");
    }
    check(r.out.find("GetClientRect -> 1920x1080") != std::string::npos,
          "GetClientRect reported 1920x1080");
    // DXGI skeleton. %08lX prints uppercase hex.
    check(r.out.find("CreateDXGIFactory1 -> hr=0x00000000") != std::string::npos,
          "CreateDXGIFactory1 succeeded");
    check(r.out.find("EnumAdapters1(0) -> hr=0x00000000") != std::string::npos,
          "EnumAdapters1(0) returned a mock adapter");
    check(r.out.find("NEXLINK Vulkan Adapter") != std::string::npos,
          "mock adapter description surfaced through GetDesc1");
    check(r.out.find("EnumAdapters1(1) -> hr=0x887A0002") != std::string::npos,
          "EnumAdapters1(1) returned DXGI_ERROR_NOT_FOUND");
    check(r.out.find("[gui-test] PASS") != std::string::npos, "app printed PASS");
    check(r.code == 0, "relinked MSVCRT GUI PE exits 0");

    if (r.code != 0) {
        std::cout << "------- captured output -------\n" << r.out
                  << "-------------------------------\n";
    }

    std::cout << (g_checks - g_failures) << "/" << g_checks
              << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
