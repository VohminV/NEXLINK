// Vulkan backend implementation for the D3D11 shim.

#include "vulkan_backend.h"

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
    if (device_) {
        vkDeviceWaitIdle(device_);
        if (cmd_pool_) vkDestroyCommandPool(device_, cmd_pool_, nullptr);
        vkDestroyDevice(device_, nullptr);
    }
    if (instance_) vkDestroyInstance(instance_, nullptr);
}

bool VulkanBackend::init() {
    if (ready_) return true;
    if (!create_instance()) return false;
    if (!pick_physical_device()) return false;
    if (!create_device()) return false;

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

bool VulkanBackend::create_instance() {
    // Prefer headless surface so we can exercise presentation without WSI.
    uint32_t ext_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> exts(ext_count);
    if (ext_count) vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, exts.data());

    bool has_headless = false;
    for (const auto& e : exts) {
        if (std::strcmp(e.extensionName, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME) == 0) {
            has_headless = true;
            break;
        }
    }

    std::vector<const char*> inst_exts;
    if (has_headless) inst_exts.push_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);

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

    // Prefer discrete GPU, then integrated, then anything.
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

bool VulkanBackend::create_device() {
    // Find a queue family that supports graphics.
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

    // Headless surface extension (if available) so swapchains can be created.
    uint32_t dev_ext_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &dev_ext_count, nullptr);
    std::vector<VkExtensionProperties> dev_exts(dev_ext_count);
    if (dev_ext_count)
        vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &dev_ext_count,
                                             dev_exts.data());

    std::vector<const char*> enabled_exts;
    for (const auto& e : dev_exts) {
        if (std::strcmp(e.extensionName, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME) == 0) {
            enabled_exts.push_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
            break;
        }
    }

    VkPhysicalDeviceFeatures features{};
    // Nothing fancy required for Phase 2.

    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = static_cast<uint32_t>(enabled_exts.size());
    dci.ppEnabledExtensionNames = enabled_exts.data();
    dci.pEnabledFeatures = &features;

    VK_CHECK(vkCreateDevice(physical_device_, &dci, nullptr, &device_));
    vkGetDeviceQueue(device_, graphics_queue_family_, 0, &graphics_queue_);
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_props_);
    return true;
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
        // Retry with weaker requirements (at least HOST_VISIBLE).
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
    vkUnmapMemory(device_, buf->memory);
    buf->mapped = nullptr;
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

VulkanBackend& backend() {
    static VulkanBackend b;
    return b;
}

} // namespace win2lin::d3d11vk
