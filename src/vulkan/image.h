#pragma once
#include "vulkan_types.inl"
#include <vulkan/vulkan_core.h>

void cmd_transition_image(CommandBuffer* cmd, Image* img, VkImageLayout layout);
void cmd_blit_image(CommandBuffer* cmd, Image* src, Image* dst);
void cmd_memory_barrier(CommandBuffer* cmd, Image* img);

Image create_image(Device* device, VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent, VkImageAspectFlags aspect_flags);

void cmd_copy_buffer_to_image(CommandBuffer* cmd, Buffer* src, Image* dst);