// d3d11_triangle.cpp - minimal D3D11 "hello triangle" test fixture (Phase 2).
//
// Exercises the shim surface: D3D11CreateDevice, CreateBuffer (with real
// Vulkan backing), Map/Unmap, IASetVertexBuffers, Draw, and exit.
// Full shader compilation / rendering is Phase 3; here we just verify the
// API surface works end-to-end without crashing.

#include <d3d11.h>
#include <dxgi.h>
#include <cstdio>
#include <cstdint>

struct Vertex {
    float pos[3];
    float color[3];
};

int main() {
    // 1. Create device + immediate context.
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // pAdapter (default)
        D3D_DRIVER_TYPE_HARDWARE,   // DriverType
        nullptr,                    // Software
        0,                          // Flags
        nullptr, 0,                 // pFeatureLevels / FeatureLevels (any)
        D3D11_SDK_VERSION,
        &device,
        &feature_level,
        &context);

    std::printf("[d3d11-test] D3D11CreateDevice -> hr=0x%08lX feature=0x%X\n",
                static_cast<unsigned long>(hr), static_cast<unsigned>(feature_level));
    if (FAILED(hr) || !device || !context) {
        std::printf("[d3d11-test] FAIL: device creation failed\n");
        return 1;
    }

    // 2. Create a dynamic vertex buffer (3 vertices, pos+color).
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
        device->Release();
        context->Release();
        return 1;
    }

    // 3. Map, write 3 vertices (a triangle), unmap.
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
        vertex_buffer->Release();
        device->Release();
        context->Release();
        return 1;
    }

    // 4. Set pipeline state and draw.
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Shaders are Phase 3; CreateVertexShader returns E_NOTIMPL for now.
    // We still call Draw to verify the API surface doesn't crash.
    context->Draw(3, 0);
    std::printf("[d3d11-test] Draw(3, 0) returned\n");

    // 5. Clean up.
    vertex_buffer->Release();
    context->Release();
    device->Release();

    std::printf("[d3d11-test] PASS\n");
    return 0;
}
