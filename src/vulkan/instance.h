#pragma once
#include <stdbool.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "memory/memory.h"
#include "vulkan_types.inl"


bool create_instance(InstanceCreateInfo* create_info, Arena* arena, Instance* instance);
void free_instance(Instance* instance);