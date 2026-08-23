#pragma once
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>


typedef struct Device
{
    VkPhysicalDevice physical_device;
    VkDevice device;

    uint32_t queue_index;
    VkQueue queue;

    VkCommandPool command_pool;

    VkDescriptorSetLayout desc_layout;
    VkDescriptorPool desc_pool;
    VkDescriptorSet desc_set;
    VkPipelineLayout pipeline_layout;

    uint32_t next_img;
} Device;

typedef struct CommandBuffer
{
    VkCommandBuffer buffer;
} CommandBuffer;

typedef struct CommandBufferAllocateInfo {

} CommandBufferAllocateInfo;
#define COMMAND_BUFFER_ALLOCATE_INFO_DEFAULTS


typedef struct Image
{
    VkImage image;
    VkImageView view;
    VkImageLayout layout;

    VkImageAspectFlags aspect;
    VkFormat format;
    VkExtent3D extent;
    uint32_t desc_offset;
    VkDeviceMemory memory;
} Image;

typedef struct Fence
{
    VkFence fence;
} Fence;
typedef struct Semaphore
{
    VkSemaphore semaphore;
} Semaphore;

typedef struct DeviceCreateInfo
{
    SDL_Window* for_window;
} DeviceCreateInfo;

#define DEVICE_CREATE_INFO_DEFAULTS

typedef struct Instance
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugger;

    struct {
        Device* data;
        uint32_t size, capacity;
    } devices;
} Instance;

typedef struct Swapchain
{
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkSurfaceFormatKHR format;
    VkExtent2D extent;

    uint32_t image_count;
    Image images[16];
    Semaphore image_semaphores[16];
} Swapchain;

typedef struct SwapchainCreateInfo {
    SDL_Window* window;
    VkSurfaceKHR surface;
    bool vsync;
} SwapchainCreateInfo;

typedef struct InstanceCreateInfo
{
    bool enable_layers;
    uint32_t max_device_count;
} InstanceCreateInfo;
#define INSTANCE_CREATE_INFO_DEFAULTS .enable_layers = true, .max_device_count = 1

typedef struct Pipeline {
    VkPipeline pipeline;
    VkPipelineBindPoint bind_point;
} Pipeline;


typedef struct Buffer {
    VkBuffer       buffer;
    VkDeviceMemory memory;
    void*          mapped;
} Buffer;


#define IMAGE_SUBRESOURCE_RANGE_DEFAULTS \
    .baseMipLevel = 0, .levelCount = VK_REMAINING_MIP_LEVELS, \
    .baseArrayLayer = 0, .layerCount = VK_REMAINING_ARRAY_LAYERS