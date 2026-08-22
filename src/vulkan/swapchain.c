#include "swapchain.h"
#include "device.h"
#include "vulkan_types.inl"
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

bool 
create_swapchain(Device* device, SwapchainCreateInfo* create_info, Swapchain* out_swapchain)
{
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, create_info->surface, &capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, create_info->surface, &format_count, NULL);
    VkSurfaceFormatKHR formats[format_count];
    vkGetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, create_info->surface, &format_count, formats);

    uint32_t present_mode_counts;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, create_info->surface, &present_mode_counts, NULL);
    VkPresentModeKHR present_modes[present_mode_counts];
    vkGetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, create_info->surface, &present_mode_counts, present_modes);

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for(int i = 0; i < present_mode_counts; i++)
    {
        if (!create_info->vsync && present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }

    out_swapchain->format = formats[0];
    for (uint32_t i = 0; i < format_count; i++)
    {
        if(formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            out_swapchain->format = formats[i];
            break;
        }
    }

    /// Extent
    {
        out_swapchain->extent = capabilities.currentExtent;
        if (capabilities.currentExtent.width == ~0u) {
            int width, height;
            SDL_GetWindowSize(create_info->window, &width, &height);

            VkExtent2D actual_extent = {
                width, height
            };
            if (actual_extent.width < capabilities.minImageExtent.width) actual_extent.width = capabilities.minImageExtent.width;
            if (actual_extent.width < capabilities.maxImageExtent.width) actual_extent.width = capabilities.maxImageExtent.width;

            if (actual_extent.height < capabilities.minImageExtent.height) actual_extent.height = capabilities.minImageExtent.height;
            if (actual_extent.height < capabilities.maxImageExtent.height) actual_extent.height = capabilities.maxImageExtent.height;
        }
    }

    uint32_t image_count;
    /// Image count
    {
        image_count = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount && image_count > capabilities.maxImageCount) image_count = capabilities.maxImageCount;
    }



    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = create_info->surface,
        .minImageCount = image_count,
        .imageFormat = out_swapchain->format.format,
        .imageColorSpace = out_swapchain->format.colorSpace,
        .imageExtent = out_swapchain->extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };
    vkCreateSwapchainKHR(device->device, &swapchain_info, NULL, &out_swapchain->swapchain);

    vkGetSwapchainImagesKHR(device->device, out_swapchain->swapchain, &out_swapchain->image_count, NULL);
    VkImage images[out_swapchain->image_count];
    vkGetSwapchainImagesKHR(device->device, out_swapchain->swapchain, &out_swapchain->image_count, images);


    for(int i = 0; i < out_swapchain->image_count; i++)
    {
        out_swapchain->images[i] = (Image){
            .image = images[i],
            .layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .extent = {
                .width = out_swapchain->extent.width,
                .height = out_swapchain->extent.height,
                .depth = 1
            }
        };

        VkImageViewCreateInfo view_info = {
            .sType  = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image  = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = out_swapchain->format.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        vkCreateImageView(device->device, &view_info, NULL, &out_swapchain->images[i].view);
    
        out_swapchain->image_semaphores[i] = create_semaphore(device);
    }
    
    return true;
}

void 
destroy_swapchain(Device* device, Swapchain* swapchain)
{
    if (!device || device->device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(device->device);
    
    if (swapchain->swapchain) 
    {

        for(int i = 0; i < swapchain->image_count; i++)
        {
            vkDestroyImageView(device->device, swapchain->images[i].view, NULL);
            destroy_semaphore(device, &swapchain->image_semaphores[i]);
        }
        vkDestroySwapchainKHR(device->device, swapchain->swapchain, NULL);    
    }
}

uint32_t 
swapchain_next_image(Device* device, Swapchain* swapchain, Semaphore* semaphore)
{
    uint32_t index;
    vkAcquireNextImageKHR(device->device, swapchain->swapchain, ~0u, semaphore->semaphore, NULL, &index);
    return index;
}