#include "ole_browser.h"
#include <ole2.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <exdisp.h>
#include <docobj.h>
#include <strsafe.h>

typedef struct {
    IOleClientSite      clientSite;
    IOleInPlaceSite     inPlaceSite;
    IOleInPlaceFrame    inPlaceFrame;
    IDocHostUIHandler   docHostUIHandler;
} BrowserSite;

struct OleBrowser {
    HWND            hWndParent;
    HWND            hWndControl;
    RECT            bounds;
    IOleObject*     pOleObject;
    IWebBrowser2*   pWebBrowser;
    BrowserSite     site;
};

// --- IOleClientSite Vtbl ---
static HRESULT STDMETHODCALLTYPE CS_QueryInterface(IOleClientSite* This, REFIID riid, void** ppvObject) {
    BrowserSite* site = (BrowserSite*)This;
    if (!ppvObject) return E_POINTER;
    *ppvObject = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IOleClientSite)) {
        *ppvObject = &site->clientSite;
    } else if (IsEqualIID(riid, &IID_IOleInPlaceSite)) {
        *ppvObject = &site->inPlaceSite;
    } else if (IsEqualIID(riid, &IID_IDocHostUIHandler)) {
        *ppvObject = &site->docHostUIHandler;
    } else {
        return E_NOINTERFACE;
    }

    return S_OK;
}

static ULONG STDMETHODCALLTYPE CS_AddRef(IOleClientSite* This) { return 1; }
static ULONG STDMETHODCALLTYPE CS_Release(IOleClientSite* This) { return 1; }
static HRESULT STDMETHODCALLTYPE CS_SaveObject(IOleClientSite* This) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE CS_GetMoniker(IOleClientSite* This, DWORD dwA, DWORD dwW, IMoniker** ppmk) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE CS_GetContainer(IOleClientSite* This, IOleContainer** ppC) { *ppC = NULL; return E_NOINTERFACE; }
static HRESULT STDMETHODCALLTYPE CS_ShowObject(IOleClientSite* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE CS_OnShowWindow(IOleClientSite* This, BOOL fShow) { return S_OK; }
static HRESULT STDMETHODCALLTYPE CS_RequestNewObjectLayout(IOleClientSite* This) { return E_NOTIMPL; }

static const IOleClientSiteVtbl s_ClientSiteVtbl = {
    CS_QueryInterface, CS_AddRef, CS_Release,
    CS_SaveObject, CS_GetMoniker, CS_GetContainer,
    CS_ShowObject, CS_OnShowWindow, CS_RequestNewObjectLayout
};

// --- IOleInPlaceSite Vtbl ---
static HRESULT STDMETHODCALLTYPE IPS_QueryInterface(IOleInPlaceSite* This, REFIID riid, void** ppvObject) {
    BrowserSite* site = (BrowserSite*)((char*)This - offsetof(BrowserSite, inPlaceSite));
    return CS_QueryInterface(&site->clientSite, riid, ppvObject);
}
static ULONG STDMETHODCALLTYPE IPS_AddRef(IOleInPlaceSite* This) { return 1; }
static ULONG STDMETHODCALLTYPE IPS_Release(IOleInPlaceSite* This) { return 1; }

static HRESULT STDMETHODCALLTYPE IPS_GetWindow(IOleInPlaceSite* This, HWND* phwnd) {
    OleBrowser* b = (OleBrowser*)((char*)This - offsetof(OleBrowser, site.inPlaceSite));
    *phwnd = b->hWndParent;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE IPS_ContextSensitiveHelp(IOleInPlaceSite* This, BOOL fEnterMode) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE IPS_CanInPlaceActivate(IOleInPlaceSite* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE IPS_OnInPlaceActivate(IOleInPlaceSite* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE IPS_OnUIActivate(IOleInPlaceSite* This) { return S_OK; }

static HRESULT STDMETHODCALLTYPE IPS_GetWindowContext(
    IOleInPlaceSite* This,
    IOleInPlaceFrame** ppFrame,
    IOleInPlaceUIWindow** ppDoc,
    LPRECT lprcPosRect,
    LPRECT lprcClipRect,
    LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
    OleBrowser* b = (OleBrowser*)((char*)This - offsetof(OleBrowser, site.inPlaceSite));
    *ppFrame = &b->site.inPlaceFrame;
    *ppDoc = NULL;

    GetClientRect(b->hWndParent, lprcPosRect);
    GetClientRect(b->hWndParent, lprcClipRect);

    lpFrameInfo->cb = sizeof(OLEINPLACEFRAMEINFO);
    lpFrameInfo->fMDIApp = FALSE;
    lpFrameInfo->hwndFrame = b->hWndParent;
    lpFrameInfo->haccel = NULL;
    lpFrameInfo->cAccelEntries = 0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE IPS_Scroll(IOleInPlaceSite* This, SIZE scrollExt) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE IPS_OnUIDeactivate(IOleInPlaceSite* This, BOOL fUndoable) { return S_OK; }
static HRESULT STDMETHODCALLTYPE IPS_OnInPlaceDeactivate(IOleInPlaceSite* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE IPS_DiscardUndoState(IOleInPlaceSite* This) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE IPS_DeactivateAndUndo(IOleInPlaceSite* This) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE IPS_OnPosRectChange(IOleInPlaceSite* This, LPCRECT lprcPosRect) { return S_OK; }

static const IOleInPlaceSiteVtbl s_InPlaceSiteVtbl = {
    IPS_QueryInterface, IPS_AddRef, IPS_Release,
    IPS_GetWindow, IPS_ContextSensitiveHelp,
    IPS_CanInPlaceActivate, IPS_OnInPlaceActivate, IPS_OnUIActivate,
    IPS_GetWindowContext, IPS_Scroll, IPS_OnUIDeactivate, IPS_OnInPlaceDeactivate,
    IPS_DiscardUndoState, IPS_DeactivateAndUndo, IPS_OnPosRectChange
};

// --- IOleInPlaceFrame Vtbl ---
static HRESULT STDMETHODCALLTYPE IPF_QueryInterface(IOleInPlaceFrame* This, REFIID riid, void** ppv) { return E_NOTIMPL; }
static ULONG STDMETHODCALLTYPE IPF_AddRef(IOleInPlaceFrame* This) { return 1; }
static ULONG STDMETHODCALLTYPE IPF_Release(IOleInPlaceFrame* This) { return 1; }
static HRESULT STDMETHODCALLTYPE IPF_GetWindow(IOleInPlaceFrame* This, HWND* phwnd) {
    OleBrowser* b = (OleBrowser*)((char*)This - offsetof(OleBrowser, site.inPlaceFrame));
    *phwnd = b->hWndParent;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE IPF_ContextSensitiveHelp(IOleInPlaceFrame* This, BOOL fEnter) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE IPF_GetBorder(IOleInPlaceFrame* This, LPRECT lprect) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE IPF_RequestBorderSpace(IOleInPlaceFrame* This, LPCBORDERWIDTHS p) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE IPF_SetBorderSpace(IOleInPlaceFrame* This, LPCBORDERWIDTHS p) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE IPF_SetActiveObject(IOleInPlaceFrame* This, IOleInPlaceActiveObject* p, LPCOLESTR s) { return S_OK; }
static HRESULT STDMETHODCALLTYPE IPF_InsertMenus(IOleInPlaceFrame* This, HMENU hmenu, LPOLEMENUGROUPWIDTHS p) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE IPF_SetMenu(IOleInPlaceFrame* This, HMENU hmenu, HOLEMENU h, HWND hw) { return S_OK; }
static HRESULT STDMETHODCALLTYPE IPF_RemoveMenus(IOleInPlaceFrame* This, HMENU hmenu) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE IPF_SetStatusText(IOleInPlaceFrame* This, LPCOLESTR t) { return S_OK; }
static HRESULT STDMETHODCALLTYPE IPF_EnableModeless(IOleInPlaceFrame* This, BOOL f) { return S_OK; }
static HRESULT STDMETHODCALLTYPE IPF_TranslateAccelerator(IOleInPlaceFrame* This, LPMSG lpmsg, WORD w) { return E_NOTIMPL; }

static const IOleInPlaceFrameVtbl s_InPlaceFrameVtbl = {
    IPF_QueryInterface, IPF_AddRef, IPF_Release,
    IPF_GetWindow, IPF_ContextSensitiveHelp,
    IPF_GetBorder, IPF_RequestBorderSpace, IPF_SetBorderSpace, IPF_SetActiveObject,
    IPF_InsertMenus, IPF_SetMenu, IPF_RemoveMenus, IPF_SetStatusText, IPF_EnableModeless, IPF_TranslateAccelerator
};

// --- IDocHostUIHandler Vtbl ---
static HRESULT STDMETHODCALLTYPE DH_QueryInterface(IDocHostUIHandler* This, REFIID riid, void** ppv) {
    BrowserSite* site = (BrowserSite*)((char*)This - offsetof(BrowserSite, docHostUIHandler));
    return CS_QueryInterface(&site->clientSite, riid, ppv);
}
static ULONG STDMETHODCALLTYPE DH_AddRef(IDocHostUIHandler* This) { return 1; }
static ULONG STDMETHODCALLTYPE DH_Release(IDocHostUIHandler* This) { return 1; }
static HRESULT STDMETHODCALLTYPE DH_ShowContextMenu(IDocHostUIHandler* This, DWORD dwID, POINT* ppt, IUnknown* pcmdtReserved, IDispatch* pdispReserved) {
    return S_FALSE; // Allow standard right-click context menu (copy, select all)
}
static HRESULT STDMETHODCALLTYPE DH_GetHostInfo(IDocHostUIHandler* This, DOCHOSTUIINFO* pInfo) {
    pInfo->cbSize = sizeof(DOCHOSTUIINFO);
    pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER | DOCHOSTUIFLAG_THEME | DOCHOSTUIFLAG_ENABLE_FORMS_AUTOCOMPLETE;
    pInfo->dwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE DH_ShowUI(IDocHostUIHandler* This, DWORD dwID, IOleInPlaceActiveObject* pActiveObject, IOleCommandTarget* pCommandTarget, IOleInPlaceFrame* pFrame, IOleInPlaceUIWindow* pDoc) { return S_OK; }
static HRESULT STDMETHODCALLTYPE DH_HideUI(IDocHostUIHandler* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE DH_UpdateUI(IDocHostUIHandler* This) { return S_OK; }
static HRESULT STDMETHODCALLTYPE DH_EnableModeless(IDocHostUIHandler* This, BOOL fEnable) { return S_OK; }
static HRESULT STDMETHODCALLTYPE DH_OnDocWindowActivate(IDocHostUIHandler* This, BOOL fActivate) { return S_OK; }
static HRESULT STDMETHODCALLTYPE DH_OnFrameWindowActivate(IDocHostUIHandler* This, BOOL fActivate) { return S_OK; }
static HRESULT STDMETHODCALLTYPE DH_ResizeBorder(IDocHostUIHandler* This, LPCRECT prcBorder, IOleInPlaceUIWindow* pUIWindow, BOOL fRameWindow) { return S_OK; }
static HRESULT STDMETHODCALLTYPE DH_TranslateAccelerator(IDocHostUIHandler* This, LPMSG lpMsg, const GUID* pguidCmdGroup, DWORD nCmdID) { return S_FALSE; }
static HRESULT STDMETHODCALLTYPE DH_GetOptionKeyPath(IDocHostUIHandler* This, LPOLESTR* pchKey, DWORD dw) { return S_FALSE; }
static HRESULT STDMETHODCALLTYPE DH_GetDropTarget(IDocHostUIHandler* This, IDropTarget* pDropTarget, IDropTarget** ppDropTarget) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE DH_GetExternal(IDocHostUIHandler* This, IDispatch** ppDispatch) { *ppDispatch = NULL; return S_FALSE; }
static HRESULT STDMETHODCALLTYPE DH_TranslateUrl(IDocHostUIHandler* This, DWORD dwTranslate, OLECHAR* pchURLIn, OLECHAR** ppchURLOut) { *ppchURLOut = NULL; return S_FALSE; }
static HRESULT STDMETHODCALLTYPE DH_FilterDataObject(IDocHostUIHandler* This, IDataObject* pDO, IDataObject** ppDORet) { *ppDORet = NULL; return S_FALSE; }

static const IDocHostUIHandlerVtbl s_DocHostUIHandlerVtbl = {
    DH_QueryInterface, DH_AddRef, DH_Release,
    DH_ShowContextMenu, DH_GetHostInfo, DH_ShowUI, DH_HideUI, DH_UpdateUI,
    DH_EnableModeless, DH_OnDocWindowActivate, DH_OnFrameWindowActivate,
    DH_ResizeBorder, DH_TranslateAccelerator, DH_GetOptionKeyPath, DH_GetDropTarget,
    DH_GetExternal, DH_TranslateUrl, DH_FilterDataObject
};

OleBrowser* OleBrowser_Create(HWND parentHwnd, const RECT* rect) {
    OleBrowser* b = (OleBrowser*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(OleBrowser));
    if (!b) return NULL;

    b->hWndParent = parentHwnd;
    if (rect) b->bounds = *rect;

    // Set Vtbls
    b->site.clientSite.lpVtbl = (IOleClientSiteVtbl*)&s_ClientSiteVtbl;
    b->site.inPlaceSite.lpVtbl = (IOleInPlaceSiteVtbl*)&s_InPlaceSiteVtbl;
    b->site.inPlaceFrame.lpVtbl = (IOleInPlaceFrameVtbl*)&s_InPlaceFrameVtbl;
    b->site.docHostUIHandler.lpVtbl = (IDocHostUIHandlerVtbl*)&s_DocHostUIHandlerVtbl;

    // Create single MSHTML WebBrowser instance
    HRESULT hr = CoCreateInstance(&CLSID_WebBrowser, NULL, CLSCTX_INPROC_SERVER, &IID_IOleObject, (void**)&b->pOleObject);
    if (FAILED(hr) || !b->pOleObject) {
        HeapFree(GetProcessHeap(), 0, b);
        return NULL;
    }

    hr = b->pOleObject->lpVtbl->SetClientSite(b->pOleObject, &b->site.clientSite);
    hr = b->pOleObject->lpVtbl->QueryInterface(b->pOleObject, &IID_IWebBrowser2, (void**)&b->pWebBrowser);

    RECT clientRect;
    GetClientRect(parentHwnd, &clientRect);
    hr = b->pOleObject->lpVtbl->DoVerb(b->pOleObject, OLEIVERB_INPLACEACTIVATE, NULL, &b->site.clientSite, 0, parentHwnd, &clientRect);

    // Initial navigate to about:blank to prepare document
    VARIANT varUrl;
    VariantInit(&varUrl);
    varUrl.vt = VT_BSTR;
    varUrl.bstrVal = SysAllocString(L"about:blank");
    b->pWebBrowser->lpVtbl->Navigate2(b->pWebBrowser, &varUrl, NULL, NULL, NULL, NULL);
    SysFreeString(varUrl.bstrVal);

    return b;
}

int OleBrowser_SetHtml(OleBrowser* browser, const char* utf8Html) {
    if (!browser || !browser->pWebBrowser) return 0;
    if (!utf8Html) utf8Html = "";

    IDispatch* pDisp = NULL;
    HRESULT hr = browser->pWebBrowser->lpVtbl->get_Document(browser->pWebBrowser, &pDisp);
    if (FAILED(hr) || !pDisp) {
        // Fallback: navigate about:blank first then retry
        VARIANT varUrl;
        VariantInit(&varUrl);
        varUrl.vt = VT_BSTR;
        varUrl.bstrVal = SysAllocString(L"about:blank");
        browser->pWebBrowser->lpVtbl->Navigate2(browser->pWebBrowser, &varUrl, NULL, NULL, NULL, NULL);
        SysFreeString(varUrl.bstrVal);

        hr = browser->pWebBrowser->lpVtbl->get_Document(browser->pWebBrowser, &pDisp);
        if (FAILED(hr) || !pDisp) return 0;
    }

    // Use IPersistStreamInit for direct, zero-leak stream writing to MSHTML document
    IPersistStreamInit* pPsi = NULL;
    hr = pDisp->lpVtbl->QueryInterface(pDisp, &IID_IPersistStreamInit, (void**)&pPsi);
    if (SUCCEEDED(hr) && pPsi) {
        size_t len = strlen(utf8Html);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem) {
            void* pMem = GlobalLock(hMem);
            if (pMem) {
                memcpy(pMem, utf8Html, len);
                GlobalUnlock(hMem);

                IStream* pStream = NULL;
                if (SUCCEEDED(CreateStreamOnHGlobal(hMem, TRUE, &pStream))) {
                    pPsi->lpVtbl->InitNew(pPsi);
                    pPsi->lpVtbl->Load(pPsi, pStream);
                    pStream->lpVtbl->Release(pStream);
                }
            } else {
                GlobalFree(hMem);
            }
        }
        pPsi->lpVtbl->Release(pPsi);
    }
    pDisp->lpVtbl->Release(pDisp);
    return 1;
}

void OleBrowser_Clear(OleBrowser* browser) {
    OleBrowser_SetHtml(browser, "<!DOCTYPE html><html><head><meta http-equiv=\"X-UA-Compatible\" content=\"IE=Edge\"></head><body></body></html>");
}

void OleBrowser_SetZoom(OleBrowser* browser, int zoomPercent) {
    if (!browser || !browser->pWebBrowser) return;

    IOleCommandTarget* pCmdTarget = NULL;
    HRESULT hr = browser->pWebBrowser->lpVtbl->QueryInterface(browser->pWebBrowser, &IID_IOleCommandTarget, (void**)&pCmdTarget);
    if (SUCCEEDED(hr) && pCmdTarget) {
        VARIANT varZoom;
        VariantInit(&varZoom);
        varZoom.vt = VT_I4;
        varZoom.lVal = zoomPercent;

        const GUID CGID_MSHTML_LOCAL = { 0xDE4BA900, 0x59CA, 0x11CF, { 0x95, 0x92, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
        pCmdTarget->lpVtbl->Exec(pCmdTarget, &CGID_MSHTML_LOCAL, OLECMDID_OPTICAL_ZOOM, OLECMDEXECOPT_DONTPROMPTUSER, &varZoom, NULL);

        pCmdTarget->lpVtbl->Release(pCmdTarget);
    }
}

void OleBrowser_Resize(OleBrowser* browser, const RECT* rect) {
    if (!browser || !browser->pOleObject || !rect) return;
    browser->bounds = *rect;

    IOleInPlaceObject* pInPlace = NULL;
    if (SUCCEEDED(browser->pOleObject->lpVtbl->QueryInterface(browser->pOleObject, &IID_IOleInPlaceObject, (void**)&pInPlace))) {
        pInPlace->lpVtbl->SetObjectRects(pInPlace, rect, rect);
        pInPlace->lpVtbl->Release(pInPlace);
    }
}

void OleBrowser_Destroy(OleBrowser* browser) {
    if (!browser) return;
    if (browser->pWebBrowser) {
        browser->pWebBrowser->lpVtbl->Release(browser->pWebBrowser);
        browser->pWebBrowser = NULL;
    }
    if (browser->pOleObject) {
        browser->pOleObject->lpVtbl->Close(browser->pOleObject, OLECLOSE_NOSAVE);
        browser->pOleObject->lpVtbl->Release(browser->pOleObject);
        browser->pOleObject = NULL;
    }
    HeapFree(GetProcessHeap(), 0, browser);
}
