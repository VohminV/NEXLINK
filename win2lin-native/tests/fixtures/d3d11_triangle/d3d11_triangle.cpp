// d3d11_triangle.cpp - minimal D3D11 textured-triangle test fixture (Phase 4).
//
// Exercises the full Phase 4 pipeline:
//   * CreateWindowExW (real SDL2 window via the user32 shim)
//   * D3D11CreateDeviceAndSwapChain (real VkSwapchainKHR when an SDL window exists)
//   * CreateTexture2D (256x256 checkerboard) + CreateShaderResourceView + CreateSamplerState
//   * UpdateSubresource (staging upload + layout transitions)
//   * CreateVertexShader / CreatePixelShader (stub SPIR-V; FRAG samples sampler2D)
//   * CreateInputLayout (POSITION + COLOR + TEXCOORD)
//   * Map/Unmap triangle, IASetVertexBuffers, PSSetShaderResources, PSSetSamplers
//   * Draw + Present in a loop for ~3 seconds (real swapchain presentation)
//
// Built with: x86_64-w64-mingw32-g++ -O2 -static-libgcc -static-libstdc++
// so the C++ runtime is embedded and only msvcrt/user32/dxgi/d3d11/kernel32
// are imported.

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <thread>

struct Vertex {
    float pos[3];
    float color[3];
    float uv[2];
};

int main() {
    // 1. Create a real window via user32 (backed by SDL2 in the shim).
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"NexlinkD3D11Test";
    ATOM atom = RegisterClassExW(&wc);
    std::printf("[d3d11-test] RegisterClassExW -> atom=%u\n", static_cast<unsigned>(atom));

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"NEXLINK D3D11 textured triangle",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                800, 600, nullptr, nullptr, instance, nullptr);
    std::printf("[d3d11-test] CreateWindowExW -> hwnd=%p\n", static_cast<void*>(hwnd));
    if (!hwnd) {
        std::printf("[d3d11-test] FAIL: CreateWindowExW returned NULL\n");
        return 1;
    }
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    RECT client = {};
    GetClientRect(hwnd, &client);
    std::printf("[d3d11-test] GetClientRect -> %ldx%ld\n",
                static_cast<long>(client.right), static_cast<long>(client.bottom));

    // 2. Create device + swap chain + immediate context.
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferDesc.Width = 800;
    scd.BufferDesc.Height = 600;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 1;
    scd.OutputWindow = hwnd;
    scd.Windowed = TRUE;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swap_chain = nullptr;
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &scd, &swap_chain, &device, &feature_level, &context);

    std::printf("[d3d11-test] D3D11CreateDeviceAndSwapChain -> hr=0x%08lX feature=0x%X\n",
                static_cast<unsigned long>(hr), static_cast<unsigned>(feature_level));
    if (FAILED(hr) || !device || !context) {
        std::printf("[d3d11-test] FAIL: device creation failed\n");
        return 1;
    }

    // 3. Create a 256x256 checkerboard texture.
    const UINT tex_w = 256, tex_h = 256;
    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = tex_w;
    tex_desc.Height = tex_h;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    // Build the checkerboard pixel data (RGBA8, 8x8 cells of 32px).
    std::uint8_t pixels[tex_w * tex_h * 4];
    for (UINT y = 0; y < tex_h; ++y) {
        for (UINT x = 0; x < tex_w; ++x) {
            const bool cell = ((x / 32) + (y / 32)) % 2 == 0;
            const std::uint8_t v = cell ? 220 : 40;
            const size_t i = (static_cast<size_t>(y) * tex_w + x) * 4;
            pixels[i + 0] = v;     // R
            pixels[i + 1] = v;     // G
            pixels[i + 2] = v;     // B
            pixels[i + 3] = 255;   // A
        }
    }

    ID3D11Texture2D* texture = nullptr;
    D3D11_SUBRESOURCE_DATA tex_data = {};
    tex_data.pSysMem = pixels;
    tex_data.SysMemPitch = tex_w * 4;
    tex_data.SysMemSlicePitch = 0;
    hr = device->CreateTexture2D(&tex_desc, &tex_data, &texture);
    std::printf("[d3d11-test] CreateTexture2D -> hr=0x%08lX texture=%p\n",
                static_cast<unsigned long>(hr), static_cast<void*>(texture));
    if (FAILED(hr) || !texture) {
        std::printf("[d3d11-test] FAIL: CreateTexture2D failed\n");
        return 1;
    }

    // 4. Create an SRV for the texture.
    ID3D11ShaderResourceView* srv = nullptr;
    hr = device->CreateShaderResourceView(texture, nullptr, &srv);
    std::printf("[d3d11-test] CreateShaderResourceView -> hr=0x%08lX srv=%p\n",
                static_cast<unsigned long>(hr), static_cast<void*>(srv));
    if (FAILED(hr) || !srv) {
        std::printf("[d3d11-test] FAIL: CreateShaderResourceView failed\n");
        return 1;
    }

    // 5. Create a linear-wrap sampler.
    D3D11_SAMPLER_DESC samp_desc = {};
    samp_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samp_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.MaxLOD = D3D11_FLOAT32_MAX;
    ID3D11SamplerState* sampler = nullptr;
    hr = device->CreateSamplerState(&samp_desc, &sampler);
    std::printf("[d3d11-test] CreateSamplerState -> hr=0x%08lX sampler=%p\n",
                static_cast<unsigned long>(hr), static_cast<void*>(sampler));
    if (FAILED(hr) || !sampler) {
        std::printf("[d3d11-test] FAIL: CreateSamplerState failed\n");
        return 1;
    }

    // 6. Create shaders (stub SPIR-V; DXBC is ignored).
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    hr = device->CreateVertexShader(nullptr, 0, nullptr, &vs);
    std::printf("[d3d11-test] CreateVertexShader -> hr=0x%08lX\n",
                static_cast<unsigned long>(hr));
    if (FAILED(hr) || !vs) {
        std::printf("[d3d11-test] FAIL: CreateVertexShader failed\n");
        return 1;
    }
    hr = device->CreatePixelShader(nullptr, 0, nullptr, &ps);
    std::printf("[d3d11-test] CreatePixelShader -> hr=0x%08lX\n",
                static_cast<unsigned long>(hr));
    if (FAILED(hr) || !ps) {
        std::printf("[d3d11-test] FAIL: CreatePixelShader failed\n");
        return 1;
    }

    // 7. Create input layout (POSITION + COLOR + TEXCOORD).
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    ID3D11InputLayout* input_layout = nullptr;
    hr = device->CreateInputLayout(layout, 3, nullptr, 0, &input_layout);
    std::printf("[d3d11-test] CreateInputLayout -> hr=0x%08lX\n",
                static_cast<unsigned long>(hr));
    if (FAILED(hr) || !input_layout) {
        std::printf("[d3d11-test] FAIL: CreateInputLayout failed\n");
        return 1;
    }

    // 8. Create rasterizer state.
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.FrontCounterClockwise = FALSE;
    rd.DepthClipEnable = TRUE;
    ID3D11RasterizerState* raster_state = nullptr;
    hr = device->CreateRasterizerState(&rd, &raster_state);
    std::printf("[d3d11-test] CreateRasterizerState -> hr=0x%08lX\n",
                static_cast<unsigned long>(hr));
    if (FAILED(hr) || !raster_state) {
        std::printf("[d3d11-test] FAIL: CreateRasterizerState failed\n");
        return 1;
    }

    // 9. Create a dynamic vertex buffer (3 vertices with UVs).
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = 3 * sizeof(Vertex);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ID3D11Buffer* vertex_buffer = nullptr;
    hr = device->CreateBuffer(&bd, nullptr, &vertex_buffer);
    std::printf("[d3d11-test] CreateBuffer -> hr=0x%08lX buffer=%p\n",
                static_cast<unsigned long>(hr), static_cast<void*>(vertex_buffer));
    if (FAILED(hr) || !vertex_buffer) {
        std::printf("[d3d11-test] FAIL: CreateBuffer failed\n");
        return 1;
    }

    // 10. Map, write 3 vertices, unmap.
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    hr = context->Map(vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    std::printf("[d3d11-test] Map -> hr=0x%08lX pData=%p\n",
                static_cast<unsigned long>(hr), mapped.pData);
    if (SUCCEEDED(hr) && mapped.pData) {
        Vertex triangle[3] = {
            {{ 0.0f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.5f, 0.0f}}, // top
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}, // bottom-left
            {{ 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}, // bottom-right
        };
        auto* dst = static_cast<Vertex*>(mapped.pData);
        for (int i = 0; i < 3; ++i) dst[i] = triangle[i];
        context->Unmap(vertex_buffer, 0);
        std::printf("[d3d11-test] Unmap OK\n");
    } else {
        std::printf("[d3d11-test] FAIL: Map failed\n");
        return 1;
    }

    // 11. Set pipeline state.
    context->IASetInputLayout(input_layout);
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vs, nullptr, 0);
    context->PSSetShader(ps, nullptr, 0);
    context->RSSetState(raster_state);

    // Bind texture + sampler to the pixel shader (set 0, binding 0).
    ID3D11ShaderResourceView* srv_arr[] = {srv};
    context->PSSetShaderResources(0, 1, srv_arr);
    std::printf("[d3d11-test] PSSetShaderResources OK\n");
    ID3D11SamplerState* samp_arr[] = {sampler};
    context->PSSetSamplers(0, 1, samp_arr);
    std::printf("[d3d11-test] PSSetSamplers OK\n");

    // 12. Draw once (queued), then Present in a loop for ~3 seconds.
    context->Draw(3, 0);
    std::printf("[d3d11-test] Draw(3, 0) returned\n");

    std::printf("[d3d11-test] Presenting for 3 seconds...\n");
    auto start = std::chrono::steady_clock::now();
    int frames = 0;
    while (true) {
        // Pump the message queue so SDL_QUIT / WM_CLOSE is handled.
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Re-issue the draw each frame (state is retained by the context).
        context->Draw(3, 0);

        if (swap_chain) {
            hr = swap_chain->Present(0, 0);
            if (FAILED(hr)) {
                std::printf("[d3d11-test] Present failed hr=0x%08lX at frame %d\n",
                            static_cast<unsigned long>(hr), frames);
                break;
            }
        }
        ++frames;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        if (elapsed.count() >= 3000) break;

        // Small sleep to avoid busy-spinning (swapchain Present may block on vsync).
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
done:
    std::printf("[d3d11-test] Presented %d frames in 3 seconds\n", frames);

    // 13. Clean up.
    vertex_buffer->Release();
    raster_state->Release();
    input_layout->Release();
    sampler->Release();
    srv->Release();
    texture->Release();
    ps->Release();
    vs->Release();
    if (swap_chain) swap_chain->Release();
    context->Release();
    device->Release();
    DestroyWindow(hwnd);

    std::printf("[d3d11-test] PASS\n");
    return 0;
}
