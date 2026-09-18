#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <shlobj.h>
#include <strsafe.h>
#include "arena.h"
#include "encoding.h"
#include "localization.h"
#include "error_diag.h"
#include "cfb_reader.h"
#include "ole_browser.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

#define IDC_BTN_OPEN    1001
#define IDC_BTN_CLOSE   1002
#define IDC_BTN_LANG    1003
#define IDC_BTN_EXIT    1004

#define IDM_LANG_KO     2001
#define IDM_LANG_EN     2002
#define IDM_LANG_FR     2003
#define IDM_LANG_JA     2004

#define IDC_ATTACH_BASE 3000

// Main Window State
static HWND g_hMainWnd = NULL;
static HWND g_hBtnOpen = NULL;
static HWND g_hBtnClose = NULL;
static HWND g_hBtnLang = NULL;
static HWND g_hBtnExit = NULL;

static HWND g_hLblFrom = NULL;
static HWND g_hTxtFrom = NULL;
static HWND g_hLblDate = NULL;
static HWND g_hTxtDate = NULL;
static HWND g_hLblTo = NULL;
static HWND g_hTxtTo = NULL;
static HWND g_hLblCc = NULL;
static HWND g_hTxtCc = NULL;
static HWND g_hLblBcc = NULL;
static HWND g_hTxtBcc = NULL;
static HWND g_hLblSubject = NULL;
static HWND g_hTxtSubject = NULL;

static HWND g_hLblAttach = NULL;
static HWND g_hAttachButtons[MAX_MSG_ATTACHMENTS];
static int  g_attachButtonCount = 0;

static HWND g_hRichEditFallback = NULL;
static OleBrowser* g_browser = NULL;
static MemoryArena* g_arena = NULL;
static NativeMsg g_currentMsg;
static int g_hasMsg = 0;
static int g_currentZoom = 100;
static HFONT g_hFontNormal = NULL;
static HFONT g_hFontBold = NULL;
static HBRUSH g_hBrushHeader = NULL;

// Feature #8: Process-level FEATURE_BROWSER_EMULATION in HKCU
static void RegisterBrowserEmulation(void) {
    HKEY hKey;
    const wchar_t* subKey = L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        const wchar_t* exeName = wcsrchr(exePath, L'\\');
        exeName = exeName ? exeName + 1 : exePath;

        DWORD value = 11001; // IE11 Standards Mode
        RegSetValueExW(hKey, exeName, 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static void UpdateHeaderLayout(int width, int height) {
    int rightMargin = width - 16;
    int dateWidth = 160;
    int dateX = rightMargin - dateWidth;
    int dateTitleWidth = 60;
    int dateTitleX = dateX - dateTitleWidth - 4;

    SetWindowPos(g_hLblDate, NULL, dateTitleX, 50, dateTitleWidth, 20, SWP_NOZORDER);
    SetWindowPos(g_hTxtDate, NULL, dateX, 50, dateWidth, 20, SWP_NOZORDER);

    int fromWidth = dateTitleX - 100;
    if (fromWidth < 100) fromWidth = 100;
    SetWindowPos(g_hTxtFrom, NULL, 95, 50, fromWidth, 20, SWP_NOZORDER);

    int fullWidth = rightMargin - 95;
    if (fullWidth < 150) fullWidth = 150;
    SetWindowPos(g_hTxtTo, NULL, 95, 74, fullWidth, 20, SWP_NOZORDER);
    SetWindowPos(g_hTxtCc, NULL, 95, 98, fullWidth, 20, SWP_NOZORDER);
    SetWindowPos(g_hTxtBcc, NULL, 95, 122, fullWidth, 20, SWP_NOZORDER);
    SetWindowPos(g_hTxtSubject, NULL, 95, 146, fullWidth, 40, SWP_NOZORDER);

    // Body bounds: Y from 194 to height - 85
    int bodyY = 194;
    int attachH = 75;
    int bodyH = height - bodyY - attachH;
    if (bodyH < 100) bodyH = 100;

    RECT bodyRect = { 0, bodyY, width, bodyY + bodyH };
    if (g_browser) {
        OleBrowser_Resize(g_browser, &bodyRect);
    }
    if (g_hRichEditFallback) {
        SetWindowPos(g_hRichEditFallback, NULL, 0, bodyY, width, bodyH, SWP_NOZORDER);
    }

    // Attachment Panel: Y from height - 75
    int attachY = height - attachH;
    SetWindowPos(g_hLblAttach, NULL, 12, attachY + 4, width - 24, 20, SWP_NOZORDER);

    int attX = 12;
    int attY = attachY + 26;
    for (int i = 0; i < g_attachButtonCount; i++) {
        if (g_hAttachButtons[i]) {
            RECT br;
            GetWindowRect(g_hAttachButtons[i], &br);
            int bw = br.right - br.left;
            if (attX + bw > width - 12) {
                attX = 12;
                attY += 24;
            }
            SetWindowPos(g_hAttachButtons[i], NULL, attX, attY, bw, 22, SWP_NOZORDER);
            attX += bw + 8;
        }
    }
}

static void ClearMessageView(void) {
    if (g_hasMsg) {
        Cfb_CloseMsg(&g_currentMsg);
        g_hasMsg = 0;
    }
    if (g_arena) {
        Arena_Reset(g_arena); // Instantly free all previous allocations!
    }

    SetWindowTextW(g_hTxtFrom, L"");
    SetWindowTextW(g_hTxtDate, L"");
    SetWindowTextW(g_hTxtTo, L"");
    SetWindowTextW(g_hTxtCc, L"");
    SetWindowTextW(g_hTxtBcc, L"");
    SetWindowTextW(g_hTxtSubject, L"");

    if (g_browser) {
        OleBrowser_Clear(g_browser);
    }
    if (g_hRichEditFallback) {
        SetWindowTextW(g_hRichEditFallback, L"");
        ShowWindow(g_hRichEditFallback, SW_HIDE);
    }

    for (int i = 0; i < g_attachButtonCount; i++) {
        if (g_hAttachButtons[i]) {
            DestroyWindow(g_hAttachButtons[i]);
            g_hAttachButtons[i] = NULL;
        }
    }
    g_attachButtonCount = 0;
}

static void ApplyLanguage(void) {
    const LocalizationStrings* loc = Localization_GetStrings(g_currentLanguage);

    SetWindowTextW(g_hMainWnd, loc->appTitle);
    SetWindowTextW(g_hBtnOpen, loc->openBtn);
    SetWindowTextW(g_hBtnClose, loc->closeBtn);
    SetWindowTextW(g_hBtnExit, loc->exitBtn);
    SetWindowTextW(g_hBtnLang, g_fixedLanguageButtonText);

    SetWindowTextW(g_hLblFrom, loc->fromTitle);
    SetWindowTextW(g_hLblDate, loc->sentTitle);
    SetWindowTextW(g_hLblTo, loc->toTitle);
    SetWindowTextW(g_hLblCc, loc->ccTitle);
    SetWindowTextW(g_hLblBcc, loc->bccTitle);
    SetWindowTextW(g_hLblSubject, loc->subjectTitle);
    SetWindowTextW(g_hLblAttach, loc->attachmentsHeader);

    if (g_hasMsg) {
        if (!g_currentMsg.displayTo || wcslen(g_currentMsg.displayTo) == 0) {
            SetWindowTextW(g_hTxtTo, loc->noneText);
        }
        if (!g_currentMsg.subject || wcslen(g_currentMsg.subject) == 0) {
            SetWindowTextW(g_hTxtSubject, loc->noneText);
        }
        if (!g_currentMsg.sentDate) {
            SetWindowTextW(g_hTxtDate, loc->noInfoText);
        }
        if (!g_currentMsg.displayBcc || wcslen(g_currentMsg.displayBcc) == 0) {
            SetWindowTextW(g_hTxtBcc, loc->noInfoText);
        }
    }
}

static void DisplayMessage(void) {
    const LocalizationStrings* loc = Localization_GetStrings(g_currentLanguage);

    SetWindowTextW(g_hTxtFrom, g_currentMsg.senderFormatted ? g_currentMsg.senderFormatted : L"");
    SetWindowTextW(g_hTxtDate, g_currentMsg.sentDate ? g_currentMsg.sentDate : loc->noInfoText);

    if (g_currentMsg.displayTo && wcslen(g_currentMsg.displayTo) > 0) {
        SetWindowTextW(g_hTxtTo, g_currentMsg.displayTo);
    } else {
        SetWindowTextW(g_hTxtTo, loc->noneText);
    }

    SetWindowTextW(g_hTxtCc, g_currentMsg.displayCc ? g_currentMsg.displayCc : L"");

    if (g_currentMsg.displayBcc && wcslen(g_currentMsg.displayBcc) > 0) {
        SetWindowTextW(g_hTxtBcc, g_currentMsg.displayBcc);
    } else {
        SetWindowTextW(g_hTxtBcc, loc->noInfoText);
    }

    if (g_currentMsg.subject && wcslen(g_currentMsg.subject) > 0) {
        SetWindowTextW(g_hTxtSubject, g_currentMsg.subject);
    } else {
        SetWindowTextW(g_hTxtSubject, loc->noneText);
    }

    // Body rendering via single OleBrowser instance
    int ok = 0;
    if (g_browser && g_currentMsg.htmlBody) {
        ok = OleBrowser_SetHtml(g_browser, g_currentMsg.htmlBody);
    }

    // Feature #10: Fallback to RichEdit if OleBrowser failed
    if (!ok && g_hRichEditFallback) {
        ShowWindow(g_hRichEditFallback, SW_SHOW);
        if (g_currentMsg.rtfBody) {
            SETTEXTEX ste = { ST_DEFAULT, CP_ACP };
            SendMessageW(g_hRichEditFallback, EM_SETTEXTEX, (WPARAM)&ste, (LPARAM)g_currentMsg.rtfBody);
        } else if (g_currentMsg.plainTextBody) {
            wchar_t* wpt = Encoding_ToWideChar(g_arena, g_currentMsg.plainTextBody, -1, CP_UTF8);
            SetWindowTextW(g_hRichEditFallback, wpt ? wpt : L"");
        }
    } else if (g_hRichEditFallback) {
        ShowWindow(g_hRichEditFallback, SW_HIDE);
    }

    // Clear old attachment buttons
    for (int i = 0; i < g_attachButtonCount; i++) {
        if (g_hAttachButtons[i]) {
            DestroyWindow(g_hAttachButtons[i]);
            g_hAttachButtons[i] = NULL;
        }
    }
    g_attachButtonCount = 0;

    // Create buttons for regular attachments
    int regularCount = 0;
    for (int i = 0; i < g_currentMsg.attachmentCount && g_attachButtonCount < MAX_MSG_ATTACHMENTS; i++) {
        if (!g_currentMsg.attachments[i].isInline) {
            wchar_t btnText[260];
            const wchar_t* fn = g_currentMsg.attachments[i].fileName;
            const wchar_t* ext = wcsrchr(fn, L'.');

            if (ext && _wcsicmp(ext, L".pdf") == 0) StringCchPrintfW(btnText, 260, L"[📄 PDF] %s", fn);
            else if (ext && _wcsicmp(ext, L".msg") == 0) StringCchPrintfW(btnText, 260, L"[✉️ MSG] %s", fn);
            else if (ext && (_wcsicmp(ext, L".zip") == 0 || _wcsicmp(ext, L".rar") == 0 || _wcsicmp(ext, L".7z") == 0)) StringCchPrintfW(btnText, 260, L"[📦 압축] %s", fn);
            else if (ext && (_wcsicmp(ext, L".png") == 0 || _wcsicmp(ext, L".jpg") == 0 || _wcsicmp(ext, L".gif") == 0)) StringCchPrintfW(btnText, 260, L"[🖼️ 이미지] %s", fn);
            else StringCchPrintfW(btnText, 260, L"[📎 파일] %s", fn);

            HDC hdc = GetDC(g_hMainWnd);
            SelectObject(hdc, g_hFontNormal);
            SIZE sz;
            GetTextExtentPoint32W(hdc, btnText, (int)wcslen(btnText), &sz);
            ReleaseDC(g_hMainWnd, hdc);

            HWND hBtn = CreateWindowExW(0, L"BUTTON", btnText,
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, sz.cx + 16, 22,
                g_hMainWnd, (HMENU)(INT_PTR)(IDC_ATTACH_BASE + i),
                GetModuleHandle(NULL), NULL);

            SendMessageW(hBtn, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            g_hAttachButtons[g_attachButtonCount++] = hBtn;
            regularCount++;
        }
    }

    if (regularCount == 0) {
        HWND hNone = CreateWindowExW(0, L"STATIC", loc->noneText,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            12, 0, 60, 20,
            g_hMainWnd, (HMENU)(INT_PTR)(IDC_ATTACH_BASE + 999),
            GetModuleHandle(NULL), NULL);
        SendMessageW(hNone, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        g_hAttachButtons[g_attachButtonCount++] = hNone;
    }

    RECT rc;
    GetClientRect(g_hMainWnd, &rc);
    UpdateHeaderLayout(rc.right, rc.bottom);
}

static void LoadMsgFile(const wchar_t* path) {
    ClearMessageView();

    ErrorDiagnostic diag;
    if (!Cfb_ReadMsg(path, g_arena, &g_currentMsg, &diag)) {
        ErrorDiag_ShowModal(g_hMainWnd, &diag, g_currentLanguage);
        return;
    }

    g_hasMsg = 1;
    DisplayMessage();
}

static void SaveAttachment(int attIndex) {
    if (!g_hasMsg || attIndex < 0 || attIndex >= g_currentMsg.attachmentCount) return;
    const NativeMsgAttachment* att = &g_currentMsg.attachments[attIndex];
    const LocalizationStrings* loc = Localization_GetStrings(g_currentLanguage);

    wchar_t savePath[MAX_PATH] = {0};
    StringCchCopyW(savePath, MAX_PATH, att->fileName);

    wchar_t desktopPath[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, desktopPath);

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMainWnd;
    ofn.lpstrFile = savePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"모든 파일 (*.*)\0*.*\0\0";
    ofn.lpstrInitialDir = desktopPath;
    ofn.lpstrTitle = loc->saveAttachmentTitle;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (GetSaveFileNameW(&ofn)) {
        ErrorDiagnostic diag;
        if (!Cfb_SaveAttachment(&g_currentMsg, att, savePath, &diag)) {
            ErrorDiag_ShowModal(g_hMainWnd, &diag, g_currentLanguage);
        }
    }
}

static void OpenFileDialog(void) {
    const LocalizationStrings* loc = Localization_GetStrings(g_currentLanguage);
    wchar_t filePath[MAX_PATH] = {0};

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMainWnd;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = loc->fileFilter;
    ofn.lpstrTitle = loc->openFileDialogTitle;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        LoadMsgFile(filePath);
    }
}

static void ShowLanguageMenu(void) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | (g_currentLanguage == APP_LANG_KO ? MF_CHECKED : 0), IDM_LANG_KO, L"한국어");
    AppendMenuW(hMenu, MF_STRING | (g_currentLanguage == APP_LANG_EN ? MF_CHECKED : 0), IDM_LANG_EN, L"English");
    AppendMenuW(hMenu, MF_STRING | (g_currentLanguage == APP_LANG_FR ? MF_CHECKED : 0), IDM_LANG_FR, L"Français");
    AppendMenuW(hMenu, MF_STRING | (g_currentLanguage == APP_LANG_JA ? MF_CHECKED : 0), IDM_LANG_JA, L"日本語");

    RECT rc;
    GetWindowRect(g_hBtnLang, &rc);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, rc.left, rc.bottom, 0, g_hMainWnd, NULL);
    DestroyMenu(hMenu);

    // Restore button state
    SendMessageW(g_hBtnLang, BM_SETSTATE, FALSE, 0);
    SetFocus(g_hMainWnd);
}

static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hBrushHeader = CreateSolidBrush(RGB(248, 249, 250));
            g_hFontNormal = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_hFontBold = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

            // Top Buttons
            g_hBtnOpen = CreateWindowExW(0, L"BUTTON", L"열기 (F2)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                10, 8, 90, 30, hWnd, (HMENU)IDC_BTN_OPEN, GetModuleHandle(NULL), NULL);
            g_hBtnClose = CreateWindowExW(0, L"BUTTON", L"닫기 (F4)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                106, 8, 90, 30, hWnd, (HMENU)IDC_BTN_CLOSE, GetModuleHandle(NULL), NULL);
            g_hBtnLang = CreateWindowExW(0, L"BUTTON", g_fixedLanguageButtonText, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                460, 8, 270, 30, hWnd, (HMENU)IDC_BTN_LANG, GetModuleHandle(NULL), NULL);
            g_hBtnExit = CreateWindowExW(0, L"BUTTON", L"종료", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                736, 8, 80, 30, hWnd, (HMENU)IDC_BTN_EXIT, GetModuleHandle(NULL), NULL);

            SendMessageW(g_hBtnOpen, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            SendMessageW(g_hBtnClose, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            SendMessageW(g_hBtnLang, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            SendMessageW(g_hBtnExit, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            // Header Labels and Read-only Edits
            #define CREATE_HDR_LBL(var, text, y) var = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_RIGHT, 10, y, 78, 20, hWnd, NULL, GetModuleHandle(NULL), NULL); SendMessageW(var, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
            #define CREATE_HDR_TXT(var, y) var = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 95, y, 200, 20, hWnd, NULL, GetModuleHandle(NULL), NULL); SendMessageW(var, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            CREATE_HDR_LBL(g_hLblFrom, L"보내는 사람:", 50);
            CREATE_HDR_TXT(g_hTxtFrom, 50);
            CREATE_HDR_LBL(g_hLblDate, L"전송:", 50);
            CREATE_HDR_TXT(g_hTxtDate, 50);

            CREATE_HDR_LBL(g_hLblTo, L"받는 사람:", 74);
            CREATE_HDR_TXT(g_hTxtTo, 74);
            CREATE_HDR_LBL(g_hLblCc, L"참조:", 98);
            CREATE_HDR_TXT(g_hTxtCc, 98);
            CREATE_HDR_LBL(g_hLblBcc, L"숨은참조:", 122);
            CREATE_HDR_TXT(g_hTxtBcc, 122);

            CREATE_HDR_LBL(g_hLblSubject, L"제목:", 146);
            g_hTxtSubject = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL, 95, 146, 200, 40, hWnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(g_hTxtSubject, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            // Attachment Header
            g_hLblAttach = CreateWindowExW(0, L"STATIC", L"첨부파일", WS_CHILD | WS_VISIBLE | SS_LEFT, 12, 500, 200, 20, hWnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(g_hLblAttach, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            // RichEdit Fallback
            LoadLibraryW(L"Msftedit.dll");
            g_hRichEditFallback = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
                WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
                0, 194, 800, 300, hWnd, NULL, GetModuleHandle(NULL), NULL);
            SendMessageW(g_hRichEditFallback, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            // OleBrowser Single Instance
            RECT rc = { 0, 194, 840, 500 };
            g_browser = OleBrowser_Create(hWnd, &rc);

            ClearMessageView();
            ApplyLanguage();
            return 0;
        }

        case WM_SIZE: {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);

            // Right-align Exit and Lang buttons
            SetWindowPos(g_hBtnExit, NULL, w - 90, 8, 80, 30, SWP_NOZORDER);
            SetWindowPos(g_hBtnLang, NULL, w - 370, 8, 272, 30, SWP_NOZORDER);

            UpdateHeaderLayout(w, h);
            InvalidateRect(hWnd, NULL, TRUE);
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(60, 64, 67));
            return (LRESULT)g_hBrushHeader;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(248, 249, 250));
            SetTextColor(hdc, RGB(32, 33, 36));
            return (LRESULT)g_hBrushHeader;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_BTN_OPEN) {
                OpenFileDialog();
            } else if (id == IDC_BTN_CLOSE) {
                ClearMessageView();
            } else if (id == IDC_BTN_LANG) {
                ShowLanguageMenu();
            } else if (id == IDC_BTN_EXIT) {
                PostMessageW(hWnd, WM_CLOSE, 0, 0);
            } else if (id >= IDM_LANG_KO && id <= IDM_LANG_JA) {
                Localization_SetLanguage((AppLanguage)(id - IDM_LANG_KO));
                ApplyLanguage();
            } else if (id >= IDC_ATTACH_BASE && id < IDC_ATTACH_BASE + MAX_MSG_ATTACHMENTS) {
                SaveAttachment(id - IDC_ATTACH_BASE);
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_F2) {
                OpenFileDialog();
                return 0;
            } else if (wParam == VK_F4) {
                ClearMessageView();
                return 0;
            } else if (wParam == VK_F8) {
                Localization_SetLanguage(Localization_GetNextLanguage(g_currentLanguage));
                ApplyLanguage();
                return 0;
            } else if (GetKeyState(VK_CONTROL) & 0x8000) {
                if (wParam == '0') {
                    g_currentZoom = 100;
                    if (g_browser) OleBrowser_SetZoom(g_browser, g_currentZoom);
                    return 0;
                } else if (wParam == VK_OEM_PLUS || wParam == VK_ADD) {
                    g_currentZoom += 10;
                    if (g_currentZoom > 300) g_currentZoom = 300;
                    if (g_browser) OleBrowser_SetZoom(g_browser, g_currentZoom);
                    return 0;
                } else if (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT) {
                    g_currentZoom -= 10;
                    if (g_currentZoom < 50) g_currentZoom = 50;
                    if (g_browser) OleBrowser_SetZoom(g_browser, g_currentZoom);
                    return 0;
                }
            }
            break;
        }

        case WM_MOUSEWHEEL: {
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                short delta = (short)HIWORD(wParam);
                if (delta > 0) g_currentZoom += 10;
                else if (delta < 0) g_currentZoom -= 10;

                if (g_currentZoom > 300) g_currentZoom = 300;
                if (g_currentZoom < 50) g_currentZoom = 50;
                if (g_browser) OleBrowser_SetZoom(g_browser, g_currentZoom);
                return 0;
            }
            break;
        }

        case WM_DESTROY: {
            ClearMessageView();
            if (g_browser) {
                OleBrowser_Destroy(g_browser);
                g_browser = NULL;
            }
            if (g_arena) {
                Arena_Destroy(g_arena);
                g_arena = NULL;
            }
            if (g_hFontNormal) DeleteObject(g_hFontNormal);
            if (g_hFontBold) DeleteObject(g_hFontBold);
            if (g_hBrushHeader) DeleteObject(g_hBrushHeader);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // Initialize OLE
    OleInitialize(NULL);
    InitCommonControls();

    // Register IE11 Browser Emulation
    RegisterBrowserEmulation();

    // Create Global 16 MB Arena
    g_arena = Arena_Create(16 * 1024 * 1024);
    g_currentLanguage = Localization_DetectLanguage();

    // Register Window Class
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"MsgViewerNativeClass";

    RegisterClassExW(&wc);

    // Create Main Window
    g_hMainWnd = CreateWindowExW(0, L"MsgViewerNativeClass", L"MSG Viewer",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 860, 680,
        NULL, NULL, hInstance, NULL);

    if (!g_hMainWnd) {
        OleUninitialize();
        return 1;
    }

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    // Check command line arguments for initial .msg file
    if (lpCmdLine && wcslen(lpCmdLine) > 0) {
        wchar_t argPath[MAX_PATH];
        StringCchCopyW(argPath, MAX_PATH, lpCmdLine);
        // Trim quotes if wrapped in "path"
        wchar_t* p = argPath;
        if (*p == L'"') {
            p++;
            size_t l = wcslen(p);
            if (l > 0 && p[l - 1] == L'"') p[l - 1] = L'\0';
        }
        LoadMsgFile(p);
    }

    // Message Loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        // Handle global accelerator keys F2, F4, F8
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_F2 || msg.wParam == VK_F4 || msg.wParam == VK_F8) {
                SendMessageW(g_hMainWnd, WM_KEYDOWN, msg.wParam, msg.lParam);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    OleUninitialize();
    return (int)msg.wParam;
}
