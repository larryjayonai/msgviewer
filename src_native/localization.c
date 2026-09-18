#include "localization.h"

AppLanguage g_currentLanguage = APP_LANG_KO;
const wchar_t* const g_fixedLanguageButtonText = L"언어 / Language / Langue / 言語 (F8)";

static const LocalizationStrings s_stringsKO = {
    L"MSG Viewer (오프라인 휴대용 MSG 뷰어) — v1.2",
    L"열기 (F2)",
    L"닫기 (F4)",
    L"종료",
    L"언어 / Language / Langue / 言語 (F8)",
    L"보내는 사람:",
    L"전송:",
    L"받는 사람:",
    L"참조:",
    L"숨은참조:",
    L"제목:",
    L"첨부파일",
    L"없음",
    L"정보 없음",
    L"MSG 파일 선택",
    L"첨부파일 저장",
    L"Outlook MSG 파일 (*.msg)\0*.msg\0모든 파일 (*.*)\0*.*\0\0",
    L"오류",
    L"1단계: 파일 열기 및 I/O",
    L"2단계: OLE 복합 파일(CFB) 구조 분석",
    L"3단계: MAPI 속성 및 본문 추출",
    L"4단계: 본문 렌더링",
    L"외부 이미지 (오프라인 차단됨)"
};

static const LocalizationStrings s_stringsEN = {
    L"MSG Viewer (Offline Portable MSG Viewer) — v1.2",
    L"Open (F2)",
    L"Close (F4)",
    L"Exit",
    L"언어 / Language / Langue / 言語 (F8)",
    L"From:",
    L"Sent:",
    L"To:",
    L"Cc:",
    L"Bcc:",
    L"Subject:",
    L"Attachments",
    L"None",
    L"No Information",
    L"Select MSG File",
    L"Save Attachment",
    L"Outlook MSG Files (*.msg)\0*.msg\0All Files (*.*)\0*.*\0\0",
    L"Error",
    L"Stage 1: File Open and I/O",
    L"Stage 2: OLE Compound File (CFB) Structure Analysis",
    L"Stage 3: MAPI Property and Body Extraction",
    L"Stage 4: Body Rendering",
    L"Remote Image (Blocked for Offline Protection)"
};

static const LocalizationStrings s_stringsFR = {
    L"MSG Viewer (Visionneuse MSG hors ligne portable) — v1.2",
    L"Ouvrir (F2)",
    L"Fermer (F4)",
    L"Quitter",
    L"언어 / Language / Langue / 言語 (F8)",
    L"De :",
    L"Envoyé :",
    L"À :",
    L"Cc :",
    L"Cci :",
    L"Objet :",
    L"Pièces jointes",
    L"Aucun",
    L"Pas d'information",
    L"Sélectionner un fichier MSG",
    L"Enregistrer la pièce jointe",
    L"Fichiers Outlook MSG (*.msg)\0*.msg\0Tous les fichiers (*.*)\0*.*\0\0",
    L"Erreur",
    L"Étape 1 : Ouverture du fichier et E/S",
    L"Étape 2 : Analyse de la structure du fichier composé OLE (CFB)",
    L"Étape 3 : Extraction des propriétés MAPI et du corps",
    L"Étape 4 : Rendu du corps",
    L"Image distante (Bloquée pour la protection hors ligne)"
};

static const LocalizationStrings s_stringsJA = {
    L"MSG Viewer (オフライン ポータブル MSG ビューアー) — v1.2",
    L"開く (F2)",
    L"閉じる (F4)",
    L"終了",
    L"언어 / Language / Langue / 言語 (F8)",
    L"差出人:",
    L"送信日時:",
    L"宛先:",
    L"CC:",
    L"BCC:",
    L"件名:",
    L"添付ファイル",
    L"なし",
    L"情報なし",
    L"MSG ファイルの選択",
    L"添付ファイルを保存",
    L"Outlook MSG ファイル (*.msg)\0*.msg\0すべてのファイル (*.*)\0*.*\0\0",
    L"エラー",
    L"ステップ 1: ファイルオープンと I/O",
    L"ステップ 2: OLE 複合ファイル (CFB) 構造解析",
    L"ステップ 3: MAPI プロパティおよび本文抽出",
    L"ステップ 4: 本文レンダリング",
    L"リモート画像 (オフライン保護のためブロック)"
};

AppLanguage Localization_DetectLanguage(void) {
    LANGID langId = GetUserDefaultUILanguage();
    WORD primaryLang = PRIMARYLANGID(langId);

    switch (primaryLang) {
        case LANG_KOREAN:   return APP_LANG_KO;
        case LANG_ENGLISH:  return APP_LANG_EN;
        case LANG_FRENCH:   return APP_LANG_FR;
        case LANG_JAPANESE: return APP_LANG_JA;
        default:            return APP_LANG_EN; // Default to English for other locales
    }
}

AppLanguage Localization_GetNextLanguage(AppLanguage current) {
    switch (current) {
        case APP_LANG_KO: return APP_LANG_EN;
        case APP_LANG_EN: return APP_LANG_FR;
        case APP_LANG_FR: return APP_LANG_JA;
        case APP_LANG_JA: return APP_LANG_KO;
        default:          return APP_LANG_KO;
    }
}

const LocalizationStrings* Localization_GetStrings(AppLanguage lang) {
    switch (lang) {
        case APP_LANG_KO: return &s_stringsKO;
        case APP_LANG_EN: return &s_stringsEN;
        case APP_LANG_FR: return &s_stringsFR;
        case APP_LANG_JA: return &s_stringsJA;
        default:          return &s_stringsKO;
    }
}

void Localization_SetLanguage(AppLanguage lang) {
    g_currentLanguage = lang;
}
