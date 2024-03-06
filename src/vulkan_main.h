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

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <array>

#include "defines.h"
#include "tiny/tiny_arena.h"

constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;
    static VkVertexInputBindingDescription get_binding_description()
    {
        VkVertexInputBindingDescription description = {};
        description.binding = 0; // index in array of possibly multiple bindings. Only one binding here so idx 0
        description.stride = sizeof(Vertex); // bytes between vertices
        description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // move to next entry after each vertex or after each instance
        return description;
    }
    static std::array<VkVertexInputAttributeDescription, 2> get_attribute_descriptions()
    {
        // An attribute description struct describes how to extract a vertex attribute from a chunk of vertex data originating from a binding description
        std::array<VkVertexInputAttributeDescription, 2> descriptions = {};
        descriptions[0].binding = 0;
        descriptions[0].location = 0; // same as in vert shader
        descriptions[0].format = VK_FORMAT_R32G32_SFLOAT; // means vec2
        descriptions[0].offset = offsetof(Vertex, pos);
        descriptions[1].binding = 0;
        descriptions[1].location = 1; // same as vert shader
        descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        descriptions[1].offset = offsetof(Vertex, color);
        return descriptions;
    }
};

// non-owning
template <typename T>
struct BufferView
{
    T* data = nullptr;
    size_t size = 0;
    inline u32 get_num_elements() { return size / sizeof(T); }
    static BufferView<T> init_from_arena(Arena* arena, u32 num_elements)
    {
        BufferView<T> ret;
        ret.data = arena_alloc_type(arena, T, num_elements);
        ret.size = num_elements;
        return ret;
    }
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
    VkDescriptorSetLayout descriptor_set_layout = {};
    VkDescriptorPool descriptor_pool = {};
    BufferView<VkDescriptorSet> descriptor_sets = {};
    VkPipelineLayout pipline_layout = {};
    VkPipeline graphics_pipeline = {};
    BufferView<VkFramebuffer> swapchain_framebuffers = {};
    VkCommandPool command_pool = {};
    BufferView<VkCommandBuffer> command_buffers = {};
    BufferView<VkSemaphore> img_available_semaphores = {};
    BufferView<VkSemaphore> render_finished_semaphores = {};
    BufferView<VkFence> inflight_fences = {};
    VkBuffer vertex_buffer = {};
    VkDeviceMemory vertex_buffer_mem = {};
    VkBuffer index_buffer = {};
    VkDeviceMemory index_buffer_mem = {};
    BufferView<VkBuffer> uniform_buffers = {};
    BufferView<VkDeviceMemory> uniform_buffers_mem = {};
    BufferView<void*> uniform_buffers_mapped = {};
    Arena arena = {};
    Arena swapchain_arena = {};
    u32 current_frame = 0;
    bool framebufferWasResized = false;
};

RuntimeData initVulkan();
void vulkanMainLoop(RuntimeData& runtime);
void vulkanCleanup(RuntimeData& runtime);
