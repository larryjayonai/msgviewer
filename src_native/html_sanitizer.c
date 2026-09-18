#include "html_sanitizer.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static const char s_base64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char* Base64_Encode(MemoryArena* arena, const unsigned char* data, size_t inputLen) {
    if (!arena || !data || inputLen == 0) return NULL;

    size_t outputLen = 4 * ((inputLen + 2) / 3);
    char* encoded = (char*)Arena_Alloc(arena, outputLen + 1);
    if (!encoded) return NULL;

    size_t i = 0, j = 0;
    while (i < inputLen) {
        unsigned int b0 = data[i++];
        unsigned int b1 = (i < inputLen) ? data[i++] : 0;
        unsigned int b2 = (i < inputLen) ? data[i++] : 0;

        unsigned int triple = (b0 << 16) | (b1 << 8) | b2;

        encoded[j++] = s_base64Table[(triple >> 18) & 0x3F];
        encoded[j++] = s_base64Table[(triple >> 12) & 0x3F];
        encoded[j++] = (i > inputLen + 1) ? '=' : s_base64Table[(triple >> 6) & 0x3F];
        encoded[j++] = (i > inputLen) ? '=' : s_base64Table[triple & 0x3F];
    }
    encoded[outputLen] = '\0';
    return encoded;
}

static int CaseInsensitiveMatch(const char* src, const char* target) {
    while (*target) {
        if (tolower((unsigned char)*src) != tolower((unsigned char)*target)) return 0;
        src++;
        target++;
    }
    return 1;
}

char* Html_SanitizeAndResolve(
    MemoryArena* arena,
    const char* rawHtml,
    MsgAttachmentInfo* attachments,
    int attachmentCount,
    const wchar_t* tooltipText
) {
    if (!arena) return NULL;
    if (!rawHtml) rawHtml = "";

    // Offline Reset CSS and Meta tag
    const char* resetCss =
        "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=Edge\">\n"
        "<style>\n"
        "body { font-family: 'Segoe UI', 'Malgun Gothic', sans-serif; font-size: 14px; line-height: 1.5; color: #222; margin: 12px; }\n"
        "table { border-collapse: collapse; }\n"
        "img { max-width: 100%; height: auto; }\n"
        ".offline-img-box { display: inline-block; padding: 4px 8px; margin: 2px; background: #f1f3f4; border: 1px dashed #dadce0; color: #5f6368; font-size: 12px; border-radius: 4px; }\n"
        "</style>\n";

    char tooltipUtf8[256] = "Remote Image (Blocked)";
    if (tooltipText) {
        WideCharToMultiByte(CP_UTF8, 0, tooltipText, -1, tooltipUtf8, sizeof(tooltipUtf8), NULL, NULL);
    }

    size_t rawLen = strlen(rawHtml);
    size_t estCap = rawLen + 16384;
    // Add extra space for base64 encoded inline images
    for (int a = 0; a < attachmentCount; a++) {
        if (attachments[a].dataSize > 0) {
            estCap += (attachments[a].dataSize * 4 / 3) + 256;
        }
    }

    char* out = (char*)Arena_Alloc(arena, estCap);
    if (!out) return NULL;

    size_t outIdx = 0;
    int headInserted = 0;

    const char* p = rawHtml;

    // If rawHtml doesn't contain <head>, inject resetCss right at start
    int hasHead = (strstr(rawHtml, "<head>") != NULL || strstr(rawHtml, "<HEAD>") != NULL || strstr(rawHtml, "<Head>") != NULL);
    if (!hasHead) {
        size_t cssLen = strlen(resetCss);
        memcpy(out + outIdx, resetCss, cssLen);
        outIdx += cssLen;
        headInserted = 1;
    }

    while (*p) {
        // Inject reset CSS after <head>
        if (!headInserted && CaseInsensitiveMatch(p, "<head>")) {
            memcpy(out + outIdx, p, 6);
            outIdx += 6;
            p += 6;
            size_t cssLen = strlen(resetCss);
            memcpy(out + outIdx, resetCss, cssLen);
            outIdx += cssLen;
            headInserted = 1;
            continue;
        }

        // 1. Strip dangerous tags: <script>, <object>, <iframe>, <embed>
        if (CaseInsensitiveMatch(p, "<script") ||
            CaseInsensitiveMatch(p, "<object") ||
            CaseInsensitiveMatch(p, "<iframe") ||
            CaseInsensitiveMatch(p, "<embed")) {
            // Find end of opening or closing tag
            const char* tagEnd = strchr(p, '>');
            if (tagEnd) {
                p = tagEnd + 1;
                continue;
            }
        }
        if (CaseInsensitiveMatch(p, "</script>") ||
            CaseInsensitiveMatch(p, "</object>") ||
            CaseInsensitiveMatch(p, "</iframe>") ||
            CaseInsensitiveMatch(p, "</embed>")) {
            p = strchr(p, '>') + 1;
            continue;
        }

        // 2. Strip inline event handlers: onload, onerror, onclick, etc.
        if (CaseInsensitiveMatch(p, "onload=") ||
            CaseInsensitiveMatch(p, "onerror=") ||
            CaseInsensitiveMatch(p, "onclick=") ||
            CaseInsensitiveMatch(p, "onmouseover=")) {
            const char* eq = strchr(p, '=');
            p = eq + 1;
            if (*p == '"' || *p == '\'') {
                char quote = *p++;
                while (*p && *p != quote) p++;
                if (*p == quote) p++;
            } else {
                while (*p && !isspace((unsigned char)*p) && *p != '>') p++;
            }
            continue;
        }

        // 3. Inspect <img> tags for Remote Image blocking and CID resolution
        if (CaseInsensitiveMatch(p, "<img")) {
            const char* imgEnd = strchr(p, '>');
            if (!imgEnd) {
                out[outIdx++] = *p++;
                continue;
            }

            // Check src= attribute inside this <img> tag
            const char* srcPos = NULL;
            const char* s = p + 4;
            while (s < imgEnd) {
                if (CaseInsensitiveMatch(s, "src=")) {
                    srcPos = s + 4;
                    break;
                }
                s++;
            }

            if (srcPos) {
                while (srcPos < imgEnd && isspace((unsigned char)*srcPos)) srcPos++;
                char quote = '\0';
                if (*srcPos == '"' || *srcPos == '\'') quote = *srcPos++;

                const char* srcValStart = srcPos;
                const char* srcValEnd = srcValStart;
                if (quote) {
                    while (srcValEnd < imgEnd && *srcValEnd != quote) srcValEnd++;
                } else {
                    while (srcValEnd < imgEnd && !isspace((unsigned char)*srcValEnd) && *srcValEnd != '>') srcValEnd++;
                }

                size_t valLen = srcValEnd - srcValStart;
                char srcVal[512] = {0};
                if (valLen < 511) {
                    memcpy(srcVal, srcValStart, valLen);
                    srcVal[valLen] = '\0';
                }

                // Check: Remote URL (http://, https://, //)
                if (CaseInsensitiveMatch(srcVal, "http://") ||
                    CaseInsensitiveMatch(srcVal, "https://") ||
                    CaseInsensitiveMatch(srcVal, "//")) {
                    // Replace with offline placeholder
                    char box[1024];
                    sprintf_s(box, sizeof(box),
                        "<span class=\"offline-img-box\" title=\"%s\">[ 🖼️ %s ]</span>",
                        srcVal, tooltipUtf8);
                    size_t boxLen = strlen(box);
                    memcpy(out + outIdx, box, boxLen);
                    outIdx += boxLen;

                    p = imgEnd + 1;
                    continue;
                }

                // Check: CID or inline image (cid:xxx or filename match)
                const char* cidKey = srcVal;
                if (CaseInsensitiveMatch(srcVal, "cid:")) cidKey = srcVal + 4;

                int matched = -1;
                for (int a = 0; a < attachmentCount; a++) {
                    if (attachments[a].data && attachments[a].dataSize > 0) {
                        char attCidUtf8[256] = {0};
                        char attFileUtf8[256] = {0};
                        if (attachments[a].contentId) {
                            WideCharToMultiByte(CP_UTF8, 0, attachments[a].contentId, -1, attCidUtf8, 256, NULL, NULL);
                        }
                        if (attachments[a].fileName) {
                            WideCharToMultiByte(CP_UTF8, 0, attachments[a].fileName, -1, attFileUtf8, 256, NULL, NULL);
                        }

                        if (attCidUtf8[0] && _stricmp(cidKey, attCidUtf8) == 0) {
                            matched = a;
                            break;
                        }
                        if (attFileUtf8[0] && _stricmp(cidKey, attFileUtf8) == 0) {
                            matched = a;
                            break;
                        }
                    }
                }

                if (matched >= 0) {
                    attachments[matched].isInline = 1;
                    char* b64 = Base64_Encode(arena, attachments[matched].data, attachments[matched].dataSize);
                    if (b64) {
                        const char* mime = attachments[matched].mimeType ? attachments[matched].mimeType : "image/png";
                        // Copy everything up to srcValStart
                        size_t prefixLen = srcValStart - p;
                        memcpy(out + outIdx, p, prefixLen);
                        outIdx += prefixLen;

                        // Insert "data:mime;base64,..."
                        int written = sprintf_s(out + outIdx, estCap - outIdx, "data:%s;base64,%s", mime, b64);
                        if (written > 0) outIdx += written;

                        // Advance p past srcValEnd
                        p = srcValEnd;
                        continue;
                    }
                }
            }
        }

        out[outIdx++] = *p++;
    }

    out[outIdx] = '\0';
    return out;
}
