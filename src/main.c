#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <vulkan/vulkan_core.h>
#include "memory/arena.h"
#include "memory/memory.h"
#include "vulkan/image.h"
#include "vulkan/instance.h"
#include "vulkan/device.h"
#include "vulkan/pipeline.h"
#include "vulkan/swapchain.h"
#include "vulkan/vulkan_types.inl"


#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio.h"

#define PL_MPEG_IMPLEMENTATION
#include "vendor/pl_mpeg.h"

char game_memory[MB(256)];
Arena game_arena;
#define FRAME_ARENA_SIZE MB(8)

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define RESOLUTION 4
#define CHUNK_WIDTH WINDOW_WIDTH/RESOLUTION
#define CHUNK_HEIGHT WINDOW_HEIGHT/RESOLUTION

#define MAX_FRAMES_IN_FLIGHT 3
struct frame_data {
    CommandBuffer cmd;
    Semaphore swapchain_semaphore;
    Fence render_fence;
    Arena frame_arena;

    Buffer video_staging;
} frames[MAX_FRAMES_IN_FLIGHT];


uint32_t frame_in_flight = 0;
uint32_t frame_index = 0;
bool running = true;
SDL_Window* window;
VkSurfaceKHR surface;
Instance instance;
Device device;
Swapchain swapchain;

Pipeline copy_pipeline;
Pipeline draw_pipeline;
Pipeline circle_brush;
Pipeline sim_pipeline;
Pipeline video_pipeline;

Image chunk_images[2];



Image draw_image;

int32_t brush_radius = 30;
#define MIN_BRUSH_RADIUS 1
#define MAX_BRUSH_RADIUS 200

struct edits {
    uint32_t brush_radius;
    uint32_t x, y;
    uint32_t material;
} next_edit;

bool first_frame = true; /// TODO: This isnt very clean

struct DrawData{
    uint32_t x, y;
    bool drawing;
} draw_data = {0};

typedef struct SimPushConstants {
    uint32_t src_img;
    uint32_t dst_img;
    int32_t offset_x, offset_y;
    uint32_t frame_index;
} SimPushConstants;

typedef struct CopyPushConstants {
    uint32_t src_img;
    uint32_t dst_img;
} CopyPushConstants;

typedef struct DrawPushConstants
{
    uint32_t draw_image_index;
    uint32_t chunk_index;
    uint32_t chunk_prev_index;
} DrawPushConstants;

typedef struct CircleBrushPushConstants {
    uint32_t chunk;
    int32_t  brush_pos_x;   
    int32_t  brush_pos_y;   
    int32_t  brush_radius;  
    uint32_t material;      
    uint32_t frame_index;
} CircleBrushPushConstants;

typedef struct VideoPushConstants
{
    uint32_t video_img;
    uint32_t chunk_img;
    uint32_t frame_idx;
} VideoPushConstants;


//// Extra code to play bad apple
bool bad_apple = false;
ma_engine engine;
ma_sound bad_apple_sound;
plm_t*   plm;
uint8_t  video_luma[CHUNK_WIDTH * CHUNK_HEIGHT];
bool     video_dirty = false;
double   video_clock = 0.0;
Image    video_image;

/// Callback for plm
static void
on_video(plm_t* p, plm_frame_t* frame, void* user)
{
    for (uint32_t y = 0; y < CHUNK_HEIGHT; y++) {
        memcpy(video_luma + y * CHUNK_WIDTH,
               frame->y.data + y * frame->y.width,
               CHUNK_WIDTH);
    }
    
    video_dirty = true;
}

void 
render_frame()
{
    Fence* fence                   = &frames[frame_in_flight].render_fence;
    Semaphore* swapchain_semaphore = &frames[frame_in_flight].swapchain_semaphore;
    CommandBuffer* cmd             = &frames[frame_in_flight].cmd;

    fence_wait(&device, fence);
    fence_reset(&device, fence);

    uint32_t image_index = swapchain_next_image(&device, &swapchain, swapchain_semaphore);
    Image* swapchain_image = &swapchain.images[image_index];
    Semaphore* render_semaphore = &swapchain.image_semaphores[image_index];

    command_buffer_reset(cmd);
    begin_command_buffer(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    if (first_frame) {
        cmd_transition_image(cmd, &chunk_images[0], VK_IMAGE_LAYOUT_GENERAL);
        cmd_transition_image(cmd, &chunk_images[1], VK_IMAGE_LAYOUT_GENERAL);
        VkClearColorValue clear_value = {
            .uint32 = { 0, 0, 0, 0 }
        };
        cmd_clear_color_image(cmd, &chunk_images[0], clear_value);
        cmd_clear_color_image(cmd, &chunk_images[1], clear_value);
        cmd_memory_barrier(cmd, &chunk_images[0]);
        cmd_memory_barrier(cmd, &chunk_images[1]);

        first_frame = false;
    }
    cmd_bind_image_set(cmd, &device, VK_PIPELINE_BIND_POINT_COMPUTE);

    if (draw_data.drawing) {
        uint32_t chunk_radius = next_edit.brush_radius / RESOLUTION;

        CircleBrushPushConstants push = {
            .chunk = chunk_images[0].desc_offset,
            .brush_radius = chunk_radius,
            .material = next_edit.material,
            .brush_pos_x = next_edit.x / RESOLUTION,
            .brush_pos_y = next_edit.y / RESOLUTION,
            .frame_index = frame_index
        };
        cmd_push(cmd, &device, sizeof(CircleBrushPushConstants), &push);
        cmd_bind_pipeline(cmd, &circle_brush);

        uint32_t diameter = chunk_radius * 2 + 1;
        uint32_t groups   = (diameter + 15) / 16;
        cmd_dispatch(cmd, groups, groups, 1);
        cmd_memory_barrier(cmd, &chunk_images[0]);
    }

    if (bad_apple && video_dirty) 
    {
        Buffer* stage = &frames[frame_in_flight].video_staging;
        uint32_t* dst = (uint32_t*)stage->mapped;
        for (uint32_t i = 0; i < CHUNK_WIDTH * CHUNK_HEIGHT; i++)
            dst[i] = video_luma[i];

        cmd_transition_image(cmd, &video_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        cmd_copy_buffer_to_image(cmd, stage, &video_image);
        cmd_transition_image(cmd, &video_image, VK_IMAGE_LAYOUT_GENERAL);

        VideoPushConstants push = {
            .video_img = video_image.desc_offset,
            .chunk_img = chunk_images[0].desc_offset,
            .frame_idx = frame_index
        };
        cmd_push(cmd, &device, sizeof(VideoPushConstants), &push);
        cmd_bind_pipeline(cmd, &video_pipeline);
        cmd_dispatch(cmd, (CHUNK_WIDTH + 15) / 16, (CHUNK_HEIGHT + 15) / 16, 1);
        cmd_memory_barrier(cmd, &chunk_images[0]);

        video_dirty = false;
    }


    CopyPushConstants copy_push = {
        .src_img = chunk_images[0].desc_offset,
        .dst_img = chunk_images[0 ^ 1].desc_offset,
    };
    cmd_push(cmd, &device, sizeof(CopyPushConstants), &copy_push);
    cmd_bind_pipeline(cmd, &copy_pipeline);
    cmd_dispatch(cmd, (WINDOW_WIDTH/2 + 15) / 16, (WINDOW_HEIGHT/2 + 15) / 16, 1);
    cmd_memory_barrier(cmd, &chunk_images[0 ^ 1]);


    for (int i = 0; i < 2; i++) {
        CopyPushConstants copy_push = {
            .src_img = chunk_images[i].desc_offset,
            .dst_img = chunk_images[i ^ 1].desc_offset,
        };
        cmd_push(cmd, &device, sizeof(CopyPushConstants), &copy_push);
        cmd_bind_pipeline(cmd, &copy_pipeline);
        cmd_dispatch(cmd, (WINDOW_WIDTH/2 + 15) / 16, (WINDOW_HEIGHT/2 + 15) / 16, 1);
        cmd_memory_barrier(cmd, &chunk_images[i ^ 1]);
        SimPushConstants sim_push = {
            .src_img = chunk_images[i].desc_offset,
            .dst_img = chunk_images[i ^ 1].desc_offset,
            .offset_x = i ? 0 : -1,
            .offset_y = i ? 0 : -1,
            .frame_index = frame_index,
        };
        cmd_push(cmd, &device, sizeof(SimPushConstants), &sim_push);
        cmd_bind_pipeline(cmd, &sim_pipeline);
        uint32_t blocks_x = WINDOW_WIDTH / 2 / RESOLUTION + 1;
        uint32_t blocks_y = WINDOW_HEIGHT / 2 / RESOLUTION + 1;
        cmd_dispatch(cmd, (blocks_x + 15) / 16, (blocks_y + 15) / 16, 1);
        cmd_memory_barrier(cmd, &chunk_images[i ^ 1]);
    }


    cmd_transition_image(cmd, &draw_image, VK_IMAGE_LAYOUT_GENERAL);
    DrawPushConstants draw_push = {
        .draw_image_index = draw_image.desc_offset,
        .chunk_index = chunk_images[1].desc_offset,
        .chunk_prev_index = chunk_images[0].desc_offset,
    };
    cmd_push(cmd, &device, sizeof(DrawPushConstants), &draw_push);
    cmd_bind_pipeline(cmd, &draw_pipeline);
    cmd_dispatch(cmd, (WINDOW_WIDTH + 15) / 16, (WINDOW_HEIGHT + 15) / 16, 1);

    /// Copy to swapchain:
    cmd_transition_image(cmd, &draw_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    cmd_transition_image(cmd, swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    cmd_blit_image(cmd, &draw_image, swapchain_image);


    cmd_transition_image(cmd, swapchain_image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    end_command_buffer(cmd);

    submit_command_buffer(cmd, device.queue, swapchain_semaphore, render_semaphore, fence);

    present(&swapchain, render_semaphore, image_index, device.queue);

    frame_in_flight = (frame_in_flight + 1) % MAX_FRAMES_IN_FLIGHT;
    frame_index++;
}

void 
create_pipelines()
{
    VkShaderModule draw_module;
    if (!load_shader_module("build/shader/draw.comp.spv", &device, &draw_module)) return;
    if (!create_compute_pipeline(&device, draw_module, &draw_pipeline)) return;
    vkDestroyShaderModule(device.device, draw_module, NULL);

    VkShaderModule circle_brush_module;
    if (!load_shader_module("build/shader/circle_brush.comp.spv", &device, &circle_brush_module)) return;
    if (!create_compute_pipeline(&device, circle_brush_module, &circle_brush)) return;
    vkDestroyShaderModule(device.device, circle_brush_module, NULL);

    VkShaderModule sim_module;
    if (!load_shader_module("build/shader/sim.comp.spv", &device, &sim_module)) return;
    if (!create_compute_pipeline(&device, sim_module, &sim_pipeline)) return;
    vkDestroyShaderModule(device.device, sim_module, NULL);

    VkShaderModule copy_module;
    if (!load_shader_module("build/shader/copy.comp.spv", &device, &copy_module)) return;
    if (!create_compute_pipeline(&device, copy_module, &copy_pipeline)) return;
    vkDestroyShaderModule(device.device, copy_module, NULL);

    VkShaderModule vid_module;
    if (!load_shader_module("build/shader/video.comp.spv", &device, &vid_module)) return;
    if (!create_compute_pipeline(&device, vid_module, &video_pipeline)) return;
    vkDestroyShaderModule(device.device, vid_module, NULL);
}

int
main(int argc, char *argv[])
{
    /// bad apple setting
    if (argc == 2 && strcmp(argv[1], "-badapple") == 0) {
        bad_apple = true;
        if (ma_engine_init(NULL, &engine) != MA_SUCCESS) return 1;
        if (ma_sound_init_from_file(&engine, "assets/bad_apple.mp3",
                MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION,
                NULL, NULL, &bad_apple_sound) != MA_SUCCESS) return 1;

        plm = plm_create_with_filename("assets/bad_apple.mpg");
        if (!plm) return 1;
        plm_set_audio_enabled(plm, FALSE);   // audio comes from the mp3
        plm_set_video_decode_callback(plm, on_video, NULL);
    }

    game_arena = arena_create(sizeof(game_memory), game_memory);
    window = SDL_CreateWindow("falling sand", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN);
    SDL_Event event;

    create_instance(&(InstanceCreateInfo){INSTANCE_CREATE_INFO_DEFAULTS}, &game_arena, &instance);
    SDL_Vulkan_CreateSurface(window, instance.instance, NULL, &surface);

    if (!create_device(&instance, &(DeviceCreateInfo){ .for_window = window }, &game_arena, &device)) return 1;
    
    if(!create_swapchain(&device, 
            &(SwapchainCreateInfo){
            .surface = surface,
            .window = window,
            .vsync = true
        }, &swapchain)) 
    {
        return false;
    } 

    VkExtent3D draw_img_extent = {
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,
        .depth = 1
    };
	VkImageUsageFlags draw_image_usages = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT | 
                                          VK_IMAGE_USAGE_STORAGE_BIT      |
                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    draw_image = create_image(&device, 
        VK_FORMAT_R16G16B16A16_SFLOAT, 
        draw_image_usages, 
        draw_img_extent, 
        VK_IMAGE_ASPECT_COLOR_BIT
    );


    VkExtent3D chunk_img_extent = {
        .width = CHUNK_WIDTH,
        .height = CHUNK_HEIGHT,
        .depth = 1
    };

    VkImageUsageFlags chunk_image_usages = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT | 
                                          VK_IMAGE_USAGE_STORAGE_BIT      |
                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    chunk_images[0] = create_image(&device, 
        VK_FORMAT_R32_UINT, 
        chunk_image_usages, 
        chunk_img_extent, 
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    chunk_images[1] = create_image(&device, 
        VK_FORMAT_R32_UINT, 
        chunk_image_usages, 
        chunk_img_extent, 
        VK_IMAGE_ASPECT_COLOR_BIT
    );


    video_image = create_image(&device, VK_FORMAT_R32_UINT,
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
    chunk_img_extent, VK_IMAGE_ASPECT_COLOR_BIT);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        frames[i].cmd = allocate_command_buffer(&device, &(CommandBufferAllocateInfo){COMMAND_BUFFER_ALLOCATE_INFO_DEFAULTS});
        frames[i].swapchain_semaphore = create_semaphore(&device);
        frames[i].render_fence        = create_fence(&device);
        void* mem = arena_alloc(&game_arena, FRAME_ARENA_SIZE);
        if (!mem) return 1;
        frames[i].frame_arena = arena_create(FRAME_ARENA_SIZE, mem);

        frames[i].video_staging = create_staging_buffer(&device, CHUNK_WIDTH * CHUNK_HEIGHT * sizeof(uint32_t));
    }

    create_pipelines();
    next_edit.material = 1;


    if (bad_apple) {
        ma_sound_start(&bad_apple_sound);
    }
    while (running)
    {
        while(SDL_PollEvent(&event))
        {
            switch (event.type) 
            {
                case SDL_EVENT_KEY_UP: {
                    switch (event.key.scancode) {
                        case SDL_SCANCODE_0:{
                            next_edit.material = 0;
                            break;
                        }break;
                        case SDL_SCANCODE_1:{
                            next_edit.material = 1;
                            break;
                        }break;
                        case SDL_SCANCODE_2:{
                            next_edit.material = 2;
                            break;
                        }break;
                        case SDL_SCANCODE_3:{
                            next_edit.material = 3;
                            break;
                        }break;
                        case SDL_SCANCODE_4:{
                            next_edit.material = 4;
                            break;
                        }break;
                        case SDL_SCANCODE_5:{
                            next_edit.material = 5;
                            break;
                        }break;
                        case SDL_SCANCODE_6:{
                            next_edit.material = 6;
                            break;
                        }break;
                        default:{}break;
                    }
                }break;
                case SDL_EVENT_QUIT: {
                    running = false;
                }break;
                case SDL_EVENT_MOUSE_WHEEL: {
                    brush_radius+= event.wheel.y*4;
                    if(event.wheel.y > 0)
                    {
                        if (brush_radius >= MAX_BRUSH_RADIUS) {
                            brush_radius = MAX_BRUSH_RADIUS;
                        }
                    }else {
                        if (brush_radius < MIN_BRUSH_RADIUS) {
                            brush_radius = MIN_BRUSH_RADIUS;
                        }
                    }
                }break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                    draw_data.drawing = true;
                    next_edit.brush_radius = brush_radius < 0 ? 1 : brush_radius;
                    next_edit.x = event.button.x;
                    next_edit.y = event.button.y;
                } break;
                case SDL_EVENT_MOUSE_BUTTON_UP: {
                    draw_data.drawing = false;
                } break;
                case SDL_EVENT_MOUSE_MOTION: {
                    if (draw_data.drawing) {
                        next_edit.x = event.motion.x;
                        next_edit.y = event.motion.y;
                    }
                } break;
            }
        }

        if (bad_apple) {
            float t;
            /// Video is linked to the audio time for simplicity
            ma_sound_get_cursor_in_seconds(&bad_apple_sound, &t); 
            if (t > video_clock) {
                plm_decode(plm, t - video_clock);
                video_clock = t;
            }
        }

        render_frame();
    }

    destroy_swapchain(&device, &swapchain);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        destroy_semaphore(&device, &frames[i].swapchain_semaphore);
        destroy_fence(&device, &frames[i].render_fence);
    }

    vkDestroySurfaceKHR(instance.instance, surface, NULL);
    free_instance(&instance);
    SDL_DestroyWindow(window);
}