#pragma once
#include <stdint.h>


typedef struct Arena 
{
    void* data;
    uint64_t capacity, front;
} Arena;

Arena arena_create(uint64_t capacity, void* memory);
void* arena_alloc(Arena* arena, uint64_t size);