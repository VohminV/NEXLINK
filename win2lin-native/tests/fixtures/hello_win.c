// Minimal Windows PE console program used as a relinker test fixture.
// It is intentionally CRT-free (built with -nostdlib) so the only imports are
// a handful of kernel32 functions, keeping the expected ELF layout stable.
#include <windows.h>

static const char kMessage[] = "Hello from relinked PE!\n";

// A global pointer forces an absolute address in .data, which the PE linker
// records as a base relocation. Linking with --dynamicbase then produces a PE
// with a .reloc table, exercising the ET_DYN (PIE) relinking path.
static const char* volatile g_message = kMessage;

void EntryPoint(void) {
    HANDLE h = GetStdHandle((DWORD)-11); // STD_OUTPUT_HANDLE
    DWORD written = 0;
    WriteFile(h, g_message, sizeof(kMessage) - 1, &written, NULL);
    ExitProcess(0);
}
