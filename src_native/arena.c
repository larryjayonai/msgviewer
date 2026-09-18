#include "arena.h"
#include <string.h>

#define ARENA_ALIGNMENT 8

MemoryArena* Arena_Create(size_t capacity) {
    if (capacity == 0) capacity = 8 * 1024 * 1024; // Default 8 MB

    // Allocate arena struct on standard heap
    MemoryArena* arena = (MemoryArena*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MemoryArena));
    if (!arena) return NULL;

    // Reserve and commit virtual memory block
    arena->buffer = (unsigned char*)VirtualAlloc(NULL, capacity, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!arena->buffer) {
        HeapFree(GetProcessHeap(), 0, arena);
        return NULL;
    }

    arena->capacity = capacity;
    arena->offset = 0;
    arena->peakOffset = 0;
    return arena;
}

void* Arena_Alloc(MemoryArena* arena, size_t size) {
    if (!arena || !arena->buffer || size == 0) return NULL;

    // Align size up to 8 bytes
    size_t aligned = (size + (ARENA_ALIGNMENT - 1)) & ~(ARENA_ALIGNMENT - 1);

    if (arena->offset + aligned > arena->capacity) {
        // Exceeded arena capacity - fail gracefully
        return NULL;
    }

    void* ptr = arena->buffer + arena->offset;
    arena->offset += aligned;
    if (arena->offset > arena->peakOffset) {
        arena->peakOffset = arena->offset;
    }
    return ptr;
}

void* Arena_AllocZero(MemoryArena* arena, size_t size) {
    void* ptr = Arena_Alloc(arena, size);
    if (ptr) {
        ZeroMemory(ptr, size);
    }
    return ptr;
}

char* Arena_Strdup(MemoryArena* arena, const char* str) {
    if (!arena || !str) return NULL;
    size_t len = strlen(str) + 1;
    char* copy = (char*)Arena_Alloc(arena, len);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

wchar_t* Arena_Wcsdup(MemoryArena* arena, const wchar_t* str) {
    if (!arena || !str) return NULL;
    size_t len = (wcslen(str) + 1) * sizeof(wchar_t);
    wchar_t* copy = (wchar_t*)Arena_Alloc(arena, len);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

void* Arena_Memdup(MemoryArena* arena, const void* data, size_t size) {
    if (!arena || !data || size == 0) return NULL;
    void* copy = Arena_Alloc(arena, size);
    if (copy) {
        memcpy(copy, data, size);
    }
    return copy;
}

void Arena_Reset(MemoryArena* arena) {
    if (!arena) return;
    // Reset offset to zero; all allocations are invalidated in 0 nanoseconds!
    arena->offset = 0;
}

void Arena_Destroy(MemoryArena* arena) {
    if (!arena) return;
    if (arena->buffer) {
        VirtualFree(arena->buffer, 0, MEM_RELEASE);
        arena->buffer = NULL;
    }
    HeapFree(GetProcessHeap(), 0, arena);
}
