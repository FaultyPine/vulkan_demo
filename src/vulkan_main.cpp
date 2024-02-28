#include "vulkan_main.h"
#include "defines.h"
#include "tiny/tiny_log.h"
#include "tiny/tiny_mem.h"
#include "tiny/tiny_arena.h"

#include <optional>
#include <vector>
#include <set>
#include <string.h>
#include <fstream>


#define CLAMP(x, min, max) (x < min ? min : (x > max ? max : x))

/* BOOKMARK:
https://vulkan-tutorial.com/en/Drawing_a_triangle/Swap_chain_recreation
and
https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Frames_in_flight
*/ 

#define VK_CHECK(vkResult) \
    TINY_ASSERT(vkResult == VK_SUCCESS);

extern GLFWwindow* glob_glfw_window;

const char* required_validation_layers[] = 
{
    "VK_LAYER_KHRONOS_validation"
};
// these along with glfw required extensions will be ensured/loaded
const char* required_instance_extension_names[] = 
{
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
};
const char* required_device_extension_names[] = 
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
constexpr bool validation_layers_enabled = true;

// reads in a (binary) file into the given arena
u8* read_file_bin(
    Arena* arena,
    const char* filename,
    size_t* filesize_out = nullptr) 
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) 
    {
        LOG_ERROR("failed to open file: %s", filename);
    }
    size_t filesize = (size_t)file.tellg();
    if (filesize_out != nullptr)
    {
        *filesize_out = filesize;
    }
    char* file_contents = (char*)arena_alloc(arena, filesize);
    file.seekg(0);
    file.read(file_contents, filesize);
    file.close();
    return (u8*)file_contents;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) 
{
    const char* messageTypeStr;
    switch (messageType)
    {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
        {
            messageTypeStr = "[SPEC-VIOLATION]";
        } break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
        {
            messageTypeStr = "[PERF]";
        } break;
        default:
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
        {
            messageTypeStr = "[GENERAL]";
        } break;
    }
    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        {
            LOG_TRACE("%s | %s", messageTypeStr, pCallbackData->pMessage);
        } break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        {
            LOG_INFO("%s | %s", messageTypeStr, pCallbackData->pMessage);
        } break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        {
            LOG_WARN("%s | %s", messageTypeStr, pCallbackData->pMessage);
        } break;
        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        {
            LOG_ERROR("%s | %s", messageTypeStr, pCallbackData->pMessage);
        } break;
    }
    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, 
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
    const VkAllocationCallbacks* pAllocator, 
    VkDebugUtilsMessengerEXT* pDebugMessenger) 
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) 
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

void setup_debug_messenger(const VkInstance& instance, VkDebugUtilsMessengerEXT& debug_messenger_out)
{
    if (!validation_layers_enabled) return;
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debug_callback;
    createInfo.pUserData = nullptr;

    VkResult result = CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debug_messenger_out);
    VK_CHECK(result);
}

// returns all the vk extensions this program needs
// including glfw required extensions
const char** get_required_instance_extensions(Arena* arena, u32& num_required_extensions)
{
    u32 glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    TINY_ASSERT(glfw_extensions != nullptr && "Vulkan not available on this machine!");
    u32 num_extra_extensions = validation_layers_enabled ? 1 : 0;
    const char** extension_names = (const char**)arena_alloc(arena, sizeof(char*) * (glfw_extension_count + num_extra_extensions));
    for (u32 i = 0; i < glfw_extension_count; i++)
    {
        extension_names[i] = glfw_extensions[i];
    }
    u32 end_glfw_extensions_idx = glfw_extension_count;
    for (u32 i = 0; i < ARRAY_SIZE(required_instance_extension_names); i++)
    {
        extension_names[end_glfw_extensions_idx + i] = required_instance_extension_names[i];
    }
    num_required_extensions = glfw_extension_count + num_extra_extensions;
    return extension_names;
}

bool checkValidationLayerSupport(Arena* arena)
{
    bool result = true;
    u32 layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    VkLayerProperties* available_layers = (VkLayerProperties*)arena_alloc(arena, sizeof(VkLayerProperties) * layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);
    // do we support all the validation layers we need?
    for (u32 i = 0; i < ARRAY_SIZE(required_validation_layers); i++)
    {
        bool layer_found = false;
        for (u32 k = 0; k < layer_count; k++) 
        {
            if (strcmp(required_validation_layers[i], available_layers[k].layerName) == 0) 
            {
                layer_found = true;
                break;
            }
        }
        if (!layer_found) 
        {
            LOG_WARN("Required validataion layer %s not supported!", required_validation_layers[i]);
            result = false;
        }
    }
    return result;
}


VkInstance createInstance(Arena* arena)
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr; // for extensions
    appInfo.pApplicationName = "VulkanDemo";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    u32 enabledLayerCount = ARRAY_SIZE(required_validation_layers);
    if (validation_layers_enabled)
    {
        if (!checkValidationLayerSupport(arena))
        {
            enabledLayerCount = 0;
            LOG_WARN("Some required validation layers aren't supported!");
        }
    }
    else
    {
        enabledLayerCount = 0;
    }

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    u32 num_required_extensions = 0;
    const char** required_instance_extensions = get_required_instance_extensions(arena, num_required_extensions);
    createInfo.enabledExtensionCount = num_required_extensions;
    createInfo.ppEnabledExtensionNames = required_instance_extensions;
    createInfo.enabledLayerCount = enabledLayerCount;
    createInfo.ppEnabledLayerNames = required_validation_layers;

    VkInstance instance;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    VK_CHECK(result);

    // this calling convention is common in vulkan.
    // we first query the amount of something with nulls passed for the actual data
    // then we allocate that amount, then query the actual info
    ArenaTemp extensions_arena = arena_temp_init(arena);
    u32 supported_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, nullptr);
    VkExtensionProperties* supported_extensions = (VkExtensionProperties*)arena_alloc(&extensions_arena, sizeof(VkExtensionProperties) * supported_extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, supported_extensions);

    TINY_ASSERT(num_required_extensions <= supported_extension_count);
    for (u32 i = 0; i < num_required_extensions; i++)
    {
        bool supports = false;
        for (u32 k = 0; k < supported_extension_count; k++)
        {
            if (strncmp(required_instance_extensions[i], supported_extensions[k].extensionName, 256))
            {
                supports = true;
                break;
            }
        }
        if (!supports)
        {
            LOG_WARN("Required extension %s not supported!", required_instance_extensions[i]);
        }
    }
    arena_temp_end(extensions_arena);
    return instance;
}


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

QueueFamilyIndices find_queue_families(
    Arena* arena, 
    VkPhysicalDevice device,
    VkSurfaceKHR surface)
{
    QueueFamilyIndices indices = {};
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    ArenaTemp arena_temp = arena_temp_init(arena); // only need the family properties temporarily
    VkQueueFamilyProperties* queue_family_properties = (VkQueueFamilyProperties*)arena_alloc(&arena_temp, sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_family_properties);
    for (u32 i = 0; i < queue_family_count; i++)
    {
        VkQueueFamilyProperties& queue_family = queue_family_properties[i];
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphics_family = i;
        }
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        if (present_support)
        {
            indices.present_family = i;
        }
        // FUTURE: might want to check for compute/transfer queues too?

        // once we find all the queues we want, early out
        if (indices.is_complete()) break;
    }
    arena_temp_end(arena_temp);
    return indices;
}

struct SwapchainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
    SwapchainSupportDetails details;
    VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &details.capabilities);
    VK_CHECK(result);
    // formats
    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
    if (format_count != 0)
    {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, details.formats.data());
    }
    // present modes
    u32 present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
    if (present_mode_count != 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, details.present_modes.data());
    }
    return details;
}

// choosing right swapchain settings
VkSurfaceFormatKHR choose_swapchain_surface_format(
    VkSurfaceFormatKHR* available_formats,
    u32 num_available_formats)
{
    for (u32 i = 0; i < num_available_formats; i++)
    {
        const VkSurfaceFormatKHR& available_format = available_formats[i];
        if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return available_format;
        }
    }
    // could "rank" the formats... or just grab one of em
    return available_formats[0];
}

/*
VK_PRESENT_MODE_FIFO_KHR guarenteed to be present, others likely better but may not exist
-----------------------------------------
VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away, which may result in tearing.
VK_PRESENT_MODE_FIFO_KHR: The swap chain is a queue where the display takes an image from the front of the queue when the display is refreshed and the program inserts rendered images at the back of the queue. If the queue is full then the program has to wait. This is most similar to vertical sync as found in modern games. The moment that the display is refreshed is known as "vertical blank".
VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank. Instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives. This may result in visible tearing.
VK_PRESENT_MODE_MAILBOX_KHR: This is another variation of the second mode. Instead of blocking the application when the queue is full, the images that are already queued are simply replaced with the newer ones. This mode can be used to render frames as fast as possible while still avoiding tearing, resulting in fewer latency issues than standard vertical sync. This is commonly known as "triple buffering", although the existence of three buffers alone does not necessarily mean that the framerate is unlocked.
*/
VkPresentModeKHR choose_swap_present_mode(
    VkPresentModeKHR* present_modes, 
    u32 num_present_modes)
{
    for (u32 i = 0; i < num_present_modes; i++)
    {
        VkPresentModeKHR present_mode = present_modes[i];
        if (present_mode = VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return present_mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

// choose resolution of the swapchain images
VkExtent2D choose_swap_extent(
    const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }
    else
    {
        s32 width, height;
        glfwGetFramebufferSize(glob_glfw_window, &width, &height);
        VkExtent2D real_extent = {(u32)width, (u32)height};
        real_extent.width = CLAMP(real_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        real_extent.height = CLAMP(real_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return real_extent;
    }
}

SwapchainInfo create_swapchain(
    Arena* arena,
    VkDevice logical_device,
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface)
{
    SwapchainSupportDetails swapchain_support = query_swapchain_support(physical_device, surface);
    VkSurfaceFormatKHR surface_format = choose_swapchain_surface_format(swapchain_support.formats.data(), swapchain_support.formats.size());
    VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_support.present_modes.data(), swapchain_support.present_modes.size());
    VkExtent2D extent = choose_swap_extent(swapchain_support.capabilities);
    // how many images in the swapchain
    // using min + 1 means we never have to wait on the driver before we can aquire another image to render to
    u32 image_count = swapchain_support.capabilities.minImageCount + 1;
    // maxImageCount = 0 means no maximum. So if there is a maximum and we've exceeded it, clamp
    if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount)
    {
        image_count = swapchain_support.capabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1; // always 1 unless doing stereoscopic 3d app
    // means we'll be rendering directly to this.
    // may change in the future since we might render to an offscreen buffer and then blit to
    // main for stuff like postprocessing. In that case this would be a USAGE_TRANSFER_DST_BIT and we'd do some mem operation to transfer rendered image to this swapchain image
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; 
    // how to handle swapchain images that will be used across multiple queue families
    QueueFamilyIndices indices = find_queue_families(arena, physical_device, surface);
    u32 queue_family_indices[] = {indices.graphics_family.value(), indices.present_family.value()};
    if (indices.graphics_family != indices.present_family)
    {
        // Images can be used across multiple queue families without explicit ownership transfers.
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        // An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family. This option offers the best performance.
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0; // Optional
        create_info.pQueueFamilyIndices = nullptr; // Optional
    }
    // can specify the swapchain to rotate or flip images
    create_info.preTransform = swapchain_support.capabilities.currentTransform;    
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE; // dont care about color of obscured pixels (another window in front of our window)
    create_info.oldSwapchain = VK_NULL_HANDLE; // for now, assume only one swapchain
    VkSwapchainKHR swapchain = {};
    VkResult result = vkCreateSwapchainKHR(logical_device, &create_info, nullptr, &swapchain);
    VK_CHECK(result);

    // get handles to swapchain images
    BufferView<VkImage> swapchain_images_buffer = {};
    u32 swapchain_image_count = 0;
    vkGetSwapchainImagesKHR(logical_device, swapchain, &swapchain_image_count, nullptr);
    VK_CHECK(result);
    swapchain_images_buffer.size = swapchain_image_count;
    swapchain_images_buffer.data = (VkImage*)arena_alloc(arena, sizeof(VkImage) * swapchain_images_buffer.size);
    result = vkGetSwapchainImagesKHR(logical_device, swapchain, &swapchain_image_count, swapchain_images_buffer.data);
    VK_CHECK(result);

    SwapchainInfo swapchain_info = {};
    swapchain_info.swapchain = swapchain;
    swapchain_info.extent = extent;
    swapchain_info.image_format = surface_format.format;
    swapchain_info.swapchain_images = swapchain_images_buffer;

    return swapchain_info;
}

bool does_physical_device_have_required_extensions(
    Arena* arena,
    VkPhysicalDevice physical_device)
{
    u32 num_required_extensions = ARRAY_SIZE(required_device_extension_names);
    u32 device_extension_count;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &device_extension_count, nullptr);
    ArenaTemp arena_temp = arena_temp_init(arena);
    VkExtensionProperties* available_extensions = (VkExtensionProperties*)arena_alloc(&arena_temp, sizeof(VkExtensionProperties) * device_extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &device_extension_count, available_extensions);

    bool has_all_extensions = true;
    // go through all the extensions we need and make sure they all exist in the list of available extensions for this physical device
    for (u32 i = 0; i < num_required_extensions; i++)
    {
        bool has_required_extension = false;
        const char* required_extension = required_device_extension_names[i];
        for (u32 k = 0; k < device_extension_count; k++)
        {
            const char* available_extension = available_extensions[k].extensionName;
            if (strcmp(required_extension, available_extension) == 0)
            {
                has_required_extension = true;
                break;
            }
        }
        if (!has_required_extension)
        {
            //return false;
            has_all_extensions = false;
            break;
        }
    }
    arena_temp_end(arena_temp);
    return has_all_extensions;
}

bool is_physical_device_suitable(Arena* arena, const VkPhysicalDevice& device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices = find_queue_families(arena, device, surface);
    // some programs create a "rating" system. If there's multiple gpus
    // you give them each a score, big points for being a dedicated gpu,
    // more points based on the max texture size, max number of textures/uniforms/whatever, etc....
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    // device must have all the extensions we use
    bool device_has_required_extensions = does_physical_device_have_required_extensions(arena, device);
    bool swapchain_adequate = false;
    // if we have all extensions, that means we have swapchain support... lets test if its good enough
    if (device_has_required_extensions)
    {
        SwapchainSupportDetails swapchain_support = query_swapchain_support(device, surface);
        swapchain_adequate = !swapchain_support.formats.empty() && !swapchain_support.present_modes.empty();
    }
    // only use dedicated gpus
    bool is_dedicated_gpu = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
   
    return 
        is_dedicated_gpu && 
        indices.is_complete() && 
        device_has_required_extensions && 
        swapchain_adequate;
}

VkPhysicalDevice find_physical_device(
    Arena* arena, 
    VkInstance instance,
    VkSurfaceKHR surface)
{
    VkPhysicalDevice chosen_physical_device = VK_NULL_HANDLE;
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    if (device_count == 0)
    {
        LOG_FATAL("No Vulkan GPUs found!");
        return VK_NULL_HANDLE;
    }
    VkPhysicalDevice* physical_devices = (VkPhysicalDevice*)arena_alloc(arena, sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, physical_devices);
    for (u32 i = 0; i < device_count; i++)
    {
        const VkPhysicalDevice& device = physical_devices[i];
        if (is_physical_device_suitable(arena, device, surface))
        {
            chosen_physical_device = device;
            break;
        }
    }
    if (chosen_physical_device == VK_NULL_HANDLE)
    {
        LOG_FATAL("Failed to find a suitable physical device!");
        return VK_NULL_HANDLE;
    }
    return chosen_physical_device;
}

// returns the number of created queues, and returns a pointer to them through q_create_infos_out allocated with the given arena
u32 get_queue_create_infos(
    Arena* arena, 
    VkPhysicalDevice physical_device, 
    VkSurfaceKHR surface,
    VkDeviceQueueCreateInfo*& q_create_infos_out)
{
    QueueFamilyIndices indices = find_queue_families(arena, physical_device, surface);
    // ensure no duplicates since graphics/present/etc might share a queue family
    std::set<u32> unique_queue_families = {indices.graphics_family.value(), indices.present_family.value()};
    u32 num_unique_queue_families = unique_queue_families.size();
    VkDeviceQueueCreateInfo* q_create_infos = (VkDeviceQueueCreateInfo*)arena_alloc(arena, sizeof(VkDeviceQueueCreateInfo) * num_unique_queue_families);
    f32 queue_priority = 1.0f;
    u32 i = 0;
    for (u32 queue_family : unique_queue_families)
    {
        VkDeviceQueueCreateInfo q_create_info = {};
        q_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        q_create_info.queueFamilyIndex = queue_family;
        q_create_info.queueCount = 1;
        q_create_info.pQueuePriorities = &queue_priority;
        q_create_infos[i] = q_create_info;
        i++;
    }
    q_create_infos_out = q_create_infos;
    return unique_queue_families.size();
}

VkDevice create_logical_device(
    Arena* arena, 
    VkInstance instance, 
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface)
{
    VkDeviceQueueCreateInfo* queue_create_infos = nullptr;
    u32 num_queues = get_queue_create_infos(arena, physical_device, surface, queue_create_infos);
    TINY_ASSERT(queue_create_infos != nullptr);

    VkPhysicalDeviceFeatures device_features = {};
    // leaving everything false for now... one would query for features if needed
    // features include things like geometry shaders, tesselation shaders, multi draw indirect, etc....
    //vkGetPhysicalDeviceFeatures(device, device_features);

    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.queueCreateInfoCount = num_queues;
    create_info.pEnabledFeatures = &device_features;
    create_info.ppEnabledExtensionNames = required_device_extension_names;
    create_info.enabledExtensionCount = ARRAY_SIZE(required_device_extension_names);
    // in past implementations of vulkan, device and instance validation layers
    // were seperate - and needed to be set like this in both the instance *and* logical device
    // this isn't the case nowadays, but setting them here helps be compatible with older versions
    if (validation_layers_enabled)
    {
        create_info.enabledLayerCount = ARRAY_SIZE(required_validation_layers);
        create_info.ppEnabledLayerNames = required_validation_layers;
    }
    else
    {
        create_info.enabledLayerCount = 0;
    }
    VkDevice logical_device;
    VkResult result = vkCreateDevice(physical_device, &create_info, nullptr, &logical_device);
    VK_CHECK(result);
    return logical_device;
}

VkSurfaceKHR get_native_window_surface(VkInstance instance)
{
    VkSurfaceKHR surface;
    VkResult result = glfwCreateWindowSurface(instance, glob_glfw_window, nullptr, &surface);
    VK_CHECK(result);
    return surface;
}

// VkImageViews need to be destroyed manually
BufferView<VkImageView> create_swapchain_image_views(
    Arena* arena,
    VkDevice logical_device,
    const SwapchainInfo& swapchain_info)
{
    BufferView<VkImageView> image_views = {};
    image_views.size = swapchain_info.swapchain_images.size;
    TINY_ASSERT(image_views.size > 0);
    image_views.data = arena_alloc_type(arena, VkImageView, image_views.size);

    for (u32 i = 0; i < swapchain_info.swapchain_images.size; i++)
    {
        const VkImage& swapchain_image = swapchain_info.swapchain_images.data[i];
        VkImageViewCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = swapchain_image; // img to view
        // how to view the img
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = swapchain_info.image_format;
        // can swizzle here (like mapping all channels to red for monochrome, etc)
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        // image's purpose & which parts can be accessed
        // color image with no mipmapping levels and no multiple layers
        // layers are often used for stereoscopic rendering - with 2 layers representing the left and right eyes
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;
        VkResult result = vkCreateImageView(logical_device, &create_info, nullptr, &image_views.data[i]);
        VK_CHECK(result);
    }
    return image_views;
}

VkShaderModule create_shader_module(
    VkDevice logical_device,
    u8* shader_binary,
    size_t shader_binary_size)
{
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = shader_binary_size;
    create_info.pCode = (u32*)shader_binary;
    VkShaderModule shader_module;
    VkResult result = vkCreateShaderModule(logical_device, &create_info, nullptr, &shader_module);
    VK_CHECK(result);
    return shader_module;
}

VkRenderPass create_render_pass(
    Arena* arena,
    VkDevice logical_device,
    const SwapchainInfo& swapchain)
{
    // attachment description
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = swapchain.image_format; // single color buffer represented by one of the images from the swapchain
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT; // no multisampling, so just 1
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // don't care about stencil buffer rn
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: Images used as color attachment | VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: Images to be presented in the swap chain | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: Images to be used as destination for a memory copy operation
    // don't care about previous image layout (since we clear it when loading (loadOp)), but image should be ready for presentation after rendering
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // subpasses
    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0; // which attachment to reference by index in attachment descriptions array (also relevant in shaders with layout(location = 0) out vec4 outColor)
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // vulkan also supports compute subpasses
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    VkRenderPass render_pass = {};  
    VkResult result = vkCreateRenderPass(logical_device, &render_pass_info, nullptr, &render_pass);
    return render_pass;
}

VkPipeline create_graphics_pipeline(
    Arena* arena,
    VkDevice logical_device,
    const SwapchainInfo& swapchain,
    VkRenderPass render_pass,
    VkPipelineLayout& pipeline_layout_out)
{
    size_t vert_binary_size, frag_binary_size;
    u8* vert_binary = read_file_bin(arena, "../src/shaders/built/vert.spv", &vert_binary_size);
    u8* frag_binary = read_file_bin(arena, "../src/shaders/built/frag.spv", &frag_binary_size);
    VkShaderModule vert_shader_module = create_shader_module(logical_device, vert_binary, vert_binary_size);
    VkShaderModule frag_shader_module = create_shader_module(logical_device, frag_binary, frag_binary_size);

    // shader stage creation - need to assign each shader binary to different stages of the graphics pipeline
    // vert
    VkPipelineShaderStageCreateInfo vert_stage_info = {};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vert_shader_module;
    vert_stage_info.pName = "main"; // entrypoint
    // can specify shader constants here
    // lets us use a single shader module with different constants to specify behavior at pipeline creation
    vert_stage_info.pSpecializationInfo = nullptr; 

    VkPipelineShaderStageCreateInfo fragshader_stage_info = {};
    fragshader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragshader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragshader_stage_info.module = frag_shader_module;
    fragshader_stage_info.pName = "main";
    fragshader_stage_info.pSpecializationInfo = nullptr; 
    
    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_stage_info, fragshader_stage_info};

    // Vertex Input
    // hard coding vertex data in vert shader rn, this'll matter when doing vertex buffers
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = nullptr;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr;

    // Input Assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    // viewports and scissors
    // viewports define the transformation from the image to the framebuffer
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    // NOTE: these are not necessarily the window size. the swapchain images are framebuffers
    // and as such can have varying dimensions. 
    viewport.width = (f32)swapchain.extent.width;
    viewport.height = (f32)swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    // scissors are rectangles that define in which regions pixels will be stored. Pixels outside the scissor rectangles will be discarded by the rasterizer
    // scissor that encompasses entire framebuffer - so nothing is cut off
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = swapchain.extent;

    const static VkDynamicState dynamic_states[] = 
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = (u32)ARRAY_SIZE(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;

    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    // omitting pViewports and pScissors fields because they are dynamic state

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // false here means discard fragments outside the near/far planes. True would mean clamping them
    rasterizer.depthClampEnable = VK_FALSE; 
    // true here means geometry never passes through rasterizer (disables output to framebuffer)
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    //VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments | VK_POLYGON_MODE_LINE: polygon edges are drawn as lines | VK_POLYGON_MODE_POINT: polygon vertices are drawn as points
    // anything besides FILL requires a GPU feature
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f; // anything thicker than 1.0 requires a gpu feature wideLines
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; 
    rasterizer.depthBiasEnable = VK_FALSE;
    // this stuff is used for shadow mapping. Not using it rn
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    // Multisampling (one method of anti aliasing) (requires GPU feature, disabled for now)
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    // for depth/stencil buffers (disabled for now)
    VkPipelineDepthStencilStateCreateInfo* depthstencil_info = nullptr;

    // Color blending
    // VkPipelineColorBlendAttachmentState - color blend config per framebuffer
    // VkPipelineColorBlendStateCreateInfo - global color blending config
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    // alpha blending
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
 
    VkPipelineColorBlendStateCreateInfo color_blending = {};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE; // blending with bitwise operation specified in logicOp
    color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0f; // Optional
    color_blending.blendConstants[1] = 0.0f; // Optional
    color_blending.blendConstants[2] = 0.0f; // Optional
    color_blending.blendConstants[3] = 0.0f; // Optional

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipline_layout_info = {};
    pipline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipline_layout_info.setLayoutCount = 0;
    pipline_layout_info.pSetLayouts = nullptr;
    pipline_layout_info.pushConstantRangeCount = 0;
    pipline_layout_info.pPushConstantRanges = nullptr;
    VkResult result = vkCreatePipelineLayout(logical_device, &pipline_layout_info, nullptr, &pipeline_layout_out);
    VK_CHECK(result);

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr; // Optional
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline_layout_out;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;
    // for allowing you to create new pipelines deriving from existing ones
    // these can be used if VK_PIPELINE_CREATE_DERIVATIVE_BIT  is specified in flags field of VkGraphicsPipelineCreateInfo
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // optional
    pipeline_info.basePipelineIndex = -1; // optional
    VkPipeline pipeline = {};
    result = vkCreateGraphicsPipelines(logical_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
    VK_CHECK(result);

    vkDestroyShaderModule(logical_device, frag_shader_module, nullptr);
    vkDestroyShaderModule(logical_device, vert_shader_module, nullptr);

    return pipeline;
}

BufferView<VkFramebuffer> create_framebuffers(
    Arena* arena,
    BufferView<VkImageView> swapchain_image_views,
    VkDevice logical_device,
    VkRenderPass render_pass,
    VkExtent2D swapchain_extent)
{
    VkFramebuffer* swapchain_framebuffers = arena_alloc_type(arena, VkFramebuffer, swapchain_image_views.size);
    BufferView<VkFramebuffer> ret = {swapchain_framebuffers, swapchain_image_views.size};
    // iterate image views and create framebuffers from them
    for (u32 i = 0; i < swapchain_image_views.size; i++)
    {
        VkImageView attachments[] = {swapchain_image_views.data[i]};
        VkFramebufferCreateInfo framebuffer_info = {};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = swapchain_extent.width;
        framebuffer_info.height = swapchain_extent.height;
        framebuffer_info.layers = 1;
        VkResult result = vkCreateFramebuffer(logical_device, &framebuffer_info, nullptr, &swapchain_framebuffers[i]);
        VK_CHECK(result);
    }
    return ret;
}


VkCommandPool create_command_pool(
    Arena* arena,
    const QueueFamilyIndices& indices,
    VkDevice logical_device)
{
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = indices.graphics_family.value();
    VkCommandPool cmd_pool = {};
    VkResult result = vkCreateCommandPool(logical_device, &pool_info, nullptr, &cmd_pool);
    VK_CHECK(result);
    return cmd_pool;
}

VkCommandBuffer create_command_buffer(
    VkDevice logical_device,
    VkCommandPool command_pool)
{
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool;
    // VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from other command buffers. | VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from primary command buffers
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    VkCommandBuffer cmd_buffer = {};
    VkResult result = vkAllocateCommandBuffers(logical_device, &alloc_info, &cmd_buffer);
    VK_CHECK(result);
    return cmd_buffer;
}

void record_cmd_buffer(
    VkCommandBuffer cmd_buffer, 
    u32 image_index,
    VkRenderPass render_pass,
    const SwapchainInfo& swapchain_info,
    BufferView<VkFramebuffer> swapchain_framebuffers,
    VkPipeline graphics_pipeline)
{
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once.
    // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be entirely within a single render pass.
    // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can be resubmitted while it is also already pending execution.
    begin_info.flags = 0; // Optional
    begin_info.pInheritanceInfo = nullptr; // Optional
    VkResult result = vkBeginCommandBuffer(cmd_buffer, &begin_info);
    VK_CHECK(result);

    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass;
    TINY_ASSERT(image_index < swapchain_framebuffers.size);
    render_pass_info.framebuffer = swapchain_framebuffers.data[image_index];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = swapchain_info.extent;
    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;
    vkCmdBeginRenderPass(cmd_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (f32)swapchain_info.extent.width;
    viewport.height = (f32)swapchain_info.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = swapchain_info.extent;
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    // vertex count, instance count, first vertex, first instance
    vkCmdDraw(cmd_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd_buffer);
    result = vkEndCommandBuffer(cmd_buffer);
    VK_CHECK(result);

}

void create_sync_objects(
    VkDevice logical_device,
    VkSemaphore& img_available_semaphore,
    VkSemaphore& render_finished_semaphore,
    VkFence& inflight_fence)
{
    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // create the fence in signaled state so the first frame we wait for it doesn't inf stall
    VkResult result = vkCreateSemaphore(logical_device, &semaphore_info, nullptr, &img_available_semaphore);
    VK_CHECK(result);
    result = vkCreateSemaphore(logical_device, &semaphore_info, nullptr, &render_finished_semaphore);
    VK_CHECK(result);
    result = vkCreateFence(logical_device, &fence_info, nullptr, &inflight_fence);
    VK_CHECK(result);
}

RuntimeData initVulkan()
{
    const u32 program_max_mem = MEGABYTES_BYTES(2);
    void* program_mem = TSYSALLOC(program_max_mem);
    RuntimeData runtime;
    runtime.arena = arena_init(program_mem, program_max_mem, "MainArena");
    Arena& arena = runtime.arena;
    // NOTE: because we set up of the debug messenger after the instance - any bugs/messages in instance creation
    // won't be shown. There is a way around this...
    runtime.instance = createInstance(&arena);
    setup_debug_messenger(runtime.instance, runtime.debug_messenger);
    runtime.surface = get_native_window_surface(runtime.instance);
    runtime.physical_device = find_physical_device(&arena, runtime.instance, runtime.surface);
    runtime.logical_device = create_logical_device(&arena, runtime.instance, runtime.physical_device, runtime.surface);
    QueueFamilyIndices indices = find_queue_families(&arena, runtime.physical_device, runtime.surface);
    vkGetDeviceQueue(runtime.logical_device, indices.graphics_family.value(), 0, &runtime.graphics_queue);
    vkGetDeviceQueue(runtime.logical_device, indices.present_family.value(), 0, &runtime.present_queue);
    
    constexpr u32 swapchain_arena_size = MEGABYTES_BYTES(1);
    void* swapchain_arena_mem = arena_alloc(&arena, swapchain_arena_size);
    runtime.swapchain_arena = arena_init(swapchain_arena_mem, swapchain_arena_size, "SwapchainArena");;
    runtime.swapchain_info = create_swapchain(&runtime.swapchain_arena, runtime.logical_device, runtime.physical_device, runtime.surface);
    runtime.swapchain_image_views = create_swapchain_image_views(&runtime.swapchain_arena, runtime.logical_device, runtime.swapchain_info);
    runtime.render_pass = create_render_pass(&arena, runtime.logical_device, runtime.swapchain_info);
    runtime.graphics_pipeline = create_graphics_pipeline(&arena, runtime.logical_device, runtime.swapchain_info, runtime.render_pass, runtime.pipline_layout);
    runtime.swapchain_framebuffers = create_framebuffers(&runtime.swapchain_arena, runtime.swapchain_image_views, runtime.logical_device, runtime.render_pass, runtime.swapchain_info.extent);

    runtime.command_pool = create_command_pool(&arena, indices, runtime.logical_device);
    runtime.command_buffer = create_command_buffer(runtime.logical_device, runtime.command_pool);
    create_sync_objects(runtime.logical_device, runtime.img_available_semaphore, runtime.render_finished_semaphore, runtime.inflight_fence);
    LOG_INFO("Vulkan initialization complete. Arena %i / %i bytes", arena.offset, arena.backing_mem_size);
    return runtime;
}

void destroy_swapchain(RuntimeData& runtime)
{
    for (u32 i = 0; i < runtime.swapchain_framebuffers.size; i++)
    {
        vkDestroyFramebuffer(runtime.logical_device, runtime.swapchain_framebuffers.data[i], nullptr);
    }
    for (u32 i = 0; i < runtime.swapchain_image_views.size; i++)
    {
        vkDestroyImageView(runtime.logical_device, runtime.swapchain_image_views.data[i], nullptr);
    }
    vkDestroySwapchainKHR(runtime.logical_device, runtime.swapchain_info.swapchain, nullptr);
    arena_clear(&runtime.swapchain_arena);
}

void recreate_swapchain(RuntimeData& runtime)
{
    s32 width, height;
    glfwGetFramebufferSize(glob_glfw_window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(glob_glfw_window, &width, &height);
        glfwWaitEvents();
    }
    LOG_INFO("recreating swapchain (%ix%i)", width, height);

    vkDeviceWaitIdle(runtime.logical_device);
    destroy_swapchain(runtime);

    runtime.swapchain_info = create_swapchain(&runtime.swapchain_arena, runtime.logical_device, runtime.physical_device, runtime.surface);
    runtime.swapchain_image_views = create_swapchain_image_views(&runtime.swapchain_arena, runtime.logical_device, runtime.swapchain_info);
    runtime.swapchain_framebuffers = create_framebuffers(&runtime.swapchain_arena, runtime.swapchain_image_views, runtime.logical_device, runtime.render_pass, runtime.swapchain_info.extent);
    // NOTE: not recreating render passes here. In theory swapchain image format may change during an app's lifetime
    // like if you drag the window from a standard monitor to a high DPI monitor. In that case we'd need to recreate the render pass
}

// draws a frame
void vulkanMainLoop(RuntimeData& runtime)
{
    // wait until previous frame is finished drawing
    vkWaitForFences(runtime.logical_device, 1, &runtime.inflight_fence, VK_TRUE, UINT64_MAX);

    // aquire image from swapchain
    u32 img_index;
    // img_available_semaphore is signaled when we aquire this image
    VkResult result = vkAcquireNextImageKHR(
        runtime.logical_device, runtime.swapchain_info.swapchain, 
        UINT64_MAX, runtime.img_available_semaphore, VK_NULL_HANDLE, &img_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // if we need to recreate the swapchain
        recreate_swapchain(runtime);
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        VK_CHECK(result); // VK_SUBOPTIMAL_KHR is considered a success code rn
    }
    // after waiting, if we know we are going to submit work (we might not if we need to recreate swapchain)
    // reset fence to unsignaled state
    vkResetFences(runtime.logical_device, 1, &runtime.inflight_fence);

    vkResetCommandBuffer(runtime.command_buffer, 0);
    record_cmd_buffer(runtime.command_buffer, img_index, runtime.render_pass, runtime.swapchain_info, runtime.swapchain_framebuffers, runtime.graphics_pipeline);

    // submitting the recorded command buffer
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore wait_semaphores[] = {runtime.img_available_semaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = waitStages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &runtime.command_buffer;
    VkSemaphore signal_semaphores[] = {runtime.render_finished_semaphore};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    // last parameter is the fence to signal when this queue is done (in this case - finished drawing)
    // since we wait on that fence at the beginning of the frame
    result = vkQueueSubmit(runtime.graphics_queue, 1, &submit_info, runtime.inflight_fence);
    VK_CHECK(result);

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores; // what to wait on before presentation can happen
    VkSwapchainKHR swapchains[] = {runtime.swapchain_info.swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &img_index;
    present_info.pResults = nullptr; // Optional
    result = vkQueuePresentKHR(runtime.present_queue, &present_info);
    VK_CHECK(result);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || runtime.framebufferWasResized) 
    {
        runtime.framebufferWasResized = false;
        recreate_swapchain(runtime);
    }
}



void vulkanCleanup(RuntimeData& runtime)
{
    vkDeviceWaitIdle(runtime.logical_device);
    if (validation_layers_enabled)
    {
        DestroyDebugUtilsMessengerEXT(runtime.instance, runtime.debug_messenger, nullptr);
    }
    destroy_swapchain(runtime);
    vkDestroySemaphore(runtime.logical_device, runtime.img_available_semaphore, nullptr);
    vkDestroySemaphore(runtime.logical_device, runtime.render_finished_semaphore, nullptr);
    vkDestroyFence(runtime.logical_device, runtime.inflight_fence, nullptr);
    vkDestroyCommandPool(runtime.logical_device, runtime.command_pool, nullptr);
    vkDestroyPipeline(runtime.logical_device, runtime.graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(runtime.logical_device, runtime.pipline_layout, nullptr);
    vkDestroyRenderPass(runtime.logical_device, runtime.render_pass, nullptr);
    vkDestroySurfaceKHR(runtime.instance, runtime.surface, nullptr);
    vkDestroyDevice(runtime.logical_device, nullptr);
    vkDestroyInstance(runtime.instance, nullptr);
}