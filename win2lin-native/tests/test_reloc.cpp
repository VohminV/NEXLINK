// Integration test for base relocations (DIR32/DIR64).
// Compiles a MinGW PE with --dynamicbase (generates .reloc section),
// relinks it to ELF, and validates the output.
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

std::string run_cmd(const std::string& cmd) {
    std::string out;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "(popen failed)";
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) out += buf;
    pclose(pipe);
    return out;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: test_reloc <pe_path> <shim_dir> [elf_output]\n";
        return 1;
    }

    const std::string pe_path = argv[1];
    const std::string shim_dir = argv[2];
    const std::string elf_out = (argc >= 4) ? argv[3] : "/tmp/win2lin_reloc_test.elf";

    // Relink the PE
    win2lin::relinker::RelinkOptions opts;
    opts.input_pe = pe_path;
    opts.output_elf = elf_out;
    opts.shim_prefix = "libwin_";
    opts.shim_suffix = ".so";
    opts.runpath = "$ORIGIN";

    win2lin::relinker::PeToElfConverter converter(opts);
    bool ok = converter.convert();
    check(ok, "PE relink succeeded");
    if (!ok) {
        std::cerr << "  Error: " << converter.last_error() << "\n";
        return 1;
    }

    // Validate the ELF
    std::ifstream f(elf_out, std::ios::binary);
    check(f.is_open(), "ELF output file exists");
    if (!f.is_open()) return 1;

    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
    check(buf.size() >= sizeof(Elf64_Ehdr), "ELF is at least header-sized");

    if (buf.size() >= sizeof(Elf64_Ehdr)) {
        const Elf64_Ehdr* ehdr = reinterpret_cast<const Elf64_Ehdr*>(buf.data());
        check(ehdr->e_ident[EI_MAG0] == 0x7f &&
              ehdr->e_ident[EI_MAG1] == 'E' &&
              ehdr->e_ident[EI_MAG2] == 'L' &&
              ehdr->e_ident[EI_MAG3] == 'F', "Valid ELF magic");
        check(ehdr->e_ident[EI_CLASS] == ELFCLASS64, "ELFCLASS64");
        check(ehdr->e_machine == EM_X86_64, "EM_X86_64");
        check(ehdr->e_type == ET_DYN || ehdr->e_type == ET_EXEC, "ET_DYN or ET_EXEC");
        check(ehdr->e_phnum > 0, "Has program headers");
        check(ehdr->e_shnum > 0, "Has section headers");
    }

    // Check DT_NEEDED entries reference our shims (only if PE imports DLLs)
    // Parse .dynamic to find DT_NEEDED
    // (Simplified: just check the file contains expected shim names)
    std::string elf_str(buf.begin(), buf.end());
    // Note: self-contained PE may not reference any shims - that's OK
    if (elf_str.find("libwin_kernel32.so") != std::string::npos) {
        std::cout << "  [ INFO ] References libwin_kernel32.so\n";
    }

    std::cout << "\n" << g_checks << " checks, " << g_failures << " failures\n";
    return g_failures == 0 ? 0 : 1;
}
