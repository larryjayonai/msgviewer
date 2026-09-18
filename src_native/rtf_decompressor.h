#ifndef RTF_DECOMPRESSOR_H
#define RTF_DECOMPRESSOR_H

#include <windows.h>
#include "arena.h"

#ifdef __cplusplus
extern "C" {
#endif

// Decompress LZFu compressed RTF into arena memory
unsigned char* Rtf_Decompress(MemoryArena* arena, const unsigned char* compressedBytes, size_t compLen, size_t* outLen);

// Extract encapsulated HTML (\fromhtml ... \htmltag) from RTF string into arena memory
char* Rtf_ExtractHtml(MemoryArena* arena, const char* rtfText);

#ifdef __cplusplus
}
#endif

#endif // RTF_DECOMPRESSOR_H
