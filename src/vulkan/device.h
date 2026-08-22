#pragma once
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "stddef.h"
#include "memory/memory.h"
#include "vulkan_types.inl"


bool create_device(Instance* instance, DeviceCreateInfo* create_info, Arena* arena, Device* out_device);

/// The instance calls this, user must not call
void internal_free_device(Instance* instance, Device* device);

CommandBuffer allocate_command_buffer(Device* device, CommandBufferAllocateInfo* alloc_info);
void command_buffer_reset(CommandBuffer* buffer);
void begin_command_buffer(CommandBuffer* buffer, VkCommandBufferUsageFlags flags);
void end_command_buffer(CommandBuffer* buffer);
void submit_command_buffer(CommandBuffer* buffer, VkQueue queue, Semaphore* wait_semaphore, Semaphore* signal_semaphore, Fence* fence);
void present(Swapchain* swapchain, Semaphore* wait_semaphore, uint32_t image_index, VkQueue queue);


void cmd_push(CommandBuffer* cmd, Device* device, uint32_t size, void* data);
void cmd_bind_image_set(CommandBuffer* cmd, Device* device, VkPipelineBindPoint bind_point);
void cmd_bind_pipeline(CommandBuffer* cmd, Pipeline* pipeline);
void cmd_dispatch(CommandBuffer* cmd, uint32_t x, uint32_t y, uint32_t z);

bool gpu_alloc(Device* device, VkMemoryRequirements reqs, VkMemoryPropertyFlags props, VkDeviceMemory* out_memory);

/// Commands
void cmd_clear_color_image(CommandBuffer* cmd, Image* img, VkClearColorValue clear_value);

Semaphore create_semaphore(Device* device);
Fence create_fence(Device* device);

void destroy_semaphore(Device* device, Semaphore* semaphore);
void destroy_fence(Device* device, Fence* fence);


void fence_wait(Device* device, Fence* fence);
void fence_reset(Device* device, Fence* fence);