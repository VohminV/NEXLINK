// libwin_d3d9.so - D3D9 → Vulkan shim.
// Implements IDirect3D9 / IDirect3DDevice9 COM vtables (ms_abi) backed by
// Vulkan. Vertex / index buffers, textures, vertex declarations, FVF, render
// states and DrawPrimitive/DrawIndexedPrimitive are translated to Vulkan
// commands. Rendering uses an embedded SPIR-V pass-through shader (position +
// diffuse color) compiled from GLSL at build time via glslangValidator.

#include "d3d9.h"

#include "common/logging.hpp"

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <cstdint>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <unordered_map>

#ifdef WIN2LIN_D3D9_HAS_SHADERS
#include "shaders_embedded.h"
#endif

namespace win2lin::d3d9 {

#define WINAPI __attribute__((ms_abi))

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// Forward declaration of global Vulkan context (defined later)
struct VulkanCtx;
extern VulkanCtx g_vk;
extern bool g_vk_ready;
extern bool g_has_shaders;

struct VulkanCtx {
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
    // Pipeline objects
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    // Descriptor objects
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptor_sets;
    // Uniform buffer (MVP + tint)
    VkBuffer uniform_buffer = VK_NULL_HANDLE;
    VkDeviceMemory uniform_memory = VK_NULL_HANDLE;
    void* uniform_mapped = nullptr;
};

VulkanCtx g_vk;
bool g_vk_ready = false;
bool g_has_shaders = false;

// ---------------------------------------------------------------------------
// Vulkan wrapper helpers
// ---------------------------------------------------------------------------
static VkResult create_buffer(VkDevice dev, VkPhysicalDevice phy, VkDeviceSize size,
                              VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                              VkBuffer* buf, VkDeviceMemory* mem, void** mapped = nullptr) {
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult r = vkCreateBuffer(dev, &bci, nullptr, buf);
    if (r != VK_SUCCESS) return r;

    VkMemoryRequirements mr{};
    vkGetBufferMemoryRequirements(dev, *buf, &mr);

    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(phy, &mp);

    uint32_t mt = 0xFFFFFFFF;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((mr.memoryTypeBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & props) == props) {
            mt = i;
            break;
        }
    }
    if (mt == 0xFFFFFFFF) {
        vkDestroyBuffer(dev, *buf, nullptr);
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = mt;
    r = vkAllocateMemory(dev, &ai, nullptr, mem);
    if (r != VK_SUCCESS) {
        vkDestroyBuffer(dev, *buf, nullptr);
        return r;
    }
    vkBindBufferMemory(dev, *buf, *mem, 0);
    if (mapped) {
        vkMapMemory(dev, *mem, 0, size, 0, mapped);
    }
    return VK_SUCCESS;
}

static VkResult create_image_view(VkDevice dev, VkImage img, VkFormat fmt, VkImageView* view) {
    VkImageViewCreateInfo ivci{};
    ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image = img;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = fmt;
    ivci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    return vkCreateImageView(dev, &ivci, nullptr, view);
}

// ---------------------------------------------------------------------------
// Resource objects owned by the device
// ---------------------------------------------------------------------------
struct D3DVertexBuffer : IDirect3DVertexBuffer9 {
    uintptr_t ref = 1;
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    void* mapped = nullptr;
    VkDeviceSize size = 0;
    DWORD fvf = 0;
    D3DPOOL pool = D3DPOOL_DEFAULT;
    DWORD usage = 0;

    static HRESULT WINAPI QueryInterface(IDirect3DVertexBuffer9*, REFIID, LPVOID*) { return E_NOINTERFACE; }
    static ULONG WINAPI AddRef(IDirect3DVertexBuffer9* iface) {
        auto self = static_cast<D3DVertexBuffer*>(iface);
        return (ULONG)(++self->ref);
    }
    static ULONG WINAPI Release(IDirect3DVertexBuffer9* iface) {
        auto self = static_cast<D3DVertexBuffer*>(iface);
        auto v = (ULONG)(--self->ref);
        if (v == 0) delete self;
        return v;
    }
    static HRESULT WINAPI Lock(IDirect3DVertexBuffer9* iface, UINT offset, UINT size, LPVOID* ptr, DWORD flags) {
        auto self = static_cast<D3DVertexBuffer*>(iface);
        (void)flags;
        if (self->mapped) {
            *ptr = static_cast<uint8_t*>(self->mapped) + offset;
            return D3D_OK;
        }
        return D3DERR_INVALIDCALL;
    }
    static HRESULT WINAPI Unlock(IDirect3DVertexBuffer9* iface) {
        auto self = static_cast<D3DVertexBuffer*>(iface);
        (void)self;
        return D3D_OK;
    }
    static HRESULT WINAPI GetDesc(IDirect3DVertexBuffer9* iface, D3DVERTEXBUFFER_DESC* desc) {
        auto self = static_cast<D3DVertexBuffer*>(iface);
        desc->Pool = self->pool;
        desc->Size = (UINT)self->size;
        desc->Usage = self->usage;
        desc->FVF = self->fvf;
        return D3D_OK;
    }
    static HRESULT WINAPI PreLoad(IDirect3DVertexBuffer9*) { return D3D_OK; }
    static HRESULT WINAPI SetPriority(IDirect3DVertexBuffer9*, DWORD) { return D3D_OK; }
    static HRESULT WINAPI GetPriority(IDirect3DVertexBuffer9*, DWORD*) { return D3D_OK; }
    static HRESULT WINAPI SetProperties(IDirect3DVertexBuffer9*, DWORD) { return D3D_OK; }
    static HRESULT WINAPI GetProperties(IDirect3DVertexBuffer9*, DWORD*) { return D3D_OK; }

    static const IDirect3DVertexBuffer9Vtbl vtbl;
};
const IDirect3DVertexBuffer9Vtbl D3DVertexBuffer::vtbl = {
    D3DVertexBuffer::QueryInterface, D3DVertexBuffer::AddRef, D3DVertexBuffer::Release,
    D3DVertexBuffer::Lock, D3DVertexBuffer::Unlock, D3DVertexBuffer::GetDesc,
    D3DVertexBuffer::PreLoad, D3DVertexBuffer::SetPriority, D3DVertexBuffer::GetPriority,
    D3DVertexBuffer::SetProperties, D3DVertexBuffer::GetProperties,
};

struct D3DIndexBuffer : IDirect3DIndexBuffer9 {
    uintptr_t ref = 1;
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    void* mapped = nullptr;
    VkDeviceSize size = 0;
    D3DFORMAT fmt = D3DFMT_UNKNOWN;
    D3DPOOL pool = D3DPOOL_DEFAULT;
    DWORD usage = 0;

    static HRESULT WINAPI QueryInterface(IDirect3DIndexBuffer9*, REFIID, LPVOID*) { return E_NOINTERFACE; }
    static ULONG WINAPI AddRef(IDirect3DIndexBuffer9* iface) {
        auto self = static_cast<D3DIndexBuffer*>(iface);
        return (ULONG)(++self->ref);
    }
    static ULONG WINAPI Release(IDirect3DIndexBuffer9* iface) {
        auto self = static_cast<D3DIndexBuffer*>(iface);
        auto v = (ULONG)(--self->ref);
        if (v == 0) delete self;
        return v;
    }
    static HRESULT WINAPI Lock(IDirect3DIndexBuffer9* iface, UINT offset, UINT size, LPVOID* ptr, DWORD flags) {
        auto self = static_cast<D3DIndexBuffer*>(iface);
        (void)flags;
        if (self->mapped) {
            *ptr = static_cast<uint8_t*>(self->mapped) + offset;
            return D3D_OK;
        }
        return D3DERR_INVALIDCALL;
    }
    static HRESULT WINAPI Unlock(IDirect3DIndexBuffer9* iface) {
        auto self = static_cast<D3DIndexBuffer*>(iface);
        (void)self;
        return D3D_OK;
    }
    static HRESULT WINAPI GetDesc(IDirect3DIndexBuffer9* iface, D3DINDEXBUFFER_DESC* desc) {
        auto self = static_cast<D3DIndexBuffer*>(iface);
        desc->Pool = self->pool;
        desc->Size = (UINT)self->size;
        desc->Usage = self->usage;
        desc->Format = self->fmt;
        return D3D_OK;
    }
    static HRESULT WINAPI PreLoad(IDirect3DIndexBuffer9*) { return D3D_OK; }

    static const IDirect3DIndexBuffer9Vtbl vtbl;
};
const IDirect3DIndexBuffer9Vtbl D3DIndexBuffer::vtbl = {
    D3DIndexBuffer::QueryInterface, D3DIndexBuffer::AddRef, D3DIndexBuffer::Release,
    D3DIndexBuffer::Lock, D3DIndexBuffer::Unlock, D3DIndexBuffer::GetDesc,
    D3DIndexBuffer::PreLoad,
};

struct D3DTexture : IDirect3DTexture9 {
    uintptr_t ref = 1;
    VkImage img = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkFormat fmt = VK_FORMAT_R8G8B8A8_UNORM;
    VkExtent2D extent{};
    UINT width = 0, height = 0;
    // LockRect support
    void* lock_mapped = nullptr;
    UINT lock_level = 0;
    RECT lock_rect{};
    DWORD lock_flags = 0;
    bool locked = false;
    std::vector<uint8_t> lock_data; // staging copy of pixel data
    UINT lock_pitch = 0;

    static HRESULT WINAPI QueryInterface(IDirect3DTexture9*, REFIID, LPVOID*) { return E_NOINTERFACE; }
    static ULONG WINAPI AddRef(IDirect3DTexture9* iface) {
        auto self = static_cast<D3DTexture*>(iface);
        return (ULONG)(++self->ref);
    }
    static ULONG WINAPI Release(IDirect3DTexture9* iface) {
        auto self = static_cast<D3DTexture*>(iface);
        auto v = (ULONG)(--self->ref);
        if (v == 0) delete self;
        return v;
    }
    static HRESULT WINAPI GetDevice(IDirect3DTexture9*, IDirect3DDevice9**) { return D3D_OK; }
    static HRESULT WINAPI SetPrivateData(IDirect3DTexture9*, REFGUID, const void*, DWORD, DWORD) { return D3D_OK; }
    static HRESULT WINAPI GetPrivateData(IDirect3DTexture9*, REFGUID, void*, DWORD*) { return D3D_OK; }
    static HRESULT WINAPI FreePrivateData(IDirect3DTexture9*, REFGUID) { return D3D_OK; }
    static HRESULT WINAPI SetPriority(IDirect3DTexture9*, DWORD) { return D3D_OK; }
    static HRESULT WINAPI GetPriority(IDirect3DTexture9*, DWORD*) { return D3D_OK; }
    static HRESULT WINAPI PreLoad(IDirect3DTexture9*) { return D3D_OK; }
    static HRESULT WINAPI GetType(IDirect3DTexture9*, D3DRESOURCETYPE* t) { if (t) *t = D3DRTYPE_TEXTURE; return D3D_OK; }
    static DWORD WINAPI SetLOD(IDirect3DTexture9*, DWORD) { return 0; }
    static DWORD WINAPI GetLOD(IDirect3DTexture9*) { return 0; }
    static DWORD WINAPI GetLevelCount(IDirect3DTexture9*) { return 1; }
    static HRESULT WINAPI SetAutoGenFilterType(IDirect3DTexture9*, D3DTEXTUREFILTERTYPE) { return D3D_OK; }
    static HRESULT WINAPI GetAutoGenFilterType(IDirect3DTexture9*, D3DTEXTUREFILTERTYPE*) { return D3D_OK; }
    static HRESULT WINAPI GenerateMipSubLevels(IDirect3DTexture9*) { return D3D_OK; }
    static HRESULT WINAPI LockRect(IDirect3DTexture9* iface, UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags) {
        auto self = static_cast<D3DTexture*>(iface);
        if (self->locked) return D3DERR_INVALIDCALL;
        self->locked = true;
        self->lock_level = Level;
        if (pRect) self->lock_rect = *pRect;
        self->lock_flags = Flags;

        // Allocate staging memory for pixel data
        UINT w = self->width;
        UINT h = self->height;
        if (pRect) {
            w = pRect->right - pRect->left;
            h = pRect->bottom - pRect->top;
        }
        // Assume 32-bit RGBA
        self->lock_pitch = w * 4;
        self->lock_data.resize(self->lock_pitch * h);
        pLockedRect->pBits = self->lock_data.data();
        pLockedRect->Pitch = self->lock_pitch;
        return D3D_OK;
    }
    static HRESULT WINAPI UnlockRect(IDirect3DTexture9* iface, UINT Level) {
        auto self = static_cast<D3DTexture*>(iface);
        if (!self->locked) return D3DERR_INVALIDCALL;
        self->locked = false;

        // Upload pixel data to Vulkan texture via staging buffer
        if (self->lock_data.empty()) return D3D_OK;

        VkDevice dev = g_vk.dev;
        VkDeviceSize uploadSize = self->lock_data.size();

        // Create staging buffer
        VkBuffer stageBuf = VK_NULL_HANDLE;
        VkDeviceMemory stageMem = VK_NULL_HANDLE;
        create_buffer(dev, g_vk.phy, uploadSize,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &stageBuf, &stageMem);

        // Copy data to staging buffer
        void* data;
        vkMapMemory(dev, stageMem, 0, uploadSize, 0, &data);
        memcpy(data, self->lock_data.data(), uploadSize);
        vkUnmapMemory(dev, stageMem);

        // One-time command buffer for upload
        VkCommandBuffer uploadCmd;
        VkCommandBufferAllocateInfo cbAi{};
        cbAi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cbAi.commandPool = g_vk.pool;
        cbAi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbAi.commandBufferCount = 1;
        vkAllocateCommandBuffers(dev, &cbAi, &uploadCmd);

        VkCommandBufferBeginInfo cbi{};
        cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(uploadCmd, &cbi);

        // Transition image to transfer destination
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = self->img;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        // Copy buffer to image
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = self->lock_pitch / 4; // in texels
        region.bufferImageHeight = 0;
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { self->width, self->height, 1 };
        vkCmdCopyBufferToImage(uploadCmd, stageBuf, self->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Transition back to shader read
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(uploadCmd);

        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &uploadCmd;
        vkQueueSubmit(g_vk.queue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(g_vk.queue);

        vkFreeCommandBuffers(dev, g_vk.pool, 1, &uploadCmd);
        vkDestroyBuffer(dev, stageBuf, nullptr);
        vkFreeMemory(dev, stageMem, nullptr);

        self->lock_data.clear();
        return D3D_OK;
    }
    static HRESULT WINAPI AddDirtyRect(IDirect3DTexture9*, const RECT*) { return D3D_OK; }

    static const IDirect3DTexture9Vtbl vtbl;
};
const IDirect3DTexture9Vtbl D3DTexture::vtbl = {
    D3DTexture::QueryInterface, D3DTexture::AddRef, D3DTexture::Release,
    D3DTexture::GetDevice, D3DTexture::SetPrivateData, D3DTexture::GetPrivateData,
    D3DTexture::FreePrivateData, D3DTexture::SetPriority, D3DTexture::GetPriority,
    D3DTexture::PreLoad, D3DTexture::GetType, D3DTexture::SetLOD, D3DTexture::GetLOD,
    D3DTexture::GetLevelCount, D3DTexture::SetAutoGenFilterType,
    D3DTexture::GetAutoGenFilterType, D3DTexture::GenerateMipSubLevels,
    D3DTexture::LockRect, D3DTexture::UnlockRect, D3DTexture::AddDirtyRect,
};

struct D3DVertexDeclaration : IDirect3DVertexDeclaration9 {
    uintptr_t ref = 1;
    DWORD fvf = 0;
    std::vector<D3DVERTEXELEMENT9> elements;
    UINT stride = 0;

    static HRESULT WINAPI QueryInterface(IDirect3DVertexDeclaration9*, REFIID, LPVOID*) { return E_NOINTERFACE; }
    static ULONG WINAPI AddRef(IDirect3DVertexDeclaration9* iface) {
        auto self = static_cast<D3DVertexDeclaration*>(iface);
        return (ULONG)(++self->ref);
    }
    static ULONG WINAPI Release(IDirect3DVertexDeclaration9* iface) {
        auto self = static_cast<D3DVertexDeclaration*>(iface);
        auto v = (ULONG)(--self->ref);
        if (v == 0) delete self;
        return v;
    }
    static HRESULT WINAPI GetDevice(IDirect3DVertexDeclaration9*, IDirect3DDevice9*) { return D3D_OK; }
    static HRESULT WINAPI GetDeclaration(IDirect3DVertexDeclaration9*, D3DVERTEXELEMENT9*, UINT*) { return D3D_OK; }

    static const IDirect3DVertexDeclaration9Vtbl vtbl;
};
const IDirect3DVertexDeclaration9Vtbl D3DVertexDeclaration::vtbl = {
    D3DVertexDeclaration::QueryInterface, D3DVertexDeclaration::AddRef, D3DVertexDeclaration::Release,
    D3DVertexDeclaration::GetDevice, D3DVertexDeclaration::GetDeclaration,
};

// COM helpers
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

    // Stream state
    IDirect3DVertexBuffer9* streams[16] = {};
    UINT stream_strides[16] = {};
    UINT stream_offsets[16] = {};
    IDirect3DIndexBuffer9* index_buffer = nullptr;
    IDirect3DVertexDeclaration9* vertex_decl = nullptr;
    DWORD fvf = 0;

    // Render state
    BOOL blend_enable = FALSE;
    DWORD src_blend = D3DBLEND_ONE;
    DWORD dst_blend = D3DBLEND_ZERO;
    BOOL depth_enable = FALSE;
    DWORD cull_mode = D3DCULL_CCW;
    DWORD fill_mode = D3DFILL_SOLID;
    DWORD zfunc = 0; // D3DCMP_LESSEQUAL by default

    // Tracked resources for cleanup
    std::vector<D3DVertexBuffer*> vertex_buffers;
    std::vector<D3DIndexBuffer*> index_buffers;
    std::vector<D3DTexture*> textures;
    std::vector<D3DVertexDeclaration*> declarations;

    // Texture stages (up to 8)
    IDirect3DTexture9* texture_stages[8] = {};

    // Shader constants
    FLOAT vs_constants[256][4] = {};
    FLOAT ps_constants[256][4] = {};

    // Transform matrices (world/view/proj)
    D3DMATRIX transforms[256] = {};

    D3D9Device(HWND h, D3DPRESENT_PARAMETERS* p) : hwnd(h) {
        if (p) memcpy(&pp, p, sizeof(pp));
    }

    ~D3D9Device() { shutdown(); }

    bool init() {
        // ---- Vulkan instance ----
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

        // ---- physical device ----
        uint32_t phyCount = 0;
        vkEnumeratePhysicalDevices(g_vk.inst, &phyCount, nullptr);
        if (phyCount == 0) { LOG_ERROR("D3D9: no Vulkan physical devices"); return false; }
        std::vector<VkPhysicalDevice> phys(phyCount);
        vkEnumeratePhysicalDevices(g_vk.inst, &phyCount, phys.data());
        g_vk.phy = phys[0];

        // ---- queue family ----
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

        // ---- logical device ----
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

        // ---- surface ----
        if (SDL_Vulkan_CreateSurface((SDL_Window*)hwnd, g_vk.inst, &g_vk.surf) != SDL_TRUE) {
            LOG_ERROR("D3D9: SDL_Vulkan_CreateSurface failed"); return false;
        }

        // ---- swapchain ----
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

        // ---- command pool / buffer ----
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

        // ---- sync ----
        VkSemaphoreCreateInfo semCi{};
        semCi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vkCreateSemaphore(g_vk.dev, &semCi, nullptr, &g_vk.image_acquired);
        vkCreateSemaphore(g_vk.dev, &semCi, nullptr, &g_vk.render_done);

        VkFenceCreateInfo fenceCi{};
        fenceCi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(g_vk.dev, &fenceCi, nullptr, &g_vk.fence);

        // ---- render pass ----
        VkAttachmentDescription att{};
        att.format = g_vk.fmt;
        att.samples = VK_SAMPLE_COUNT_1_BIT;
        att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;

        VkRenderPassCreateInfo rpCi{};
        rpCi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpCi.attachmentCount = 1;
        rpCi.pAttachments = &att;
        rpCi.subpassCount = 1;
        rpCi.pSubpasses = &subpass;
        vkCreateRenderPass(g_vk.dev, &rpCi, nullptr, &g_vk.render_pass);

        // ---- descriptor set layout (UBO + sampler) ----
        VkDescriptorSetLayoutBinding bindings[2] = {};
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo dslCi{};
        dslCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslCi.bindingCount = 2;
        dslCi.pBindings = bindings;
        vkCreateDescriptorSetLayout(g_vk.dev, &dslCi, nullptr, &g_vk.descriptor_set_layout);

        // ---- pipeline layout ----
        VkPipelineLayoutCreateInfo plCi{};
        plCi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plCi.setLayoutCount = 1;
        plCi.pSetLayouts = &g_vk.descriptor_set_layout;
        vkCreatePipelineLayout(g_vk.dev, &plCi, nullptr, &g_vk.pipeline_layout);

        // ---- descriptor pool ----
        VkDescriptorPoolSize poolSizes[2] = {};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 4;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 16;

        VkDescriptorPoolCreateInfo dpCi{};
        dpCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpCi.maxSets = 4;
        dpCi.poolSizeCount = 2;
        dpCi.pPoolSizes = poolSizes;
        vkCreateDescriptorPool(g_vk.dev, &dpCi, nullptr, &g_vk.descriptor_pool);

        // ---- uniform buffer ----
        VkResult r = create_buffer(g_vk.dev, g_vk.phy, 256,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   &g_vk.uniform_buffer, &g_vk.uniform_memory, &g_vk.uniform_mapped);
        if (r != VK_SUCCESS) {
            LOG_ERROR("D3D9: uniform buffer creation failed");
            return false;
        }

        // ---- allocate descriptor sets for each swapchain image ----
        uint32_t ds_imgCount = (uint32_t)g_vk.images.size();
        g_vk.descriptor_sets.resize(ds_imgCount);
        std::vector<VkDescriptorSetLayout> layouts(ds_imgCount, g_vk.descriptor_set_layout);
        VkDescriptorSetAllocateInfo dsaCi{};
        dsaCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsaCi.descriptorPool = g_vk.descriptor_pool;
        dsaCi.descriptorSetCount = ds_imgCount;
        dsaCi.pSetLayouts = layouts.data();
        vkAllocateDescriptorSets(g_vk.dev, &dsaCi, g_vk.descriptor_sets.data());

        // ---- update each descriptor set with uniform buffer ----
        for (uint32_t i = 0; i < ds_imgCount; ++i) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = g_vk.uniform_buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = 256;

            VkWriteDescriptorSet writes[1] = {};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = g_vk.descriptor_sets[i];
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].pBufferInfo = &bufferInfo;
            vkUpdateDescriptorSets(g_vk.dev, 1, writes, 0, nullptr);
        }

        // ---- graphics pipeline ----
        create_pipeline();

        g_vk_ready = true;
        g_has_shaders = true;
        LOG_INFO("D3D9: device initialized, swapchain %ux%u", g_vk.extent.width, g_vk.extent.height);
        return true;
    }

    void create_pipeline() {
        if (!g_has_shaders) return;

        // Shader modules
        VkShaderModuleCreateInfo smci{};
        smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        VkShaderModule vert_mod, frag_mod;
        smci.codeSize = g_vert_spv_size;
        smci.pCode = g_vert_spv;
        vkCreateShaderModule(g_vk.dev, &smci, nullptr, &vert_mod);
        smci.codeSize = g_frag_spv_size;
        smci.pCode = g_frag_spv;
        vkCreateShaderModule(g_vk.dev, &smci, nullptr, &frag_mod);

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert_mod;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag_mod;
        stages[1].pName = "main";

        // Vertex input: pos(float2) + color(DWORD as uint) + texcoord(float2)
        // stride = 8 + 4 + 8 = 20, align to 24 for simplicity
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = 24;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // Attribute 0: position (float2)
        // Attribute 1: color (unpacked from uint)
        // Attribute 2: texcoord (float2)
        VkVertexInputAttributeDescription attr[3] = {};
        attr[0].binding = 0;
        attr[0].location = 0;
        attr[0].format = VK_FORMAT_R32G32_SFLOAT;
        attr[0].offset = 0;
        attr[1].binding = 0;
        attr[1].location = 1;
        attr[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attr[1].offset = 8;
        attr[2].binding = 0;
        attr[2].location = 2;
        attr[2].format = VK_FORMAT_R32G32_SFLOAT;
        attr[2].offset = 16;

        VkPipelineVertexInputStateCreateInfo viCi{};
        viCi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        viCi.vertexBindingDescriptionCount = 1;
        viCi.pVertexBindingDescriptions = &binding;
        viCi.vertexAttributeDescriptionCount = 3;
        viCi.pVertexAttributeDescriptions = attr;

        VkPipelineInputAssemblyStateCreateInfo iaCi{};
        iaCi.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        iaCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport viewport{};
        viewport.width = (float)g_vk.extent.width;
        viewport.height = (float)g_vk.extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent = g_vk.extent;

        VkPipelineViewportStateCreateInfo vpCi{};
        vpCi.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vpCi.viewportCount = 1;
        vpCi.pViewports = &viewport;
        vpCi.scissorCount = 1;
        vpCi.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rsCi{};
        rsCi.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rsCi.polygonMode = VK_POLYGON_MODE_FILL;
        rsCi.cullMode = VK_CULL_MODE_BACK_BIT;
        rsCi.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rsCi.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo msCi{};
        msCi.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        msCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState cbAtt{};
        cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        cbAtt.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo cbCi{};
        cbCi.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cbCi.attachmentCount = 1;
        cbCi.pAttachments = &cbAtt;

        VkPipelineDepthStencilStateCreateInfo dsCi{};
        dsCi.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        dsCi.depthTestEnable = VK_FALSE;
        dsCi.depthWriteEnable = VK_FALSE;

        VkGraphicsPipelineCreateInfo gpCi{};
        gpCi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpCi.stageCount = 2;
        gpCi.pStages = stages;
        gpCi.pVertexInputState = &viCi;
        gpCi.pInputAssemblyState = &iaCi;
        gpCi.pViewportState = &vpCi;
        gpCi.pRasterizationState = &rsCi;
        gpCi.pMultisampleState = &msCi;
        gpCi.pColorBlendState = &cbCi;
        gpCi.pDepthStencilState = &dsCi;
        gpCi.layout = g_vk.pipeline_layout;
        gpCi.renderPass = g_vk.render_pass;
        gpCi.subpass = 0;

        vkCreateGraphicsPipelines(g_vk.dev, VK_NULL_HANDLE, 1, &gpCi, nullptr, &g_vk.pipeline);

        vkDestroyShaderModule(g_vk.dev, frag_mod, nullptr);
        vkDestroyShaderModule(g_vk.dev, vert_mod, nullptr);

        // Framebuffer for current swapchain image
        VkFramebufferCreateInfo fbCi{};
        fbCi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbCi.renderPass = g_vk.render_pass;
        fbCi.attachmentCount = 1;
        fbCi.pAttachments = &g_vk.views[0];
        fbCi.width = g_vk.extent.width;
        fbCi.height = g_vk.extent.height;
        fbCi.layers = 1;
        vkCreateFramebuffer(g_vk.dev, &fbCi, nullptr, &g_vk.framebuffer);
    }

    void shutdown() {
        if (!g_vk.dev) return;
        vkDeviceWaitIdle(g_vk.dev);
        for (auto vb : vertex_buffers) {
            if (vb->buf) vkDestroyBuffer(g_vk.dev, vb->buf, nullptr);
            if (vb->mem) vkFreeMemory(g_vk.dev, vb->mem, nullptr);
            delete vb;
        }
        for (auto ib : index_buffers) {
            if (ib->buf) vkDestroyBuffer(g_vk.dev, ib->buf, nullptr);
            if (ib->mem) vkFreeMemory(g_vk.dev, ib->mem, nullptr);
            delete ib;
        }
        for (auto tx : textures) {
            if (tx->view) vkDestroyImageView(g_vk.dev, tx->view, nullptr);
            if (tx->sampler) vkDestroySampler(g_vk.dev, tx->sampler, nullptr);
            if (tx->img) vkDestroyImage(g_vk.dev, tx->img, nullptr);
            if (tx->mem) vkFreeMemory(g_vk.dev, tx->mem, nullptr);
            delete tx;
        }
        for (auto vd : declarations) delete vd;
        vertex_buffers.clear();
        index_buffers.clear();
        textures.clear();
        declarations.clear();

        if (g_vk.framebuffer) vkDestroyFramebuffer(g_vk.dev, g_vk.framebuffer, nullptr);
        if (g_vk.pipeline) vkDestroyPipeline(g_vk.dev, g_vk.pipeline, nullptr);
        if (g_vk.pipeline_layout) vkDestroyPipelineLayout(g_vk.dev, g_vk.pipeline_layout, nullptr);
        if (g_vk.descriptor_set_layout) vkDestroyDescriptorSetLayout(g_vk.dev, g_vk.descriptor_set_layout, nullptr);
        if (g_vk.descriptor_pool) vkDestroyDescriptorPool(g_vk.dev, g_vk.descriptor_pool, nullptr);
        if (g_vk.uniform_buffer) vkDestroyBuffer(g_vk.dev, g_vk.uniform_buffer, nullptr);
        if (g_vk.uniform_memory) vkFreeMemory(g_vk.dev, g_vk.uniform_memory, nullptr);
        if (g_vk.render_pass) vkDestroyRenderPass(g_vk.dev, g_vk.render_pass, nullptr);
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
        g_has_shaders = false;
    }

    // --- Resource creation ---
    HRESULT create_vertex_buffer(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool,
                                 IDirect3DVertexBuffer9** pp) {
        auto vb = new D3DVertexBuffer();
        vb->size = length;
        vb->fvf = fvf;
        vb->pool = pool;
        vb->usage = usage;
        VkResult r = create_buffer(g_vk.dev, g_vk.phy, length,
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   &vb->buf, &vb->mem, &vb->mapped);
        if (r != VK_SUCCESS) { delete vb; *pp = nullptr; return D3DERR_OUTOFVIDEOMEMORY; }
        vb->lpVtbl = &D3DVertexBuffer::vtbl;
        vertex_buffers.push_back(vb);
        *pp = static_cast<IDirect3DVertexBuffer9*>(vb);
        return D3D_OK;
    }

    HRESULT create_index_buffer(UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool,
                                IDirect3DIndexBuffer9** pp) {
        auto ib = new D3DIndexBuffer();
        ib->size = length;
        ib->fmt = format;
        ib->pool = pool;
        ib->usage = usage;
        VkBufferUsageFlags usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        VkResult r = create_buffer(g_vk.dev, g_vk.phy, length,
                                   usage_flags,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   &ib->buf, &ib->mem, &ib->mapped);
        if (r != VK_SUCCESS) { delete ib; *pp = nullptr; return D3DERR_OUTOFVIDEOMEMORY; }
        ib->lpVtbl = &D3DIndexBuffer::vtbl;
        index_buffers.push_back(ib);
        *pp = static_cast<IDirect3DIndexBuffer9*>(ib);
        return D3D_OK;
    }

    HRESULT create_texture(UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format,
                           D3DPOOL pool, IDirect3DTexture9** pp) {
        (void)levels; (void)usage; (void)pool;
        auto tx = new D3DTexture();
        tx->width = width;
        tx->height = height;
        VkFormat vkfmt = VK_FORMAT_R8G8B8A8_UNORM;
        tx->fmt = vkfmt;
        tx->extent = { width, height };

        VkImageCreateInfo ici{};
        ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.format = vkfmt;
        ici.extent = { width, height, 1 };
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(g_vk.dev, &ici, nullptr, &tx->img) != VK_SUCCESS) {
            delete tx; *pp = nullptr; return D3DERR_OUTOFVIDEOMEMORY;
        }

        VkMemoryRequirements mr{};
        vkGetImageMemoryRequirements(g_vk.dev, tx->img, &mr);
        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(g_vk.phy, &mp);
        uint32_t mt = 0;
        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
            if ((mr.memoryTypeBits & (1u << i)) &&
                (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                mt = i; break;
            }
        }
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = mr.size;
        ai.memoryTypeIndex = mt;
        vkAllocateMemory(g_vk.dev, &ai, nullptr, &tx->mem);
        vkBindImageMemory(g_vk.dev, tx->img, tx->mem, 0);

        VkImageViewCreateInfo ivci{};
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = tx->img;
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = vkfmt;
        ivci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCreateImageView(g_vk.dev, &ivci, nullptr, &tx->view);

        VkSamplerCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sci.magFilter = VK_FILTER_LINEAR;
        sci.minFilter = VK_FILTER_LINEAR;
        vkCreateSampler(g_vk.dev, &sci, nullptr, &tx->sampler);

        tx->lpVtbl = &D3DTexture::vtbl;
        textures.push_back(tx);
        *pp = static_cast<IDirect3DTexture9*>(tx);
        return D3D_OK;
    }

    HRESULT create_vertex_declaration(const D3DVERTEXELEMENT9* elements, IDirect3DVertexDeclaration9** pp) {
        auto vd = new D3DVertexDeclaration();
        if (elements) {
            for (UINT i = 0; elements[i].Stream != 0xff; ++i) {
                vd->elements.push_back(elements[i]);
            }
        }
        vd->lpVtbl = &D3DVertexDeclaration::vtbl;
        declarations.push_back(vd);
        *pp = static_cast<IDirect3DVertexDeclaration9*>(vd);
        return D3D_OK;
    }

    // --- COM methods ---
    static HRESULT WINAPI QueryInterface(IDirect3DDevice9*, REFIID, LPVOID*) { return E_NOINTERFACE; }
    static ULONG WINAPI AddRef(IDirect3DDevice9* iface) {
        auto self = static_cast<D3D9Device*>(iface);
        return (ULONG)(++self->ref);
    }
    static ULONG WINAPI Release(IDirect3DDevice9* iface) {
        auto self = static_cast<D3D9Device*>(iface);
        auto v = (ULONG)(--self->ref);
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
    static HRESULT WINAPI CreateTexture(IDirect3DDevice9* iface, UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, IDirect3DTexture9** pp, HANDLE*) {
        return static_cast<D3D9Device*>(iface)->create_texture(w, h, levels, usage, fmt, pool, pp);
    }
    static HRESULT WINAPI CreateVolumeTexture(IDirect3DDevice9*, UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture9**, HANDLE*) { return D3D_OK; }
    static HRESULT WINAPI CreateCubeTexture(IDirect3DDevice9*, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture9**, HANDLE*) { return D3D_OK; }
    static HRESULT WINAPI CreateVertexBuffer(IDirect3DDevice9* iface, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool, IDirect3DVertexBuffer9** pp, HANDLE*) {
        return static_cast<D3D9Device*>(iface)->create_vertex_buffer(length, usage, fvf, pool, pp);
    }
    static HRESULT WINAPI CreateIndexBuffer(IDirect3DDevice9* iface, UINT length, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, IDirect3DIndexBuffer9** pp, HANDLE*) {
        return static_cast<D3D9Device*>(iface)->create_index_buffer(length, usage, fmt, pool, pp);
    }
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
        auto self = static_cast<D3D9Device*>(iface);
        self->scene_begun = true;
        vkResetCommandBuffer(g_vk.cmd, 0);
        VkCommandBufferBeginInfo cbi{};
        cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(g_vk.cmd, &cbi);

        // Update uniform buffer: simple orthographic projection
        float w = (float)g_vk.extent.width;
        float h = (float)g_vk.extent.height;
        float mvp[16] = {
            2.0f/w, 0.0f,   0.0f, 0.0f,
            0.0f,  -2.0f/h, 0.0f, 0.0f,
            0.0f,   0.0f,   1.0f, 0.0f,
           -1.0f,   1.0f,   0.0f, 1.0f
        };
        // Apply stored world/view transforms
        float tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        memcpy(g_vk.uniform_mapped, mvp, sizeof(mvp));
        memcpy((uint8_t*)g_vk.uniform_mapped + 64, tint, sizeof(tint));

        // Transition swapchain image to color attachment
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = g_vk.images[0];
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(g_vk.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkRenderPassBeginInfo rpbi{};
        rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass = g_vk.render_pass;
        rpbi.framebuffer = g_vk.framebuffer;
        rpbi.renderArea.extent = g_vk.extent;
        VkClearValue clear{};
        clear.color = { {0.0f, 0.1f, 0.2f, 1.0f} };
        rpbi.clearValueCount = 1;
        rpbi.pClearValues = &clear;
        vkCmdBeginRenderPass(g_vk.cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(g_vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk.pipeline);
        if (!g_vk.descriptor_sets.empty()) {
            vkCmdBindDescriptorSets(g_vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk.pipeline_layout, 0, 1, &g_vk.descriptor_sets[0], 0, nullptr);
        }
        VkViewport vp{ 0, 0, (float)g_vk.extent.width, (float)g_vk.extent.height, 0.0f, 1.0f };
        VkRect2D sc{ {0, 0}, g_vk.extent };
        vkCmdSetViewport(g_vk.cmd, 0, 1, &vp);
        vkCmdSetScissor(g_vk.cmd, 0, 1, &sc);

        self->scene_begun = true;
        return D3D_OK;
    }
    static HRESULT WINAPI EndScene(IDirect3DDevice9* iface) {
        auto self = static_cast<D3D9Device*>(iface);
        self->scene_begun = false;
        vkCmdEndRenderPass(g_vk.cmd);
        // Transition back to present
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = g_vk.images[0];
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(g_vk.cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        vkEndCommandBuffer(g_vk.cmd);
        return D3D_OK;
    }
    static HRESULT WINAPI Clear(IDirect3DDevice9*, DWORD, const D3DRECT*, DWORD, D3DCOLOR, FLOAT, DWORD) { return D3D_OK; }
    static HRESULT WINAPI SetTransform(IDirect3DDevice9* iface, D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) {
        auto self = static_cast<D3D9Device*>(iface);
        if (state >= D3DTS_WORLD0 && state <= D3DTS_WORLD0 + 255) {
            self->transforms[state - D3DTS_WORLD0] = *matrix;
        } else if (state == D3DTS_VIEW) {
            self->transforms[0] = *matrix; // store view in slot 0
        } else if (state == D3DTS_PROJECTION) {
            self->transforms[1] = *matrix; // store projection in slot 1
        }
        return D3D_OK;
    }
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
    static HRESULT WINAPI SetRenderState(IDirect3DDevice9* iface, D3DRENDERSTATETYPE state, DWORD val) {
        auto self = static_cast<D3D9Device*>(iface);
        switch (state) {
        case D3DRS_ALPHABLENDENABLE: self->blend_enable = (BOOL)val; break;
        case D3DRS_SRCBLEND: self->src_blend = val; break;
        case D3DRS_DESTBLEND: self->dst_blend = val; break;
        case D3DRS_ZENABLE: self->depth_enable = (BOOL)val; break;
        case D3DRS_CULLMODE: self->cull_mode = val; break;
        case D3DRS_FILLMODE: self->fill_mode = val; break;
        }
        return D3D_OK;
    }
    static HRESULT WINAPI GetRenderState(IDirect3DDevice9*, D3DRENDERSTATETYPE, DWORD*) { return D3D_OK; }
    static HRESULT WINAPI CreateStateBlock(IDirect3DDevice9*, D3DSTATEBLOCKTYPE, IDirect3DStateBlock9**) { return D3D_OK; }
    static HRESULT WINAPI GetStateBlock(IDirect3DDevice9*, DWORD, IDirect3DStateBlock9**) { return D3D_OK; }
    static HRESULT WINAPI BeginStateBlock(IDirect3DDevice9*) { return D3D_OK; }
    static HRESULT WINAPI EndStateBlock(IDirect3DDevice9*, DWORD*) { return D3D_OK; }
    static HRESULT WINAPI SetClipStatus(IDirect3DDevice9*, const D3DCLIPSTATUS9*) { return D3D_OK; }
    static HRESULT WINAPI GetClipStatus(IDirect3DDevice9*, D3DCLIPSTATUS9*) { return D3D_OK; }
    static HRESULT WINAPI GetTexture(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9**) { return D3D_OK; }
    static HRESULT WINAPI SetTexture(IDirect3DDevice9* iface, DWORD stage, IDirect3DBaseTexture9* tex) {
        auto self = static_cast<D3D9Device*>(iface);
        if (stage < 8) {
            self->texture_stages[stage] = reinterpret_cast<IDirect3DTexture9*>(tex);
        }
        // Update descriptor set binding 1 with the texture
        if (!g_vk.descriptor_sets.empty() && tex) {
            auto d3d_tex = reinterpret_cast<D3DTexture*>(tex);
            VkDescriptorImageInfo imageInfo{};
            imageInfo.sampler = d3d_tex->sampler;
            imageInfo.imageView = d3d_tex->view;
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = g_vk.descriptor_sets[0];
            write.dstBinding = 1;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo = &imageInfo;
            vkUpdateDescriptorSets(g_vk.dev, 1, &write, 0, nullptr);
        }
        return D3D_OK;
    }
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

    static HRESULT WINAPI DrawPrimitive(IDirect3DDevice9* iface, D3DPRIMITIVETYPE type, UINT start, UINT count) {
        auto self = static_cast<D3D9Device*>(iface);
        if (!g_has_shaders) return D3D_OK;
        vkCmdBindPipeline(g_vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk.pipeline);
        if (!g_vk.descriptor_sets.empty()) {
            vkCmdBindDescriptorSets(g_vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk.pipeline_layout, 0, 1, &g_vk.descriptor_sets[0], 0, nullptr);
        }
        // Bind vertex buffer from stream 0
        if (self->streams[0]) {
            auto vb = static_cast<D3DVertexBuffer*>(self->streams[0]);
            VkDeviceSize offset = self->stream_offsets[0];
            vkCmdBindVertexBuffers(g_vk.cmd, 0, 1, &vb->buf, &offset);
        }
        vkCmdDraw(g_vk.cmd, count, 1, start, 0);
        return D3D_OK;
    }

    static HRESULT WINAPI DrawIndexedPrimitive(IDirect3DDevice9* iface, D3DPRIMITIVETYPE type, INT base, UINT min, UINT count, UINT start, UINT pc) {
        auto self = static_cast<D3D9Device*>(iface);
        if (!g_has_shaders) return D3D_OK;
        vkCmdBindPipeline(g_vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk.pipeline);
        if (!g_vk.descriptor_sets.empty()) {
            vkCmdBindDescriptorSets(g_vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk.pipeline_layout, 0, 1, &g_vk.descriptor_sets[0], 0, nullptr);
        }
        if (self->streams[0]) {
            auto vb = static_cast<D3DVertexBuffer*>(self->streams[0]);
            VkDeviceSize offset = self->stream_offsets[0];
            vkCmdBindVertexBuffers(g_vk.cmd, 0, 1, &vb->buf, &offset);
        }
        if (self->index_buffer) {
            auto ib = static_cast<D3DIndexBuffer*>(self->index_buffer);
            VkIndexType it = (ib->fmt == D3DFMT_INDEX16) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
            vkCmdBindIndexBuffer(g_vk.cmd, ib->buf, 0, it);
        }
        vkCmdDrawIndexed(g_vk.cmd, count, 1, start, base, 0);
        return D3D_OK;
    }

    static HRESULT WINAPI DrawPrimitiveUP(IDirect3DDevice9*, D3DPTRIMITIVETYPE, UINT, const VOID*, UINT, const D3DVERTEXELEMENT9*, UINT) { return D3D_OK; }
    static HRESULT WINAPI DrawIndexedPrimitiveUP(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT, const VOID*, UINT, D3DFORMAT, const VOID*, UINT) { return D3D_OK; }
    static HRESULT WINAPI ProcessVertices(IDirect3DDevice9*, UINT, UINT, UINT, IDirect3DVertexBuffer9*, DWORD) { return D3D_OK; }
    static HRESULT WINAPI CreateVertexDeclaration(IDirect3DDevice9* iface, const D3DVERTEXELEMENT9* elements, IDirect3DVertexDeclaration9** pp) {
        return static_cast<D3D9Device*>(iface)->create_vertex_declaration(elements, pp);
    }
    static HRESULT WINAPI SetVertexDeclaration(IDirect3DDevice9* iface, IDirect3DVertexDeclaration9* decl) {
        static_cast<D3D9Device*>(iface)->vertex_decl = decl;
        return D3D_OK;
    }
    static HRESULT WINAPI GetVertexDeclaration(IDirect3DDevice9*, IDirect3DVertexDeclaration9**) { return D3D_OK; }
    static HRESULT WINAPI SetFVF(IDirect3DDevice9* iface, DWORD fvf) {
        static_cast<D3D9Device*>(iface)->fvf = fvf;
        return D3D_OK;
    }
    static HRESULT WINAPI GetFVF(IDirect3DDevice9*, DWORD*) { return D3D_OK; }
    static HRESULT WINAPI CreateVertexShader(IDirect3DDevice9*, const DWORD*, IDirect3DVertexShader9**) { return D3D_OK; }
    static HRESULT WINAPI SetVertexShader(IDirect3DDevice9*, IDirect3DVertexShader9*) { return D3D_OK; }
    static HRESULT WINAPI GetVertexShader(IDirect3DDevice9*, IDirect3DVertexShader9**) { return D3D_OK; }
    static HRESULT WINAPI SetVertexShaderConstantF(IDirect3DDevice9* iface, UINT start, const FLOAT* data, UINT count) {
        auto self = static_cast<D3D9Device*>(iface);
        for (UINT i = 0; i < count && start + i < 256; ++i) {
            memcpy(self->vs_constants[start + i], data + i * 4, sizeof(float) * 4);
        }
        return D3D_OK;
    }
    static HRESULT WINAPI GetVertexShaderConstantF(IDirect3DDevice9*, UINT, FLOAT*, UINT) { return D3D_OK; }
    static HRESULT WINAPI SetVertexShaderConstantI(IDirect3DDevice9*, UINT, const INT*, UINT) { return D3D_OK; }
    static HRESULT WINAPI GetVertexShaderConstantI(IDirect3DDevice9*, UINT, INT*, UINT) { return D3D_OK; }
    static HRESULT WINAPI SetVertexShaderConstantB(IDirect3DDevice9*, UINT, const BOOL*, UINT) { return D3D_OK; }
    static HRESULT WINAPI GetVertexShaderConstantB(IDirect3DDevice9*, UINT, BOOL*, UINT) { return D3D_OK; }
    static HRESULT WINAPI SetStreamSourceFreq(IDirect3DDevice9*, UINT, UINT) { return D3D_OK; }
    static HRESULT WINAPI SetStreamSource(IDirect3DDevice9* iface, UINT stream, IDirect3DVertexBuffer9* vb, UINT offset, UINT stride) {
        auto self = static_cast<D3D9Device*>(iface);
        self->streams[stream] = vb;
        self->stream_offsets[stream] = offset;
        self->stream_strides[stream] = stride;
        return D3D_OK;
    }
    static HRESULT WINAPI GetStreamSource(IDirect3DDevice9*, UINT, IDirect3DVertexBuffer9**, UINT*, UINT*) { return D3D_OK; }
    static HRESULT WINAPI SetIndices(IDirect3DDevice9* iface, IDirect3DIndexBuffer9* ib) {
        static_cast<D3D9Device*>(iface)->index_buffer = ib;
        return D3D_OK;
    }
    static HRESULT WINAPI GetIndices(IDirect3DDevice9*, IDirect3DIndexBuffer9**) { return D3D_OK; }
    static HRESULT WINAPI CreatePixelShader(IDirect3DDevice9*, const DWORD*, IDirect3DPixelShader9**) { return D3D_OK; }
    static HRESULT WINAPI SetPixelShader(IDirect3DDevice9*, IDirect3DPixelShader9*) { return D3D_OK; }
    static HRESULT WINAPI GetPixelShader(IDirect3DDevice9*, IDirect3DPixelShader9**) { return D3D_OK; }
    static HRESULT WINAPI SetPixelShaderConstantF(IDirect3DDevice9* iface, UINT start, const FLOAT* data, UINT count) {
        auto self = static_cast<D3D9Device*>(iface);
        for (UINT i = 0; i < count && start + i < 256; ++i) {
            memcpy(self->ps_constants[start + i], data + i * 4, sizeof(float) * 4);
        }
        return D3D_OK;
    }
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

        vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
        vkResetFences(dev, 1, &fence);

        uint32_t idx = 0;
        VkResult r = vkAcquireNextImageKHR(dev, sc, UINT64_MAX, ia, VK_NULL_HANDLE, &idx);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) return D3D_OK;

        // Simple frame limiter: cap at ~60 FPS to prevent 100% CPU / overheating
        // during extended play sessions
        static uint64_t frame_count = 0;
        static auto last_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - last_time).count();
        if (frame_count % 60 == 0) {
            if (elapsed < 1000000 / 60) { // target 60 FPS
                std::this_thread::sleep_for(std::chrono::microseconds(1000000 / 60 - elapsed));
            }
            last_time = std::chrono::steady_clock::now();
        }
        frame_count++;

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &ia;
        si.pWaitDstStageMask = &waitStage;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &g_vk.cmd;
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

struct D3D9 : IDirect3D9 {
    uintptr_t ref = 1;

    static HRESULT WINAPI QueryInterface(IDirect3D9*, REFIID, LPVOID*) { return E_NOINTERFACE; }
    static ULONG WINAPI AddRef(IDirect3D9* iface) {
        auto self = static_cast<D3D9*>(iface);
        return (ULONG)(++self->ref);
    }
    static ULONG WINAPI Release(IDirect3D9* iface) {
        auto self = static_cast<D3D9*>(iface);
        auto v = (ULONG)(--self->ref);
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