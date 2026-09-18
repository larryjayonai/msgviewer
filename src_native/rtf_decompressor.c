#include "rtf_decompressor.h"
#include <string.h>
#include <ctype.h>

static const char* s_rtfPrebuf =
    "{\\rtf1\\ansi\\mac\\deff0\\deflang1033{\\fonttbl{\\f0\\fnil\\fcharset0 Times New Roman;}"
    "{\\f1\\fnil\\fcharset0 Symbol;}{\\f2\\fswiss\\fcharset0 Arial;}}{\\colortbl;\\red0\\green0\\blue0;"
    "\\red0\\green0\\blue255;\\red0\\green255\\blue255;\\red0\\green255\\blue0;\\red255\\green0\\blue255;"
    "\\red255\\green0\\blue0;\\red255\\green255\\blue0;\\red255\\green255\\blue255;\\red128\\green128\\blue128;}"
    "{\\stylesheet{\\normal\\fi0\\li0\\ri0\\sa0\\sb0\\fs24\\cf0 Normal;}{\\*\\cs10\\fs20\\cf0 Default Paragraph Font;}}"
    "{\\info{\\version1\\edmins0\\nofpages1\\nofwords0\\nofchars0\\vern32454}}";

unsigned char* Rtf_Decompress(MemoryArena* arena, const unsigned char* compressedBytes, size_t compLen, size_t* outLen) {
    if (outLen) *outLen = 0;
    if (!arena || !compressedBytes || compLen < 16) return NULL;

    unsigned int compSize = *(unsigned int*)(compressedBytes);
    unsigned int uncompSize = *(unsigned int*)(compressedBytes + 4);
    unsigned int magic = *(unsigned int*)(compressedBytes + 8);
    // unsigned int crc = *(unsigned int*)(compressedBytes + 12);

    // Limit uncompressed size to reasonable bound (e.g. 32 MB) to prevent malicious bombs
    if (uncompSize > 32 * 1024 * 1024) uncompSize = 32 * 1024 * 1024;

    if (magic == 0x414d454c) { // "LZFu"
        unsigned char dictionary[4096];
        size_t prebufLen = strlen(s_rtfPrebuf);
        if (prebufLen > 4096) prebufLen = 4096;
        memcpy(dictionary, s_rtfPrebuf, prebufLen);
        size_t writeOffset = prebufLen;

        unsigned char* output = (unsigned char*)Arena_Alloc(arena, uncompSize + 1);
        if (!output) return NULL;

        size_t inIdx = 16;
        size_t outIdx = 0;

        while (inIdx < compLen && outIdx < uncompSize) {
            unsigned char controlByte = compressedBytes[inIdx++];

            for (int bit = 0; bit < 8 && outIdx < uncompSize; bit++) {
                int isLiteral = ((controlByte >> bit) & 1) == 1;
                if (isLiteral) {
                    if (inIdx >= compLen) break;
                    unsigned char b = compressedBytes[inIdx++];
                    output[outIdx++] = b;
                    dictionary[writeOffset] = b;
                    writeOffset = (writeOffset + 1) % 4096;
                } else {
                    if (inIdx + 1 >= compLen) break;
                    unsigned char b1 = compressedBytes[inIdx++];
                    unsigned char b2 = compressedBytes[inIdx++];

                    int offset = (b1 << 4) | (b2 >> 4);
                    int length = (b2 & 0x0F) + 2;

                    for (int i = 0; i < length && outIdx < uncompSize; i++) {
                        unsigned char b = dictionary[(offset + i) % 4096];
                        output[outIdx++] = b;
                        dictionary[writeOffset] = b;
                        writeOffset = (writeOffset + 1) % 4096;
                    }
                }
            }
        }

        output[outIdx] = '\0';
        if (outLen) *outLen = outIdx;
        return output;
    } else if (magic == 0x75465a4d) { // "MZFu" uncompressed
        size_t copyLen = uncompSize;
        if (copyLen > compLen - 16) copyLen = compLen - 16;

        unsigned char* output = (unsigned char*)Arena_Alloc(arena, copyLen + 1);
        if (!output) return NULL;
        memcpy(output, compressedBytes + 16, copyLen);
        output[copyLen] = '\0';
        if (outLen) *outLen = copyLen;
        return output;
    }

    // Unknown magic, duplicate input
    unsigned char* copy = (unsigned char*)Arena_Memdup(arena, compressedBytes, compLen);
    if (outLen) *outLen = compLen;
    return copy;
}

char* Rtf_ExtractHtml(MemoryArena* arena, const char* rtfText) {
    if (!arena || !rtfText) return NULL;
    if (!strstr(rtfText, "\\fromhtml")) return NULL;

    size_t rtfLen = strlen(rtfText);
    char* htmlBuffer = (char*)Arena_Alloc(arena, rtfLen + 1);
    if (!htmlBuffer) return NULL;

    size_t htmlIdx = 0;
    const char* p = rtfText;

    while (*p) {
        const char* tagPos = strstr(p, "\\htmltag");
        if (!tagPos) break;

        p = tagPos + 8; // skip "\\htmltag"
        while (*p && isdigit((unsigned char)*p)) p++;
        if (*p == ' ') p++;

        const char* endTag = p;
        while (*endTag && *endTag != '\\' && *endTag != '}' && *endTag != '{') {
            endTag++;
        }

        size_t tagLen = endTag - p;
        if (tagLen > 0) {
            memcpy(htmlBuffer + htmlIdx, p, tagLen);
            htmlIdx += tagLen;
        }
        p = endTag;
    }

    if (htmlIdx == 0) return NULL;

    htmlBuffer[htmlIdx] = '\0';
    return htmlBuffer;
}
