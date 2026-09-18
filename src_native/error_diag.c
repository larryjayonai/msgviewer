#include "error_diag.h"
#include <strsafe.h>

void ErrorDiag_Init(ErrorDiagnostic* diag, ErrorStage stage, const wchar_t* filePath, DWORD win32Err) {
    if (!diag) return;
    ZeroMemory(diag, sizeof(ErrorDiagnostic));
    diag->stage = stage;
    diag->win32ErrorCode = win32Err;
    if (filePath) {
        StringCchCopyW(diag->filePath, MAX_PATH, filePath);
    }
}

void ErrorDiag_SetDetail(ErrorDiagnostic* diag, const wchar_t* errorType, const wchar_t* detail, const wchar_t* action) {
    if (!diag) return;
    if (errorType) StringCchCopyW(diag->errorType, 128, errorType);
    if (detail) StringCchCopyW(diag->technicalDetail, 512, detail);
    if (action) StringCchCopyW(diag->recommendedAction, 256, action);
}

void ErrorDiag_ShowModal(HWND parent, const ErrorDiagnostic* diag, AppLanguage lang) {
    if (!diag) return;
    const LocalizationStrings* loc = Localization_GetStrings(lang);

    const wchar_t* stageName = L"";
    switch (diag->stage) {
        case ERROR_STAGE_IO:     stageName = loc->stage1Name; break;
        case ERROR_STAGE_CFB:    stageName = loc->stage2Name; break;
        case ERROR_STAGE_MAPI:   stageName = loc->stage3Name; break;
        case ERROR_STAGE_RENDER: stageName = loc->stage4Name; break;
    }

    wchar_t message[2048];
    switch (lang) {
        case APP_LANG_KO:
            StringCchPrintfW(message, 2048,
                L"[메일 열기 실패 안내]\n\n"
                L"• 실패 위치: %s\n"
                L"• 파일 경로: %s\n"
                L"• 오류 유형: %s\n"
                L"• 기술 세부: %s (코드: 0x%08X)\n"
                L"• 조치 방법: %s",
                stageName,
                diag->filePath[0] ? diag->filePath : L"(없음)",
                diag->errorType[0] ? diag->errorType : L"알 수 없는 오류",
                diag->technicalDetail[0] ? diag->technicalDetail : L"세부 정보 없음",
                diag->win32ErrorCode,
                diag->recommendedAction[0] ? diag->recommendedAction : L"파일이 정상적인 Outlook .msg 파일인지 확인해 주세요."
            );
            break;
        case APP_LANG_FR:
            StringCchPrintfW(message, 2048,
                L"[Échec de l'ouverture du message]\n\n"
                L"• Étape d'échec : %s\n"
                L"• Chemin du fichier : %s\n"
                L"• Type d'erreur : %s\n"
                L"• Détails techniques : %s (Code : 0x%08X)\n"
                L"• Action recommandée : %s",
                stageName,
                diag->filePath[0] ? diag->filePath : L"(Aucun)",
                diag->errorType[0] ? diag->errorType : L"Erreur inconnue",
                diag->technicalDetail[0] ? diag->technicalDetail : L"Aucun détail",
                diag->win32ErrorCode,
                diag->recommendedAction[0] ? diag->recommendedAction : L"Veuillez vérifier que le fichier est un fichier Outlook .msg valide."
            );
            break;
        case APP_LANG_JA:
            StringCchPrintfW(message, 2048,
                L"[メールを開けませんでした]\n\n"
                L"• 失敗段階: %s\n"
                L"• ファイルパス: %s\n"
                L"• エラー種別: %s\n"
                L"• 技術詳細: %s (コード: 0x%08X)\n"
                L"• 対処方法: %s",
                stageName,
                diag->filePath[0] ? diag->filePath : L"(なし)",
                diag->errorType[0] ? diag->errorType : L"不明なエラー",
                diag->technicalDetail[0] ? diag->technicalDetail : L"詳細なし",
                diag->win32ErrorCode,
                diag->recommendedAction[0] ? diag->recommendedAction : L"ファイルが正常な Outlook .msg ファイルであるか確認してください。"
            );
            break;
        case APP_LANG_EN:
        default:
            StringCchPrintfW(message, 2048,
                L"[Failed to Open Message]\n\n"
                L"• Failure Stage: %s\n"
                L"• File Path: %s\n"
                L"• Error Type: %s\n"
                L"• Technical Detail: %s (Code: 0x%08X)\n"
                L"• Recommended Action: %s",
                stageName,
                diag->filePath[0] ? diag->filePath : L"(None)",
                diag->errorType[0] ? diag->errorType : L"Unknown Error",
                diag->technicalDetail[0] ? diag->technicalDetail : L"No details",
                diag->win32ErrorCode,
                diag->recommendedAction[0] ? diag->recommendedAction : L"Please verify that the file is a valid Outlook .msg file."
            );
            break;
    }

    MessageBoxW(parent, message, loc->errorTitle, MB_OK | MB_ICONERROR);
}
