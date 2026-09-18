#include "encoding.h"

wchar_t* Encoding_ToWideChar(MemoryArena* arena, const char* data, int length, UINT codePage) {
    if (!arena || !data || length == 0) return NULL;
    if (length < 0) length = (int)strlen(data);

    // If codePage is 0, use CP_ACP or detect
    if (codePage == 0) {
        codePage = Encoding_DetectCodePage((const unsigned char*)data, length);
    }

    int needed = MultiByteToWideChar(codePage, 0, data, length, NULL, 0);
    if (needed <= 0 && codePage != CP_ACP) {
        // Fallback to CP_ACP
        needed = MultiByteToWideChar(CP_ACP, 0, data, length, NULL, 0);
        codePage = CP_ACP;
    }

    if (needed <= 0) return NULL;

    wchar_t* wbuf = (wchar_t*)Arena_Alloc(arena, (needed + 1) * sizeof(wchar_t));
    if (!wbuf) return NULL;

    MultiByteToWideChar(codePage, 0, data, length, wbuf, needed);
    wbuf[needed] = L'\0';
    return wbuf;
}

UINT Encoding_DetectCodePage(const unsigned char* data, size_t length) {
    if (!data || length == 0) return CP_ACP;

    // 1. Check BOM
    if (length >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) return CP_UTF8;
    if (length >= 2 && data[0] == 0xFF && data[1] == 0xFE) return 1200; // UTF-16LE
    if (length >= 2 && data[0] == 0xFE && data[1] == 0xFF) return 1201; // UTF-16BE

    // 2. Check for valid UTF-8 sequences
    size_t i = 0;
    int utf8MultiByteCount = 0;
    int invalidUtf8 = 0;

    while (i < length) {
        unsigned char c = data[i];
        if (c < 0x80) {
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < length && (data[i + 1] & 0xC0) == 0x80) {
                utf8MultiByteCount++;
                i += 2;
            } else {
                invalidUtf8 = 1;
                break;
            }
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < length && (data[i + 1] & 0xC0) == 0x80 && (data[i + 2] & 0xC0) == 0x80) {
                utf8MultiByteCount++;
                i += 3;
            } else {
                invalidUtf8 = 1;
                break;
            }
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < length && (data[i + 1] & 0xC0) == 0x80 && (data[i + 2] & 0xC0) == 0x80 && (data[i + 3] & 0xC0) == 0x80) {
                utf8MultiByteCount++;
                i += 4;
            } else {
                invalidUtf8 = 1;
                break;
            }
        } else {
            invalidUtf8 = 1;
            break;
        }
    }

    if (!invalidUtf8 && utf8MultiByteCount > 0) {
        return CP_UTF8;
    }

    // Default to system ANSI code page
    return CP_ACP;
}

char* Encoding_ToUtf8(MemoryArena* arena, const wchar_t* wstr) {
    if (!arena || !wstr) return NULL;
    int len = (int)wcslen(wstr);
    if (len == 0) return Arena_Strdup(arena, "");

    int needed = WideCharToMultiByte(CP_UTF8, 0, wstr, len, NULL, 0, NULL, NULL);
    if (needed <= 0) return NULL;

    char* ubuf = (char*)Arena_Alloc(arena, needed + 1);
    if (!ubuf) return NULL;

    WideCharToMultiByte(CP_UTF8, 0, wstr, len, ubuf, needed, NULL, NULL);
    ubuf[needed] = '\0';
    return ubuf;
}
