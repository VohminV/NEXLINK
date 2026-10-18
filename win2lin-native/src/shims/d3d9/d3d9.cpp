// libwin_d3d9.so - D3D9 → Vulkan shim (stub rendering).
// Implements IDirect3D9 / IDirect3DDevice9 COM vtables (ms_abi) backed by a
// minimal Vulkan device + swapchain so 2000-2010 era games can initialize D3D9.
// Rendering is stubbed (clear only); Present swaps the Vulkan swapchain image.

#include "d3d9.h"

#include "common/logging.hpp"

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace win2lin::d3d9 {

#define WINAPI __attribute__((ms_abi))

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// ---------------------------------------------------------------------------
// Global Vulkan context
// ---------------------------------------------------------------------------
static struct {
    VkInstance inst = VK_NULL_HANDLE;
    VkPhysicalDevice phy = VK_NULL_HANDLE;
    VkDevice dev = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t qfam = 0;
    VkSurfaceKHR surf = VK_NULL_HANDLE;
    VkSwapchainKHR sc = VK_NULL_HANDLE;
    VkFormat fmt = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D extent{};
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkSemaphore image_acquired = VK_NULL_HANDLE;
    VkSemaphore render_done = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
} g_vk;

static bool g_vk_ready = false;

// ---------------------------------------------------------------------------
// COM helpers
// ---------------------------------------------------------------------------
static ULONG com_addref(uintptr_t* ref) {
    return (ULONG)(++(*ref));
}
static ULONG com_release(uintptr_t* ref) {
    return (ULONG)(--(*ref));
}

// ===========================================================================
// IDirect3DDevice9 (defined first so D3D9::CreateDevice can instantiate it)
// ===========================================================================
struct D3D9Device : IDirect3DDevice9 {
    uintptr_t ref = 1;
    HWND hwnd = nullptr;
    D3DPRESENT_PARAMETERS pp{};
    bool scene_begun = false;

    D3D9Device(HWND h, D3DPRESENT_PARAMETERS* p) : hwnd(h) {
        if (p) memcpy(&pp, p, sizeof(pp));
    }

    ~D3D9Device() { shutdown(); }

    bool init() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "win2lin-d3d9";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "win2lin";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo = &appInfo;

        uint32_t extCount = 0;
        if (SDL_Vulkan_GetInstanceExtensions((SDL_Window*)hwnd, &extCount, nullptr) == SDL_TRUE) {
            std::vector<const char*> tmp(extCount);
            if (SDL_Vulkan_GetInstanceExtensions((SDL_Window*)hwnd, &extCount, tmp.data()) == SDL_TRUE) {
                ci.enabledExtensionCount = extCount;
                ci.ppEnabledExtensionNames = tmp.data();
            }
        }

        if (vkCreateInstance(&ci, nullptr, &g_vk.inst) != VK_SUCCESS) {
            LOG_ERROR("D3D9: vkCreateInstance failed");
            return false;
        }

        uint32_t phyCount = 0;
        vkEnumeratePhysicalDevices(g_vk.inst, &phyCount, nullptr);
        if (phyCount == 0) { LOG_ERROR("D3D9: no Vulkan physical devices"); return false; }
        std::vector<VkPhysicalDevice> phys(phyCount);
        vkEnumeratePhysicalDevices(g_vk.inst, &phyCount, phys.data());
        g_vk.phy = phys[0];

        uint32_t qfCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(g_vk.phy, &qfCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qfCount);
        vkGetPhysicalDeviceQueueFamilyProperties(g_vk.phy, &qfCount, qfs.data());
        bool found = false;
        for (uint32_t i = 0; i < qfCount; ++i) {
            if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                g_vk.qfam = i; found = true; break;
            }
        }
        if (!found) { LOG_ERROR("D3D9: no graphics queue"); return false; }

        float prio = 1.0f;
        VkDeviceQueueCreateInfo dqi{};
        dqi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        dqi.queueFamilyIndex = g_vk.qfam;
        dqi.queueCount = 1;
        dqi.pQueuePriorities = &prio;

        VkDeviceCreateInfo devCi{};
        devCi.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        devCi.queueCreateInfoCount = 1;
        devCi.pQueueCreateInfos = &dqi;
        const char* devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        devCi.enabledExtensionCount = 1;
        devCi.ppEnabledExtensionNames = devExts;

        if (vkCreateDevice(g_vk.phy, &devCi, nullptr, &g_vk.dev) != VK_SUCCESS) {
            LOG_ERROR("D3D9: vkCreateDevice failed"); return false;
        }
        vkGetDeviceQueue(g_vk.dev, g_vk.qfam, 0, &g_vk.queue);

        if (SDL_Vulkan_CreateSurface((SDL_Window*)hwnd, g_vk.inst, &g_vk.surf) != SDL_TRUE) {
            LOG_ERROR("D3D9: SDL_Vulkan_CreateSurface failed"); return false;
        }

        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_vk.phy, g_vk.surf, &caps);
        g_vk.extent = caps.currentExtent;
        if (g_vk.extent.width == 0xFFFFFFFF) {
            g_vk.extent.width = pp.BackBufferWidth ? pp.BackBufferWidth : 1280;
            g_vk.extent.height = pp.BackBufferHeight ? pp.BackBufferHeight : 720;
        }

        VkSwapchainCreateInfoKHR scCi{};
        scCi.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scCi.surface = g_vk.surf;
        scCi.minImageCount = 2;
        scCi.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        scCi.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        scCi.imageExtent = g_vk.extent;
        scCi.imageArrayLayers = 1;
        scCi.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        scCi.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        scCi.preTransform = caps.currentTransform;
        scCi.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        scCi.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        scCi.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(g_vk.dev, &scCi, nullptr, &g_vk.sc) != VK_SUCCESS) {
            LOG_ERROR("D3D9: vkCreateSwapchainKHR failed"); return false;
        }

        uint32_t imgCount = 0;
        vkGetSwapchainImagesKHR(g_vk.dev, g_vk.sc, &imgCount, nullptr);
        g_vk.images.resize(imgCount);
        vkGetSwapchainImagesKHR(g_vk.dev, g_vk.sc, &imgCount, g_vk.images.data());
        g_vk.views.resize(imgCount);
        for (uint32_t i = 0; i < imgCount; ++i) {
            VkImageViewCreateInfo ivCi{};
            ivCi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ivCi.image = g_vk.images[i];
            ivCi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ivCi.format = g_vk.fmt;
            ivCi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            vkCreateImageView(g_vk.dev, &ivCi, nullptr, &g_vk.views[i]);
        }

        VkCommandPoolCreateInfo cpCi{};
        cpCi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cpCi.queueFamilyIndex = g_vk.qfam;
        cpCi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCreateCommandPool(g_vk.dev, &cpCi, nullptr, &g_vk.pool);

        VkCommandBufferAllocateInfo cbAi{};
        cbAi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbAi.commandPool = g_vk.pool;
        cbAi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAi.commandBufferCount = 1;
        vkAllocateCommandBuffers(g_vk.dev, &cbAi, &g_vk.cmd);

        VkSemaphoreCreateInfo semCi{};
        semCi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vkCreateSemaphore(g_vk.dev, &semCi, nullptr, &g_vk.image_acquired);
        vkCreateSemaphore(g_vk.dev, &semCi, nullptr, &g_vk.render_done);

        VkFenceCreateInfo fenceCi{};
        fenceCi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(g_vk.dev, &fenceCi, nullptr, &g_vk.fence);

        g_vk_ready = true;
        LOG_INFO("D3D9: device initialized, swapchain %ux%u", g_vk.extent.width, g_vk.extent.height);
        return true;
    }

    void shutdown() {
        if (!g_vk.dev) return;
        vkDeviceWaitIdle(g_vk.dev);
        if (g_vk.fence) vkDestroyFence(g_vk.dev, g_vk.fence, nullptr);
        if (g_vk.render_done) vkDestroySemaphore(g_vk.dev, g_vk.render_done, nullptr);
        if (g_vk.image_acquired) vkDestroySemaphore(g_vk.dev, g_vk.image_acquired, nullptr);
        if (g_vk.cmd) vkFreeCommandBuffers(g_vk.dev, g_vk.pool, 1, &g_vk.cmd);
        if (g_vk.pool) vkDestroyCommandPool(g_vk.dev, g_vk.pool, nullptr);
        for (auto v : g_vk.views) if (v) vkDestroyImageView(g_vk.dev, v, nullptr);
        if (g_vk.sc) vkDestroySwapchainKHR(g_vk.dev, g_vk.sc, nullptr);
        if (g_vk.surf) vkDestroySurfaceKHR(g_vk.inst, g_vk.surf, nullptr);
        if (g_vk.dev) vkDestroyDevice(g_vk.dev, nullptr);
        if (g_vk.inst) vkDestroyInstance(g_vk.inst, nullptr);
        memset(&g_vk, 0, sizeof(g_vk));
        g_vk_ready = false;
    }

    // --- COM methods ---
    static HRESULT WINAPI QueryInterface(IDirect3DDevice9*, REFIID, LPVOID*) { return E_NOINTERFACE; }
    static ULONG WINAPI AddRef(IDirect3DDevice9* iface) {
        auto self = static_cast<D3D9Device*>(iface);
        return com_addref(&self->ref);
    }
    static ULONG WINAPI Release(IDirect3DDevice9* iface) {
        auto self = static_cast<D3D9Device*>(iface);
        auto v = com_release(&self->ref);
        if (v == 0) delete self;
        return v;
    }
    static HRESULT WINAPI TestCooperativeLevel(IDirect3DDevice9*) { return D3D_OK; }
    static UINT WINAPI GetAvailableTextureMem(IDirect3DDevice9*) { return 0; }
    static HRESULT WINAPI EvictManagedResources(IDirect3DDevice9*) { return D3D_OK; }
    static HRESULT WINAPI GetDirect3D(IDirect3DDevice9*, IDirect3D9**) { return D3D_OK; }
    static HRESULT WINAPI GetDeviceCaps(IDirect3DDevice9*, D3DCAPS9*) { return D3D_OK; }
    static HRESULT WINAPI GetDisplayMode(IDirect3DDevice9*, UINT, D3DDISPLAYMODE*) { return D3D_OK; }
    static HRESULT WINAPI GetCreationParameters(IDirect3DDevice9*, D3DDEVICE_CREATION_PARAMETERS*) { return D3D_OK; }
    static HRESULT WINAPI SetCursorProperties(IDirect3DDevice9*, UINT, UINT, IDirect3DSurface9*) { return D3D_OK; }
    static HRESULT WINAPI SetCursorPosition(IDirect3DDevice9*, INT, INT, DWORD) { return D3D_OK; }
    static BOOL WINAPI ShowCursor(IDirect3DDevice9*, BOOL) { return TRUE; }
    static HRESULT WINAPI CreateRenderTarget(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*) { return D3D_OK; }
    static HRESULT WINAPI CreateDepthStencilSurface(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*) { return D3D_OK; }
    static HRESULT WINAPI CreateSurface(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*) { return D3D_OK; }
    static HRESULT WINAPI CreateTexture(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*) { return D3D_OK; }
    static HRESULT WINAPI CreateVolumeTexture(IDirect3DDevice9*, UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture9**, HANDLE*) { return D3D_OK; }
    static HRESULT WINAPI CreateCubeTexture(IDirect3DDevice9*, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture9**, HANDLE*) { return D3D_OK; }
    static HRESULT WINAPI CreateVertexBuffer(IDirect3DDevice9*, UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9**, HANDLE*) { return D3D_OK; }
    static HRESULT WINAPI CreateIndexBuffer(IDirect3DDevice9*, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer9**, HANDLE*) { return D3D_OK; }
    static HRESULT WINAPI CreateImageSurface(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, IDirect3DSurface9**) { return D3D_OK; }
    static HRESULT WINAPI LockRect(IDirect3DSurface9*, D3DLOCKED_RECT*, const D3DRECT*, DWORD) { return D3D_OK; }
    static HRESULT WINAPI UnlockRect(IDirect3DSurface9*) { return D3D_OK; }
    static HRESULT WINAPI LockBox(IDirect3DVolume9*, D3DLOCKED_BOX*, const D3DBOX*, DWORD) { return D3D_OK; }
    static HRESULT WINAPI UnlockBox(IDirect3DVolume9*) { return D3D_OK; }
    static HRESULT WINAPI GetRectangle(IDirect3DSurface9*, D3DRECT*) { return D3D_OK; }
    static HRESULT WINAPI SetRenderTarget(IDirect3DDevice9*, DWORD, IDirect3DSurface9*) { return D3D_OK; }
    static HRESULT WINAPI GetRenderTarget(IDirect3DDevice9*, DWORD, IDirect3DSurface9**) { return D3D_OK; }
    static HRESULT WINAPI SetDepthStencilSurface(IDirect3DDevice9*, IDirect3DSurface9*) { return D3D_OK; }
    static HRESULT WINAPI GetDepthStencilSurface(IDirect3DDevice9*, IDirect3DSurface9**) { return D3D_OK; }
    static HRESULT WINAPI BeginScene(IDirect3DDevice9* iface) {
        static_cast<D3D9Device*>(iface)->scene_begun = true;
        return D3D_OK;
    }
    static HRESULT WINAPI EndScene(IDirect3DDevice9* iface) {
        static_cast<D3D9Device*>(iface)->scene_begun = false;
        return D3D_OK;
    }
    static HRESULT WINAPI Clear(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, FLOAT, DWORD) { return D3D_OK; }
    static HRESULT WINAPI SetTransform(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*) { return D3D_OK; }
    static HRESULT WINAPI GetTransform(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, D3DMATRIX*) { return D3D_OK; }
    static HRESULT WINAPI MultiplyTransform(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*) { return D3D_OK; }
    static HRESULT WINAPI SetViewport(IDirect3DDevice9*, const D3DVIEWPORT9*) { return D3D_OK; }
    static HRESULT WINAPI GetViewport(IDirect3DDevice9*, D3DVIEWPORT9*) { return D3D_OK; }
    static HRESULT WINAPI SetMaterial(IDirect3DDevice9*, const D3DMATERIAL9*) { return D3D_OK; }
    static HRESULT WINAPI GetMaterial(IDirect3DDevice9*, D3DMATERIAL9*) { return D3D_OK; }
    static HRESULT WINAPI SetLight(IDirect3DDevice9*, DWORD, const D3DLIGHT9*) { return D3D_OK; }
    static HRESULT WINAPI GetLight(IDirect3DDevice9*, DWORD, D3DLIGHT9*) { return D3D_OK; }
    static HRESULT WINAPI LightEnable(IDirect3DDevice9*, DWORD, BOOL) { return D3D_OK; }
    static HRESULT WINAPI GetLightEnable(IDirect3DDevice9*, DWORD, BOOL*) { return D3D_OK; }
    static HRESULT WINAPI SetClipPlane(IDirect3DDevice9*, DWORD, const FLOAT*) { return D3D_OK; }
    static HRESULT WINAPI GetClipPlane(IDirect3DDevice9*, DWORD, FLOAT*) { return D3D_OK; }
    static HRESULT WINAPI SetRenderState(IDirect3DDevice9*, D3DRENDERSTATETYPE, DWORD) { return D3D_OK; }
    static HRESULT WINAPI GetRenderState(IDirect3DDevice9*, D3DRENDERSTATETYPE, DWORD*) { return D3D_OK; }
    static HRESULT WINAPI CreateStateBlock(IDirect3DDevice9*, D3DSTATEBLOCKTYPE, IDirect3DStateBlock9**) { return D3D_OK; }
    static HRESULT WINAPI GetStateBlock(IDirect3DDevice9*, DWORD, IDirect3DStateBlock9**) { return D3D_OK; }
    static HRESULT WINAPI BeginStateBlock(IDirect3DDevice9*) { return D3D_OK; }
    static HRESULT WINAPI EndStateBlock(IDirect3DDevice9*, DWORD*) { return D3D_OK; }
    static HRESULT WINAPI SetClipStatus(IDirect3DDevice9*, const D3DCLIPSTATUS9*) { return D3D_OK; }
    static HRESULT WINAPI GetClipStatus(IDirect3DDevice9*, D3DCLIPSTATUS9*) { return D3D_OK; }
    static HRESULT WINAPI GetTexture(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9**) { return D3D_OK; }
    static HRESULT WINAPI SetTexture(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*) { return D3D_OK; }
    static HRESULT WINAPI GetSamplerState(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD*) { return D3D_OK; }
    static HRESULT WINAPI SetSamplerState(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD) { return D3D_OK; }
    static HRESULT WINAPI ValidateDevice(IDirect3DDevice9*, DWORD*) { return D3D_OK; }
    static HRESULT WINAPI GetInfo(IDirect3DDevice9*, DWORD, VOID*, DWORD) { return D3D_OK; }
    static HRESULT WINAPI SetPreferredOffset(IDirect3DDevice9*, UINT) { return D3D_OK; }
    static HRESULT WINAPI SetResourcePriority(IDirect3DDevice9*, IDirect3DResource9*, DWORD) { return D3D_OK; }
    static HRESULT WINAPI PreLoad(IDirect3DDevice9*, IDirect3DResource9*) { return D3D_OK; }
    static HRESULT WINAPI SetCurrentTexturePalette(IDirect3DDevice9*, UINT) { return D3D_OK; }
    static HRESULT WINAPI GetCurrentTexturePalette(IDirect3DDevice9*, UINT*) { return D3D_OK; }
    static HRESULT WINAPI SetScissorRect(IDirect3DDevice9*, const D3DRECT*) { return D3D_OK; }
    static HRESULT WINAPI GetScissorRect(IDirect3DDevice9*, D3DRECT*) { return D3D_OK; }
    static HRESULT WINAPI SetSoftwareVertexProcessing(IDirect3DDevice9*, BOOL) { return D3D_OK; }
    static BOOL WINAPI GetSoftwareVertexProcessing(IDirect3DDevice9*) { return TRUE; }
    static HRESULT WINAPI SetNPatchMode(IDirect3DDevice9*, FLOAT) { return D3D_OK; }
    static FLOAT WINAPI GetNPatchMode(IDirect3DDevice9*) { return 0.0f; }
    static HRESULT WINAPI DrawPrimitive(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT) { return D3D_OK; }
    static HRESULT WINAPI DrawIndexedPrimitive(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT) { return D3D_OK; }
    static HRESULT WINAPI DrawPrimitiveUP(IDirect3DDevice9*, D3DPTRIMITIVETYPE, UINT, const VOID*, UINT, const D3DVERTEXELEMENT9*, UINT) { return D3D_OK; }
    static HRESULT WINAPI DrawIndexedPrimitiveUP(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT, const VOID*, UINT, D3DFORMAT, const VOID*, UINT) { return D3D_OK; }
    static HRESULT WINAPI ProcessVertices(IDirect3DDevice9*, UINT, UINT, UINT, IDirect3DVertexBuffer9*, DWORD) { return D3D_OK; }
    static HRESULT WINAPI CreateVertexDeclaration(IDirect3DDevice9*, const D3DVERTEXELEMENT9*, IDirect3DVertexDeclaration9**) { return D3D_OK; }
    static HRESULT WINAPI SetVertexDeclaration(IDirect3DDevice9*, IDirect3DVertexDeclaration9*) { return D3D_OK; }
    static HRESULT WINAPI GetVertexDeclaration(IDirect3DDevice9*, IDirect3DVertexDeclaration9**) { return D3D_OK; }
    static HRESULT WINAPI SetFVF(IDirect3DDevice9*, DWORD) { return D3D_OK; }
    static HRESULT WINAPI GetFVF(IDirect3DDevice9*, DWORD*) { return D3D_OK; }
    static HRESULT WINAPI CreateVertexShader(IDirect3DDevice9*, const DWORD*, IDirect3DVertexShader9**) { return D3D_OK; }
    static HRESULT WINAPI SetVertexShader(IDirect3DDevice9*, IDirect3DVertexShader9*) { return D3D_OK; }
    static HRESULT WINAPI GetVertexShader(IDirect3DDevice9*, IDirect3DVertexShader9**) { return D3D_OK; }
    static HRESULT WINAPI SetVertexShaderConstantF(IDirect3DDevice9*, UINT, const FLOAT*, UINT) { return D3D_OK; }
    static HRESULT WINAPI GetVertexShaderConstantF(IDirect3DDevice9*, UINT, FLOAT*, UINT) { return D3D_OK; }
    static HRESULT WINAPI SetVertexShaderConstantI(IDirect3DDevice9*, UINT, const INT*, UINT) { return D3D_OK; }
    static HRESULT WINAPI GetVertexShaderConstantI(IDirect3DDevice9*, UINT, INT*, UINT) { return D3D_OK; }
    static HRESULT WINAPI SetVertexShaderConstantB(IDirect3DDevice9*, UINT, const BOOL*, UINT) { return D3D_OK; }
    static HRESULT WINAPI GetVertexShaderConstantB(IDirect3DDevice9*, UINT, BOOL*, UINT) { return D3D_OK; }
    static HRESULT WINAPI SetStreamSourceFreq(IDirect3DDevice9*, UINT, UINT) { return D3D_OK; }
    static HRESULT WINAPI SetStreamSource(IDirect3DDevice9*, UINT, IDirect3DVertexBuffer9*, UINT, UINT) { return D3D_OK; }
    static HRESULT WINAPI GetStreamSource(IDirect3DDevice9*, UINT, IDirect3DVertexBuffer9**, UINT*, UINT*) { return D3D_OK; }
    static HRESULT WINAPI SetIndices(IDirect3DDevice9*, IDirect3DIndexBuffer9*) { return D3D_OK; }
    static HRESULT WINAPI GetIndices(IDirect3DDevice9*, IDirect3DIndexBuffer9**) { return D3D_OK; }
    static HRESULT WINAPI CreatePixelShader(IDirect3DDevice9*, const DWORD*, IDirect3DPixelShader9**) { return D3D_OK; }
    static HRESULT WINAPI SetPixelShader(IDirect3DDevice9*, IDirect3DPixelShader9*) { return D3D_OK; }
    static HRESULT WINAPI GetPixelShader(IDirect3DDevice9*, IDirect3DPixelShader9**) { return D3D_OK; }
    static HRESULT WINAPI SetPixelShaderConstantF(IDirect3DDevice9*, UINT, const FLOAT*, UINT) { return D3D_OK; }
    static HRESULT WINAPI GetPixelShaderConstantF(IDirect3DDevice9*, UINT, FLOAT*, UINT) { return D3D_OK; }
    static HRESULT WINAPI SetPixelShaderConstantI(IDirect3DDevice9*, UINT, const INT*, UINT) { return D3D_OK; }
    static HRESULT WINAPI GetPixelShaderConstantI(IDirect3DDevice9*, UINT, INT*, UINT) { return D3D_OK; }
    static HRESULT WINAPI SetPixelShaderConstantB(IDirect3DDevice9*, UINT, const BOOL*, UINT) { return D3D_OK; }
    static HRESULT WINAPI GetPixelShaderConstantB(IDirect3DDevice9*, UINT, BOOL*, UINT) { return D3D_OK; }
    static HRESULT WINAPI DrawRectPatch(IDirect3DDevice9*, UINT, const D3DRECTPATCH_INFO*, const D3DRECTPATCH_INFO*) { return D3D_OK; }
    static HRESULT WINAPI DrawTriPatch(IDirect3DDevice9*, UINT, const D3DTRIPATCH_INFO*, const D3DTRIPATCH_INFO*) { return D3D_OK; }
    static HRESULT WINAPI DeletePatch(IDirect3DDevice9*, UINT) { return D3D_OK; }
    static HRESULT WINAPI CreateQuery(IDirect3DDevice9*, D3DQUERYTYPE, IDirect3DQuery9**) { return D3D_OK; }

    static HRESULT WINAPI Present(IDirect3DDevice9* iface, const D3DRECT*, const D3DRECT*, HWND, RGNDATA*) {
        if (!g_vk_ready) return D3D_OK;
        VkDevice dev = g_vk.dev;
        VkSwapchainKHR sc = g_vk.sc;
        VkQueue queue = g_vk.queue;
        VkSemaphore ia = g_vk.image_acquired;
        VkSemaphore rd = g_vk.render_done;
        VkFence fence = g_vk.fence;
        VkCommandBuffer cmd = g_vk.cmd;

        vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
        vkResetFences(dev, 1, &fence);

        uint32_t idx = 0;
        VkResult r = vkAcquireNextImageKHR(dev, sc, UINT64_MAX, ia, VK_NULL_HANDLE, &idx);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) return D3D_OK;

        VkCommandBufferBeginInfo cbi{};
        cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &cbi);

        VkClearColorValue clear{ {0.0f, 0.1f, 0.2f, 1.0f} };
        VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage(cmd, g_vk.images[idx], VK_IMAGE_LAYOUT_UNDEFINED, &clear, 1, &range);

        vkEndCommandBuffer(cmd);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &ia;
        si.pWaitDstStageMask = &waitStage;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &rd;
        vkQueueSubmit(queue, 1, &si, fence);

        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &rd;
        pi.swapchainCount = 1;
        pi.pSwapchains = &sc;
        pi.pImageIndices = &idx;
        vkQueuePresentKHR(queue, &pi);

        return D3D_OK;
    }

    static const IDirect3DDevice9Vtbl vtbl;
};

const IDirect3DDevice9Vtbl D3D9Device::vtbl = {
    D3D9Device::QueryInterface, D3D9Device::AddRef, D3D9Device::Release,
    D3D9Device::TestCooperativeLevel, D3D9Device::GetAvailableTextureMem,
    D3D9Device::EvictManagedResources, D3D9Device::GetDirect3D, D3D9Device::GetDeviceCaps,
    D3D9Device::GetDisplayMode, D3D9Device::GetCreationParameters, D3D9Device::SetCursorProperties,
    D3D9Device::SetCursorPosition, D3D9Device::ShowCursor, D3D9Device::CreateRenderTarget,
    D3D9Device::CreateDepthStencilSurface, D3D9Device::CreateSurface, D3D9Device::CreateTexture,
    D3D9Device::CreateVolumeTexture, D3D9Device::CreateCubeTexture, D3D9Device::CreateVertexBuffer,
    D3D9Device::CreateIndexBuffer, D3D9Device::CreateImageSurface, D3D9Device::LockRect,
    D3D9Device::UnlockRect, D3D9Device::LockBox, D3D9Device::UnlockBox, D3D9Device::GetRectangle,
    D3D9Device::SetRenderTarget, D3D9Device::GetRenderTarget, D3D9Device::SetDepthStencilSurface,
    D3D9Device::GetDepthStencilSurface, D3D9Device::BeginScene, D3D9Device::EndScene, D3D9Device::Clear,
    D3D9Device::SetTransform, D3D9Device::GetTransform, D3D9Device::MultiplyTransform, D3D9Device::SetViewport,
    D3D9Device::GetViewport, D3D9Device::SetMaterial, D3D9Device::GetMaterial, D3D9Device::SetLight,
    D3D9Device::GetLight, D3D9Device::LightEnable, D3D9Device::GetLightEnable, D3D9Device::SetClipPlane,
    D3D9Device::GetClipPlane, D3D9Device::SetRenderState, D3D9Device::GetRenderState, D3D9Device::CreateStateBlock,
    D3D9Device::GetStateBlock, D3D9Device::BeginStateBlock, D3D9Device::EndStateBlock, D3D9Device::SetClipStatus,
    D3D9Device::GetClipStatus, D3D9Device::GetTexture, D3D9Device::SetTexture, D3D9Device::GetSamplerState,
    D3D9Device::SetSamplerState, D3D9Device::ValidateDevice, D3D9Device::GetInfo, D3D9Device::SetPreferredOffset,
    D3D9Device::SetResourcePriority, D3D9Device::PreLoad, D3D9Device::SetCurrentTexturePalette,
    D3D9Device::GetCurrentTexturePalette, D3D9Device::SetScissorRect, D3D9Device::GetScissorRect,
    D3D9Device::SetSoftwareVertexProcessing, D3D9Device::GetSoftwareVertexProcessing, D3D9Device::SetNPatchMode,
    D3D9Device::GetNPatchMode, D3D9Device::DrawPrimitive, D3D9Device::DrawIndexedPrimitive, D3D9Device::DrawPrimitiveUP,
    D3D9Device::DrawIndexedPrimitiveUP, D3D9Device::ProcessVertices, D3D9Device::CreateVertexDeclaration,
    D3D9Device::SetVertexDeclaration, D3D9Device::GetVertexDeclaration, D3D9Device::SetFVF,
    D3D9Device::GetFVF, D3D9Device::CreateVertexShader, D3D9Device::SetVertexShader,
    D3D9Device::GetVertexShader, D3D9Device::SetVertexShaderConstantF, D3D9Device::GetVertexShaderConstantF,
    D3D9Device::SetVertexShaderConstantI, D3D9Device::GetVertexShaderConstantI, D3D9Device::SetVertexShaderConstantB,
    D3D9Device::GetVertexShaderConstantB, D3D9Device::SetStreamSourceFreq, D3D9Device::SetStreamSource,
    D3D9Device::GetStreamSource, D3D9Device::SetIndices, D3D9Device::GetIndices,
    D3D9Device::CreatePixelShader, D3D9Device::SetPixelShader, D3D9Device::GetPixelShader,
    D3D9Device::SetPixelShaderConstantF, D3D9Device::GetPixelShaderConstantF, D3D9Device::SetPixelShaderConstantI,
    D3D9Device::GetPixelShaderConstantI, D3D9Device::SetPixelShaderConstantB, D3D9Device::GetPixelShaderConstantB,
    D3D9Device::DrawRectPatch, D3D9Device::DrawTriPatch, D3D9Device::DeletePatch, D3D9Device::CreateQuery,
    D3D9Device::Present,
};

// ===========================================================================
// IDirect3D9
// ===========================================================================
struct D3D9 : IDirect3D9 {
    uintptr_t ref = 1;

    static HRESULT WINAPI QueryInterface(IDirect3D9*, REFIID, LPVOID*) { return E_NOINTERFACE; }
    static ULONG WINAPI AddRef(IDirect3D9* iface) {
        auto self = static_cast<D3D9*>(iface);
        return com_addref(&self->ref);
    }
    static ULONG WINAPI Release(IDirect3D9* iface) {
        auto self = static_cast<D3D9*>(iface);
        auto v = com_release(&self->ref);
        if (v == 0) delete self;
        return v;
    }
    static HRESULT WINAPI RegisterSoftwareDevice(IDirect3D9*, void*) { return D3D_OK; }
    static UINT WINAPI GetAdapterCount(IDirect3D9*) { return 1; }
    static HRESULT WINAPI GetAdapterIdentifier(IDirect3D9*, UINT, DWORD, void*) { return D3D_OK; }
    static UINT WINAPI GetAdapterModeCount(IDirect3D9*, UINT, D3DFORMAT) { return 1; }
    static HRESULT WINAPI EnumAdapterModes(IDirect3D9*, UINT, D3DFORMAT, UINT, D3DDISPLAYMODE*) { return D3D_OK; }
    static HRESULT WINAPI GetAdapterDisplayMode(IDirect3D9*, UINT, D3DDISPLAYMODE* mode) {
        if (mode) { mode->Width = 1280; mode->Height = 720; mode->RefreshRate = 60; mode->Format = D3DFMT_X8R8G8B8; }
        return D3D_OK;
    }
    static HRESULT WINAPI CheckDeviceType(IDirect3D9*, UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, BOOL) { return D3D_OK; }
    static HRESULT WINAPI CheckDeviceFormat(IDirect3D9*, UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT) { return D3D_OK; }
    static HRESULT WINAPI CheckDeviceMultiSampleType(IDirect3D9*, UINT, D3DDEVTYPE, D3DFORMAT, BOOL, D3DMULTISAMPLE_TYPE, DWORD*) { return D3D_OK; }
    static HRESULT WINAPI CheckDepthStencilMatch(IDirect3D9*, UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT) { return D3D_OK; }
    static HRESULT WINAPI CheckDeviceFormatConversion(IDirect3D9*, UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT) { return D3D_OK; }
    static HRESULT WINAPI GetDeviceCaps(IDirect3D9*, UINT, D3DDEVTYPE, D3DCAPS9* caps) {
        if (caps) {
            memset(caps, 0, sizeof(*caps));
            caps->DeviceType = D3DDEVTYPE_HAL;
            caps->AdapterOrdinal = 0;
            caps->PresentationIntervals = 0x00000001;
            caps->MaxTextureWidth = 4096;
            caps->MaxTextureHeight = 4096;
            caps->MaxTextureRepeat = 256;
            caps->MaxTextureAspectRatio = 256;
            caps->MaxAnisotropy = 16;
            caps->MaxVertexW = 16384;
            caps->GuardBandLeft = -32768;
            caps->GuardBandTop = -32768;
            caps->GuardBandRight = 32767;
            caps->GuardBandBottom = 32767;
            caps->StencilCaps = 0x000000FF;
            caps->FVFCaps = 0x00000001;
            caps->TextureOpCaps = 0x0003FFFF;
            caps->MaxTextureBlendStages = 8;
            caps->MaxSimultaneousTextures = 8;
            caps->VertexProcessingCaps = 0x00000001;
            caps->MaxStreams = 16;
            caps->MaxStreamStride = 255;
            caps->VertexShaderVersion = 0xFFFE0300;
            caps->MaxVertexShaderConst = 256;
            caps->PixelShaderVersion = 0xFFFE0300;
            caps->PixelShader1xMaxValue = 8192;
            caps->NumSimultaneousRTs = 4;
            caps->VertexTextureFilterCaps = 0x00000001;
            caps->MaxVShaderInstructionsExecuted = 65535;
            caps->MaxPShaderInstructionsExecuted = 65535;
        }
        return D3D_OK;
    }
    static HMONITOR WINAPI GetAdapterMonitor(IDirect3D9*, UINT) { return nullptr; }
    static HRESULT WINAPI CreateDevice(IDirect3D9*, UINT, D3DDEVTYPE, HWND hFocusWindow,
                                       DWORD, D3DPRESENT_PARAMETERS* pParams,
                                       IDirect3DDevice9** ppOut) {
        LOG_INFO("D3D9: CreateDevice window=%p w=%u h=%u fmt=%u",
                 (void*)hFocusWindow,
                 pParams ? pParams->BackBufferWidth : 0,
                 pParams ? pParams->BackBufferHeight : 0,
                 pParams ? (UINT)pParams->BackBufferFormat : 0);

        auto dev = new D3D9Device(hFocusWindow, pParams);
        if (!dev->init()) {
            delete dev;
            *ppOut = nullptr;
            return D3DERR_OUTOFVIDEOMEMORY;
        }
        *ppOut = static_cast<IDirect3DDevice9*>(dev);
        return D3D_OK;
    }

    static const IDirect3D9Vtbl vtbl;
};

const IDirect3D9Vtbl D3D9::vtbl = {
    D3D9::QueryInterface, D3D9::AddRef, D3D9::Release,
    D3D9::RegisterSoftwareDevice, D3D9::GetAdapterCount, D3D9::GetAdapterIdentifier,
    D3D9::GetAdapterModeCount, D3D9::EnumAdapterModes, D3D9::GetAdapterDisplayMode,
    D3D9::CheckDeviceType, D3D9::CheckDeviceFormat, D3D9::CheckDeviceMultiSampleType,
    D3D9::CheckDepthStencilMatch, D3D9::CheckDeviceFormatConversion, D3D9::GetDeviceCaps,
    D3D9::GetAdapterMonitor, D3D9::CreateDevice,
};

extern "C" __attribute__((ms_abi)) IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion) {
    (void)SDKVersion;
    LOG_INFO("D3D9: Direct3DCreate9(%u)", SDKVersion);
    return new D3D9();
}

} // namespace win2lin::d3d9
