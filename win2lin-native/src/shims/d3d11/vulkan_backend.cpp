// Vulkan backend implementation for the D3D11 shim (Phase 4).

#include "vulkan_backend.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace win2lin::d3d11vk {

namespace {

#define VK_CHECK(expr)                                                         \
    do {                                                                       \
        VkResult _vr = (expr);                                                 \
        if (_vr != VK_SUCCESS) {                                               \
            fprintf(stderr, "[NEXLINK D3D11] VK_CHECK failed: %s -> %d\n",     \
                    #expr, static_cast<int>(_vr));                             \
            return false;                                                      \
        }                                                                      \
    } while (0)

#define VK_CHECK_VOID(expr)                                                    \
    do {                                                                       \
        VkResult _vr = (expr);                                                 \
        if (_vr != VK_SUCCESS) {                                               \
            fprintf(stderr, "[NEXLINK D3D11] VK_CHECK failed: %s -> %d\n",     \
                    #expr, static_cast<int>(_vr));                             \
        }                                                                      \
    } while (0)

} // namespace

VulkanBackend::~VulkanBackend() {
    auto mark = [t0 = std::chrono::steady_clock::now()](const char* what) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t0)
                      .count();
        fprintf(stderr, "[d3d11-dtor-timing] %6lld ms %s\n",
                static_cast<long long>(ms), what);
    };
    mark("dtor-start");
    if (device_) {
        vkDeviceWaitIdle(device_);
        mark("wait-idle");
        destroy_swapchain();
        mark("swapchain-destroyed");
        destroy_render_target();
        if (framebuffer_) vkDestroyFramebuffer(device_, framebuffer_, nullptr);
        if (render_pass_) vkDestroyRenderPass(device_, render_pass_, nullptr);
        if (swapchain_render_pass_)
            vkDestroyRenderPass(device_, swapchain_render_pass_, nullptr);
        if (cmd_pool_) vkDestroyCommandPool(device_, cmd_pool_, nullptr);
        mark("resources-destroyed");
        vkDestroyDevice(device_, nullptr);
        mark("device-destroyed");
    }
    if (swapchain_.surface) vkDestroySurfaceKHR(instance_, swapchain_.surface, nullptr);
    if (headless_surface_) vkDestroySurfaceKHR(instance_, headless_surface_, nullptr);
    if (instance_) vkDestroyInstance(instance_, nullptr);
    mark("instance-destroyed");
}

bool VulkanBackend::init() {
    if (ready_) return true;
    if (!create_instance({})) return false;
    if (!pick_physical_device()) return false;
    if (!create_device({})) return false;

    // One-shot command pool.
    VkCommandPoolCreateInfo pool_ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_ci.queueFamilyIndex = graphics_queue_family_;
    VK_CHECK(vkCreateCommandPool(device_, &pool_ci, nullptr, &cmd_pool_));

    ready_ = true;
    fprintf(stderr, "[NEXLINK D3D11] Vulkan backend initialised (queue family %u)\n",
            graphics_queue_family_);
    return true;
}

bool VulkanBackend::init_with_sdl_window(void* sdl_window, uint32_t width,
                                         uint32_t height) {
    if (ready_) {
        // Already initialised headless; cannot retrofit a surface.
        if (!swapchain_.valid) {
            fprintf(stderr,
                    "[NEXLINK D3D11] backend already init headless; cannot add swapchain\n");
        }
        return swapchain_.valid;
    }
    auto* window = static_cast<SDL_Window*>(sdl_window);
    if (!window) return init(); // fall back to headless

    // Query required instance extensions from SDL.
    unsigned int ext_count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &ext_count, nullptr)) {
        fprintf(stderr, "[NEXLINK D3D11] SDL_Vulkan_GetInstanceExtensions failed: %s\n",
                SDL_GetError());
        return init();
    }
    std::vector<const char*> inst_exts(ext_count);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &ext_count, inst_exts.data())) {
        fprintf(stderr, "[NEXLINK D3D11] SDL_Vulkan_GetInstanceExtensions(2) failed\n");
        return init();
    }

    if (!create_instance(inst_exts)) return false;
    if (!pick_physical_device()) return false;

    // Enable VK_KHR_swapchain for presentation.
    std::vector<const char*> dev_exts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    if (!create_device(dev_exts)) return false;

    // One-shot command pool.
    VkCommandPoolCreateInfo pool_ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_ci.queueFamilyIndex = graphics_queue_family_;
    VK_CHECK(vkCreateCommandPool(device_, &pool_ci, nullptr, &cmd_pool_));

    // Create the surface from the SDL window.
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance_, &surface)) {
        fprintf(stderr, "[NEXLINK D3D11] SDL_Vulkan_CreateSurface failed: %s\n",
                SDL_GetError());
        return false;
    }
    swapchain_.surface = surface;

    // Verify the physical device supports presenting on this surface.
    VkBool32 supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, graphics_queue_family_,
                                         surface, &supported);
    if (!supported) {
        fprintf(stderr, "[NEXLINK D3D11] queue family cannot present on surface\n");
        return false;
    }

    // Determine swapchain extent.
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface, &caps);
    uint32_t sw = width;
    uint32_t sh = height;
    if (caps.currentExtent.width != UINT32_MAX) {
        sw = caps.currentExtent.width;
        sh = caps.currentExtent.height;
    } else {
        if (sw < caps.minImageExtent.width) sw = caps.minImageExtent.width;
        if (sw > caps.maxImageExtent.width) sw = caps.maxImageExtent.width;
        if (sh < caps.minImageExtent.height) sh = caps.minImageExtent.height;
        if (sh > caps.maxImageExtent.height) sh = caps.maxImageExtent.height;
    }

    if (!create_swapchain_objects(sw, sh)) return false;

    ready_ = true;
    fprintf(stderr,
            "[NEXLINK D3D11] Vulkan backend initialised with SDL swapchain %ux%u "
            "(%zu images)\n",
            sw, sh, swapchain_.images.size());
    return true;
}

bool VulkanBackend::create_instance(const std::vector<const char*>& extra_inst_exts) {
    uint32_t ext_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> exts(ext_count);
    if (ext_count) vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, exts.data());

    std::vector<const char*> inst_exts = extra_inst_exts;

    // Always try to enable headless surface as a fallback if present.
    bool has_headless = false;
    for (const auto& e : exts) {
        if (std::strcmp(e.extensionName, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME) == 0) {
            has_headless = true;
            break;
        }
    }
    // Only add headless if not already coming from SDL (which provides
    // VK_KHR_surface + platform surface).
    bool has_khr_surface = false;
    for (const char* e : inst_exts) {
        if (std::strcmp(e, VK_KHR_SURFACE_EXTENSION_NAME) == 0) has_khr_surface = true;
    }
    if (has_headless && !has_khr_surface)
        inst_exts.push_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "NEXLINK";
    app.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app.pEngineName = "NEXLINK";
    app.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = static_cast<uint32_t>(inst_exts.size());
    ici.ppEnabledExtensionNames = inst_exts.data();

    VK_CHECK(vkCreateInstance(&ici, nullptr, &instance_));
    return true;
}

bool VulkanBackend::pick_physical_device() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
        fprintf(stderr, "[NEXLINK D3D11] no Vulkan physical devices found\n");
        return false;
    }
    std::vector<VkPhysicalDevice> devs(count);
    vkEnumeratePhysicalDevices(instance_, &count, devs.data());

    VkPhysicalDevice best = VK_NULL_HANDLE;
    int best_score = -1;
    for (auto d : devs) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(d, &props);
        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score = 3;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score = 2;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) score = 1;
        else score = 0;
        if (score > best_score) {
            best_score = score;
            best = d;
        }
    }
    if (best == VK_NULL_HANDLE) {
        fprintf(stderr, "[NEXLINK D3D11] no suitable physical device\n");
        return false;
    }
    physical_device_ = best;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(best, &props);
    fprintf(stderr, "[NEXLINK D3D11] selected GPU: %s (type %d)\n",
            props.deviceName, static_cast<int>(props.deviceType));
    return true;
}

bool VulkanBackend::create_device(const std::vector<const char*>& dev_exts) {
    uint32_t qf_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &qf_count, nullptr);
    std::vector<VkQueueFamilyProperties> qfs(qf_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &qf_count, qfs.data());

    graphics_queue_family_ = UINT32_MAX;
    for (uint32_t i = 0; i < qf_count; ++i) {
        if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_family_ = i;
            break;
        }
    }
    if (graphics_queue_family_ == UINT32_MAX) {
        fprintf(stderr, "[NEXLINK D3D11] no graphics queue family\n");
        return false;
    }

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = graphics_queue_family_;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;

    // Filter device extensions to those actually available.
    uint32_t dev_ext_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &dev_ext_count, nullptr);
    std::vector<VkExtensionProperties> avail(dev_ext_count);
    if (dev_ext_count)
        vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &dev_ext_count,
                                             avail.data());

    std::vector<const char*> enabled;
    for (const char* want : dev_exts) {
        bool found = false;
        for (const auto& a : avail) {
            if (std::strcmp(a.extensionName, want) == 0) {
                found = true;
                break;
            }
        }
        if (found) enabled.push_back(want);
        else fprintf(stderr, "[NEXLINK D3D11] device extension %s not available\n", want);
    }

    // Also enable headless surface extension if present (for headless path).
    for (const auto& a : avail) {
        if (std::strcmp(a.extensionName, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME) == 0) {
            bool already = false;
            for (const char* e : enabled)
                if (std::strcmp(e, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME) == 0) already = true;
            if (!already) enabled.push_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
            break;
        }
    }

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = static_cast<uint32_t>(enabled.size());
    dci.ppEnabledExtensionNames = enabled.data();
    dci.pEnabledFeatures = &features;

    VK_CHECK(vkCreateDevice(physical_device_, &dci, nullptr, &device_));
    vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_props_);    return true;
}

uint32_t VulkanBackend::find_memory_type(uint32_t type_bits,
                                         VkMemoryPropertyFlags props) const {
    for (uint32_t i = 0; i < mem_props_.memoryTypeCount; ++i) {
        if ((type_bits & (1u << i)) &&
            (mem_props_.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return UINT32_MAX;
}

bool VulkanBackend::create_buffer(VkDeviceSize size,
                                  VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags mem_props,
                                  VulkanBuffer* out) {
    if (!ready_ || !out) return false;

    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buf = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(device_, &bci, nullptr, &buf));

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device_, buf, &req);

    uint32_t type_idx = find_memory_type(req.memoryTypeBits, mem_props);
    if (type_idx == UINT32_MAX) {
        type_idx = find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        if (type_idx == UINT32_MAX) {
            fprintf(stderr, "[NEXLINK D3D11] no suitable memory type for buffer (bits=0x%x)\n",
                    req.memoryTypeBits);
            vkDestroyBuffer(device_, buf, nullptr);
            return false;
        }
    }

    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = type_idx;

    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkResult r = vkAllocateMemory(device_, &mai, nullptr, &mem);
    if (r != VK_SUCCESS) {
        fprintf(stderr, "[NEXLINK D3D11] vkAllocateMemory failed: %d\n", static_cast<int>(r));
        vkDestroyBuffer(device_, buf, nullptr);
        return false;
    }

    VK_CHECK(vkBindBufferMemory(device_, buf, mem, 0));

    out->buffer = buf;
    out->memory = mem;
    out->size = size;
    out->mapped = nullptr;
    out->memory_type = type_idx;
    return true;
}

void VulkanBackend::destroy_buffer(VulkanBuffer* buf) {
    if (!buf || !device_) return;
    if (buf->mapped) {
        vkUnmapMemory(device_, buf->memory);
        buf->mapped = nullptr;
    }
    if (buf->buffer) vkDestroyBuffer(device_, buf->buffer, nullptr);
    if (buf->memory) vkFreeMemory(device_, buf->memory, nullptr);
    buf->buffer = VK_NULL_HANDLE;
    buf->memory = VK_NULL_HANDLE;
}

void* VulkanBackend::map_buffer(VulkanBuffer* buf) {
    if (!buf || !buf->memory) return nullptr;
    if (buf->mapped) return buf->mapped;
    void* p = nullptr;
    if (vkMapMemory(device_, buf->memory, 0, buf->size, 0, &p) == VK_SUCCESS) {
        buf->mapped = p;
        return p;
    }
    return nullptr;
}

void VulkanBackend::unmap_buffer(VulkanBuffer* buf) {
    if (!buf || !buf->mapped) return;
    // Flush so the GPU sees the updated data even if memory is non-coherent.
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    range.memory = buf->memory;
    range.offset = 0;
    range.size = VK_WHOLE_SIZE;
    vkFlushMappedMemoryRanges(device_, 1, &range);
    vkUnmapMemory(device_, buf->memory);
    buf->mapped = nullptr;
}

void VulkanBackend::copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    if (!src || !dst || size == 0) return;
    VkCommandBuffer cmd = begin_one_shot();
    if (!cmd) return;
    VkBufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    end_one_shot(cmd); // submits + waits idle + frees (one-shot copy)
}

VkCommandBuffer VulkanBackend::begin_one_shot() {
    if (!ready_) return VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = cmd_pool_;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device_, &ai, &cmd) != VK_SUCCESS) return VK_NULL_HANDLE;

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) return VK_NULL_HANDLE;
    return cmd;
}

void VulkanBackend::end_one_shot(VkCommandBuffer cmd) {
    if (!cmd) return;
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(graphics_queue_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue_);
    vkFreeCommandBuffers(device_, cmd_pool_, 1, &cmd);
}

// ---------------------------------------------------------------------------
// Offscreen render target (headless path)
// ---------------------------------------------------------------------------
bool VulkanBackend::create_render_target(uint32_t width, uint32_t height,
                                         VkFormat format) {
    if (!ready_) return false;
    destroy_render_target();

    VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = format;
    ici.extent = {width, height, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK(vkCreateImage(device_, &ici, nullptr, &rt_.image));

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device_, rt_.image, &req);
    uint32_t type_idx = find_memory_type(
        req.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if (type_idx == UINT32_MAX)
        type_idx = find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (type_idx == UINT32_MAX) {
        fprintf(stderr, "[NEXLINK D3D11] no memory type for render target\n");
        return false;
    }

    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = type_idx;
    VK_CHECK(vkAllocateMemory(device_, &mai, nullptr, &rt_.memory));
    VK_CHECK(vkBindImageMemory(device_, rt_.image, rt_.memory, 0));

    VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.image = rt_.image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = format;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VK_CHECK(vkCreateImageView(device_, &vci, nullptr, &rt_.view));

    rt_.width = width;
    rt_.height = height;
    rt_.format = format;

    if (framebuffer_) {
        vkDestroyFramebuffer(device_, framebuffer_, nullptr);
        framebuffer_ = VK_NULL_HANDLE;
    }

    fprintf(stderr, "[NEXLINK D3D11] Created render target %ux%u (VkImage %p)\n",
            width, height, static_cast<void*>(rt_.image));
    return true;
}

void VulkanBackend::destroy_render_target() {
    if (!device_) return;
    if (rt_.view) vkDestroyImageView(device_, rt_.view, nullptr);
    if (rt_.image) vkDestroyImage(device_, rt_.image, nullptr);
    if (rt_.memory) vkFreeMemory(device_, rt_.memory, nullptr);
    rt_ = {};
}

// ---------------------------------------------------------------------------
// Swapchain (SDL path)
// ---------------------------------------------------------------------------
bool VulkanBackend::create_swapchain_objects(uint32_t width, uint32_t height) {
    if (!device_ || !swapchain_.surface) return false;

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, swapchain_.surface, &caps);

    // Pick a format: prefer B8G8R8A8_UNORM, fall back to first available.
    uint32_t fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, swapchain_.surface, &fmt_count,
                                         nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmt_count);
    if (fmt_count)
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, swapchain_.surface,
                                             &fmt_count, formats.data());
    VkSurfaceFormatKHR chosen{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    if (fmt_count) {
        bool found = false;
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen = f;
                found = true;
                break;
            }
        }
        if (!found) chosen = formats[0];
    }

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = swapchain_.surface;
    sci.minImageCount = image_count;
    sci.imageFormat = chosen.format;
    sci.imageColorSpace = chosen.colorSpace;
    sci.imageExtent = {width, height};
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR; // vsync; universally supported
    sci.clipped = VK_TRUE;
    sci.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(device_, &sci, nullptr, &swapchain_.handle));

    // Retrieve swapchain images + views.
    uint32_t actual_count = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_.handle, &actual_count, nullptr);
    swapchain_.images.resize(actual_count);
    vkGetSwapchainImagesKHR(device_, swapchain_.handle, &actual_count,
                            swapchain_.images.data());

    swapchain_.views.resize(actual_count);
    for (uint32_t i = 0; i < actual_count; ++i) {
        VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vci.image = swapchain_.images[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = chosen.format;
        vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VK_CHECK(vkCreateImageView(device_, &vci, nullptr, &swapchain_.views[i]));
    }

    // Sync primitives.
    VkSemaphoreCreateInfo semi{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK_CHECK(vkCreateSemaphore(device_, &semi, nullptr, &swapchain_.image_available));
    VK_CHECK(vkCreateSemaphore(device_, &semi, nullptr, &swapchain_.render_finished));
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateFence(device_, &fci, nullptr, &swapchain_.in_flight));

    swapchain_.format = chosen.format;
    swapchain_.width = width;
    swapchain_.height = height;
    swapchain_.image_index = 0;

    // Framebuffers depend on swapchain render pass; create after it.
    // Do NOT set swapchain_.valid yet — only after everything succeeds.
    if (!ensure_swapchain_render_pass()) return false;
    swapchain_.framebuffers.resize(actual_count);
    for (uint32_t i = 0; i < actual_count; ++i) {
        VkFramebufferCreateInfo fci2{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fci2.renderPass = swapchain_render_pass_;
        fci2.attachmentCount = 1;
        fci2.pAttachments = &swapchain_.views[i];
        fci2.width = width;
        fci2.height = height;
        fci2.layers = 1;
        VK_CHECK(vkCreateFramebuffer(device_, &fci2, nullptr, &swapchain_.framebuffers[i]));
    }

    // Persistent frame command buffer.
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = cmd_pool_;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(device_, &cai, &frame_cmd_));

    fprintf(stderr, "[NEXLINK D3D11] Created VkSwapchainKHR %p (%ux%u, %u images)\n",
            static_cast<void*>(swapchain_.handle), width, height, actual_count);
    swapchain_.valid = true;
    return true;
}

void VulkanBackend::shutdown_swapchain() {
    if (!device_ || !swapchain_.handle) return;
    vkDeviceWaitIdle(device_);
    // TEMP-DRAIN: bounded delay (replicates fixture Sleep(50) experiment)
    SDL_Delay(50);
    destroy_swapchain();
}

void VulkanBackend::destroy_swapchain() {
    if (!device_) return;
    auto mark = [t0 = std::chrono::steady_clock::now()](const char* what) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t0)
                      .count();
        fprintf(stderr, "[d3d11-dtor-timing] %6lld ms   destroy_swapchain: %s\n",
                static_cast<long long>(ms), what);
    };
    mark("start");
    if (swapchain_.in_flight) vkDestroyFence(device_, swapchain_.in_flight, nullptr);
    if (swapchain_.image_available)
        vkDestroySemaphore(device_, swapchain_.image_available, nullptr);
    if (swapchain_.render_finished)
        vkDestroySemaphore(device_, swapchain_.render_finished, nullptr);
    mark("sync-objects");
    for (auto fb : swapchain_.framebuffers)
        if (fb) vkDestroyFramebuffer(device_, fb, nullptr);
    for (auto v : swapchain_.views)
        if (v) vkDestroyImageView(device_, v, nullptr);
    mark("views-framebuffers");
    if (swapchain_.handle) vkDestroySwapchainKHR(device_, swapchain_.handle, nullptr);
    mark("swapchain-handle");
    swapchain_ = {};
}

bool VulkanBackend::acquire_next_image() {
    if (!swapchain_.valid) return false;
    VkResult r = vkAcquireNextImageKHR(device_, swapchain_.handle, UINT64_MAX,
                                       swapchain_.image_available, VK_NULL_HANDLE,
                                       &swapchain_.image_index);
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "[NEXLINK D3D11] vkAcquireNextImageKHR failed: %d\n",
                static_cast<int>(r));
        return false;
    }
    return true;
}

bool VulkanBackend::submit_frame(VkCommandBuffer cmd) {
    if (!swapchain_.valid || !cmd) return false;
    // Wait for the previous frame using this fence.
    vkWaitForFences(device_, 1, &swapchain_.in_flight, VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &swapchain_.in_flight);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &swapchain_.image_available;
    si.pWaitDstStageMask = &wait_stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &swapchain_.render_finished;
    VK_CHECK(vkQueueSubmit(graphics_queue_, 1, &si, swapchain_.in_flight));
    return true;
}

bool VulkanBackend::present_frame() {
    if (!swapchain_.valid) return false;
    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &swapchain_.render_finished;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain_.handle;
    pi.pImageIndices = &swapchain_.image_index;
    VkResult r = vkQueuePresentKHR(graphics_queue_, &pi);
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "[NEXLINK D3D11] vkQueuePresentKHR failed: %d\n",
                static_cast<int>(r));
        return false;
    }
    return true;
}

VkFramebuffer VulkanBackend::current_framebuffer() {
    if (swapchain_.valid && !swapchain_.framebuffers.empty())
        return swapchain_.framebuffers[swapchain_.image_index];
    return framebuffer_;
}

VkRenderPass VulkanBackend::current_render_pass() {
    return swapchain_.valid ? swapchain_render_pass_ : render_pass_;
}

uint32_t VulkanBackend::current_width() const {
    return swapchain_.valid ? swapchain_.width : rt_.width;
}

uint32_t VulkanBackend::current_height() const {
    return swapchain_.valid ? swapchain_.height : rt_.height;
}

// ---------------------------------------------------------------------------
// Render passes
// ---------------------------------------------------------------------------
bool VulkanBackend::ensure_render_pass() {
    if (render_pass_) return true;
    if (!ready_ || rt_.format == VK_FORMAT_UNDEFINED) return false;

    VkAttachmentDescription color{};
    color.format = rt_.format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;
    VK_CHECK(vkCreateRenderPass(device_, &rpci, nullptr, &render_pass_));
    fprintf(stderr, "[NEXLINK D3D11] Created VkRenderPass %p (offscreen, format=%d)\n",
            static_cast<void*>(render_pass_), static_cast<int>(rt_.format));
    return true;
}

bool VulkanBackend::ensure_swapchain_render_pass() {
    if (swapchain_render_pass_) return true;
    if (!device_ || !swapchain_.handle) return false;

    VkAttachmentDescription color{};
    color.format = swapchain_.format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpci.attachmentCount = 1;
    rpci.pAttachments = &color;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;
    VK_CHECK(vkCreateRenderPass(device_, &rpci, nullptr, &swapchain_render_pass_));
    fprintf(stderr, "[NEXLINK D3D11] Created VkRenderPass %p (swapchain, format=%d)\n",
            static_cast<void*>(swapchain_render_pass_), static_cast<int>(swapchain_.format));
    return true;
}

// ---------------------------------------------------------------------------
// Framebuffer (offscreen)
// ---------------------------------------------------------------------------
bool VulkanBackend::ensure_framebuffer() {
    if (framebuffer_) return true;
    if (!render_pass_ || !rt_.view) return false;

    VkImageView attachments[] = {rt_.view};
    VkFramebufferCreateInfo fci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fci.renderPass = render_pass_;
    fci.attachmentCount = 1;
    fci.pAttachments = attachments;
    fci.width = rt_.width;
    fci.height = rt_.height;
    fci.layers = 1;
    VK_CHECK(vkCreateFramebuffer(device_, &fci, nullptr, &framebuffer_));
    fprintf(stderr, "[NEXLINK D3D11] Created VkFramebuffer %p (%ux%u)\n",
            static_cast<void*>(framebuffer_), rt_.width, rt_.height);
    return true;
}

// ---------------------------------------------------------------------------
// Readback: copy the RT image to a host-visible buffer and write a PPM.
// ---------------------------------------------------------------------------
bool VulkanBackend::readback_and_write_ppm(const std::string& path) {
    if (!ready_ || !rt_.image) return false;

    const uint32_t w = rt_.width;
    const uint32_t h = rt_.height;
    const VkDeviceSize buf_size = static_cast<VkDeviceSize>(w) * h * 4;

    VulkanBuffer staging{};
    if (!create_buffer(buf_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &staging)) {
        fprintf(stderr, "[NEXLINK D3D11] readback: staging buffer alloc failed\n");
        return false;
    }

    VkCommandBuffer cmd = begin_one_shot();
    if (!cmd) {
        destroy_buffer(&staging);
        return false;
    }

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = rt_.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {w, h, 1};
    vkCmdCopyImageToBuffer(cmd, rt_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           staging.buffer, 1, &region);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    end_one_shot(cmd);

    void* pixels = map_buffer(&staging);
    if (!pixels) {
        destroy_buffer(&staging);
        return false;
    }

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        fprintf(stderr, "[NEXLINK D3D11] readback: cannot open %s\n", path.c_str());
        unmap_buffer(&staging);
        destroy_buffer(&staging);
        return false;
    }
    std::fprintf(f, "P6\n%u %u\n255\n", w, h);
    const auto* src = static_cast<const uint8_t*>(pixels);
    for (uint32_t i = 0; i < w * h; ++i) {
        std::fputc(src[i * 4 + 0], f);
        std::fputc(src[i * 4 + 1], f);
        std::fputc(src[i * 4 + 2], f);
    }
    std::fclose(f);
    unmap_buffer(&staging);
    destroy_buffer(&staging);

    fprintf(stderr, "[NEXLINK D3D11] Present: wrote %s (%ux%u)\n", path.c_str(), w, h);
    return true;
}

VulkanBackend& backend() {
    static VulkanBackend b;
    return b;
}

} // namespace win2lin::d3d11vk
