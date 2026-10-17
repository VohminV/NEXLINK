// d3d11_triangle.cpp - minimal D3D11 "hello triangle" test fixture (Phase 3).
//
// Exercises the full pipeline: D3D11CreateDeviceAndSwapChain, CreateBuffer,
// CreateVertexShader / CreatePixelShader (stub SPIR-V), CreateInputLayout,
// CreateRasterizerState, Map/Unmap, IASetVertexBuffers, Draw (records a
// VkCommandBuffer), and Present (reads back to triangle.ppm).

#include <d3d11.h>
#include <dxgi.h>
#include <cstdio>
#include <cstdint>
#include <cstring>

struct Vertex {
    float pos[3];
    float color[3];
};

int main() {
    // 1. Create device + swap chain + immediate context.
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferDesc.Width = 640;
    scd.BufferDesc.Height = 480;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 1;
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

    // 2. Create shaders (stub SPIR-V; DXBC is ignored).
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

    // 3. Create input layout matching the stub VS (position + color).
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    ID3D11InputLayout* input_layout = nullptr;
    hr = device->CreateInputLayout(layout, 2, nullptr, 0, &input_layout);
    std::printf("[d3d11-test] CreateInputLayout -> hr=0x%08lX\n",
                static_cast<unsigned long>(hr));
    if (FAILED(hr) || !input_layout) {
        std::printf("[d3d11-test] FAIL: CreateInputLayout failed\n");
        return 1;
    }

    // 4. Create rasterizer state (no culling, solid fill).
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

    // 5. Create a dynamic vertex buffer (3 vertices, pos+color).
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

    // 6. Map, write 3 vertices (a triangle), unmap.
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    hr = context->Map(vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    std::printf("[d3d11-test] Map -> hr=0x%08lX pData=%p\n",
                static_cast<unsigned long>(hr), mapped.pData);
    if (SUCCEEDED(hr) && mapped.pData) {
        Vertex triangle[3] = {
            {{ 0.0f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},   // top, red
            {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},   // bottom-left, green
            {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},   // bottom-right, blue
        };
        auto* dst = static_cast<Vertex*>(mapped.pData);
        for (int i = 0; i < 3; ++i) dst[i] = triangle[i];
        context->Unmap(vertex_buffer, 0);
        std::printf("[d3d11-test] Unmap OK\n");
    } else {
        std::printf("[d3d11-test] FAIL: Map failed\n");
        return 1;
    }

    // 7. Set pipeline state.
    context->IASetInputLayout(input_layout);
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vs, nullptr, 0);
    context->PSSetShader(ps, nullptr, 0);
    context->RSSetState(raster_state);

    // 8. Draw.
    context->Draw(3, 0);
    std::printf("[d3d11-test] Draw(3, 0) returned\n");

    // 9. Present -> reads back to triangle.ppm.
    if (swap_chain) {
        hr = swap_chain->Present(0, 0);
        std::printf("[d3d11-test] Present -> hr=0x%08lX\n",
                    static_cast<unsigned long>(hr));
    }

    // 10. Clean up.
    vertex_buffer->Release();
    raster_state->Release();
    input_layout->Release();
    ps->Release();
    vs->Release();
    if (swap_chain) swap_chain->Release();
    context->Release();
    device->Release();

    std::printf("[d3d11-test] PASS\n");
    return 0;
}
