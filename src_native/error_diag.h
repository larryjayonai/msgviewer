#ifndef ERROR_DIAG_H
#define ERROR_DIAG_H

#include <windows.h>
#include "localization.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ERROR_STAGE_IO = 1,       // 1단계: 파일 열기 및 I/O
    ERROR_STAGE_CFB = 2,      // 2단계: OLE 복합 파일(CFB) 구조 분석
    ERROR_STAGE_MAPI = 3,     // 3단계: MAPI 속성 및 본문 추출
    ERROR_STAGE_RENDER = 4    // 4단계: 본문 렌더링
} ErrorStage;

typedef struct {
    ErrorStage stage;
    DWORD win32ErrorCode;
    wchar_t filePath[MAX_PATH];
    wchar_t errorType[128];
    wchar_t technicalDetail[512];
    wchar_t recommendedAction[256];
} ErrorDiagnostic;

void ErrorDiag_Init(ErrorDiagnostic* diag, ErrorStage stage, const wchar_t* filePath, DWORD win32Err);
void ErrorDiag_SetDetail(ErrorDiagnostic* diag, const wchar_t* errorType, const wchar_t* detail, const wchar_t* action);
void ErrorDiag_ShowModal(HWND parent, const ErrorDiagnostic* diag, AppLanguage lang);

#ifdef __cplusplus
}
#endif

#endif // ERROR_DIAG_H
