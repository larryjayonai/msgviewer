#ifndef CFB_READER_H
#define CFB_READER_H

#include <windows.h>
#include "arena.h"
#include "error_diag.h"
#include "html_sanitizer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MSG_ATTACHMENTS 64
#define MAX_MSG_RECIPIENTS 128

typedef struct NativeMsgAttachment {
    wchar_t* fileName;
    wchar_t* contentId;
    char* mimeType;
    unsigned char* data;      // Populated only for small/inline images; NULL for large regular attachments
    size_t dataSize;
    unsigned int streamSector;// Starting sector in CFB for lazy streaming
    int isInline;
    int isEmbeddedMsg;
} NativeMsgAttachment;

typedef struct NativeMsg {
    wchar_t* subject;
    wchar_t* senderName;
    wchar_t* senderEmail;
    wchar_t* senderFormatted;
    wchar_t* sentDate;
    wchar_t* displayTo;
    wchar_t* displayCc;
    wchar_t* displayBcc;
    char* htmlBody;
    char* plainTextBody;
    char* rtfBody;
    UINT codePage;

    NativeMsgAttachment attachments[MAX_MSG_ATTACHMENTS];
    int attachmentCount;

    // Internal CFB file mapping handle for lazy streaming
    HANDLE hFile;
    HANDLE hMapping;
    const unsigned char* fileView;
    size_t fileSize;
} NativeMsg;

// Opens and parses an MSG file into arena memory using Memory-Mapped Files
// Returns 1 on success, 0 on failure (diag filled with 4-stage error details)
int Cfb_ReadMsg(const wchar_t* filePath, MemoryArena* arena, NativeMsg* outMsg, ErrorDiagnostic* diag);

// Lazy streaming: saves an attachment directly from CFB sectors to target file without loading into RAM
int Cfb_SaveAttachment(const NativeMsg* msg, const NativeMsgAttachment* att, const wchar_t* destPath, ErrorDiagnostic* diag);

// Releases file mapping handles (called when closing email or opening new one)
void Cfb_CloseMsg(NativeMsg* msg);

#ifdef __cplusplus
}
#endif

#endif // CFB_READER_H
