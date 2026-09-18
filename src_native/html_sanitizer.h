#ifndef HTML_SANITIZER_H
#define HTML_SANITIZER_H

#include <windows.h>
#include "arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MsgAttachmentInfo {
    const wchar_t* fileName;
    const wchar_t* contentId;
    const char* mimeType;
    const unsigned char* data;
    size_t dataSize;
    int isInline;
} MsgAttachmentInfo;

// Processes raw HTML:
// 1. Injects IE=Edge meta and offline email reset CSS
// 2. Strips <script>, <object>, <iframe>, on* handlers
// 3. Blocks remote images (http/https) and inserts offline placeholder boxes
// 4. Resolves CID and inline images to Base64 data: URIs
// 5. Returns clean, safe, fully offline HTML string in arena
char* Html_SanitizeAndResolve(
    MemoryArena* arena,
    const char* rawHtml,
    MsgAttachmentInfo* attachments,
    int attachmentCount,
    const wchar_t* tooltipText
);

// Encodes binary data to Base64 in arena
char* Base64_Encode(MemoryArena* arena, const unsigned char* data, size_t inputLen);

#ifdef __cplusplus
}
#endif

#endif // HTML_SANITIZER_H
