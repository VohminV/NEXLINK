#pragma once

// Vulkan backend for the D3D11 shim (Phase 4).
//
// Responsibilities:
//   * Own the VkInstance / VkPhysicalDevice / VkDevice / VkQueue.
//   * Optional SDL2-backed presentation: init_with_sdl_window() enables the
//     VK_KHR_surface / VK_KHR_swapchain path via SDL_Vulkan_*, creating a
//     real VkSwapchainKHR with per-image views/framebuffers and semaphores.
//   * Headless fallback: init() keeps the offscreen render-target path so
//     tests can run without a display.
//   * Allocate VkBuffer + VkDeviceMemory for D3D11 buffers.
//   * Create VkRenderPass / VkFramebuffer / VkPipeline for the triangle.
//
// All public methods are SysV-ABI (Linux-side infrastructure, not called
// from the PE).  The D3D11 shim (ms_abi) calls into this.

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

// Offscreen "swap chain" target (headless path).
struct VulkanRenderTarget {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
};

// Real VkSwapchainKHR wrapper (SDL path).
struct VulkanSwapchain {
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> framebuffers;
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore render_finished = VK_NULL_HANDLE;
    VkFence in_flight = VK_NULL_HANDLE;
    uint32_t image_index = 0;
    bool valid = false;
};

class VulkanBackend {
public:
    VulkanBackend() = default;
    ~VulkanBackend();

    VulkanBackend(const VulkanBackend&) = delete;
    VulkanBackend& operator=(const VulkanBackend&) = delete;

    // Headless init (no SDL window). Returns true on success.
    bool init();

    // SDL-backed init: enables VK_KHR_surface / VK_KHR_swapchain, creates a
    // VkSurfaceKHR from the SDL_Window, then a real VkSwapchainKHR.
    // sdl_window is an SDL_Window*.
    bool init_with_sdl_window(void* sdl_window, uint32_t width, uint32_t height);

    bool is_ready() const { return ready_; }
    bool has_swapchain() const { return swapchain_.valid; }

    VkInstance instance() const { return instance_; }
    VkPhysicalDevice physical_device() const { return physical_device_; }
    VkDevice device() const { return device_; }
    VkQueue graphics_queue() const { return graphics_queue_; }
    uint32_t graphics_queue_family() const { return graphics_queue_family_; }
    VkCommandPool command_pool() const { return cmd_pool_; }
    VkFormat swapchain_format() const {
        return swapchain_.valid ? swapchain_.format : rt_.format;
    }

    // ---- Buffers -----------------------------------------------------------
    bool create_buffer(VkDeviceSize size,
                       VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags mem_props,
                       VulkanBuffer* out);
    void destroy_buffer(VulkanBuffer* buf);
    void* map_buffer(VulkanBuffer* buf);
    void unmap_buffer(VulkanBuffer* buf);
    void copy_buffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

    // ---- Offscreen render target (headless swap chain substitute) ----------
    bool create_render_target(uint32_t width, uint32_t height, VkFormat format);
    VulkanRenderTarget& render_target() { return rt_; }
    const VulkanRenderTarget& render_target() const { return rt_; }

    // ---- Swapchain accessors ------------------------------------------------
    VulkanSwapchain& swapchain() { return swapchain_; }
    const VulkanSwapchain& swapchain() const { return swapchain_; }

    // Wait for the GPU, then destroy the swapchain while the presenting
    // window is still alive (called from the DXGI swapchain destructor).
    // Destroying the window first leaves in-flight presents that make both
    // SDL_DestroyWindow() and vkDestroySwapchainKHR() stall for seconds.
    void shutdown_swapchain();

    // Acquire the next swapchain image (waits on in_flight fence).
    bool acquire_next_image();

    // Submit a recorded command buffer for the current image with semaphore
    // sync (image_available -> render_finished, signals in_flight fence).
    bool submit_frame(VkCommandBuffer cmd);

    // Present the current image, waiting on render_finished.
    bool present_frame();

    // Current framebuffer for the acquired image (swapchain) or offscreen RT.
    VkFramebuffer current_framebuffer();
    VkRenderPass current_render_pass();
    uint32_t current_width() const;
    uint32_t current_height() const;

    // ---- Render pass --------------------------------------------------------
    VkRenderPass render_pass() const { return render_pass_; }
    bool ensure_render_pass();
    bool ensure_swapchain_render_pass();

    // ---- Framebuffer (offscreen) -------------------------------------------
    VkFramebuffer framebuffer() const { return framebuffer_; }
    bool ensure_framebuffer();

    VkFormat color_attachment_format() const { return swapchain_format(); }

    // ---- Readback (headless path only) --------------------------------------
    bool readback_and_write_ppm(const std::string& path);

    // ---- One-shot command helpers --------------------------------------------
    VkCommandBuffer begin_one_shot();
    void end_one_shot(VkCommandBuffer cmd);

    uint32_t find_memory_type(uint32_t type_bits,
                              VkMemoryPropertyFlags props) const;

private:
    bool create_instance(const std::vector<const char*>& extra_inst_exts);
    bool pick_physical_device();
    bool create_device(const std::vector<const char*>& dev_exts);
    void destroy_render_target();
    void destroy_swapchain();
    bool create_swapchain_objects(uint32_t width, uint32_t height);

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

    // Offscreen render target (headless path).
    VulkanRenderTarget rt_{};

    // Real swapchain (SDL path).
    VulkanSwapchain swapchain_{};

    // Render passes + framebuffers.
    VkRenderPass render_pass_ = VK_NULL_HANDLE;         // offscreen
    VkRenderPass swapchain_render_pass_ = VK_NULL_HANDLE; // swapchain
    VkFramebuffer framebuffer_ = VK_NULL_HANDLE;

    // Persistent per-frame command buffer for swapchain rendering.
    VkCommandBuffer frame_cmd_ = VK_NULL_HANDLE;
};

// Process-wide singleton (thread-safe lazy init).
VulkanBackend& backend();

} // namespace win2lin::d3d11vk
