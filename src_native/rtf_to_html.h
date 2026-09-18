#ifndef RTF_TO_HTML_H
#define RTF_TO_HTML_H

#include <windows.h>
#include "arena.h"

#ifdef __cplusplus
extern "C" {
#endif

// Converts pure RTF (without \fromhtml) into well-formed HTML (tables, bold, italic, colors, lists)
char* Rtf_ConvertToHtml(MemoryArena* arena, const char* rtfText);

#ifdef __cplusplus
}
#endif

#endif // RTF_TO_HTML_H
