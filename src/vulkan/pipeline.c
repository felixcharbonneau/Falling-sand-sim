#include "pipeline.h"
#include "vulkan_types.inl"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <vulkan/vulkan_core.h>


bool 
load_shader_module(const char* file_path, Device* device, VkShaderModule* out_module)
{
    FILE* fptr = fopen(file_path, "rb");
    if (!fptr) return false;

    fseek(fptr, 0, SEEK_END);
    uint32_t size = ftell(fptr);
    fseek(fptr, 0, SEEK_SET);

    uint32_t data[size / sizeof(uint32_t)];
    fread((char*)data, 1, size, fptr);
    fclose(fptr);

    VkShaderModuleCreateInfo module_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(data),
        .pCode = data
    };
    VkShaderModule shader_module;
    if (vkCreateShaderModule(device->device, &module_info, NULL, &shader_module) != VK_SUCCESS) {
        return false;
    }
    *out_module = shader_module;
    return true;
}

bool
create_compute_pipeline(Device* device, VkShaderModule module, Pipeline* out_pipeline)
{
    out_pipeline->bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
    VkComputePipelineCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = device->pipeline_layout,
        .stage = {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = module,
            .pName  = "main",
        },
    };
    if(vkCreateComputePipelines(device->device, NULL, 1, &create_info,NULL, &out_pipeline->pipeline) != VK_SUCCESS)
    {
        return false;
    }
    return true;
}