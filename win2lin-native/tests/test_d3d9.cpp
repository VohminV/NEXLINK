// Unit test for libwin_d3d9.so - verifies the shim loads and exports
// Direct3DCreate9. Does not create a Vulkan device (needs a display).
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <string>

using IDirect3D9 = void;
using UINT = uint32_t;
typedef IDirect3D9* (*Direct3DCreate9_t)(UINT);

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool cond, const char* what) {
    ++g_checks;
    if (cond) {
        std::printf("  [ OK ] %s\n", what);
    } else {
        ++g_failures;
        std::printf("  [FAIL] %s\n", what);
    }
}

} // namespace

int main() {
    void* handle = dlopen("libwin_d3d9.so", RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        handle = dlopen("../../build/lib/libwin_d3d9.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (!handle) {
        handle = dlopen("/home/vita/NEXLINK/win2lin-native/build/lib/libwin_d3d9.so", RTLD_NOW | RTLD_LOCAL);
    }
    check(handle != nullptr, "dlopen libwin_d3d9.so");
    if (!handle) return 1;

    void* sym = dlsym(handle, "Direct3DCreate9");
    Direct3DCreate9_t d3d_create = nullptr;
    memcpy(&d3d_create, &sym, sizeof(d3d_create));
    check(d3d_create != nullptr, "dlsym Direct3DCreate9");

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    dlclose(handle);
    return g_failures == 0 ? 0 : 1;
}
