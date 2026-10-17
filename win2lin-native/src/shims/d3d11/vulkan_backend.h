#pragma once

// Vulkan backend for the D3D11 shim.
//
// Responsibilities:
//   * Own the VkInstance / VkPhysicalDevice / VkDevice / VkQueue.
//   * Provide headless presentation (VK_EXT_headless_surface) so we can
//     exercise the full D3D11 path without a real X11/Wayland window.
//   * Allocate VkBuffer + VkDeviceMemory for D3D11 buffers with correct
//     memory-type selection (host-visible for DYNAMIC/CPU_ACCESS_WRITE).
//
// All public methods are SysV-ABI (this is Linux-side infrastructure, not
// called from the PE).  The D3D11 shim (ms_abi) calls into this.

#include <cstdint>
#include <mutex>
#include <unordered_map>

#include <vulkan/vulkan.h>

namespace win2lin::d3d11vk {

// Result of mapping a D3D11 buffer to Vulkan.
struct VulkanBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr; // non-null only for HOST_VISIBLE buffers
    uint32_t memory_type = 0;
};

class VulkanBackend {
public:
    VulkanBackend() = default;
    ~VulkanBackend();

    VulkanBackend(const VulkanBackend&) = delete;
    VulkanBackend& operator=(const VulkanBackend&) = delete;

    // Initialise instance + device.  Returns true on success.
    bool init();

    bool is_ready() const { return ready_; }

    VkInstance instance() const { return instance_; }
    VkPhysicalDevice physical_device() const { return physical_device_; }
    VkDevice device() const { return device_; }
    VkQueue graphics_queue() const { return graphics_queue_; }
    uint32_t graphics_queue_family() const { return graphics_queue_family_; }

    // Create a buffer with the given Vulkan usage / memory properties.
    // Returns false on failure (details logged to stderr).
    bool create_buffer(VkDeviceSize size,
                       VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags mem_props,
                       VulkanBuffer* out);

    // Destroy a buffer created by create_buffer().
    void destroy_buffer(VulkanBuffer* buf);

    // Map / unmap a host-visible buffer.
    void* map_buffer(VulkanBuffer* buf);
    void unmap_buffer(VulkanBuffer* buf);

    // Command buffer helpers (one-shot submit).
    VkCommandBuffer begin_one_shot();
    void end_one_shot(VkCommandBuffer cmd);

    // Find a memory type index matching the required properties.
    // Returns UINT32_MAX if none found.
    uint32_t find_memory_type(uint32_t type_bits,
                              VkMemoryPropertyFlags props) const;

private:
    bool create_instance();
    bool pick_physical_device();
    bool create_device();

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    uint32_t graphics_queue_family_ = 0;
    VkPhysicalDeviceMemoryProperties mem_props_{};
    VkCommandPool cmd_pool_ = VK_NULL_HANDLE;
    bool ready_ = false;
};

// Process-wide singleton (thread-safe lazy init).
VulkanBackend& backend();

} // namespace win2lin::d3d11vk
