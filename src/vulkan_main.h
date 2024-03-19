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
#include <optional>

#include "defines.h"
#include "tiny/tiny_arena.h"

constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

struct Vertex
{
    glm::vec2 pos;
    glm::vec2 uv;
    glm::vec3 color;
    static VkVertexInputBindingDescription get_binding_description()
    {
        VkVertexInputBindingDescription description = {};
        description.binding = 0; // index in array of possibly multiple bindings. Only one binding here so idx 0
        description.stride = sizeof(Vertex); // bytes between vertices
        description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // move to next entry after each vertex or after each instance
        return description;
    }
    static std::array<VkVertexInputAttributeDescription, 3> get_attribute_descriptions()
    {
        // An attribute description struct describes how to extract a vertex attribute from a chunk of vertex data originating from a binding description
        std::array<VkVertexInputAttributeDescription, 3> descriptions = {};
        descriptions[0].binding = 0;
        descriptions[0].location = 0; // same as in vert shader
        descriptions[0].format = VK_FORMAT_R32G32_SFLOAT; // means vec2
        descriptions[0].offset = offsetof(Vertex, pos);
        descriptions[1].binding = 0;
        descriptions[1].location = 1; // same as vert shader
        descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        descriptions[1].offset = offsetof(Vertex, uv);
        descriptions[2].binding = 0;
        descriptions[2].location = 2; // same as vert shader
        descriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        descriptions[2].offset = offsetof(Vertex, color);
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
    u32 image_count = 0;
};

struct QueueFamilyIndices 
{
    std::optional<u32> graphics_family = {};
    std::optional<u32> present_family = {};
    bool is_complete()
    {
        // check if all values have been filled
        return graphics_family.has_value() && 
            present_family.has_value();
    }
};

struct CloudData
{
    glm::vec4 cameraOffset = glm::vec4(0, 0, 35.0, 0.0);
    //     ( pointMagnitudeScalar, cloudDensityNoiseScalar, cloudDensityNoiseFreq, cloudDensityPointLengthFreq )
    glm::vec4 cloudDensityParams = glm::vec4(0.05, 0.5, 0.5, 0.7);
    glm::vec4 sun_dir_and_time = glm::vec4(1, 5, 1, 0);
};

struct RuntimeData
{
    CloudData cloud = {};
    VkInstance instance = {};
    VkDebugUtilsMessengerEXT debug_messenger = {};
    VkPhysicalDevice physical_device = {};
    VkDevice logical_device = {};
    VkQueue graphics_queue = {};
    VkQueue present_queue = {};
    QueueFamilyIndices indices = {};
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
    VkDescriptorPool imgui_pool = {};
    Arena arena = {};
    Arena swapchain_arena = {};
    u32 current_frame = 0;
    bool framebufferWasResized = false;
};

RuntimeData initVulkan();
void vulkanMainLoop(RuntimeData& runtime);
void vulkanCleanup(RuntimeData& runtime);
