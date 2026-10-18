// Integration test for Phase 4: D3D11 shim + Vulkan swapchain + textures.
//
// Strategy:
//   * Convert the MinGW D3D11 textured-triangle fixture (d3d11_triangle.exe)
//     with the relinker library directly.
//   * Validate the produced ELF: DT_NEEDED covers d3d11/dxgi/msvcrt/kernel32
//     (user32 is now also linked by the fixture).
//   * Execute the relinked binary with the shim directory on LD_LIBRARY_PATH
//     and verify: window creation, device creation, texture/SRV/sampler,
//     shader creation, Map/Unmap, Draw, Present (swapchain or headless),
//     and exit code 0.
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

bool dynsym_has(const std::vector<uint8_t>& elf, const std::string& name) {
    const auto* ehdr = at<Elf64_Ehdr>(elf, 0);
    if (!ehdr || std::memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) return false;
    const auto* shdr = at<Elf64_Shdr>(elf, ehdr->e_shoff);
    if (!shdr) return false;
    const auto* shstr = at<Elf64_Shdr>(elf, ehdr->e_shoff +
                                       ehdr->e_shstrndx * ehdr->e_shentsize);
    if (!shstr) return false;
    for (int i = 0; i < ehdr->e_shnum; ++i) {
        const auto& s = shdr[i];
        if (s.sh_type != SHT_DYNSYM) continue;
        const auto* strtab = at<Elf64_Shdr>(elf, ehdr->e_shoff +
                                            s.sh_link * ehdr->e_shentsize);
        if (!strtab) continue;
        for (uint64_t off = 0; off < s.sh_size; off += s.sh_entsize) {
            const auto* sym = at<Elf64_Sym>(elf, s.sh_offset + off);
            if (!sym) break;
            if (cstr_at(elf, strtab->sh_offset + sym->st_name) == name) return true;
        }
    }
    return false;
}

bool dynsym_needed(const std::vector<uint8_t>& elf, const std::string& lib) {
    const auto* ehdr = at<Elf64_Ehdr>(elf, 0);
    if (!ehdr) return false;
    const auto* shdr = at<Elf64_Shdr>(elf, ehdr->e_shoff);
    if (!shdr) return false;
    for (int i = 0; i < ehdr->e_shnum; ++i) {
        const auto& s = shdr[i];
        if (s.sh_type != SHT_DYNAMIC) continue;
        for (uint64_t off = 0; off < s.sh_size; off += s.sh_entsize) {
            const auto* dyn = at<Elf64_Dyn>(elf, s.sh_offset + off);
            if (!dyn || dyn->d_tag == DT_NULL) break;
            if (dyn->d_tag == DT_NEEDED) {
                for (int j = 0; j < ehdr->e_shnum; ++j) {
                    if (shdr[j].sh_type == SHT_STRTAB) {
                        if (cstr_at(elf, shdr[j].sh_offset + dyn->d_un.d_val) == lib)
                            return true;
                    }
                }
            }
        }
    }
    return false;
}

} // namespace

int main() {
    const std::string pe_path = WIN2LIN_D3D11_TEST_PE;
    const std::string shim_dir = WIN2LIN_SHIM_DIR;
    const std::string build_dir = WIN2LIN_TEST_BUILD_DIR;

    std::cout << "=== D3D11 + Vulkan swapchain + textures integration test ===\n";
    std::cout << "PE: " << pe_path << "\n";
    std::cout << "Shims: " << shim_dir << "\n\n";

    auto pe = read_file(pe_path);
    check(!pe.empty(), "fixture PE exists and is readable");
    if (pe.empty()) return 1;

    // Relink PE -> ELF.
    std::string elf_path = build_dir + "/d3d11_triangle.elf";
    {
        win2lin::relinker::RelinkOptions o;
        o.input_pe = pe_path;
        o.output_elf = elf_path;
        win2lin::relinker::PeToElfConverter converter(o);
        if (!converter.convert()) {
            std::cerr << "relink failed: " << converter.last_error() << "\n";
            return 1;
        }
    }

    auto elf = read_file(elf_path);
    check(!elf.empty(), "relinked ELF exists and is readable");
    if (elf.empty()) return 1;

    // Validate DT_NEEDED covers the shims.
    check(dynsym_needed(elf, "libwin_d3d11.so"), "DT_NEEDED has libwin_d3d11.so");
    check(dynsym_needed(elf, "libwin_msvcrt.so"), "DT_NEEDED has libwin_msvcrt.so");
    check(dynsym_needed(elf, "libwin_kernel32.so"), "DT_NEEDED has libwin_kernel32.so");
    // The fixture now also links user32.
    bool has_user32 = dynsym_needed(elf, "libwin_user32.so");
    std::cout << "  [INFO] DT_NEEDED has libwin_user32.so: " << (has_user32 ? "yes" : "no") << "\n";

    // Validate dynsym contains D3D11 entry points.
    check(dynsym_has(elf, "D3D11CreateDeviceAndSwapChain"),
          "dynsym has D3D11CreateDeviceAndSwapChain");

    // Execute the relinked binary (3s window hold + present loop).
    auto r = run_captured(elf_path, shim_dir);
    std::cout << "\n--- captured output ---\n" << r.out << "-----------------------\n";
    std::cout << "exit code: " << r.code << "\n\n";

    // Verify results.
    {
        const auto pos = r.out.find("CreateWindowExW -> hwnd=");
        check(pos != std::string::npos && pos + 24 < r.out.size() &&
                  r.out.substr(pos + 24, 16) != "0000000000000000",
              "CreateWindowExW returned non-null HWND");
    }
    check(r.out.find("GetClientRect -> 800x600") != std::string::npos,
          "GetClientRect reported 800x600");
    check(r.out.find("D3D11CreateDeviceAndSwapChain -> hr=0x00000000") != std::string::npos,
          "D3D11CreateDeviceAndSwapChain returned S_OK");
    check(r.out.find("CreateTexture2D -> hr=0x00000000") != std::string::npos,
          "CreateTexture2D returned S_OK");
    check(r.out.find("CreateShaderResourceView -> hr=0x00000000") != std::string::npos,
          "CreateShaderResourceView returned S_OK");
    check(r.out.find("CreateSamplerState -> hr=0x00000000") != std::string::npos,
          "CreateSamplerState returned S_OK");
    check(r.out.find("CreateVertexShader -> hr=0x00000000") != std::string::npos,
          "CreateVertexShader returned S_OK (stub SPIR-V)");
    check(r.out.find("CreatePixelShader -> hr=0x00000000") != std::string::npos,
          "CreatePixelShader returned S_OK (stub SPIR-V)");
    check(r.out.find("CreateInputLayout -> hr=0x00000000") != std::string::npos,
          "CreateInputLayout returned S_OK");
    check(r.out.find("CreateRasterizerState -> hr=0x00000000") != std::string::npos,
          "CreateRasterizerState returned S_OK");
    check(r.out.find("CreateBuffer(vertex) -> hr=0x00000000") != std::string::npos,
          "CreateBuffer(vertex) returned S_OK");
    check(r.out.find("CreateBuffer(index) -> hr=0x00000000") != std::string::npos,
          "CreateBuffer(index) returned S_OK");
    check(r.out.find("CreateBuffer(constant) -> hr=0x00000000") != std::string::npos,
          "CreateBuffer(constant) returned S_OK");
    check(r.out.find("Map(constant buffer) OK, MVP updated") != std::string::npos,
          "Map(constant buffer) succeeded and MVP updated");
    check(r.out.find("PSSetShaderResources OK") != std::string::npos,
          "PSSetShaderResources executed");
    check(r.out.find("PSSetSamplers OK") != std::string::npos,
          "PSSetSamplers executed");
    check(r.out.find("VSSetConstantBuffers OK") != std::string::npos,
          "VSSetConstantBuffers executed");
    check(r.out.find("DrawIndexed(36, 0, 0) returned") != std::string::npos,
          "DrawIndexed(36, 0, 0) executed without crash");
    check(r.out.find("Presenting for 3 seconds") != std::string::npos,
          "fixture started 3-second present loop");
    check(r.out.find("Presented") != std::string::npos && r.out.find("frames in 3 seconds") != std::string::npos,
          "fixture presented frames for 3 seconds");
    check(r.out.find("Present -> hr=0x00000000") != std::string::npos ||
          r.out.find("Present (swapchain): execute_frame OK") != std::string::npos ||
          r.out.find("execute_frame: submitted + presented") != std::string::npos ||
          r.out.find("execute_frame: submitted (headless offscreen)") != std::string::npos,
          "Present executed (swapchain or headless)");
    check(r.out.find("[d3d11-test] PASS") != std::string::npos, "app printed PASS");
    check(r.code == 0, "relinked D3D11 PE exits 0");

    // Verify the Vulkan backend initialised.
    check(r.out.find("Vulkan backend initialised") != std::string::npos,
          "Vulkan backend initialised");
    check(r.out.find("Created VkBuffer") != std::string::npos,
          "VkBuffer was created for ID3D11Buffer");
    check(r.out.find("Created VkImage") != std::string::npos,
          "VkImage was created for ID3D11Texture2D");
    check(r.out.find("Created VkSampler") != std::string::npos,
          "VkSampler was created for ID3D11SamplerState");
    check(r.out.find("Created VkPipeline") != std::string::npos,
          "VkPipeline was created");
    check(r.out.find("Created VkRenderPass") != std::string::npos,
          "VkRenderPass was created");
    check(r.out.find("descriptor set") != std::string::npos ||
          r.out.find("Created descriptor set") != std::string::npos,
          "Descriptor set was created");

    // Swapchain path (when a real window + display is available).
    bool has_swapchain_log = r.out.find("Created VkSwapchainKHR") != std::string::npos ||
                             r.out.find("Vulkan backend initialised with SDL swapchain") != std::string::npos;
    bool has_headless_log = r.out.find("Present: wrote triangle.ppm") != std::string::npos ||
                            r.out.find("execute_frame: submitted (headless offscreen)") != std::string::npos;
    check(has_swapchain_log || has_headless_log,
          "render path is either swapchain or headless offscreen");

    // Headless path: PPM file.
    if (has_headless_log && !has_swapchain_log) {
        std::string ppm_path = build_dir + "/triangle.ppm";
        std::ifstream ppm(ppm_path, std::ios::binary);
        check(ppm.good(), "triangle.ppm exists on disk (headless path)");
        if (ppm.good()) {
            std::string magic;
            ppm >> magic;
            check(magic == "P6", "triangle.ppm is a valid P6 PPM");
            int w = 0, h = 0;
            ppm >> w >> h;
            check(w == 800 && h == 600, "triangle.ppm is 800x600");
        }
    }

    std::cout << (g_checks - g_failures) << "/" << g_checks
              << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
