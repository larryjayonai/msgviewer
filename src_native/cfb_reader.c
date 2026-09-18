#include "cfb_reader.h"
#include "encoding.h"
#include "rtf_decompressor.h"
#include "rtf_to_html.h"
#include "html_sanitizer.h"
#include <stdio.h>
#include <string.h>
#include <strsafe.h>

#define CFB_MAGIC 0xE11AB1A1E011CFD0ULL // 0xD0CF11E0A1B11AE1 in little endian
#define MAX_DIR_ENTRIES 1024

#pragma pack(push, 1)
typedef struct {
    unsigned char  abSig[8];            // 0xD0CF11E0A1B11AE1
    unsigned char  clsid[16];
    unsigned short uMinorVersion;
    unsigned short uDllVersion;         // 3 = 512, 4 = 4096
    unsigned short uByteOrder;          // 0xFFFE
    unsigned short uSectorShift;        // 9 for 512, 12 for 4096
    unsigned short uMiniSectorShift;    // 6 for 64
    unsigned short usReserved;
    unsigned int   ulReserved1;
    unsigned int   csectDir;
    unsigned int   csectFat;            // Number of FAT sectors
    unsigned int   sectDirStart;        // Starting sector of Directory
    unsigned int   ulSignature;
    unsigned int   ulMiniSectorCutoff;  // 4096
    unsigned int   sectMiniFatStart;    // Starting sector of MiniFAT
    unsigned int   csectMiniFat;
    unsigned int   sectDifStart;
    unsigned int   csectDif;
    unsigned int   difat[109];          // First 109 FAT sector IDs
} CfbHeader;

typedef struct {
    wchar_t        ab[32];              // UTF-16 name
    unsigned short cb;                  // Name length in bytes including \0
    unsigned char  mse;                 // Object type: 1 = Storage, 2 = Stream, 5 = Root
    unsigned char  bflags;
    unsigned int   sidLeftSib;
    unsigned int   sidRightSib;
    unsigned int   sidChild;
    unsigned char  clsId[16];
    unsigned int   dwUserFlags;
    FILETIME       timeCreated;
    FILETIME       timeModified;
    unsigned int   sectStart;           // Starting sector ID
    unsigned int   ulSizeLow;           // Stream size (low 32-bits)
    unsigned int   ulSizeHigh;          // Stream size (high 32-bits)
} CfbDirEntry;
#pragma pack(pop)

typedef struct {
    const unsigned char* view;
    size_t fileSize;
    unsigned int sectorSize;
    unsigned int miniSectorSize;
    unsigned int miniSectorCutoff;
    unsigned int* fatTable;
    unsigned int fatEntryCount;
    unsigned int* miniFatTable;
    unsigned int miniFatEntryCount;
    const unsigned char* miniStream;
    size_t miniStreamSize;
    CfbDirEntry* dirEntries;
    int dirEntryCount;
    MemoryArena* arena;
} CfbContext;

static const unsigned char* GetSectorPtr(CfbContext* ctx, unsigned int sectId) {
    if (sectId >= 0xFFFFFFFC) return NULL;
    size_t offset = (size_t)(sectId + 1) * ctx->sectorSize;
    if (offset + ctx->sectorSize > ctx->fileSize) return NULL;
    return ctx->view + offset;
}

static unsigned char* ReadStreamBytes(CfbContext* ctx, unsigned int startSect, size_t size) {
    if (size == 0) return NULL;
    unsigned char* buf = (unsigned char*)Arena_Alloc(ctx->arena, size + 2);
    if (!buf) return NULL;

    if (size < ctx->miniSectorCutoff && ctx->miniStream) {
        // Read from MiniStream
        size_t bytesRead = 0;
        unsigned int currSect = startSect;
        while (bytesRead < size && currSect < ctx->miniFatEntryCount && currSect < 0xFFFFFFFC) {
            size_t miniOffset = (size_t)currSect * ctx->miniSectorSize;
            if (miniOffset >= ctx->miniStreamSize) break;
            size_t toCopy = ctx->miniSectorSize;
            if (bytesRead + toCopy > size) toCopy = size - bytesRead;
            memcpy(buf + bytesRead, ctx->miniStream + miniOffset, toCopy);
            bytesRead += toCopy;
            currSect = ctx->miniFatTable[currSect];
        }
    } else {
        // Read from regular FAT sectors
        size_t bytesRead = 0;
        unsigned int currSect = startSect;
        while (bytesRead < size && currSect < ctx->fatEntryCount && currSect < 0xFFFFFFFC) {
            const unsigned char* sptr = GetSectorPtr(ctx, currSect);
            if (!sptr) break;
            size_t toCopy = ctx->sectorSize;
            if (bytesRead + toCopy > size) toCopy = size - bytesRead;
            memcpy(buf + bytesRead, sptr, toCopy);
            bytesRead += toCopy;
            currSect = ctx->fatTable[currSect];
        }
    }

    buf[size] = '\0';
    buf[size + 1] = '\0';
    return buf;
}

static void FormatFileTime(FILETIME ft, wchar_t* outBuf, size_t bufSize) {
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    StringCchPrintfW(outBuf, bufSize, L"%04d-%02d-%02d %02d:%02d:%02d",
        stLocal.wYear, stLocal.wMonth, stLocal.wDay,
        stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
}

int Cfb_ReadMsg(const wchar_t* filePath, MemoryArena* arena, NativeMsg* outMsg, ErrorDiagnostic* diag) {
    if (!filePath || !arena || !outMsg) return 0;
    ZeroMemory(outMsg, sizeof(NativeMsg));

    // 1단계: 파일 열기 및 I/O
    ErrorDiag_Init(diag, ERROR_STAGE_IO, filePath, 0);

    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        ErrorDiag_Init(diag, ERROR_STAGE_IO, filePath, err);
        ErrorDiag_SetDetail(diag, L"파일 열기 실패",
            err == ERROR_FILE_NOT_FOUND ? L"지정된 파일을 찾을 수 없습니다." :
            err == ERROR_ACCESS_DENIED ? L"파일 접근 권한이 거부되었습니다." :
            err == ERROR_SHARING_VIOLATION ? L"다른 프로그램에서 파일을 사용 중입니다." :
            L"파일 핸들을 생성할 수 없습니다.",
            L"파일이 올바른 위치에 있고 다른 프로그램에서 열려있지 않은지 확인해 주세요.");
        return 0;
    }

    LARGE_INTEGER liSize;
    if (!GetFileSizeEx(hFile, &liSize) || liSize.QuadPart < 512) {
        CloseHandle(hFile);
        ErrorDiag_Init(diag, ERROR_STAGE_IO, filePath, GetLastError());
        ErrorDiag_SetDetail(diag, L"파일 크기 오류",
            L"파일 크기가 512바이트 미만이거나 0바이트입니다.",
            L"파일 내용이 비어있지 않은지 확인해 주세요.");
        return 0;
    }

    // Memory mapping (backed by the disk file itself, works with 0MB pagefile!)
    HANDLE hMapping = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) {
        CloseHandle(hFile);
        ErrorDiag_Init(diag, ERROR_STAGE_IO, filePath, GetLastError());
        ErrorDiag_SetDetail(diag, L"메모리 매핑 실패",
            L"CreateFileMappingW 생성에 실패했습니다.",
            L"시스템 메모리 리소스 상태를 확인해 주세요.");
        return 0;
    }

    const unsigned char* view = (const unsigned char*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!view) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        ErrorDiag_Init(diag, ERROR_STAGE_IO, filePath, GetLastError());
        ErrorDiag_SetDetail(diag, L"가상 주소 매핑 실패",
            L"MapViewOfFile 매핑에 실패했습니다.",
            L"32비트 가상 주소 공간이 부족할 수 있습니다.");
        return 0;
    }

    outMsg->hFile = hFile;
    outMsg->hMapping = hMapping;
    outMsg->fileView = view;
    outMsg->fileSize = (size_t)liSize.QuadPart;

    // 2단계: OLE 복합 파일(CFB) 구조 분석
    ErrorDiag_Init(diag, ERROR_STAGE_CFB, filePath, 0);

    const CfbHeader* hdr = (const CfbHeader*)view;
    if (*(const unsigned __int64*)hdr->abSig != CFB_MAGIC) {
        ErrorDiag_SetDetail(diag, L"OLE 복합 파일 시그니처 불일치",
            L"파일 헤더 시그니처가 OLE CFB 매직(0xD0CF11E0)과 일치하지 않습니다.",
            L"선택한 파일이 올바른 Outlook .msg 파일인지 확인해 주세요.");
        Cfb_CloseMsg(outMsg);
        return 0;
    }

    CfbContext ctx;
    ZeroMemory(&ctx, sizeof(CfbContext));
    ctx.view = view;
    ctx.fileSize = outMsg->fileSize;
    ctx.arena = arena;
    ctx.sectorSize = 1 << hdr->uSectorShift;
    ctx.miniSectorSize = 1 << hdr->uMiniSectorShift;
    ctx.miniSectorCutoff = hdr->ulMiniSectorCutoff;

    // Build FAT Table
    unsigned int fatSectorsTotal = hdr->csectFat;
    ctx.fatEntryCount = fatSectorsTotal * (ctx.sectorSize / sizeof(unsigned int));
    ctx.fatTable = (unsigned int*)Arena_Alloc(arena, ctx.fatEntryCount * sizeof(unsigned int));
    if (!ctx.fatTable) {
        ErrorDiag_SetDetail(diag, L"메모리 할당 실패", L"FAT 테이블 적재를 위한 메모리가 부족합니다.", L"시스템 RAM을 확보해 주세요.");
        Cfb_CloseMsg(outMsg);
        return 0;
    }

    unsigned int fatLoaded = 0;
    for (unsigned int i = 0; i < 109 && i < fatSectorsTotal; i++) {
        unsigned int sId = hdr->difat[i];
        const unsigned char* sptr = GetSectorPtr(&ctx, sId);
        if (sptr) {
            unsigned int entriesPerSect = ctx.sectorSize / sizeof(unsigned int);
            memcpy(ctx.fatTable + fatLoaded, sptr, ctx.sectorSize);
            fatLoaded += entriesPerSect;
        }
    }

    // Read Directory Entries
    ctx.dirEntries = (CfbDirEntry*)Arena_Alloc(arena, MAX_DIR_ENTRIES * sizeof(CfbDirEntry));
    ctx.dirEntryCount = 0;

    unsigned int dirSect = hdr->sectDirStart;
    while (dirSect < ctx.fatEntryCount && dirSect < 0xFFFFFFFC && ctx.dirEntryCount < MAX_DIR_ENTRIES) {
        const unsigned char* sptr = GetSectorPtr(&ctx, dirSect);
        if (!sptr) break;

        int entriesInSect = ctx.sectorSize / 128;
        for (int e = 0; e < entriesInSect && ctx.dirEntryCount < MAX_DIR_ENTRIES; e++) {
            const CfbDirEntry* de = (const CfbDirEntry*)(sptr + (e * 128));
            if (de->mse != 0) { // Not empty entry
                ctx.dirEntries[ctx.dirEntryCount++] = *de;
            }
        }
        dirSect = ctx.fatTable[dirSect];
    }

    if (ctx.dirEntryCount == 0) {
        ErrorDiag_SetDetail(diag, L"디렉터리 스트림 손상", L"CFB 디렉터리 항목을 읽을 수 없습니다.", L"파일이 손상되었습니다.");
        Cfb_CloseMsg(outMsg);
        return 0;
    }

    // Find Root Entry (entry type 5)
    CfbDirEntry* root = NULL;
    for (int d = 0; d < ctx.dirEntryCount; d++) {
        if (ctx.dirEntries[d].mse == 5) {
            root = &ctx.dirEntries[d];
            break;
        }
    }

    // Read MiniStream from Root entry if present
    if (root && root->ulSizeLow > 0) {
        ctx.miniStreamSize = root->ulSizeLow;
        ctx.miniStream = ReadStreamBytes(&ctx, root->sectStart, root->ulSizeLow);

        // Read MiniFAT
        if (hdr->csectMiniFat > 0) {
            ctx.miniFatEntryCount = hdr->csectMiniFat * (ctx.sectorSize / sizeof(unsigned int));
            ctx.miniFatTable = (unsigned int*)Arena_Alloc(arena, ctx.miniFatEntryCount * sizeof(unsigned int));
            if (ctx.miniFatTable) {
                unsigned int mfLoaded = 0;
                unsigned int mfSect = hdr->sectMiniFatStart;
                while (mfSect < ctx.fatEntryCount && mfSect < 0xFFFFFFFC && mfLoaded < ctx.miniFatEntryCount) {
                    const unsigned char* sptr = GetSectorPtr(&ctx, mfSect);
                    if (!sptr) break;
                    unsigned int count = ctx.sectorSize / sizeof(unsigned int);
                    memcpy(ctx.miniFatTable + mfLoaded, sptr, ctx.sectorSize);
                    mfLoaded += count;
                    mfSect = ctx.fatTable[mfSect];
                }
            }
        }
    }

    // 3단계: MAPI 속성 및 본문 추출
    ErrorDiag_Init(diag, ERROR_STAGE_MAPI, filePath, 0);

    for (int d = 0; d < ctx.dirEntryCount; d++) {
        CfbDirEntry* de = &ctx.dirEntries[d];
        if (de->mse != 2) continue; // Only Streams

        // Match Top-level streams: "__substg1.0_PPPPVVVV"
        if (wcsncmp(de->ab, L"__substg1.0_", 12) == 0) {
            wchar_t tag[9] = {0};
            wcsncpy_s(tag, 9, de->ab + 12, 8);
            wchar_t propId[5] = {0};
            wchar_t propType[5] = {0};
            wcsncpy_s(propId, 5, tag, 4);
            wcsncpy_s(propType, 5, tag + 4, 4);

            size_t streamSize = de->ulSizeLow;
            unsigned char* sdata = ReadStreamBytes(&ctx, de->sectStart, streamSize);
            if (!sdata) continue;

            wchar_t* wtext = NULL;
            if (_wcsicmp(propType, L"001F") == 0) { // Unicode
                wtext = (wchar_t*)sdata;
            } else if (_wcsicmp(propType, L"001E") == 0) { // ANSI
                wtext = Encoding_ToWideChar(arena, (const char*)sdata, (int)streamSize, outMsg->codePage);
            }

            if (_wcsicmp(propId, L"0037") == 0 && !outMsg->subject) { // Subject
                outMsg->subject = wtext ? Arena_Wcsdup(arena, wtext) : NULL;
            } else if (_wcsicmp(propId, L"0C1A") == 0 && !outMsg->senderName) { // Sender Name
                outMsg->senderName = wtext ? Arena_Wcsdup(arena, wtext) : NULL;
            } else if ((_wcsicmp(propId, L"0C1F") == 0 || _wcsicmp(propId, L"5D01") == 0 || _wcsicmp(propId, L"39FE") == 0) && !outMsg->senderEmail) {
                outMsg->senderEmail = wtext ? Arena_Wcsdup(arena, wtext) : NULL;
            } else if (_wcsicmp(propId, L"0E04") == 0 && !outMsg->displayTo) { // Display To
                outMsg->displayTo = wtext ? Arena_Wcsdup(arena, wtext) : NULL;
            } else if (_wcsicmp(propId, L"0E03") == 0 && !outMsg->displayCc) { // Display Cc
                outMsg->displayCc = wtext ? Arena_Wcsdup(arena, wtext) : NULL;
            } else if (_wcsicmp(propId, L"0E02") == 0 && !outMsg->displayBcc) { // Display Bcc
                outMsg->displayBcc = wtext ? Arena_Wcsdup(arena, wtext) : NULL;
            } else if (_wcsicmp(propId, L"1000") == 0 && !outMsg->plainTextBody) { // Plain Text Body
                outMsg->plainTextBody = wtext ? Encoding_ToUtf8(arena, wtext) : Arena_Strdup(arena, (const char*)sdata);
            } else if (_wcsicmp(propId, L"1013") == 0 && !outMsg->htmlBody) { // HTML Body
                outMsg->htmlBody = Arena_Strdup(arena, (const char*)sdata);
            } else if (_wcsicmp(propId, L"1009") == 0 && !outMsg->rtfBody) { // Compressed RTF
                size_t decompLen = 0;
                unsigned char* decomp = Rtf_Decompress(arena, sdata, streamSize, &decompLen);
                if (decomp) {
                    outMsg->rtfBody = (char*)decomp;
                }
            } else if (_wcsicmp(propId, L"3FDE") == 0 && streamSize >= 4) { // PR_INTERNET_CPID
                outMsg->codePage = *(unsigned int*)sdata;
            } else if (_wcsicmp(propId, L"3FFD") == 0 && streamSize >= 4 && outMsg->codePage == 0) { // PR_MESSAGE_CODEPAGE
                outMsg->codePage = *(unsigned int*)sdata;
            } else if ((_wcsicmp(propId, L"0E06") == 0 || _wcsicmp(propId, L"3007") == 0) && streamSize >= 8 && !outMsg->sentDate) { // Delivery Time
                wchar_t dateBuf[64];
                FormatFileTime(*(FILETIME*)sdata, dateBuf, 64);
                outMsg->sentDate = Arena_Wcsdup(arena, dateBuf);
            }
        }
    }

    // Parse Attachments (__attach_version1.0_#...)
    for (int d = 0; d < ctx.dirEntryCount && outMsg->attachmentCount < MAX_MSG_ATTACHMENTS; d++) {
        CfbDirEntry* de = &ctx.dirEntries[d];
        if (de->mse == 1 && wcsncmp(de->ab, L"__attach_version1.0_#", 21) == 0) {
            NativeMsgAttachment* att = &outMsg->attachments[outMsg->attachmentCount];
            ZeroMemory(att, sizeof(NativeMsgAttachment));

            // Traverse child streams of attachment storage
            int childSid = de->sidChild;
            while (childSid >= 0 && childSid < ctx.dirEntryCount) {
                CfbDirEntry* cde = &ctx.dirEntries[childSid];
                if (cde->mse == 2 && wcsncmp(cde->ab, L"__substg1.0_", 12) == 0) {
                    wchar_t aTag[9] = {0};
                    wcsncpy_s(aTag, 9, cde->ab + 12, 8);
                    wchar_t apId[5] = {0};
                    wchar_t apType[5] = {0};
                    wcsncpy_s(apId, 5, aTag, 4);
                    wcsncpy_s(apType, 5, aTag + 4, 4);

                    size_t asize = cde->ulSizeLow;
                    if (_wcsicmp(apId, L"3707") == 0 || (_wcsicmp(apId, L"3704") == 0 && !att->fileName)) { // Filename
                        unsigned char* adata = ReadStreamBytes(&ctx, cde->sectStart, asize);
                        if (adata) {
                            if (_wcsicmp(apType, L"001F") == 0) att->fileName = Arena_Wcsdup(arena, (const wchar_t*)adata);
                            else att->fileName = Encoding_ToWideChar(arena, (const char*)adata, (int)asize, outMsg->codePage);
                        }
                    } else if (_wcsicmp(apId, L"3716") == 0) { // ContentId
                        unsigned char* adata = ReadStreamBytes(&ctx, cde->sectStart, asize);
                        if (adata) {
                            wchar_t* cid = (_wcsicmp(apType, L"001F") == 0) ? (wchar_t*)adata : Encoding_ToWideChar(arena, (const char*)adata, (int)asize, outMsg->codePage);
                            if (cid) {
                                // Trim angle brackets <cid>
                                while (*cid == L'<') cid++;
                                size_t clen = wcslen(cid);
                                while (clen > 0 && cid[clen - 1] == L'>') cid[--clen] = L'\0';
                                att->contentId = Arena_Wcsdup(arena, cid);
                            }
                        }
                    } else if (_wcsicmp(apId, L"370E") == 0) { // MimeType
                        unsigned char* adata = ReadStreamBytes(&ctx, cde->sectStart, asize);
                        if (adata) {
                            if (_wcsicmp(apType, L"001F") == 0) {
                                att->mimeType = Encoding_ToUtf8(arena, (const wchar_t*)adata);
                            } else {
                                att->mimeType = Arena_Strdup(arena, (const char*)adata);
                            }
                        }
                    } else if (_wcsicmp(apId, L"3701") == 0 && _wcsicmp(apType, L"0102") == 0) { // AttachDataBinary
                        att->streamSector = cde->sectStart;
                        att->dataSize = asize;

                        // Memory optimization: if attachment is < 256KB or inline candidate, load into RAM; otherwise keep streamSector for lazy streaming
                        if (asize <= 256 * 1024 || att->contentId != NULL) {
                            att->data = ReadStreamBytes(&ctx, cde->sectStart, asize);
                        }
                    }
                }
                childSid = cde->sidRightSib; // Traverse sibling chain
            }

            if (!att->fileName) {
                att->fileName = Arena_Wcsdup(arena, L"attachment.dat");
            }

            // Guess MimeType if empty
            if (!att->mimeType) {
                const wchar_t* ext = wcsrchr(att->fileName, L'.');
                if (ext) {
                    if (_wcsicmp(ext, L".png") == 0) att->mimeType = Arena_Strdup(arena, "image/png");
                    else if (_wcsicmp(ext, L".jpg") == 0 || _wcsicmp(ext, L".jpeg") == 0) att->mimeType = Arena_Strdup(arena, "image/jpeg");
                    else if (_wcsicmp(ext, L".gif") == 0) att->mimeType = Arena_Strdup(arena, "image/gif");
                    else if (_wcsicmp(ext, L".pdf") == 0) att->mimeType = Arena_Strdup(arena, "application/pdf");
                    else if (_wcsicmp(ext, L".txt") == 0) att->mimeType = Arena_Strdup(arena, "text/plain");
                    else att->mimeType = Arena_Strdup(arena, "application/octet-stream");
                }
            }

            outMsg->attachmentCount++;
        }
    }

    // Format Sender: "Name <Email>" or "Name" or "Email"
    if (outMsg->senderName && outMsg->senderEmail && _wcsicmp(outMsg->senderName, outMsg->senderEmail) != 0) {
        wchar_t sbuf[512];
        StringCchPrintfW(sbuf, 512, L"%s <%s>", outMsg->senderName, outMsg->senderEmail);
        outMsg->senderFormatted = Arena_Wcsdup(arena, sbuf);
    } else {
        outMsg->senderFormatted = outMsg->senderName ? outMsg->senderName : outMsg->senderEmail ? outMsg->senderEmail : L"";
    }

    // Final Body Resolution
    // 1. Try HTML body
    char* finalHtml = outMsg->htmlBody;

    // 2. If no HTML, try extracting from RTF
    if (!finalHtml && outMsg->rtfBody) {
        finalHtml = Rtf_ExtractHtml(arena, outMsg->rtfBody);
    }

    // 3. If still no HTML, convert pure RTF to HTML (Feature #2)
    if (!finalHtml && outMsg->rtfBody) {
        finalHtml = Rtf_ConvertToHtml(arena, outMsg->rtfBody);
    }

    // 4. If still no HTML, convert Plain Text to HTML
    if (!finalHtml && outMsg->plainTextBody) {
        size_t ptLen = strlen(outMsg->plainTextBody);
        char* ptHtml = (char*)Arena_Alloc(arena, ptLen * 2 + 512);
        if (ptHtml) {
            sprintf_s(ptHtml, ptLen * 2 + 512,
                "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=Edge\">"
                "<style>body{font-family:'Segoe UI','Malgun Gothic',sans-serif;font-size:14px;line-height:1.5;color:#222;margin:16px;white-space:pre-wrap;}</style></head>"
                "<body>%s</body></html>", outMsg->plainTextBody);
            finalHtml = ptHtml;
        }
    }

    if (!finalHtml) {
        finalHtml = "<!DOCTYPE html><html><head><meta http-equiv=\"X-UA-Compatible\" content=\"IE=Edge\"></head><body></body></html>";
    }

    // Prepare attachment info array for HTML sanitization
    MsgAttachmentInfo attInfos[MAX_MSG_ATTACHMENTS];
    for (int i = 0; i < outMsg->attachmentCount; i++) {
        attInfos[i].fileName = outMsg->attachments[i].fileName;
        attInfos[i].contentId = outMsg->attachments[i].contentId;
        attInfos[i].mimeType = outMsg->attachments[i].mimeType;
        attInfos[i].data = outMsg->attachments[i].data;
        attInfos[i].dataSize = outMsg->attachments[i].dataSize;
        attInfos[i].isInline = 0;
    }

    // Run HTML Sanitizer (Injects IE=Edge & Reset CSS, blocks remote images, resolves inline Base64)
    outMsg->htmlBody = Html_SanitizeAndResolve(arena, finalHtml, attInfos, outMsg->attachmentCount, L"외부 이미지 (오프라인 차단됨)");

    // Update isInline status back to outMsg->attachments
    for (int i = 0; i < outMsg->attachmentCount; i++) {
        outMsg->attachments[i].isInline = attInfos[i].isInline;
    }

    return 1;
}

int Cfb_SaveAttachment(const NativeMsg* msg, const NativeMsgAttachment* att, const wchar_t* destPath, ErrorDiagnostic* diag) {
    if (!msg || !att || !destPath) return 0;

    HANDLE hOut = CreateFileW(destPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOut == INVALID_HANDLE_VALUE) {
        ErrorDiag_Init(diag, ERROR_STAGE_IO, destPath, GetLastError());
        ErrorDiag_SetDetail(diag, L"첨부파일 저장 실패", L"대상 파일을 생성할 수 없습니다.", L"디스크 쓰기 권한이나 여유 공간을 확인해 주세요.");
        return 0;
    }

    // Case 1: Data is already in RAM (small attachment or inline image)
    if (att->data && att->dataSize > 0) {
        DWORD written = 0;
        BOOL ok = WriteFile(hOut, att->data, (DWORD)att->dataSize, &written, NULL);
        CloseHandle(hOut);
        return ok;
    }

    // Case 2: Lazy Streaming directly from mapped file / sectors in 64KB chunks
    if (msg->fileView && att->streamSector > 0 && att->dataSize > 0) {
        const CfbHeader* hdr = (const CfbHeader*)msg->fileView;
        unsigned int sectorSize = 1 << hdr->uSectorShift;

        // Traverse FAT sectors
        const unsigned int* difat = hdr->difat;
        unsigned int fatSectId = difat[0];
        const unsigned int* fatTable = (const unsigned int*)(msg->fileView + ((size_t)(fatSectId + 1) * sectorSize));

        size_t bytesLeft = att->dataSize;
        unsigned int currSect = att->streamSector;

        unsigned char chunkBuffer[65536]; // 64 KB chunk buffer
        size_t chunkFilled = 0;

        while (bytesLeft > 0 && currSect < 0xFFFFFFFC) {
            size_t offset = (size_t)(currSect + 1) * sectorSize;
            if (offset + sectorSize > msg->fileSize) break;
            const unsigned char* sptr = msg->fileView + offset;

            size_t toCopy = sectorSize;
            if (toCopy > bytesLeft) toCopy = bytesLeft;

            size_t srcOff = 0;
            while (srcOff < toCopy) {
                size_t space = sizeof(chunkBuffer) - chunkFilled;
                size_t piece = toCopy - srcOff;
                if (piece > space) piece = space;

                memcpy(chunkBuffer + chunkFilled, sptr + srcOff, piece);
                chunkFilled += piece;
                srcOff += piece;

                if (chunkFilled == sizeof(chunkBuffer)) {
                    DWORD written = 0;
                    WriteFile(hOut, chunkBuffer, (DWORD)chunkFilled, &written, NULL);
                    chunkFilled = 0;
                }
            }

            bytesLeft -= toCopy;
            currSect = fatTable[currSect];
        }

        if (chunkFilled > 0) {
            DWORD written = 0;
            WriteFile(hOut, chunkBuffer, (DWORD)chunkFilled, &written, NULL);
        }

        CloseHandle(hOut);
        return 1;
    }

    CloseHandle(hOut);
    return 0;
}

void Cfb_CloseMsg(NativeMsg* msg) {
    if (!msg) return;
    if (msg->fileView) {
        UnmapViewOfFile(msg->fileView);
        msg->fileView = NULL;
    }
    if (msg->hMapping) {
        CloseHandle(msg->hMapping);
        msg->hMapping = NULL;
    }
    if (msg->hFile && msg->hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(msg->hFile);
        msg->hFile = NULL;
    }
}
