#pragma once
#include "vulkan_types.inl"

bool load_shader_module(const char* file_path, Device* device, VkShaderModule* out_module);

bool create_compute_pipeline(Device* device, VkShaderModule module, Pipeline* out_pipeline);