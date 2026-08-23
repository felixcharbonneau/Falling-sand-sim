#include "image.h"
#include "device.h"
#include <assert.h>
#include <vulkan/vulkan_core.h>


void 
cmd_transition_image(
    CommandBuffer* cmd, 
    Image* img, 
    VkImageLayout layout
) {
    VkImageMemoryBarrier2 image_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
        .oldLayout = img->layout,
        .newLayout = layout,
        .subresourceRange = {
            IMAGE_SUBRESOURCE_RANGE_DEFAULTS,
            .aspectMask = img->aspect,
        },
        .image = img->image,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED
    };

    VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &image_barrier
    };

    vkCmdPipelineBarrier2(cmd->buffer, &dep_info);
    img->layout = layout;
}

void 
cmd_memory_barrier(CommandBuffer* cmd, Image* img)
{
    cmd_transition_image(cmd, img, img->layout);
}


void
cmd_blit_image(CommandBuffer* cmd, Image* src, Image* dst)
{
    VkImageBlit2 region = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .srcSubresource = {
            .aspectMask = src->aspect, .mipLevel = 0,
            .baseArrayLayer = 0, .layerCount = 1,
        },
        .srcOffsets = {
            { 0, 0, 0 },
            { (int32_t)src->extent.width, (int32_t)src->extent.height, 1 },
        },
        .dstSubresource = {
            .aspectMask = dst->aspect, .mipLevel = 0,
            .baseArrayLayer = 0, .layerCount = 1,
        },
        .dstOffsets = {
            { 0, 0, 0 },
            { (int32_t)dst->extent.width, (int32_t)dst->extent.height, 1 },
        },
    };

    VkBlitImageInfo2 blit_info = {
        .sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .srcImage       = src->image,
        .srcImageLayout = src->layout,
        .dstImage       = dst->image,
        .dstImageLayout = dst->layout,
        .regionCount    = 1,
        .pRegions       = &region,
        .filter         = VK_FILTER_NEAREST,
    };
    vkCmdBlitImage2(cmd->buffer, &blit_info);
}


Image 
create_image(Device* device, VkFormat format, VkImageUsageFlags usage_flags, VkExtent3D extent, VkImageAspectFlags aspect_flags)
{
    Image out_img = {0};
    out_img.desc_offset = UINT32_MAX;
    out_img.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    out_img.format = format;
    out_img.extent = extent;
    out_img.aspect = aspect_flags;

    VkImageCreateInfo img_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = extent,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .arrayLayers = 1,
        .mipLevels = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage_flags
    };
    vkCreateImage(device->device, &img_info, NULL, &out_img.image);
   
    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(device->device, out_img.image, &reqs);
    gpu_alloc(device, reqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &out_img.memory);
    vkBindImageMemory(device->device, out_img.image, out_img.memory, 0);

    VkImageViewCreateInfo img_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .image = out_img.image,
        .format = format,
        .subresourceRange = {
            IMAGE_SUBRESOURCE_RANGE_DEFAULTS,
            .aspectMask = aspect_flags
        }
    };
    vkCreateImageView(device->device, &img_view_info, NULL, &out_img.view);

    if (usage_flags & VK_IMAGE_USAGE_STORAGE_BIT) {
        uint32_t desc_index = device->next_img++;
        out_img.desc_offset = desc_index;

        VkDescriptorImageInfo desc_info = {
            .imageView = out_img.view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 0,
            .dstSet = device->desc_set,
            .dstArrayElement = desc_index,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &desc_info
        };
        vkUpdateDescriptorSets(device->device, 1, &write, 0, NULL);
    }
    return out_img;
}

void
cmd_copy_buffer_to_image(CommandBuffer* cmd, Buffer* src, Image* dst)
{
    VkBufferImageCopy region = {
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageExtent      = dst->extent,
    };
    vkCmdCopyBufferToImage(cmd->buffer, src->buffer, dst->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}