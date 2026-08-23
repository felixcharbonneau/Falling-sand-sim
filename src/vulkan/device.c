#include "device.h"
#include "vulkan_types.inl"
#include <SDL3/SDL_vulkan.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>
#include <vulkan/vulkan_core.h>


bool 
create_device(Instance* instance, DeviceCreateInfo* create_info, Arena* arena, Device* out_device)
{
    if (instance->devices.size >= instance->devices.capacity) return false;
    Device* slot = instance->devices.data + instance->devices.size;
    *slot = (Device){0};
    instance->devices.size++;

    uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(instance->instance, &physical_device_count, NULL);
    if (!physical_device_count) return false;
    VkPhysicalDevice pdevs[physical_device_count];
    vkEnumeratePhysicalDevices(instance->instance, &physical_device_count, pdevs);

    /// Just taking the first device for now, should edit later to make sure it can support the application.
    VkPhysicalDevice physical_device;
    physical_device = pdevs[0];
    slot->physical_device = physical_device;

    uint32_t queue_props_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_props_count, NULL);
    VkQueueFamilyProperties queue_props[queue_props_count];
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_props_count, queue_props);


    uint32_t queue_index = 0;
    bool queue_found = false;
    for (uint32_t i = 0; i < queue_props_count; i++) 
    {
        if (queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
        {
            if(SDL_Vulkan_GetPresentationSupport(instance->instance, physical_device, i)) 
            {
                queue_index = i;
                queue_found = true;
                break;
            }
        }
    }
    if (!queue_found) return false;

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info =
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pQueuePriorities = &priority,
        .queueCount = 1,
        .queueFamilyIndex = queue_index
    };


    const char* device_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkPhysicalDeviceVulkan12Features features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing                          = VK_TRUE,
        .runtimeDescriptorArray                      = VK_TRUE,
        .descriptorBindingPartiallyBound             = VK_TRUE,
        .descriptorBindingUpdateUnusedWhilePending   = VK_TRUE,
        .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
        .bufferDeviceAddress                         = VK_TRUE,
        .scalarBlockLayout                           = VK_TRUE,
        .shaderStorageImageArrayNonUniformIndexing = VK_TRUE
    };
    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
        .pNext = &features12,
    };


    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features13,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledLayerCount = 0,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_exts
    };

    vkCreateDevice(physical_device, &device_info, NULL, &slot->device);

    slot->queue_index = queue_index;
    vkGetDeviceQueue(slot->device, slot->queue_index, 0, &slot->queue);


    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_index
    };
    vkCreateCommandPool(slot->device, &pool_info, NULL, &slot->command_pool);


    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 65536,
        .stageFlags = VK_SHADER_STAGE_ALL
    };
    VkDescriptorBindingFlags flags =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &flags
    };
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flags_info,
        .bindingCount = 1,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .pBindings = &binding,
    };
    vkCreateDescriptorSetLayout(slot->device, &layout_info, NULL, &slot->desc_layout);

    VkDescriptorPoolSize size = { 
        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 
        .descriptorCount = 65536
    };
    VkDescriptorPoolCreateInfo desc_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &size
    };
    vkCreateDescriptorPool(slot->device, &desc_pool_info, NULL, &slot->desc_pool);
    VkDescriptorSetAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = slot->desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &slot->desc_layout
    };
    vkAllocateDescriptorSets(slot->device, &alloc, &slot->desc_set);

    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = 128
    };
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pSetLayouts = &slot->desc_layout,
        .setLayoutCount = 1,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range
    };
    vkCreatePipelineLayout(slot->device, &pipeline_layout_info, NULL, &slot->pipeline_layout);

    if (out_device) *out_device = *slot;
    return true;
}

void 
cmd_push(CommandBuffer* cmd, Device* device, uint32_t size, void* data)
{
    assert(size <= 128);
    vkCmdPushConstants(cmd->buffer, device->pipeline_layout, VK_SHADER_STAGE_ALL, 0, size, data);
}


void 
cmd_bind_image_set(CommandBuffer* cmd, Device* device, VkPipelineBindPoint bind_point)
{
    vkCmdBindDescriptorSets(cmd->buffer, bind_point, device->pipeline_layout, 0, 1, &device->desc_set, 0, NULL);
}


void 
internal_free_device(Instance* instance, Device* device)
{
    if (device->device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(device->device);
    vkDestroyCommandPool(device->device, device->command_pool, NULL);
    vkDestroyDevice(device->device, NULL);
}

CommandBuffer 
allocate_command_buffer(Device* device, CommandBufferAllocateInfo* info)
{
    CommandBuffer out_buffer;

    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = device->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(device->device, &alloc_info, &out_buffer.buffer);
    return out_buffer;
}

void 
command_buffer_reset(CommandBuffer* buffer)
{
    vkResetCommandBuffer(buffer->buffer, 0);
}

void 
begin_command_buffer(CommandBuffer* buffer, VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags
    };
    vkBeginCommandBuffer(buffer->buffer, &begin_info);
}

void 
end_command_buffer(CommandBuffer* buffer)
{
    vkEndCommandBuffer(buffer->buffer);
}

void 
submit_command_buffer(CommandBuffer* buffer, VkQueue queue, Semaphore* wait_semaphore, Semaphore* signal_semaphore, Fence* fence)
{
    uint32_t wait_semaphore_count = wait_semaphore ? 1 : 0;
    uint32_t signal_semaphore_count = signal_semaphore ? 1 : 0;
    VkSemaphoreSubmitInfo wait_info = {};
    VkSemaphoreSubmitInfo signal_info = {};

    if (wait_semaphore) {
        wait_info = (VkSemaphoreSubmitInfo){
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = wait_semaphore->semaphore,
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
            .deviceIndex = 0,
            .value = 1
        };
    }
    if (signal_semaphore) {
        signal_info = (VkSemaphoreSubmitInfo){
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = signal_semaphore->semaphore,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
            .deviceIndex = 0,
            .value = 1
        };
    }

    VkCommandBufferSubmitInfo cmd_submit = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = buffer->buffer
    };

    VkSubmitInfo2 submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = wait_semaphore_count,
        .pWaitSemaphoreInfos = &wait_info,
        .signalSemaphoreInfoCount = signal_semaphore_count,
        .pSignalSemaphoreInfos = &signal_info,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_submit
    };

    vkQueueSubmit2(queue, 1, &submit, (fence ? fence->fence : VK_NULL_HANDLE));
}

Semaphore 
create_semaphore(Device* device)
{
    Semaphore out_semaphore;
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    vkCreateSemaphore(device->device, &semaphore_info, NULL, &out_semaphore.semaphore);
    return out_semaphore;
}
Fence 
create_fence(Device* device)
{
    Fence out_fence;
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT 
    };
    vkCreateFence(device->device, &fence_info, NULL, &out_fence.fence);
    return out_fence;
}

void 
destroy_semaphore(Device* device, Semaphore* semaphore)
{
    vkDestroySemaphore(device->device, semaphore->semaphore, NULL);
}

void 
destroy_fence(Device* device, Fence* fence)
{
    vkDestroyFence(device->device, fence->fence, NULL);
}


void 
fence_wait(Device* device, Fence* fence)
{
    vkWaitForFences(device->device, 1, &fence->fence, VK_TRUE, UINT64_MAX);
}
void 
fence_reset(Device* device, Fence* fence)
{
    vkResetFences(device->device, 1, &fence->fence);
}


void 
cmd_clear_color_image(CommandBuffer* cmd, Image* img, VkClearColorValue clear_value)
{
    vkCmdClearColorImage(cmd->buffer, img->image, img->layout, &clear_value, 1, &(VkImageSubresourceRange){
            IMAGE_SUBRESOURCE_RANGE_DEFAULTS,
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        }
    );
}

void 
present(Swapchain* swapchain, Semaphore* wait_semaphore, uint32_t image_index, VkQueue queue)
{
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pSwapchains = &swapchain->swapchain,
        .swapchainCount = 1,
        .pWaitSemaphores = &wait_semaphore->semaphore,
        .waitSemaphoreCount = 1,
        .pImageIndices = &image_index
    };
    vkQueuePresentKHR(queue, &present_info);
}


static uint32_t 
find_memory_type(Device* device, uint32_t type_filter, VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(device->physical_device, &mem_properties);
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) 
    {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & props) == props) 
        {
            return i;
        } 
    }
    return 0;
}
bool 
gpu_alloc(Device* device, VkMemoryRequirements reqs, VkMemoryPropertyFlags props, VkDeviceMemory* out_memory)
{
    uint32_t mem_type_index = find_memory_type(device, reqs.memoryTypeBits, props);
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = reqs.size,
        .memoryTypeIndex = mem_type_index
    };
    if(vkAllocateMemory(device->device, &alloc_info, NULL, out_memory) != VK_SUCCESS)
    {
        return false;
    }
    return true;
}

void 
cmd_bind_pipeline(CommandBuffer* cmd, Pipeline* pipeline)
{
    vkCmdBindPipeline(cmd->buffer, pipeline->bind_point, pipeline->pipeline);
}

void 
cmd_dispatch(CommandBuffer* cmd, uint32_t x, uint32_t y, uint32_t z)
{
    vkCmdDispatch(cmd->buffer, x, y, z);
}

Buffer
create_staging_buffer(Device* device, VkDeviceSize size)
{
    Buffer b = {0};
    VkBufferCreateInfo info = {
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = size,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    vkCreateBuffer(device->device, &info, NULL, &b.buffer);

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device->device, b.buffer, &req);

    gpu_alloc(device, req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &b.memory);

    vkBindBufferMemory(device->device, b.buffer, b.memory, 0);
    vkMapMemory(device->device, b.memory, 0, size, 0, &b.mapped);
    return b;
}