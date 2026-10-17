#pragma once

// Vulkan backend for the D3D11 shim.
//
// Responsibilities:
//   * Own the VkInstance / VkPhysicalDevice / VkDevice / VkQueue.
//   * Provide headless presentation (VK_EXT_headless_surface) so we can
//     exercise the full D3D11 path without a real X11/Wayland window.
//   * Allocate VkBuffer + VkDeviceMemory for D3D11 buffers with correct
//     memory-type selection (host-visible for DYNAMIC / CPU_ACCESS_WRITE).
//   * Manage the "swap chain" as an offscreen VkImage + VkImageView that
//     Present() reads back into a host-visible buffer and writes to disk.
//   * Create VkRenderPass / VkFramebuffer / VkPipeline for the triangle.
//
// All public methods are SysV-ABI (this is Linux-side infrastructure, not
// called from the PE).  The D3D11 shim (ms_abi) calls into this.

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

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

// Offscreen "swap chain" target: a color attachment image + view.
struct VulkanRenderTarget {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
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
    VkCommandPool command_pool() const { return cmd_pool_; }
    VkFormat swapchain_format() const { return rt_.format; }

    // ---- Buffers -----------------------------------------------------------
    bool create_buffer(VkDeviceSize size,
                       VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags mem_props,
                       VulkanBuffer* out);
    void destroy_buffer(VulkanBuffer* buf);
    void* map_buffer(VulkanBuffer* buf);
    void unmap_buffer(VulkanBuffer* buf);

    // ---- Offscreen render target (swap chain substitute) -------------------
    // Creates a VkImage + VkImageView of the requested size / format.
    bool create_render_target(uint32_t width, uint32_t height, VkFormat format);
    VulkanRenderTarget& render_target() { return rt_; }
    const VulkanRenderTarget& render_target() const { return rt_; }

    // ---- Render pass --------------------------------------------------------
    // Single color attachment, loadOp from clear color, storeOp = STORE.
    // Created lazily on first ensure_render_pass() call.
    VkRenderPass render_pass() const { return render_pass_; }
    bool ensure_render_pass();

    // ---- Framebuffer --------------------------------------------------------
    // Bound to the current render target.  Recreated if RT changes size.
    VkFramebuffer framebuffer() const { return framebuffer_; }
    bool ensure_framebuffer();

    // ---- Graphics pipeline ---------------------------------------------------
    // The pipeline is owned by the caller (context) and built from D3D11
    // state.  This helper just exposes the render pass / format it needs.
    VkFormat color_attachment_format() const { return rt_.format; }

    // ---- Readback ------------------------------------------------------------
    // Transition the RT image to TRANSFER_SRC, copy to a host-visible buffer,
    // map it, and write a PPM file.  Returns true on success.
    bool readback_and_write_ppm(const std::string& path);

    // ---- One-shot command helpers --------------------------------------------
    VkCommandBuffer begin_one_shot();
    void end_one_shot(VkCommandBuffer cmd);

    uint32_t find_memory_type(uint32_t type_bits,
                              VkMemoryPropertyFlags props) const;

private:
    bool create_instance();
    bool pick_physical_device();
    bool create_device();
    void destroy_render_target();

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    uint32_t graphics_queue_family_ = 0;
    VkPhysicalDeviceMemoryProperties mem_props_{};
    VkCommandPool cmd_pool_ = VK_NULL_HANDLE;
    bool ready_ = false;

    // Headless surface (optional).
    VkSurfaceKHR headless_surface_ = VK_NULL_HANDLE;

    // Offscreen render target.
    VulkanRenderTarget rt_{};

    // Render pass + framebuffer.
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
};

// Process-wide singleton (thread-safe lazy init).
VulkanBackend& backend();

} // namespace win2lin::d3d11vk
