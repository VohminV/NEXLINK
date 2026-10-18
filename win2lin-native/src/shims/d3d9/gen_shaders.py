#!/usr/bin/env python3
"""Generate C header with embedded SPIR-V shader arrays from .spv files."""
import sys
import struct


def main():
    if len(sys.argv) < 3 or (len(sys.argv) - 2) % 2 != 0:
        sys.stderr.write("Usage: gen_shaders.py <output.h> <spv1> <name1> [<spv2> <name2> ...]\n")
        return 1
    out_path = sys.argv[1]
    pairs = sys.argv[2:]
    with open(out_path, 'w') as f:
        f.write("// Auto-generated SPIR-V shaders, do not edit\n")
        f.write("#include <cstdint>\n")
        f.write("#include <cstddef>\n\n")
        for i in range(0, len(pairs), 2):
            spv_path = pairs[i]
            name = pairs[i + 1]
            with open(spv_path, 'rb') as sf:
                data = sf.read()
            if len(data) % 4 != 0:
                sys.stderr.write("SPIR-V size not multiple of 4: %s\n" % spv_path)
                return 1
            words = struct.unpack('<%dI' % (len(data) // 4), data)
            f.write("static const uint32_t %s[] = {\n" % name)
            for j, w in enumerate(words):
                if j % 12 == 0:
                    f.write("    ")
                f.write("0x%08x, " % w)
                if j % 12 == 11:
                    f.write("\n")
            if len(words) % 12 != 0:
                f.write("\n")
            f.write("};\n")
            f.write("static const size_t %s_size = %d;\n\n" % (name, len(data)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
