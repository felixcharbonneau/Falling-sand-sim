#ifndef COMMON_H
#define COMMON_H

#define AIR       0u
#define SAND      1u
#define WATER     2u
#define ROCK      3u
#define OBSIDIAN  4u
#define LAVA      5u
#define SMOKE     6u
#define MAT_COUNT 7
#define SHADES    4

const vec3 g_palette[MAT_COUNT * SHADES] = vec3[](
    // air (flat)
    vec3(1.0,1.0,1.0), vec3(1.0,1.0,1.0), vec3(1.0,1.0,1.0), vec3(1.0,1.0,1.0),
    // sand — darker shades shift browner, not just dimmer
    vec3(0.90,0.84,0.66), vec3(0.82,0.75,0.57), vec3(0.72,0.63,0.45), vec3(0.62,0.52,0.36),
    // water — tightened range, reads as one flat body
    vec3(0.28,0.58,0.88), vec3(0.25,0.54,0.85), vec3(0.22,0.50,0.81), vec3(0.19,0.46,0.78),
    // rock — darker overall
    vec3(0.40,0.38,0.37), vec3(0.33,0.32,0.31), vec3(0.27,0.26,0.25), vec3(0.21,0.20,0.20),
    // obsidian
    vec3(0.10,0.06,0.14), vec3(0.07,0.04,0.11), vec3(0.05,0.02,0.09), vec3(0.03,0.01,0.06),
    // lava
    vec3(1.00,0.72,0.20), vec3(1.00,0.45,0.08), vec3(0.92,0.28,0.04), vec3(0.70,0.15,0.02),
    // smoke — wider range, lightest shades pale enough to read as wisps
    vec3(0.84,0.84,0.86), vec3(0.68,0.68,0.71), vec3(0.48,0.48,0.51), vec3(0.30,0.30,0.33)
);

uint material(uint c) { return c & 0xFFu; }
uint variant (uint c) { return (c >> 8) & 0xFFu; }
uint pack(uint mat, uint var) { return mat | (var << 8); }

bool 
is_powder(uint m)
{
    return material(m) == SAND;
}

bool
is_liquid(uint m)
{
    return material(m) == WATER || material(m) == LAVA;
}
bool
is_gas(uint m)
{
    return material(m) == SMOKE;
}

bool can_fall(uint m)   { return is_powder(m) || is_liquid(m); }

bool is_solid_m(uint m) { return material(m) != AIR; }

bool 
displaces(uint mover, uint target) {
    if (target == AIR) return true;
    if(mover != AIR && is_gas(target)) return true;
    return is_powder(mover) && is_liquid(target); // powder sink through water
}

uint 
pcg(uint v)
{
    uint state = v;
    v = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

uint
hash_rand(ivec2 value, uint frame_index)
{
    return pcg(frame_index ^ pcg(uint(value.x) ^ pcg(uint(value.y))));
}


void
set_cell(uint chunk, ivec2 coord, uint mat, uint frame_index)
{
    uint cell = 0u;
    if (mat != AIR) {
        cell = pack(mat, hash_rand(coord, frame_index) & 3u);
    }
    imageStore(g_images_u32[chunk], coord, uvec4(cell, 0, 0, 0));
}
#endif