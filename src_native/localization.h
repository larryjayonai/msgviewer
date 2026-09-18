#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_LANG_KO = 0, // 한국어
    APP_LANG_EN = 1, // English
    APP_LANG_FR = 2, // Français
    APP_LANG_JA = 3  // 日本語
} AppLanguage;

typedef struct {
    const wchar_t* appTitle;
    const wchar_t* openBtn;
    const wchar_t* closeBtn;
    const wchar_t* exitBtn;
    const wchar_t* fixedLangBtn;
    const wchar_t* fromTitle;
    const wchar_t* sentTitle;
    const wchar_t* toTitle;
    const wchar_t* ccTitle;
    const wchar_t* bccTitle;
    const wchar_t* subjectTitle;
    const wchar_t* attachmentsHeader;
    const wchar_t* noneText;
    const wchar_t* noInfoText;
    const wchar_t* openFileDialogTitle;
    const wchar_t* saveAttachmentTitle;
    const wchar_t* fileFilter;
    const wchar_t* errorTitle;
    const wchar_t* stage1Name;
    const wchar_t* stage2Name;
    const wchar_t* stage3Name;
    const wchar_t* stage4Name;
    const wchar_t* offlineImageTooltip;
} LocalizationStrings;

extern AppLanguage g_currentLanguage;
extern const wchar_t* const g_fixedLanguageButtonText;

// Detect language from Windows UI Language
AppLanguage Localization_DetectLanguage(void);

// Get next language in cycle (KO -> EN -> FR -> JA -> KO)
AppLanguage Localization_GetNextLanguage(AppLanguage current);

// Get string bundle for a language
const LocalizationStrings* Localization_GetStrings(AppLanguage lang);

// Set global language
void Localization_SetLanguage(AppLanguage lang);

#ifdef __cplusplus
}
#endif

#endif // LOCALIZATION_H
