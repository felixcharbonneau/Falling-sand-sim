#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(rgba16f, set = 0, binding = 0) uniform image2D g_images[];
layout(rgba8,   set = 0, binding = 0) uniform image2D  g_images_rgba8[];
layout(r32ui,   set = 0, binding = 0) uniform uimage2D g_images_u32[];

#define PUSH_CONSTANTS(Name) \
    layout(push_constant, scalar) uniform Registers { Name } registers