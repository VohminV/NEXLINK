# NEXLINK

**NEXLINK** is an experimental compatibility layer that runs native Windows
(x86-64) game binaries on Linux **without Wine, Proton, or DXVK**. Instead of
emulating the Windows loader and API surface at runtime, NEXLINK *statically
relinks* a Windows PE executable into a native Linux ELF and satisfies its
imports with purpose-built native shared libraries ("shims").

The core of the project lives in [`win2lin-native/`](win2lin-native).

```
 Windows .exe ──► win2lin-relinker ──► .elf  +  DT_NEEDED libwin_*.so
                        │                │
                        │                └── resolved at load time by ld.so
                        └── LIEF parses PE headers, imports, base relocations
```

## Why

Wine/Proton translate Windows calls into POSIX at runtime and pay for it with a
large compatibility surface, process-model emulation, and an ever-growing set of
per-game hacks. NEXLINK takes the opposite approach:

* **Ahead-of-time**: the PE is rewritten on disk into a real ELF. The Linux
  kernel and `ld.so` load it like any other program.
* **No Windows loader in the loop**: imports become ordinary ELF dynamic
  symbols resolved against `libwin_<dll>.so` shims.
* **No D3D translation**: graphics shims target Vulkan directly.
* **Deterministic**: the transformation is static, so failures are debuggable
  at the binary level rather than in a runtime emulator.

## Repository layout

| Path | Purpose |
| --- | --- |
| `win2lin-native/src/relinker/` | PE → ELF static relinker (LIEF-based) and its CLI |
| `win2lin-native/src/shims/kernel32/` | `libwin_kernel32.so` — POSIX backing for kernel32.dll |
| `win2lin-native/src/shims/msvcrt/` | `libwin_msvcrt.so` — MSVCRT/C runtime shim |
| `win2lin-native/src/shims/user32/` | `libwin_user32.so` — windowing, messages, GDI (SDL2-backed) |
| `win2lin-native/src/shims/dxgi/` | `libwin_dxgi.so` — DXGI stub |
| `win2lin-native/src/shims/d3d9/` | `libwin_d3d9.so` — D3D9 → Vulkan (device, swapchain, VB/IB, textures, draw) |
| `win2lin-native/src/shims/d3dx9/` | `libwin_d3dx9.so` — D3DX9 helper stubs |
| `win2lin-native/src/shims/d3dcompiler/` | `libwin_d3dcompiler.so` — HLSL compiler stub |
| `win2lin-native/src/shims/advapi32/` | `libwin_advapi32.so` — registry, crypto, security stubs |
| `win2lin-native/src/shims/ws2_32/` | `libwin_ws2_32.so` — Winsock stubs |
| `win2lin-native/src/shims/winmm/` | `libwin_winmm.so` — multimedia timer/audio stubs |
| `win2lin-native/src/shims/dinput8/` | `libwin_dinput8.so` — DirectInput stub |
| `win2lin-native/src/shims/dsound/` | `libwin_dsound.so` — DirectSound stub |
| `win2lin-native/src/shims/version/` | `libwin_version.so` — file version info stubs |
| `win2lin-native/src/shims/shell32/` | `libwin_shell32.so` — shell stubs |
| `win2lin-native/src/shims/shlwapi/` | `libwin_shlwapi.so` — lightweight shell stubs |
| `win2lin-native/src/shims/ole32/` | `libwin_ole32.so` — COM stubs |
| `win2lin-native/src/shims/oleaut32/` | `libwin_oleaut32.so` — COM automation stubs |
| `win2lin-native/src/shims/imm32/` | `libwin_imm32.so` — IME stubs |
| `win2lin-native/src/shims/usp10/` | `libwin_usp10.so` — Uniscribe stubs |
| `win2lin-native/src/shims/uxtheme/` | `libwin_uxtheme.so` — visual styles stubs |
| `win2lin-native/src/shims/dwmapi/` | `libwin_dwmapi.so` — DWM stubs |
| `win2lin-native/src/shims/iphlpapi/` | `libwin_iphlpapi.so` — IP Helper stubs |
| `win2lin-native/src/shims/crypt32/` | `libwin_crypt32.so` — CryptoAPI stubs |
| `win2lin-native/src/shims/secur32/` | `libwin_secur32.so` — SSPI stubs |
| `win2lin-native/src/shims/gdiplus/` | `libwin_gdiplus.so` — GDI+ stubs |
| `win2lin-native/src/shims/msvcp71/80/90/` | `libwin_msvcp*.so` — C++ runtime stubs |
| `win2lin-native/src/shims/msacm32/` | `libwin_msacm32.so` — ACM stubs |
| `win2lin-native/src/gui/` | `win2lin-gui` — SDL2 front-end for relinker |
| `win2lin-native/src/common/` | Logging, utilities, Windows type/ABI definitions |
| `win2lin-native/tests/` | Unit + integration tests and MinGW fixtures |

## How it works

### 1. Relinking (`win2lin-relinker`)

Using [LIEF](https://lief.re) to parse the PE, the relinker:

* copies every PE section into the output, preserving `p_offset == RVA`
  (all PE image bases are page-aligned, so the ELF congruence rule holds for
  free);
* maps each imported DLL to a shim (`kernel32.dll → libwin_kernel32.so`, …) and
  emits one undefined dynamic symbol per imported function;
* patches each **IAT slot** with an `R_X86_64_GLOB_DAT` relocation so `ld.so`
  fills it at load time;
* rewrites **base relocations**: if the PE has a `.reloc` table the output is a
  PIE (`ET_DYN`) with `R_X86_64_RELATIVE` entries; otherwise the output is a
  fixed-base `ET_EXEC` anchored at the PE image base;
* synthesizes the ELF dynamic metadata (`.dynstr`, `.dynsym`, SYSV `.hash`,
  `.rela.dyn`, `.dynamic`), the `PT_INTERP` string, and a `$ORIGIN` runpath so
  shims are found next to the binary.

The **Microsoft x64 ABI** is preserved across the boundary: shims are compiled
with `__attribute__((ms_abi))`, so arguments stay in `RCX/RDX/R8/R9` and the
32-byte shadow space contract is honored.

A tiny **entry trampoline** is emitted at `e_entry`: the kernel enters a process
with `rsp % 16 == 0`, but a PE entry point is written expecting `rsp % 16 == 8`
(as if it had been `call`ed). The trampoline realigns the stack, `call`s the
real PE entry, and provides a clean `exit(0)` stub if it ever returns.

### 2. Shim libraries (`libwin_*.so`)

Each shim exports Microsoft x64 ABI entry points under their Windows names and
backs them with POSIX primitives. `libwin_kernel32.so` currently implements the
following (see `src/shims/kernel32/kernel32.cpp`):

* **Process/threads**: `ExitProcess`, `TerminateProcess`, `GetCurrentProcess(Id)`,
  `CreateThread`, `WaitForSingleObject`, `WaitForMultipleObjects`,
  `GetCurrentThread(Id)`, `Sleep`, `SwitchToThread`, `FlushInstructionCache`
* **Memory**: `VirtualAlloc`, `VirtualFree`, `GetProcessHeap`, `HeapCreate`,
  `HeapDestroy`, `HeapAlloc`, `HeapReAlloc`, `HeapFree`, `HeapSize`
* **Modules**: `GetModuleHandleA/W`, `LoadLibraryA/W`, `LoadLibraryExA/W`,
  `GetProcAddress`, `FreeLibrary` (native `dlopen`/`dlsym`)
* **Files**: `CreateFileA/W`, `ReadFile`, `WriteFile`, `CloseHandle`,
  `DeleteFileW`, `GetFileSize`, `SetFilePointer`
* **Console**: `AllocConsole`, `AttachConsole`, `FreeConsole`, `GetStdHandle`,
  `SetStdHandle`, `GetConsoleMode`, `SetConsoleMode`
* **Time/error**: `GetTickCount(64)`, `GetSystemTimeAsFileTime`,
  `QueryPerformanceCounter/Frequency`, `GetLastError`, `SetLastError`
* **Misc**: `IsDebuggerPresent`, `OutputDebugStringA/W`

### 3. Graphics & GUI

`d3d9` translates Direct3D 9 calls to Vulkan: device creation, swapchain,
vertex/index buffers, textures, fixed-function draw calls and present. The GUI
(`win2lin-gui`) provides an SDL2 front-end: pick a Windows `.exe`, choose a
Linux target directory, run the relinker, then launch the resulting ELF with
`LD_LIBRARY_PATH` pointing at the shim `.so` files.

No SPIR-V runtime compilation is required (no RDNA 2 GPU needed); shader bytecode
is precompiled from GLSL to SPIR-V at build time via `glslangValidator`.

## Building

### Dependencies

* CMake ≥ 3.24, a C++20 compiler (GCC 13+)
* [LIEF](https://lief.re) (development headers + library, discoverable via
  `pkg-config`)
* Vulkan loader development files
* SDL2 development files
* `glslangValidator` (for D3D9 shader compilation)
* Python 3 (for embedding SPIR-V shaders)
* *(tests only)* `x86_64-w64-mingw32-gcc` to build the PE fixture; tests skip
  the conversion checks if it is missing

```bash
cd win2lin-native
cmake -S . -B build -G Ninja
cmake --build build
```

Artifacts are placed in `win2lin-native/build/bin` and
`win2lin-native/build/lib`.

## Usage

```bash
# Relink a Windows executable into a native ELF
build/bin/win2lin-relinker game.exe game.elf

# Run it; shims are found via the baked $ORIGIN runpath
LD_LIBRARY_PATH=build/lib ./game.elf
```

Options:

| Flag | Meaning |
| --- | --- |
| `--linux-fixed-base` | Disable ASLR: emit `ET_EXEC` at the PE image base |
| `--no-feature-check` | Skip the host CPU feature validation (AVX2/FMA/BMI/LZCNT) |

## Testing

```bash
cd win2lin-native/build
ctest --output-on-failure
```

The test suite converts a MinGW-built fixture PE, validates the emitted ELF by
parsing its raw bytes (ELF header, program headers, `.dynamic`, `.dynsym`,
trampoline), executes it, and asserts it prints through `libwin_kernel32.so`.
Negative paths (missing input, non-PE input) are covered too.

## Requirements & constraints

* **Host**: Linux x86-64 with glibc and a working `ld.so`.
* **Target**: PE32+ (x86-64) executables. 32-bit PE is not supported.
* Graphics shims currently require a Vulkan-capable GPU/driver.
* This is research-grade software; expect incomplete API coverage and rough
  edges.

## Status

Phase 1 (relinker + `kernel32` shim + tests) is functional: CRT-free PE
executables relink and run end-to-end, for both fixed-base (`ET_EXEC`) and PIE
(`ET_DYN`) inputs.

Phase 2 adds `msvcrt`, `user32`, `dxgi` and the `win2lin-gui` front-end.

Phase 3 adds D3D9 → Vulkan rendering (device, swapchain, draw, present) plus
stubs for `advapi32`, `ws2_32`, `winmm`, `dinput8`, `dsound`, `version`,
`shell32`, `shlwapi`, `ole32`, `oleaut32`, `d3dx9_*`, `d3dcompiler_*`,
`imm32`, `usp10`, `uxtheme`, `dwmapi`, `iphlpapi`, `crypt32`, `secur32`,
`gdiplus`, `msvcp71/80/90`, `msacm32` — covering the import surface of most
2000–2010 era Windows games.

Graphics rendering: `Clear` + `Present` work, vertex/index buffers and textures
are bound to Vulkan via descriptor sets, and `DrawPrimitive`/`DrawIndexedPrimitive`
issue `vkCmdDraw`/`vkCmdDrawIndexed`. Fixed-function pipeline state (lights,
materials, complex blend states) is partially translated — enough to render
colored/textured geometry from most 2000–2010 era D3D9 games.

## CI

GitHub Actions workflow builds and tests on every push/PR (see
`.github/workflows/ci.yml`).

## License

Not yet decided.
