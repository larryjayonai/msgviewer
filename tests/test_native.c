#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include "../src_native/arena.h"
#include "../src_native/encoding.h"
#include "../src_native/rtf_decompressor.h"
#include "../src_native/rtf_to_html.h"
#include "../src_native/html_sanitizer.h"
#include "../src_native/cfb_reader.h"
#include "../src_native/error_diag.h"

int g_passed = 0;
int g_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (cond) { \
            printf("[PASS] %s\n", msg); \
            g_passed++; \
        } else { \
            printf("[FAIL] %s (Line %d)\n", msg, __LINE__); \
            g_failed++; \
        } \
    } while (0)

void Test_Arena(void) {
    printf("\n--- Testing Memory Arena ---\n");
    MemoryArena* arena = Arena_Create(1024 * 1024);
    TEST_ASSERT(arena != NULL, "Arena created");

    void* p1 = Arena_Alloc(arena, 100);
    TEST_ASSERT(p1 != NULL && arena->offset >= 100, "Arena alloc 100 bytes");

    char* str = Arena_Strdup(arena, "Hello World");
    TEST_ASSERT(strcmp(str, "Hello World") == 0, "Arena_Strdup");

    wchar_t* wstr = Arena_Wcsdup(arena, L"테스트 문자열");
    TEST_ASSERT(wcscmp(wstr, L"테스트 문자열") == 0, "Arena_Wcsdup");

    Arena_Reset(arena);
    TEST_ASSERT(arena->offset == 0, "Arena_Reset resets offset to 0");

    Arena_Destroy(arena);
}

void Test_RtfToHtml(void) {
    printf("\n--- Testing Pure RTF to HTML Converter ---\n");
    MemoryArena* arena = Arena_Create(1024 * 1024);

    const char* rtf = "{\\rtf1\\ansi\\deff0{\\colortbl;\\red255\\green0\\blue0;}\\b Hello\\b0 \\i World\\i0 \\par\\cf1 Red Text\\cf0\\par\\trowd\\intbl Cell1\\cell\\intbl Cell2\\cell\\row}";
    char* html = Rtf_ConvertToHtml(arena, rtf);

    TEST_ASSERT(html != NULL, "Rtf_ConvertToHtml returned output");
    TEST_ASSERT(strstr(html, "<b>Hello</b>") != NULL, "Bold preserved");
    TEST_ASSERT(strstr(html, "<i>World</i>") != NULL, "Italic preserved");
    TEST_ASSERT(strstr(html, "color:#FF0000") != NULL, "Color table preserved");
    TEST_ASSERT(strstr(html, "<table>") != NULL && strstr(html, "Cell1</td>") != NULL, "Table row/cell preserved");

    Arena_Destroy(arena);
}

void Test_HtmlSanitizerAndRemoteImages(void) {
    printf("\n--- Testing HTML Sanitizer & Remote Image Blocker ---\n");
    MemoryArena* arena = Arena_Create(1024 * 1024);

    const char* dirtyHtml = "<html><head><title>Test</title></head><body><script>alert('hack');</script>"
                            "<img src=\"https://evil.com/tracker.png\" alt=\"Logo\">"
                            "<img src=\"cid:my_inline_img\">"
                            "<p onload=\"evil()\">Hello</p></body></html>";

    unsigned char dummyImg[4] = { 0x89, 'P', 'N', 'G' };
    MsgAttachmentInfo atts[1];
    atts[0].contentId = L"my_inline_img";
    atts[0].fileName = L"image.png";
    atts[0].mimeType = "image/png";
    atts[0].data = dummyImg;
    atts[0].dataSize = 4;
    atts[0].isInline = 0;

    char* safeHtml = Html_SanitizeAndResolve(arena, dirtyHtml, atts, 1, L"외부 이미지 (오프라인 차단됨)");

    TEST_ASSERT(safeHtml != NULL, "Sanitizer produced output");
    TEST_ASSERT(strstr(safeHtml, "<script") == NULL, "Script tag removed");
    TEST_ASSERT(strstr(safeHtml, "onload=") == NULL, "Inline event handler removed");
    TEST_ASSERT(strstr(safeHtml, "offline-img-box") != NULL, "Remote image replaced with offline placeholder");
    TEST_ASSERT(strstr(safeHtml, "https://evil.com/tracker.png") != NULL, "Original URL preserved in tooltip title");
    TEST_ASSERT(strstr(safeHtml, "data:image/png;base64,") != NULL, "CID converted to Base64 data URI");
    TEST_ASSERT(atts[0].isInline == 1, "Attachment marked as inline");
    TEST_ASSERT(strstr(safeHtml, "content=\"IE=Edge\"") != NULL, "IE=Edge meta injected");

    Arena_Destroy(arena);
}

void Test_CfbReading(void) {
    printf("\n--- Testing Native CFB Reader on Real MSG Samples ---\n");
    MemoryArena* arena = Arena_Create(16 * 1024 * 1024);

    NativeMsg msg;
    ErrorDiagnostic diag;

    // 1. Standard MSG
    int ok = Cfb_ReadMsg(L"tests\\samples\\sample_standard.msg", arena, &msg, &diag);
    TEST_ASSERT(ok == 1, "sample_standard.msg opened successfully");
    TEST_ASSERT(msg.subject != NULL && wcslen(msg.subject) > 0, "Subject parsed");
    TEST_ASSERT(msg.senderFormatted != NULL && wcslen(msg.senderFormatted) > 0, "Sender parsed");
    TEST_ASSERT(msg.htmlBody != NULL && strlen(msg.htmlBody) > 0, "HtmlBody parsed");
    TEST_ASSERT(msg.attachmentCount == 1, "Attachment count is 1");
    Cfb_CloseMsg(&msg);
    Arena_Reset(arena);

    // 2. Inline Image MSG
    ok = Cfb_ReadMsg(L"tests\\samples\\sample_inline_image.msg", arena, &msg, &diag);
    TEST_ASSERT(ok == 1, "sample_inline_image.msg opened successfully");
    TEST_ASSERT(msg.attachmentCount >= 1, "Attachment found in inline msg");
    printf("DEBUG: att count=%d, att[0].fileName=%ls, att[0].contentId=%ls, dataSize=%zu, data=%p\n",
           msg.attachmentCount, msg.attachments[0].fileName, msg.attachments[0].contentId, msg.attachments[0].dataSize, msg.attachments[0].data);
    printf("FULL HTML: %s\n", msg.htmlBody);
    TEST_ASSERT(strstr(msg.htmlBody, "data:image/") != NULL, "Inline image resolved to Base64 in body");
    Cfb_CloseMsg(&msg);
    Arena_Reset(arena);

    // 3. No Attachments MSG
    ok = Cfb_ReadMsg(L"tests\\samples\\sample_no_attachments.msg", arena, &msg, &diag);
    TEST_ASSERT(ok == 1, "sample_no_attachments.msg opened successfully");
    TEST_ASSERT(msg.attachmentCount == 0, "No attachments verified");
    Cfb_CloseMsg(&msg);
    Arena_Reset(arena);

    // 4. Corrupt file test (Stage 2 diagnostic verification)
    ok = Cfb_ReadMsg(L"README.md", arena, &msg, &diag);
    TEST_ASSERT(ok == 0, "Non-CFB file fails as expected");
    TEST_ASSERT(diag.stage == ERROR_STAGE_CFB, "Error diagnosed at Stage 2 (CFB Structure)");
    TEST_ASSERT(wcslen(diag.errorType) > 0, "Error type filled");
    TEST_ASSERT(wcslen(diag.technicalDetail) > 0, "Technical detail filled");

    Arena_Destroy(arena);
}

int main(void) {
    printf("====================================================\n");
    printf("Running MSG Viewer v1.2-revB Native C Test Suite\n");
    printf("====================================================\n");

    Test_Arena();
    Test_RtfToHtml();
    Test_HtmlSanitizerAndRemoteImages();
    Test_CfbReading();

    printf("\n====================================================\n");
    printf("Test Results: %d Passed, %d Failed\n", g_passed, g_failed);
    printf("====================================================\n");

    return g_failed == 0 ? 0 : 1;
}
