#include "instance.h"
#include <SDL3/SDL_vulkan.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <vulkan/vulkan_core.h>
#include "device.h"


static VkBool32 VKAPI_PTR _debug_print(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData)
{
    fprintf(stderr, "[VALIDATION]: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}

bool 
create_instance(InstanceCreateInfo* create_info, Arena* arena, Instance* instance)
{
    *instance = (Instance){};

    Device* devices = (Device*)arena_alloc(arena, sizeof(Device) * create_info->max_device_count);
    if (!devices) return false;
    instance->devices.data     = devices;
    instance->devices.capacity = create_info->max_device_count;
    instance->devices.size     = 0;


    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_3
    };
    uint32_t extension_count = 0;
    const char* extension_names[16];
    const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);

    for (uint32_t i = 0; i < extension_count; i++) 
    {
        extension_names[i] = sdl_extensions[i];
    }

    VkInstanceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = 0,
        .enabledLayerCount = 0 
    };

    const char* const layer_names[1] = 
    {
        "VK_LAYER_KHRONOS_validation"
    };

    if (create_info->enable_layers) 
    {
        info.enabledLayerCount = 1;
        info.ppEnabledLayerNames = layer_names;

        extension_names[extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }

    info.enabledExtensionCount   = extension_count;
    info.ppEnabledExtensionNames = extension_names;

    vkCreateInstance(&info, NULL, &instance->instance);

    if (create_info->enable_layers) 
    {
        VkDebugUtilsMessengerCreateInfoEXT debug_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageType =  VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .pfnUserCallback = _debug_print
        };
        PFN_vkCreateDebugUtilsMessengerEXT fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance->instance, "vkCreateDebugUtilsMessengerEXT");
        if (fn) {
            fn(instance->instance, &debug_info, NULL, &instance->debugger);
        }
    }

    return true;
}

void 
free_instance(Instance* instance)
{
    for (int i = 0; i < instance->devices.size; i++) 
    {
        internal_free_device(instance, &instance->devices.data[i]);
    }

    if (instance->debugger != VK_NULL_HANDLE) 
    {
        PFN_vkDestroyDebugUtilsMessengerEXT fn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (fn) 
        {
            fn(instance->instance, instance->debugger, NULL);
        }
        instance->debugger = VK_NULL_HANDLE;
    }

    if (instance->instance != VK_NULL_HANDLE) 
    {
        vkDestroyInstance(instance->instance, NULL);
        instance->instance = VK_NULL_HANDLE;
    }
}