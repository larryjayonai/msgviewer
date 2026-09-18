#include "rtf_to_html.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_RTF_COLORS 64

typedef struct {
    unsigned char r, g, b;
} RtfColor;

typedef struct {
    char* buf;
    size_t cap;
    size_t len;
    MemoryArena* arena;
} HtmlBuffer;

static void HtmlBuf_Init(HtmlBuffer* hb, MemoryArena* arena, size_t initialCap) {
    hb->arena = arena;
    hb->cap = initialCap > 1024 ? initialCap : 1024;
    hb->buf = (char*)Arena_Alloc(arena, hb->cap);
    hb->len = 0;
    if (hb->buf) hb->buf[0] = '\0';
}

static void HtmlBuf_Append(HtmlBuffer* hb, const char* str) {
    if (!hb || !hb->buf || !str) return;
    size_t slen = strlen(str);
    if (hb->len + slen + 1 > hb->cap) {
        size_t newCap = (hb->cap * 2) + slen + 1024;
        char* newBuf = (char*)Arena_Alloc(hb->arena, newCap);
        if (!newBuf) return;
        memcpy(newBuf, hb->buf, hb->len);
        hb->buf = newBuf;
        hb->cap = newCap;
    }
    memcpy(hb->buf + hb->len, str, slen);
    hb->len += slen;
    hb->buf[hb->len] = '\0';
}

static void HtmlBuf_AppendChar(HtmlBuffer* hb, char c) {
    char s[2] = { c, '\0' };
    HtmlBuf_Append(hb, s);
}

static void HtmlBuf_AppendUtf8(HtmlBuffer* hb, unsigned int cp) {
    char u[5] = {0};
    if (cp <= 0x7F) {
        u[0] = (char)cp;
    } else if (cp <= 0x7FF) {
        u[0] = (char)(0xC0 | (cp >> 6));
        u[1] = (char)(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        u[0] = (char)(0xE0 | (cp >> 12));
        u[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        u[2] = (char)(0x80 | (cp & 0x3F));
    } else {
        u[0] = (char)(0xF0 | (cp >> 18));
        u[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        u[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        u[3] = (char)(0x80 | (cp & 0x3F));
    }
    HtmlBuf_Append(hb, u);
}

char* Rtf_ConvertToHtml(MemoryArena* arena, const char* rtfText) {
    if (!arena || !rtfText) return NULL;

    RtfColor colorTbl[MAX_RTF_COLORS];
    int colorCount = 0;
    ZeroMemory(colorTbl, sizeof(colorTbl));

    // Parse colortbl if present
    const char* ctblPos = strstr(rtfText, "{\\colortbl");
    if (ctblPos) {
        const char* cp = ctblPos + 10;
        while (*cp && *cp != '}' && colorCount < MAX_RTF_COLORS) {
            if (*cp == ';') {
                colorCount++;
                cp++;
                continue;
            }
            if (strncmp(cp, "\\red", 4) == 0) {
                int r = 0, g = 0, b = 0;
                sscanf_s(cp, "\\red%d\\green%d\\blue%d;", &r, &g, &b);
                if (colorCount < MAX_RTF_COLORS) {
                    colorTbl[colorCount].r = (unsigned char)r;
                    colorTbl[colorCount].g = (unsigned char)g;
                    colorTbl[colorCount].b = (unsigned char)b;
                }
                while (*cp && *cp != ';') cp++;
                if (*cp == ';') {
                    colorCount++;
                    cp++;
                }
            } else {
                cp++;
            }
        }
    }

    HtmlBuffer hb;
    HtmlBuf_Init(&hb, arena, strlen(rtfText) * 2 + 1024);

    HtmlBuf_Append(&hb, "<!DOCTYPE html><html><head><meta charset=\"utf-8\">");
    HtmlBuf_Append(&hb, "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=Edge\">");
    HtmlBuf_Append(&hb, "<style>body{font-family:'Segoe UI','Malgun Gothic',sans-serif;font-size:14px;line-height:1.5;color:#222;margin:16px;}");
    HtmlBuf_Append(&hb, "table{border-collapse:collapse;margin:8px 0;}td,th{border:1px solid #ccc;padding:6px 10px;}</style></head><body>");

    const char* p = rtfText;
    int groupDepth = 0;
    int skipGroupDepth = 0;
    int bold = 0, italic = 0, underline = 0;
    int inTable = 0, inRow = 0, inCell = 0;
    int currentColor = -1;

    while (*p) {
        if (*p == '{') {
            groupDepth++;
            p++;
            // Check destination groups to skip
            if (*p == '\\' && *(p + 1) == '*') {
                skipGroupDepth = groupDepth;
            } else if (strncmp(p, "\\fonttbl", 8) == 0 ||
                       strncmp(p, "\\colortbl", 9) == 0 ||
                       strncmp(p, "\\stylesheet", 11) == 0 ||
                       strncmp(p, "\\info", 5) == 0) {
                skipGroupDepth = groupDepth;
            }
            continue;
        }

        if (*p == '}') {
            if (skipGroupDepth == groupDepth) {
                skipGroupDepth = 0;
            }
            groupDepth--;
            p++;
            continue;
        }

        if (skipGroupDepth > 0) {
            p++;
            continue;
        }

        if (*p == '\\') {
            p++; // skip '\\'
            if (*p == '\0') break;

            if (*p == '\\' || *p == '{' || *p == '}') {
                HtmlBuf_AppendChar(&hb, *p);
                p++;
                continue;
            }

            if (*p == '\'') {
                // Hex byte \'hh
                p++;
                if (isxdigit((unsigned char)*p) && isxdigit((unsigned char)*(p + 1))) {
                    char hex[3] = { *p, *(p + 1), '\0' };
                    unsigned int byteVal = 0;
                    sscanf_s(hex, "%x", &byteVal);
                    if (byteVal == '<') HtmlBuf_Append(&hb, "&lt;");
                    else if (byteVal == '>') HtmlBuf_Append(&hb, "&gt;");
                    else if (byteVal == '&') HtmlBuf_Append(&hb, "&amp;");
                    else HtmlBuf_AppendChar(&hb, (char)byteVal);
                    p += 2;
                }
                continue;
            }

            // Extract control word
            char word[64] = {0};
            int wlen = 0;
            while (*p && isalpha((unsigned char)*p) && wlen < 63) {
                word[wlen++] = *p++;
            }
            word[wlen] = '\0';

            // Optional param
            int hasParam = 0;
            int param = 0;
            if (*p == '-' || isdigit((unsigned char)*p)) {
                hasParam = 1;
                param = (int)strtol(p, (char**)&p, 10);
            }
            if (*p == ' ') p++; // optional delimiter space

            // Handle control words
            if (strcmp(word, "par") == 0 || strcmp(word, "line") == 0) {
                HtmlBuf_Append(&hb, "<br>");
            } else if (strcmp(word, "tab") == 0) {
                HtmlBuf_Append(&hb, "&nbsp;&nbsp;&nbsp;&nbsp;");
            } else if (strcmp(word, "bullet") == 0) {
                HtmlBuf_Append(&hb, "&bull;&nbsp;");
            } else if (strcmp(word, "b") == 0) {
                int enable = (!hasParam || param != 0);
                if (enable && !bold) { HtmlBuf_Append(&hb, "<b>"); bold = 1; }
                else if (!enable && bold) { HtmlBuf_Append(&hb, "</b>"); bold = 0; }
            } else if (strcmp(word, "i") == 0) {
                int enable = (!hasParam || param != 0);
                if (enable && !italic) { HtmlBuf_Append(&hb, "<i>"); italic = 1; }
                else if (!enable && italic) { HtmlBuf_Append(&hb, "</i>"); italic = 0; }
            } else if (strcmp(word, "ul") == 0) {
                if (!underline) { HtmlBuf_Append(&hb, "<u>"); underline = 1; }
            } else if (strcmp(word, "ulnone") == 0) {
                if (underline) { HtmlBuf_Append(&hb, "</u>"); underline = 0; }
            } else if (strcmp(word, "cf") == 0 && hasParam) {
                if (currentColor >= 0) {
                    HtmlBuf_Append(&hb, "</span>");
                    currentColor = -1;
                }
                if (param > 0 && param < colorCount) {
                    char cstyle[64];
                    sprintf_s(cstyle, sizeof(cstyle), "<span style=\"color:#%02X%02X%02X;\">",
                              colorTbl[param].r, colorTbl[param].g, colorTbl[param].b);
                    HtmlBuf_Append(&hb, cstyle);
                    currentColor = param;
                }
            } else if (strcmp(word, "trowd") == 0) {
                if (!inTable) {
                    HtmlBuf_Append(&hb, "<table>");
                    inTable = 1;
                }
                if (!inRow) {
                    HtmlBuf_Append(&hb, "<tr>");
                    inRow = 1;
                }
                inCell = 0;
            } else if (strcmp(word, "cell") == 0) {
                if (inCell) HtmlBuf_Append(&hb, "</td>");
                inCell = 0;
            } else if (strcmp(word, "intbl") == 0) {
                if (!inCell) {
                    HtmlBuf_Append(&hb, "<td>");
                    inCell = 1;
                }
            } else if (strcmp(word, "row") == 0) {
                if (inCell) { HtmlBuf_Append(&hb, "</td>"); inCell = 0; }
                if (inRow) { HtmlBuf_Append(&hb, "</tr>"); inRow = 0; }
            } else if (strcmp(word, "u") == 0 && hasParam) {
                // Unicode character \uN
                unsigned int cp = (unsigned int)(param < 0 ? param + 65536 : param);
                HtmlBuf_AppendUtf8(&hb, cp);
                // RTF specifies a replacement char follows \uN; skip if it's '?'
                if (*p == '?') p++;
            }
            continue;
        }

        // Normal character
        if (*p == '<') HtmlBuf_Append(&hb, "&lt;");
        else if (*p == '>') HtmlBuf_Append(&hb, "&gt;");
        else if (*p == '&') HtmlBuf_Append(&hb, "&amp;");
        else if (*p == '\r' || *p == '\n') { /* ignore raw newlines in RTF */ }
        else HtmlBuf_AppendChar(&hb, *p);

        p++;
    }

    if (currentColor >= 0) HtmlBuf_Append(&hb, "</span>");
    if (bold) HtmlBuf_Append(&hb, "</b>");
    if (italic) HtmlBuf_Append(&hb, "</i>");
    if (underline) HtmlBuf_Append(&hb, "</u>");
    if (inCell) HtmlBuf_Append(&hb, "</td>");
    if (inRow) HtmlBuf_Append(&hb, "</tr>");
    if (inTable) HtmlBuf_Append(&hb, "</table>");

    HtmlBuf_Append(&hb, "</body></html>");
    return hb.buf;
}
