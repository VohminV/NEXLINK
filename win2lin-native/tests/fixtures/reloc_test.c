// Simple PE fixture for testing base relocations (DIR32 + DIR64).
// Compiled with -Wl,--dynamicbase to generate a .reloc section.
#include <windows.h>

void EntryPoint(void) {
    // Simple function that does nothing - just returns
    return;
}
