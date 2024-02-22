#pragma once

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#error Unsupported platform
#endif

#define GLFW_INCLUDE_VULKAN
#include "glfw/glfw3.h"
#include <GLFW/glfw3native.h>

#include "tiny/tiny_arena.h"

template <typename T>
struct BufferView
{
    T* data = nullptr;
    size_t size = 0;
};

struct SwapchainInfo
{
    VkSwapchainKHR swapchain = {};
    BufferView<VkImage> swapchain_images = {};
    VkFormat image_format = {};
    VkExtent2D extent = {};
};

struct RuntimeData
{
    VkInstance instance = {};
    VkDebugUtilsMessengerEXT debug_messenger = {};
    VkPhysicalDevice physical_device = {};
    VkDevice logical_device = {};
    VkQueue graphics_queue = {};
    VkQueue present_queue = {};
    VkSurfaceKHR surface = {};
    SwapchainInfo swapchain_info = {};
    BufferView<VkImageView> swapchain_image_views = {};
    VkRenderPass render_pass = {};
    VkPipelineLayout pipline_layout = {};
    VkPipeline graphics_pipeline = {};
    BufferView<VkFramebuffer> swapchain_framebuffers = {};
    VkCommandPool command_pool = {};
    VkCommandBuffer command_buffer = {};
    VkSemaphore img_available_semaphore = {};
    VkSemaphore render_finished_semaphore = {};
    VkFence inflight_fence = {};
    Arena arena = {};
};

RuntimeData initVulkan();
void vulkanMainLoop(RuntimeData& runtime);
void vulkanCleanup(RuntimeData& runtime);
