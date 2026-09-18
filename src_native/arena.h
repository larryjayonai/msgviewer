#ifndef ARENA_H
#define ARENA_H

#include <windows.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MemoryArena {
    unsigned char* buffer;
    size_t capacity;
    size_t offset;
    size_t peakOffset;
} MemoryArena;

// Create an arena with specified reserved capacity (e.g. 8 MB)
MemoryArena* Arena_Create(size_t capacity);

// Allocate aligned memory from arena (bump allocation)
void* Arena_Alloc(MemoryArena* arena, size_t size);

// Allocate zero-initialized memory
void* Arena_AllocZero(MemoryArena* arena, size_t size);

// Duplicate ANSI string into arena
char* Arena_Strdup(MemoryArena* arena, const char* str);

// Duplicate Wide string into arena
wchar_t* Arena_Wcsdup(MemoryArena* arena, const wchar_t* str);

// Duplicate binary data into arena
void* Arena_Memdup(MemoryArena* arena, const void* data, size_t size);

// Reset arena offset to 0 (frees all allocations instantly, zero leaks!)
void Arena_Reset(MemoryArena* arena);

// Destroy arena and release virtual memory
void Arena_Destroy(MemoryArena* arena);

#ifdef __cplusplus
}
#endif

#endif // ARENA_H
