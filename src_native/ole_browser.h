#ifndef OLE_BROWSER_H
#define OLE_BROWSER_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OleBrowser OleBrowser;

// Creates the single reusable MSHTML browser window inside parent HWND
OleBrowser* OleBrowser_Create(HWND parentHwnd, const RECT* rect);

// Updates HTML body directly without recreating the browser instance (0 COM leaks!)
int OleBrowser_SetHtml(OleBrowser* browser, const char* utf8Html);

// Clears body to blank
void OleBrowser_Clear(OleBrowser* browser);

// Adjusts zoom level (e.g. 100 for 100%, 120 for 120%, etc.)
void OleBrowser_SetZoom(OleBrowser* browser, int zoomPercent);

// Resizes browser to match new bounds
void OleBrowser_Resize(OleBrowser* browser, const RECT* rect);

// Destroys browser at application termination
void OleBrowser_Destroy(OleBrowser* browser);

#ifdef __cplusplus
}
#endif

#endif // OLE_BROWSER_H
