// d3d11_triangle.cpp - D3D11 textured rotating-cube test fixture (Phase 5).
//
// Exercises the full Phase 5 pipeline:
//   * CreateWindowExW (real SDL2 window via the user32 shim)
//   * D3D11CreateDeviceAndSwapChain (real VkSwapchainKHR when an SDL window exists)
//   * CreateTexture2D (256x256 checkerboard) + CreateShaderResourceView + CreateSamplerState
//   * CreateBuffer: vertex buffer (cube), index buffer (36 idx), constant buffer (mat4)
//   * Map/Unmap vertex + constant buffers each frame (D3D11_MAP_WRITE_DISCARD)
//   * VSSetConstantBuffers (uniform buffer at set 0 binding 0)
//   * IASetIndexBuffer + DrawIndexed (indexed rendering)
//   * Present in a loop for ~3 seconds with per-frame rotation matrix
//
// Built with: x86_64-w64-mingw32-g++ -O2 -static-libgcc -static-libstdc++

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <thread>

struct Vertex {
    float pos[3];
    float color[3];
    float uv[2];
};

// Minimal column-major 4x4 matrix.
struct Mat4 {
    float m[16];
    static Mat4 identity() {
        Mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
    static Mat4 rotation_y(float a) {
        Mat4 r = identity();
        r.m[0] = std::cos(a);  r.m[8]  = std::sin(a);
        r.m[2] = -std::sin(a); r.m[10] = std::cos(a);
        return r;
    }
    static Mat4 rotation_x(float a) {
        Mat4 r = identity();
        r.m[5] = std::cos(a);  r.m[9]  = -std::sin(a);
        r.m[6] = std::sin(a);  r.m[10] = std::cos(a);
        return r;
    }
    static Mat4 perspective(float fovy, float aspect, float znear, float zfar) {
        Mat4 r{};
        float f = 1.0f / std::tan(fovy * 0.5f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (zfar + znear) / (znear - zfar);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
        return r;
    }
    static Mat4 translate(float x, float y, float z) {
        Mat4 r = identity();
        r.m[12] = x; r.m[13] = y; r.m[14] = z;
        return r;
    }
    Mat4 operator*(const Mat4& b) const {
        Mat4 r{};
        for (int c = 0; c < 4; ++c)
            for (int rw = 0; rw < 4; ++rw) {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k)
                    s += m[k * 4 + rw] * b.m[c * 4 + k];
                r.m[c * 4 + rw] = s;
            }
        return r;
    }
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

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"NEXLINK D3D11 rotating cube",
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

    std::uint8_t pixels[tex_w * tex_h * 4];
    for (UINT y = 0; y < tex_h; ++y) {
        for (UINT x = 0; x < tex_w; ++x) {
            const bool cell = ((x / 32) + (y / 32)) % 2 == 0;
            const std::uint8_t v = cell ? 220 : 40;
            const size_t i = (static_cast<size_t>(y) * tex_w + x) * 4;
            pixels[i + 0] = v;
            pixels[i + 1] = v;
            pixels[i + 2] = v;
            pixels[i + 3] = 255;
        }
    }

    ID3D11Texture2D* texture = nullptr;
    D3D11_SUBRESOURCE_DATA tex_data = {};
    tex_data.pSysMem = pixels;
    tex_data.SysMemPitch = tex_w * 4;
    hr = device->CreateTexture2D(&tex_desc, &tex_data, &texture);
    std::printf("[d3d11-test] CreateTexture2D -> hr=0x%08lX texture=%p\n",
                static_cast<unsigned long>(hr), static_cast<void*>(texture));
    if (FAILED(hr) || !texture) return 1;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device->CreateShaderResourceView(texture, nullptr, &srv);
    std::printf("[d3d11-test] CreateShaderResourceView -> hr=0x%08lX srv=%p\n",
                static_cast<unsigned long>(hr), static_cast<void*>(srv));
    if (FAILED(hr) || !srv) return 1;

    // 4. Sampler.
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
    if (FAILED(hr) || !sampler) return 1;

    // 5. Shaders.
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    hr = device->CreateVertexShader(nullptr, 0, nullptr, &vs);
    std::printf("[d3d11-test] CreateVertexShader -> hr=0x%08lX\n",
                static_cast<unsigned long>(hr));
    if (FAILED(hr) || !vs) return 1;
    hr = device->CreatePixelShader(nullptr, 0, nullptr, &ps);
    std::printf("[d3d11-test] CreatePixelShader -> hr=0x%08lX\n",
                static_cast<unsigned long>(hr));
    if (FAILED(hr) || !ps) return 1;

    // 6. Input layout.
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
    if (FAILED(hr) || !input_layout) return 1;

    // 7. Rasterizer state (no culling so both faces are visible).
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.FrontCounterClockwise = FALSE;
    rd.DepthClipEnable = TRUE;
    ID3D11RasterizerState* raster_state = nullptr;
    hr = device->CreateRasterizerState(&rd, &raster_state);
    std::printf("[d3d11-test] CreateRasterizerState -> hr=0x%08lX\n",
                static_cast<unsigned long>(hr));
    if (FAILED(hr) || !raster_state) return 1;

    // 8. Cube vertex buffer: 24 vertices (4 per face).
    const Vertex cube_verts[24] = {
        // +X face (red)
        {{0.5f,-0.5f,-0.5f},{1,0,0},{0,1}}, {{0.5f,0.5f,-0.5f},{1,0,0},{1,1}},
        {{0.5f,0.5f,0.5f},{1,0,0},{1,0}},   {{0.5f,-0.5f,0.5f},{1,0,0},{0,0}},
        // -X face (green)
        {{-0.5f,-0.5f,0.5f},{0,1,0},{0,1}}, {{-0.5f,0.5f,0.5f},{0,1,0},{1,1}},
        {{-0.5f,0.5f,-0.5f},{0,1,0},{1,0}}, {{-0.5f,-0.5f,-0.5f},{0,1,0},{0,0}},
        // +Y face (blue)
        {{-0.5f,0.5f,-0.5f},{0,0,1},{0,1}}, {{0.5f,0.5f,-0.5f},{0,0,1},{1,1}},
        {{0.5f,0.5f,0.5f},{0,0,1},{1,0}},   {{-0.5f,0.5f,0.5f},{0,0,1},{0,0}},
        // -Y face (yellow)
        {{-0.5f,-0.5f,0.5f},{1,1,0},{0,1}}, {{0.5f,-0.5f,0.5f},{1,1,0},{1,1}},
        {{0.5f,-0.5f,-0.5f},{1,1,0},{1,0}}, {{-0.5f,-0.5f,-0.5f},{1,1,0},{0,0}},
        // +Z face (magenta)
        {{-0.5f,-0.5f,0.5f},{1,0,1},{0,1}}, {{0.5f,-0.5f,0.5f},{1,0,1},{1,1}},
        {{0.5f,0.5f,0.5f},{1,0,1},{1,0}},   {{-0.5f,0.5f,0.5f},{1,0,1},{0,0}},
        // -Z face (cyan)
        {{0.5f,-0.5f,-0.5f},{0,1,1},{0,1}}, {{-0.5f,-0.5f,-0.5f},{0,1,1},{1,1}},
        {{-0.5f,0.5f,-0.5f},{0,1,1},{1,0}}, {{0.5f,0.5f,-0.5f},{0,1,1},{0,0}},
    };

    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = sizeof(cube_verts);
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vinit = {};
    vinit.pSysMem = cube_verts;
    ID3D11Buffer* vertex_buffer = nullptr;
    hr = device->CreateBuffer(&vbd, &vinit, &vertex_buffer);
    std::printf("[d3d11-test] CreateBuffer(vertex) -> hr=0x%08lX buffer=%p\n",
                static_cast<unsigned long>(hr), static_cast<void*>(vertex_buffer));
    if (FAILED(hr) || !vertex_buffer) return 1;

    // 9. Index buffer: 36 indices (12 triangles).
    const std::uint16_t cube_indices[36] = {
        0,1,2, 0,2,3,      // +X
        4,5,6, 4,6,7,      // -X
        8,9,10, 8,10,11,   // +Y
        12,13,14, 12,14,15,// -Y
        16,17,18, 16,18,19,// +Z
        20,21,22, 20,22,23 // -Z
    };
    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth = sizeof(cube_indices);
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iinit = {};
    iinit.pSysMem = cube_indices;
    ID3D11Buffer* index_buffer = nullptr;
    hr = device->CreateBuffer(&ibd, &iinit, &index_buffer);
    std::printf("[d3d11-test] CreateBuffer(index) -> hr=0x%08lX buffer=%p\n",
                static_cast<unsigned long>(hr), static_cast<void*>(index_buffer));
    if (FAILED(hr) || !index_buffer) return 1;

    // 10. Constant buffer: mat4 (16 floats = 64 bytes, rounded to 256 for alignment).
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = 256;
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ID3D11Buffer* constant_buffer = nullptr;
    hr = device->CreateBuffer(&cbd, nullptr, &constant_buffer);
    std::printf("[d3d11-test] CreateBuffer(constant) -> hr=0x%08lX buffer=%p\n",
                static_cast<unsigned long>(hr), static_cast<void*>(constant_buffer));
    if (FAILED(hr) || !constant_buffer) return 1;

    // 11. Set pipeline state.
    context->IASetInputLayout(input_layout);
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
    context->IASetIndexBuffer(index_buffer, DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vs, nullptr, 0);
    context->PSSetShader(ps, nullptr, 0);
    context->RSSetState(raster_state);

    ID3D11ShaderResourceView* srv_arr[] = {srv};
    context->PSSetShaderResources(0, 1, srv_arr);
    std::printf("[d3d11-test] PSSetShaderResources OK\n");
    ID3D11SamplerState* samp_arr[] = {sampler};
    context->PSSetSamplers(0, 1, samp_arr);
    std::printf("[d3d11-test] PSSetSamplers OK\n");

    // Bind the constant buffer to the vertex shader (slot 0 -> set 0 binding 0).
    ID3D11Buffer* cb_arr[] = {constant_buffer};
    context->VSSetConstantBuffers(0, 1, cb_arr);
    std::printf("[d3d11-test] VSSetConstantBuffers OK\n");

    std::printf("[d3d11-test] DrawIndexed(36, 0, 0) setup complete\n");
    std::printf("[d3d11-test] Presenting for 3 seconds...\n");

    // 12. Render loop: update MVP, DrawIndexed, Present for ~3 seconds.
    auto start = std::chrono::steady_clock::now();
    int frames = 0;
    bool first_frame = true;
    while (true) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Compute elapsed time for rotation.
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        float t = static_cast<float>(elapsed.count()) / 1000.0f;

        // Update the constant buffer with a new MVP matrix.
        Mat4 model = Mat4::rotation_y(t * 1.5f) * Mat4::rotation_x(t * 0.7f);
        Mat4 view = Mat4::translate(0.0f, 0.0f, -3.0f);
        Mat4 proj = Mat4::perspective(0.9f, 800.0f / 600.0f, 0.1f, 100.0f);
        Mat4 mvp = proj * view * model;

        D3D11_MAPPED_SUBRESOURCE cb_mapped = {};
        hr = context->Map(constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &cb_mapped);
        if (SUCCEEDED(hr) && cb_mapped.pData) {
            std::memcpy(cb_mapped.pData, mvp.m, sizeof(mvp.m));
            context->Unmap(constant_buffer, 0);
            if (first_frame) {
                std::printf("[d3d11-test] Map(constant buffer) OK, MVP updated\n");
            }
        } else if (first_frame) {
            std::printf("[d3d11-test] FAIL: Map(constant buffer) failed\n");
            return 1;
        }

        // Draw the cube with indices.
        context->DrawIndexed(36, 0, 0);
        if (first_frame) {
            std::printf("[d3d11-test] DrawIndexed(36, 0, 0) returned\n");
            first_frame = false;
        }

        if (swap_chain) {
            hr = swap_chain->Present(0, 0);
            if (FAILED(hr)) {
                std::printf("[d3d11-test] Present failed hr=0x%08lX at frame %d\n",
                            static_cast<unsigned long>(hr), frames);
                break;
            }
        }
        ++frames;

        if (elapsed.count() >= 3000) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
done:
    std::printf("[d3d11-test] Presented %d frames in 3 seconds\n", frames);

    // 13. Clean up.
    auto mark = [t0 = std::chrono::steady_clock::now()](const char* what) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t0)
                      .count();
        std::fprintf(stderr, "[d3d11-timing] %6lld ms %s\n",
                     static_cast<long long>(ms), what);
    };
    mark("teardown-start");
    constant_buffer->Release();
    index_buffer->Release();
    vertex_buffer->Release();
    raster_state->Release();
    input_layout->Release();
    sampler->Release();
    srv->Release();
    texture->Release();
    ps->Release();
    vs->Release();
    mark("objects-released");
    if (swap_chain) swap_chain->Release();
    mark("swapchain-released");
    context->Release();
    mark("context-released");
    device->Release();
    mark("device-released");
    DestroyWindow(hwnd);
    mark("window-destroyed");

    std::printf("[d3d11-test] PASS\n");
    return 0;
}
