#include "arena.h"

Arena 
arena_create(uint64_t capacity, void* memory)
{
    return (Arena){
        .data = memory,
        .capacity = capacity,
        .front    = 0
    };
}

void* 
arena_alloc(Arena* arena, uint64_t size)
{
    if (arena->capacity - arena->front < size || !arena || !size) return 0;
    void* block = (char*)arena->data + arena->front;
    arena->front += size; 
    return block;
}