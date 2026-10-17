#include "pe_to_elf.hpp"
#include "common/logging.hpp"
#include <iostream>
#include <string>

void print_usage() {
    std::cout << "Usage: win2lin-relinker [OPTIONS] <input_pe> <output_elf>\n"
              << "Options:\n"
              << "  --linux-fixed-base      Disable ASLR for fixed base PE\n"
              << "  --no-feature-check     Skip CPU feature validation\n"
              << "  --help                 Show this help\n";
}

int main(int argc, char* argv[]) {
    win2lin::logging::set_level(win2lin::logging::Level::Info);

    if (argc < 3) {
        print_usage();
        return 1;
    }

    win2lin::relinker::RelinkOptions opts;
    int pos = 1;
    for (int i = 1; i < argc - 2; i++) {
        std::string arg = argv[i];
        if (arg == "--linux-fixed-base") {
            opts.linux_fixed_base = true;
        } else if (arg == "--no-feature-check") {
            opts.validate_features = false;
        } else if (arg == "--help") {
            print_usage();
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage();
            return 1;
        }
        pos++;
    }

    opts.input_pe = argv[argc - 2];
    opts.output_elf = argv[argc - 1];

    win2lin::relinker::PeToElfConverter converter(opts);
    if (!converter.convert()) {
        std::cerr << "Conversion failed\n";
        return 1;
    }

    return 0;
}
