// ------------------------------------------------------------
// textSplitter.cpp
// v1.1
// ------------------------------------------------------------

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <stdio.h>

#include "plugin2.h" // AviUtl2公式ヘッダー 

// ビジュアルスタイル有効化 (コモンコントロール v6)
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ==============================================================================
//  定数・設定
// ==============================================================================

// コントロールID
#define IDC_SPLIT_BTN    1001   // メイン画面の分割ボタン
#define IDC_CONFIRM_MSG  2001   // 確認ダイアログのメッセージ
#define IDC_CONFIRM_OK   2002   // 確認ダイアログのOKボタン
#define IDC_CONFIRM_NO   2003   // 確認ダイアログのキャンセルボタン

// デザインカラー定義 (ダークテーマ)
#define COL_BG          RGB(52, 52, 52)     // 背景色
#define COL_BTN_BG      RGB(96, 96, 96)     // ボタン通常色
#define COL_BTN_PUSH    RGB(70, 70, 70)     // ボタン押下色
#define COL_TEXT        RGB(255, 255, 255)  // 文字色

// ==============================================================================
//  グローバル変数
// ==============================================================================
EDIT_HANDLE* g_edit = nullptr;      // 編集ハンドル
HBRUSH g_hBrBg = nullptr;           // 背景用ブラシ
HBRUSH g_hBrBtn = nullptr;          // ボタン用ブラシ
HBRUSH g_hBrBtnPush = nullptr;      // ボタン押下用ブラシ
HFONT hGuiFont = nullptr;           // UI用フォント
wchar_t g_confirmMsg[256];          // 確認メッセージ受け渡し用バッファ

// ==============================================================================
//  ユーティリティ関数
// ==============================================================================

// UTF-8文字列を ワイド文字列(UTF-16) に変換
std::wstring Utf8ToWide(const char* src) {
    if (!src || src[0] == '\0') return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
    if (size <= 0) return L"";
    std::vector<wchar_t> buf(size);
    MultiByteToWideChar(CP_UTF8, 0, src, -1, buf.data(), size);
    return std::wstring(buf.data());
}

// ワイド文字列(UTF-16)を UTF-8文字列 に変換
std::string WideToUtf8(const std::wstring& src) {
    if (src.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, src.c_str(), -1, NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    std::vector<char> buf(size);
    WideCharToMultiByte(CP_UTF8, 0, src.c_str(), -1, buf.data(), size, NULL, NULL);
    return std::string(buf.data());
}

// エイリアスデータから「テキスト=...」の内容を抽出する
std::string ExtractTextFromAlias(const char* alias) {
    if (!alias) return "";
    std::string data = alias;
    std::string key = "テキスト=";
    
    // キーの検索
    size_t pos = data.find("\n" + key);
    if (pos == std::string::npos) {
        if (data.find(key) == 0) pos = 0; else return "";
    } else { 
        pos += 1; // 改行文字分進める
    }
    
    pos += key.length();
    
    // 行末までを取得
    size_t endPos = data.find_first_of("\r\n", pos);
    if (endPos == std::string::npos) endPos = data.length();
    
    return data.substr(pos, endPos - pos);
}

// ==============================================================================
//  ダイアログ構築クラス (DialogBuilder)
//  メモリ上でダイアログテンプレートを作成するためのヘルパークラスです。
// ==============================================================================
class DialogBuilder {
    std::vector<unsigned char> data;
public:
    // 4バイト境界へのパディング
    void Align() { while (data.size() % 4 != 0) data.push_back(0); }
    
    // データ追加ヘルパー
    void Add(const void* p, size_t s) { const unsigned char* b = (const unsigned char*)p; data.insert(data.end(), b, b + s); }
    void AddW(WORD w) { Add(&w, 2); }
    void AddD(DWORD d) { Add(&d, 4); }
    void AddS(LPCWSTR s) { if (!s) AddW(0); else Add(s, (wcslen(s) + 1) * 2); }
    
    // ダイアログ定義の開始
    void Begin(LPCWSTR title, int w, int h, DWORD style) {
        data.clear();
        Align();
        DWORD exStyle = WS_EX_CONTROLPARENT;
        AddD(style); AddD(exStyle);
        AddW(0); // アイテム数 (後で更新されます) [Offset 8]
        AddW(0); AddW(0); AddW(w); AddW(h);
        AddW(0); AddW(0); AddS(title); AddW(9); AddS(L"Yu Gothic UI");
    }
    
    // コントロールの追加
    void AddControl(LPCWSTR className, LPCWSTR text, WORD id, int x, int y, int w, int h, DWORD style) {
        Align();
        if (wcscmp(className, L"BUTTON") == 0) style |= BS_OWNERDRAW; // ボタンは自前描画(フラット化のため)
        
        AddD(style | WS_CHILD | WS_VISIBLE);
        AddD(0); AddW(x); AddW(y); AddW(w); AddW(h); AddW(id);
        
        // クラス名の指定 (定義済みクラスはIDで指定)
        if (wcscmp(className, L"BUTTON") == 0) { AddW(0xFFFF); AddW(0x0080); }
        else if (wcscmp(className, L"STATIC") == 0) { AddW(0xFFFF); AddW(0x0082); }
        else AddS(className);
        
        AddS(text); AddW(0); // テキストと作成データ

        // アイテム数をカウントアップしてヘッダー(Offset 8)を更新
        if (data.size() >= 10) {
            WORD* pCount = (WORD*)&data[8];
            (*pCount)++;
        }
    }
    
    DLGTEMPLATE* Get() { return (DLGTEMPLATE*)data.data(); }
};

// ==============================================================================
//  確認ダイアログのプロシージャ (ダークテーマ対応)
// ==============================================================================
INT_PTR CALLBACK ConfirmDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG:
        // メッセージを設定
        SetDlgItemTextW(hwnd, IDC_CONFIRM_MSG, g_confirmMsg);
        // フォント適用
        EnumChildWindows(hwnd, [](HWND hChild, LPARAM lp){ 
            SendMessage(hChild, WM_SETFONT, (WPARAM)hGuiFont, 0); return TRUE; 
        }, 0);
        return TRUE;

    // --- 色設定 (ダークテーマ) ---
    case WM_CTLCOLORDLG: return (INT_PTR)g_hBrBg;
    case WM_CTLCOLORSTATIC: SetTextColor((HDC)wp, COL_TEXT); SetBkMode((HDC)wp, TRANSPARENT); return (INT_PTR)g_hBrBg;

    // --- ボタンの自前描画 (フラットデザイン) ---
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

// ==============================================================================
//  メインロジック: テキスト分割処理
// ==============================================================================
void SplitText(EDIT_SECTION* edit) {
    // 1. 選択オブジェクトの取得
    OBJECT_HANDLE source = edit->get_focus_object();
    if (!source) {
        MessageBoxW(NULL, L"テキストオブジェクトを選択してください", L"エラー", MB_OK);
        return;
    }

    // 2. エイリアスデータの取得
    const char* alias = edit->get_object_alias(source);
    if (!alias) {
        MessageBoxW(NULL, L"データの取得に失敗しました", L"エラー", MB_OK);
        return;
    }

    // 3. テキスト内容の抽出
    std::string textUtf8Str = ExtractTextFromAlias(alias);
    if (textUtf8Str.empty()) {
        MessageBoxW(NULL, L"テキストが見つかりません", L"確認", MB_OK);
        return;
    }

    // 4. 文字数カウント (UTF-16変換)
    std::wstring text = Utf8ToWide(textUtf8Str.c_str());
    while (!text.empty() && text.back() == 0) text.pop_back();

    // 5. 確認ダイアログの表示
    swprintf_s(g_confirmMsg, 256, L"%d 個のオブジェクトに分割します。\n実行しますか？", (int)text.length());

    static DialogBuilder dbConf;
    if (dbConf.Get() == nullptr) {
        dbConf.Begin(L"確認", 260, 100, WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_SETFONT | DS_MODALFRAME | DS_CENTER);
        dbConf.AddControl(L"STATIC", L"", IDC_CONFIRM_MSG, 10, 20, 240, 20, SS_CENTER);
        dbConf.AddControl(L"BUTTON", L"はい", IDC_CONFIRM_OK, 40, 60, 80, 25, BS_PUSHBUTTON);
        dbConf.AddControl(L"BUTTON", L"いいえ", IDC_CONFIRM_NO, 140, 60, 80, 25, BS_PUSHBUTTON);
    }
    
    if (DialogBoxIndirectParamW(GetModuleHandle(NULL), dbConf.Get(), NULL, ConfirmDlgProc, 0) != IDOK) {
        return; // キャンセル時は何もしない
    }

    // 6. 分割実行 (1文字ずつオブジェクト生成)
    OBJECT_LAYER_FRAME info = edit->get_object_layer_frame(source);
    for (size_t i = 0; i < text.length(); i++) {
        std::wstring charStr = text.substr(i, 1);
        std::string charUtf8 = WideToUtf8(charStr);
        int targetLayer = info.layer + 1 + (int)i;
        
        // オブジェクト複製
        OBJECT_HANDLE newObj = edit->create_object_from_alias(alias, targetLayer, info.start, info.end - info.start);
        if (newObj) {
            // テキスト内容を1文字に書き換え
            edit->set_object_item_value(newObj, L"テキスト", L"テキスト", charUtf8.c_str());
        }
    }
}

// ==============================================================================
//  メインウィンドウのプロシージャ (ドッキングウィンドウ)
// ==============================================================================
INT_PTR CALLBACK MainDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG:
        {
            if (!hGuiFont) hGuiFont = CreateFontW(14,0,0,0,FW_NORMAL,0,0,0,SHIFTJIS_CHARSET,0,0,0,0,L"Yu Gothic UI");
            
            // ウィンドウサイズに合わせてボタンを配置
            RECT rc; GetClientRect(hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            HWND hBtn = CreateWindowW(L"BUTTON", L"テキスト分割", 
                WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON|BS_OWNERDRAW, 
                0, 0, w, h, hwnd, (HMENU)IDC_SPLIT_BTN, GetModuleHandle(NULL), NULL);
            SendMessage(hBtn, WM_SETFONT, (WPARAM)hGuiFont, 0);
        }
        return TRUE;

    // リサイズ対応 (ボタンを追従させる)
    case WM_SIZE: {
        int w = LOWORD(lp); int h = HIWORD(lp);
        HWND hBtn = GetDlgItem(hwnd, IDC_SPLIT_BTN);
        if (hBtn) SetWindowPos(hBtn, NULL, 0, 0, w, h, SWP_NOZORDER);
        return TRUE;
    }

    case WM_MOUSEACTIVATE: return MA_ACTIVATE;

    // ウィンドウ移動判定 (どこを掴んでも移動可能に)
    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &pt);
        RECT rc; GetClientRect(hwnd, &rc);
        if (pt.x >= rc.right - 5 && pt.y >= rc.bottom - 5) return HTBOTTOMRIGHT; // 右下でリサイズ
        if (pt.x >= rc.right - 2) return HTRIGHT;
        if (pt.y >= rc.bottom - 2) return HTBOTTOM;
        return HTCAPTION; // それ以外は移動
    }

    // 背景塗りつぶし (チラつき防止)
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, g_hBrBg);
        return TRUE;
    }

    // --- 色設定 & ボタン描画 ---
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

// ==============================================================================
//  プラグイン エントリーポイント
// ==============================================================================
EXTERN_C __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
    host->set_plugin_information(L"テキスト分割プラグイン V1.1");
    
    // ブラシの作成
    g_hBrBg = CreateSolidBrush(COL_BG);
    g_hBrBtn = CreateSolidBrush(COL_BTN_BG);
    g_hBrBtnPush = CreateSolidBrush(COL_BTN_PUSH);

    // ウィンドウメニューへの登録 (ドッキングウィンドウ)
    static DialogBuilder db;
    db.Begin(L"分割", 200, 25, WS_POPUP | WS_VISIBLE | DS_SETFONT | WS_CLIPCHILDREN);
    HWND hwnd = CreateDialogIndirectParamW(GetModuleHandle(NULL), db.Get(), NULL, MainDlgProc, 0);
    if (hwnd) host->register_window_client(L"テキスト分割", hwnd);

    // オブジェクト右クリックメニューへの登録
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

