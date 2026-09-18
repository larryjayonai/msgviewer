#ifndef ENCODING_H
#define ENCODING_H

#include <windows.h>
#include "arena.h"

#ifdef __cplusplus
extern "C" {
#endif

// Convert a byte buffer with specified codepage to UTF-16 wchar_t string in arena
wchar_t* Encoding_ToWideChar(MemoryArena* arena, const char* data, int length, UINT codePage);

// Detect codepage from byte data if not specified (checks UTF-8, CJK, CP1252)
UINT Encoding_DetectCodePage(const unsigned char* data, size_t length);

// Convert wide string to UTF-8 char string in arena
char* Encoding_ToUtf8(MemoryArena* arena, const wchar_t* wstr);

#ifdef __cplusplus
}
#endif

#endif // ENCODING_H
