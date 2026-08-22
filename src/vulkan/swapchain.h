#pragma once
#include "vulkan_types.inl"

bool create_swapchain(Device* device, SwapchainCreateInfo* create_info, Swapchain* out_swapchain);

void destroy_swapchain(Device* device, Swapchain* swapchain);

uint32_t swapchain_next_image(Device* device, Swapchain* swapchain, Semaphore* semaphore);