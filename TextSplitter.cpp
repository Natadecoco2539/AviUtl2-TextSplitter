// ------------------------------------------------------------
// textSplitter.cpp
// v1.2
// ------------------------------------------------------------

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <stdio.h>

#include "plugin2.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// --- 定数定義 ---
#define IDC_SPLIT_BTN    1001
#define IDC_CONFIRM_MSG  2001
#define IDC_CONFIRM_OK   2002
#define IDC_CONFIRM_NO   2003

// --- カラー定義 ---
#define COL_BG          RGB(52, 52, 52)   
#define COL_BTN_BG      RGB(96, 96, 96)
#define COL_BTN_PUSH    RGB(70, 70, 70)
#define COL_TEXT        RGB(255, 255, 255)

// --- グローバル変数 ---
EDIT_HANDLE* g_edit = nullptr;
HBRUSH g_hBrBg = nullptr;
HBRUSH g_hBrBtn = nullptr;
HBRUSH g_hBrBtnPush = nullptr;
HFONT hGuiFont = nullptr;
wchar_t g_confirmMsg[256]; 

// --- ユーティリティ ---
std::wstring Utf8ToWide(const char* src) {
    if (!src || src[0] == '\0') return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
    if (size <= 0) return L"";
    std::vector<wchar_t> buf(size);
    MultiByteToWideChar(CP_UTF8, 0, src, -1, buf.data(), size);
    return std::wstring(buf.data());
}

std::string WideToUtf8(const std::wstring& src) {
    if (src.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, src.c_str(), -1, NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    std::vector<char> buf(size);
    WideCharToMultiByte(CP_UTF8, 0, src.c_str(), -1, buf.data(), size, NULL, NULL);
    return std::string(buf.data());
}

std::string ExtractTextFromAlias(const char* alias) {
    if (!alias) return "";
    std::string data = alias;
    std::string key = "テキスト=";
    size_t pos = data.find("\n" + key);
    if (pos == std::string::npos) {
        if (data.find(key) == 0) pos = 0; else return "";
    } else { 
        pos += 1; 
    }
    pos += key.length();
    size_t endPos = data.find_first_of("\r\n", pos);
    if (endPos == std::string::npos) endPos = data.length();
    return data.substr(pos, endPos - pos);
}

// --- ダイアログ構築クラス ---
class DialogBuilder {
    std::vector<unsigned char> data;
public:
    void Align() { while (data.size() % 4 != 0) data.push_back(0); }
    void Add(const void* p, size_t s) { const unsigned char* b = (const unsigned char*)p; data.insert(data.end(), b, b + s); }
    void AddW(WORD w) { Add(&w, 2); }
    void AddD(DWORD d) { Add(&d, 4); }
    void AddS(LPCWSTR s) { if (!s) AddW(0); else Add(s, (wcslen(s) + 1) * 2); }
    
    void Begin(LPCWSTR title, int w, int h, DWORD style) {
        data.clear();
        Align();
        DWORD exStyle = WS_EX_CONTROLPARENT;
        AddD(style); AddD(exStyle);
        AddW(0); 
        AddW(0); AddW(0); AddW(w); AddW(h);
        AddW(0); AddW(0); AddS(title); AddW(9); AddS(L"Yu Gothic UI");
    }
    
    void AddControl(LPCWSTR className, LPCWSTR text, WORD id, int x, int y, int w, int h, DWORD style) {
        Align();
        if (wcscmp(className, L"BUTTON") == 0) style |= BS_OWNERDRAW;
        AddD(style | WS_CHILD | WS_VISIBLE);
        AddD(0); AddW(x); AddW(y); AddW(w); AddW(h); AddW(id);
        if (wcscmp(className, L"BUTTON") == 0) { AddW(0xFFFF); AddW(0x0080); }
        else if (wcscmp(className, L"STATIC") == 0) { AddW(0xFFFF); AddW(0x0082); }
        else AddS(className);
        AddS(text); AddW(0);

        if (data.size() >= 10) {
            WORD* pCount = (WORD*)&data[8];
            (*pCount)++;
        }
    }
    DLGTEMPLATE* Get() { return (DLGTEMPLATE*)data.data(); }
};

// --- 確認ダイアログプロシージャ ---
INT_PTR CALLBACK ConfirmDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG:
        SetDlgItemTextW(hwnd, IDC_CONFIRM_MSG, g_confirmMsg);
        EnumChildWindows(hwnd, [](HWND hChild, LPARAM lp){ 
            SendMessage(hChild, WM_SETFONT, (WPARAM)hGuiFont, 0); return TRUE; 
        }, 0);
        return TRUE;

    case WM_CTLCOLORDLG: return (INT_PTR)g_hBrBg;
    case WM_CTLCOLORSTATIC: SetTextColor((HDC)wp, COL_TEXT); SetBkMode((HDC)wp, TRANSPARENT); return (INT_PTR)g_hBrBg;

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT p = (LPDRAWITEMSTRUCT)lp;
        if (p->CtlType == ODT_BUTTON) {
            HBRUSH hBrush = (p->itemState & ODS_SELECTED) ? g_hBrBtnPush : g_hBrBtn;
            FillRect(p->hDC, &p->rcItem, hBrush);
            SetTextColor(p->hDC, COL_TEXT); SetBkMode(p->hDC, TRANSPARENT);
            WCHAR t[32]; GetWindowTextW(p->hwndItem, t, 32);
            DrawTextW(p->hDC, t, -1, &p->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        return FALSE;
    }

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_CONFIRM_OK) EndDialog(hwnd, IDOK);
        else if (LOWORD(wp) == IDC_CONFIRM_NO) EndDialog(hwnd, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

// --- メイン: テキスト分割処理 ---
void SplitText(EDIT_SECTION* edit) {
    OBJECT_HANDLE source = edit->get_focus_object();
    if (!source) { MessageBoxW(NULL, L"テキストオブジェクトを選択してください", L"エラー", MB_OK); return; }

    const char* alias = edit->get_object_alias(source);
    if (!alias) return;

    std::string textUtf8Str = ExtractTextFromAlias(alias);
    if (textUtf8Str.empty()) { MessageBoxW(NULL, L"テキストが見つかりません", L"確認", MB_OK); return; }

    std::wstring text = Utf8ToWide(textUtf8Str.c_str());
    
    // 1. 文字数カウント
    int charCount = 0;
    for (size_t i = 0; i < text.length(); ) {
        if (text[i] == L'<') { // タグスキップ
            size_t close = text.find(L'>', i);
            if (close != std::wstring::npos) { i = close + 1; continue; }
        }
        if (text[i] == L'\r' || text[i] == L'\n') { i++; continue; } 
        
        charCount++;
        if (IS_HIGH_SURROGATE(text[i]) && i + 1 < text.length() && IS_LOW_SURROGATE(text[i+1])) i += 2;
        else i++;
    }

    if (charCount == 0) return;

    swprintf_s(g_confirmMsg, 256, L"%d 個のオブジェクトに分割します。", charCount);

    static DialogBuilder dbConf;
    if (dbConf.Get() == nullptr) {
        dbConf.Begin(L"確認", 260, 100, WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_SETFONT | DS_MODALFRAME | DS_CENTER);
        dbConf.AddControl(L"STATIC", L"", IDC_CONFIRM_MSG, 10, 20, 240, 20, SS_CENTER);
        dbConf.AddControl(L"BUTTON", L"はい", IDC_CONFIRM_OK, 40, 60, 80, 25, BS_PUSHBUTTON);
        dbConf.AddControl(L"BUTTON", L"いいえ", IDC_CONFIRM_NO, 140, 60, 80, 25, BS_PUSHBUTTON);
    }
    
    if (DialogBoxIndirectParamW(GetModuleHandle(NULL), dbConf.Get(), NULL, ConfirmDlgProc, 0) != IDOK) return;

    // 2. 分割実行
    OBJECT_LAYER_FRAME info = edit->get_object_layer_frame(source);
    std::wstring accumulatedTags = L""; 
    int createdCount = 0;

    for (size_t i = 0; i < text.length(); ) {
        if (text[i] == L'<' && i + 1 < text.length()) {
            size_t close = text.find(L'>', i);
            if (close != std::wstring::npos) {
                std::wstring tag = text.substr(i, close - i + 1);
                if (tag.find(L"<r") == 0) accumulatedTags.clear();
                else accumulatedTags += tag;
                i = close + 1;
                continue;
            }
        }

        std::wstring charStr;
        if (IS_HIGH_SURROGATE(text[i]) && i + 1 < text.length() && IS_LOW_SURROGATE(text[i+1])) {
            charStr = text.substr(i, 2); i += 2;
        } else {
            charStr = text.substr(i, 1); i += 1;
        }

        if (charStr == L"\r" || charStr == L"\n") continue;

        std::string finalUtf8 = WideToUtf8(accumulatedTags + charStr);
        int targetLayer = info.layer + 1 + createdCount;
        
        OBJECT_HANDLE newObj = edit->create_object_from_alias(alias, targetLayer, info.start, info.end - info.start);
        if (newObj) {
            edit->set_object_item_value(newObj, L"テキスト", L"テキスト", finalUtf8.c_str());
            createdCount++;
        }
    }
}

// --- メインダイアログプロシージャ ---
INT_PTR CALLBACK MainDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG:
        if (!hGuiFont) hGuiFont = CreateFontW(14,0,0,0,FW_NORMAL,0,0,0,SHIFTJIS_CHARSET,0,0,0,0,L"Yu Gothic UI");
        return TRUE;

    case WM_SIZE: {
        int w = LOWORD(lp); int h = HIWORD(lp);
        HWND hBtn = GetDlgItem(hwnd, IDC_SPLIT_BTN);
        if (hBtn) SetWindowPos(hBtn, NULL, 0, 0, w, h, SWP_NOZORDER);
        return TRUE;
    }

    case WM_MOUSEACTIVATE: return MA_ACTIVATE;

    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &pt);
        RECT rc; GetClientRect(hwnd, &rc);
        if (pt.x >= rc.right - 5 && pt.y >= rc.bottom - 5) return HTBOTTOMRIGHT;
        if (pt.x >= rc.right - 2) return HTRIGHT;
        if (pt.y >= rc.bottom - 2) return HTBOTTOM;
        return HTCAPTION;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_hBrBg);
        return TRUE;
    }

    case WM_CTLCOLORDLG: return (INT_PTR)g_hBrBg;
    case WM_CTLCOLORSTATIC: SetTextColor((HDC)wp, COL_TEXT); SetBkMode((HDC)wp, TRANSPARENT); return (INT_PTR)g_hBrBg;
    case WM_CTLCOLORBTN: return (INT_PTR)g_hBrBg;

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT p = (LPDRAWITEMSTRUCT)lp;
        if (p->CtlType == ODT_BUTTON) {
            HBRUSH hBrush = (p->itemState & ODS_SELECTED) ? g_hBrBtnPush : g_hBrBtn;
            FillRect(p->hDC, &p->rcItem, hBrush);
            SetTextColor(p->hDC, COL_TEXT); SetBkMode(p->hDC, TRANSPARENT);
            WCHAR t[32]; GetWindowTextW(p->hwndItem, t, 32);
            DrawTextW(p->hDC, t, -1, &p->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        return FALSE;
    }

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_SPLIT_BTN) {
            if (g_edit) g_edit->call_edit_section(SplitText);
        }
        return TRUE;
    }
    return FALSE;
}

// --- エントリーポイント (重複修正済み) ---
EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
    host->set_plugin_information(L"テキスト分割プラグイン v1.2");
    g_hBrBg = CreateSolidBrush(COL_BG);
    g_hBrBtn = CreateSolidBrush(COL_BTN_BG);
    g_hBrBtnPush = CreateSolidBrush(COL_BTN_PUSH);

    static DialogBuilder db;
    // 初期サイズ 200x40
    db.Begin(L"分割", 200, 40, WS_POPUP | WS_VISIBLE | DS_SETFONT | WS_CLIPCHILDREN);
    // 全面ボタン配置
    db.AddControl(L"BUTTON", L"テキスト分割", IDC_SPLIT_BTN, 0, 0, 200, 40, BS_PUSHBUTTON);

    HWND hwnd = CreateDialogIndirectParamW(GetModuleHandle(NULL), db.Get(), NULL, MainDlgProc, 0);
    if (hwnd) host->register_window_client(L"テキスト分割", hwnd);

    host->register_object_menu(L"テキスト分割", SplitText);
    g_edit = host->create_edit_handle();
}

EXTERN_C __declspec(dllexport) bool InitializePlugin(DWORD) { return true; }

EXTERN_C __declspec(dllexport) void UninitializePlugin() {
    DeleteObject(g_hBrBg); 
    DeleteObject(g_hBrBtn);
    DeleteObject(g_hBrBtnPush);
    if (hGuiFont) DeleteObject(hGuiFont);
}
