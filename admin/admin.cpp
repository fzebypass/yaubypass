// ---------------------------------------------------------------------------
//  PRUDA Admin Panel
//  Generates licence keys. Pick a duration, pick how many, press the button -
//  the keys are created on the server and copied to the clipboard.
// ---------------------------------------------------------------------------

#include <windows.h>
#include <string>
#include <thread>
#include <vector>

#include "license_client.hpp"

static HWND g_hWnd = nullptr;
static HWND g_hToken = nullptr, g_hDays = nullptr, g_hCount = nullptr;
static HWND g_hStatus = nullptr, g_hKeys = nullptr;
static HFONT g_font = nullptr, g_fontTitle = nullptr, g_fontSmall = nullptr, g_fontMono = nullptr;

static const COLORREF COL_BG = RGB(9, 8, 7);
static const COLORREF COL_PANEL = RGB(20, 19, 16);
static const COLORREF COL_TEXT = RGB(236, 234, 228);
static const COLORREF COL_SUB = RGB(150, 133, 96);
static const COLORREF COL_ACCENT = RGB(226, 186, 96);
static const COLORREF COL_OK = RGB(163, 226, 120);
static const COLORREF COL_ERR = RGB(235, 92, 92);

static const int ID_TOKEN = 1001, ID_DAYS = 1002, ID_COUNT = 1003, ID_KEYS = 1004;
static const int ID_BTN_CREATE = 1010, ID_BTN_30 = 1011, ID_BTN_MONTH = 1012, ID_BTN_LFT = 1013;
static const int WM_APP_DONE = WM_APP + 1;

struct Result { bool ok; std::wstring message; std::wstring keys; };

// ------------------------------------------------------------------ helpers
static std::string GetTextA(HWND hwnd) {
	wchar_t buf[256]{};
	GetWindowTextW(hwnd, buf, 256);
	return license::narrow(buf);
}

static void SetStatus(const std::wstring& text, COLORREF colour) {
	SetWindowTextW(g_hStatus, text.c_str());
	SetPropW(g_hStatus, L"col", (HANDLE)(INT_PTR)colour);
	InvalidateRect(g_hWnd, nullptr, FALSE);
}

static COLORREF GetStatusColour() {
	HANDLE h = GetPropW(g_hStatus, L"col");
	return h ? (COLORREF)(INT_PTR)h : COL_SUB;
}

static void CopyToClipboard(const std::wstring& text) {
	if (text.empty()) return;
	if (!OpenClipboard(g_hWnd)) return;
	EmptyClipboard();
	const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
	HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
	if (mem) {
		void* dst = GlobalLock(mem);
		if (dst) {
			memcpy(dst, text.c_str(), bytes);
			GlobalUnlock(mem);
			SetClipboardData(CF_UNICODETEXT, mem);
		}
	}
	CloseClipboard();
}

static std::wstring ReadTextFileNextToExe(const wchar_t* name) {
	const std::wstring path = license::exe_dir() + name;
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) return L"";
	char buf[256]{};
	DWORD read = 0;
	BOOL ok = ReadFile(h, buf, sizeof(buf) - 1, &read, nullptr);
	CloseHandle(h);
	if (!ok || read == 0) return L"";
	std::wstring value = license::widen(std::string(buf, read));
	while (!value.empty() && (value.back() == L'\r' || value.back() == L'\n' || value.back() == L' ' || value.back() == L'\t'))
		value.pop_back();
	const size_t start = value.find_first_not_of(L" \t");
	return start == std::wstring::npos ? L"" : value.substr(start);
}

static HWND MakeEdit(HWND parent, int id, int x, int y, int w, int h, bool numeric) {
	HWND c = CreateWindowExW(0, L"EDIT", L"",
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | (numeric ? ES_NUMBER : 0),
		x, y, w, h, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
	SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
	SendMessageW(c, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));
	return c;
}

static HWND MakeLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h, HFONT font) {
	HWND c = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, w, h, parent, nullptr, nullptr, nullptr);
	SendMessageW(c, WM_SETFONT, (WPARAM)(font ? font : g_font), TRUE);
	return c;
}

static HWND MakeButton(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) {
	HWND c = CreateWindowExW(0, L"BUTTON", text,
		WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, x, y, w, h, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
	SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
	return c;
}

static void DrawOwnButton(const DRAWITEMSTRUCT* d) {
	const bool pressed = (d->itemState & ODS_SELECTED) != 0;
	const bool disabled = (d->itemState & ODS_DISABLED) != 0;

	COLORREF bg = COL_ACCENT;
	if (disabled) bg = RGB(52, 48, 38);
	else if (pressed) bg = RGB(190, 152, 70);

	HBRUSH brush = CreateSolidBrush(bg);
	FillRect(d->hDC, &d->rcItem, brush);
	DeleteObject(brush);

	wchar_t text[64]{};
	GetWindowTextW(d->hwndItem, text, 64);

	SetBkMode(d->hDC, TRANSPARENT);
	SetTextColor(d->hDC, disabled ? RGB(140, 132, 112) : RGB(16, 14, 10));
	HGDIOBJ old = SelectObject(d->hDC, g_font);
	DrawTextW(d->hDC, text, -1, (RECT*)&d->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	SelectObject(d->hDC, old);
}

// ------------------------------------------------------------------- worker
static void CreateKeys(HWND hwnd) {
	const std::string token = GetTextA(g_hToken);
	const int days = GetDlgItemInt(hwnd, ID_DAYS, nullptr, FALSE);
	int count = GetDlgItemInt(hwnd, ID_COUNT, nullptr, FALSE);

	if (token.empty()) {
		SetStatus(L"Bitte den Admin-Token eingeben (oder admin.txt anlegen).", COL_ERR);
		return;
	}
	if (count < 1) count = 1;
	if (count > 100) count = 100;
	SetWindowTextW(g_hCount, std::to_wstring(count).c_str());

	SetStatus(days == 0 ? L"Lifetime-Keys werden erstellt ..." : L"Keys werden erstellt ...", COL_SUB);
	SetWindowTextW(g_hKeys, L"");

	std::thread([hwnd, token, days, count]() {
		license::Response r = license::admin_create(token, days, count);

		auto* result = new Result();
		result->ok = r.ok;

		if (r.ok) {
			std::wstring list;
			for (size_t i = 0; i < r.keys.size(); ++i) {
				list += license::widen(r.keys[i]);
				if (i + 1 < r.keys.size()) list += L"\r\n";
			}
			result->keys = list;
			result->message = license::widen(r.message) + L"  -  Ablauf: " + license::widen(r.expires);
		}
		else {
			result->message = license::widen(r.message);
		}
		PostMessageW(hwnd, WM_APP_DONE, 0, (LPARAM)result);
		}).detach();
}

// ------------------------------------------------------------------- window
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE:
	{
		g_hWnd = hwnd;

		g_font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
		g_fontTitle = CreateFontW(-22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
		g_fontSmall = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
		g_fontMono = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

		MakeLabel(hwnd, L"PRUDA KEYS", 26, 16, 300, 32, g_fontTitle);
		MakeLabel(hwnd, L"Lizenz-Keys erstellen - werden automatisch in die Zwischenablage kopiert", 26, 48, 520, 20, g_fontSmall);

		MakeLabel(hwnd, L"Admin-Token", 26, 86, 200, 20, nullptr);
		g_hToken = MakeEdit(hwnd, ID_TOKEN, 26, 106, 508, 28, false);
		SendMessageW(g_hToken, EM_SETPASSWORDCHAR, (WPARAM)L'*', 0);

		MakeLabel(hwnd, L"Tage  (0 = Lifetime)", 26, 146, 200, 20, nullptr);
		g_hDays = MakeEdit(hwnd, ID_DAYS, 26, 166, 70, 28, true);
		SetWindowTextW(g_hDays, L"0");

		MakeButton(hwnd, L"30 Tage", ID_BTN_30, 104, 166, 78, 28);
		MakeButton(hwnd, L"1 Monat", ID_BTN_MONTH, 188, 166, 78, 28);
		MakeButton(hwnd, L"Lifetime", ID_BTN_LFT, 272, 166, 78, 28);

		MakeLabel(hwnd, L"Anzahl", 372, 146, 100, 20, nullptr);
		g_hCount = MakeEdit(hwnd, ID_COUNT, 372, 166, 70, 28, true);
		SetWindowTextW(g_hCount, L"1");

		MakeButton(hwnd, L"KEYS ERSTELLEN", ID_BTN_CREATE, 26, 212, 200, 36);
		g_hStatus = MakeLabel(hwnd, L"Bereit.", 240, 220, 300, 20, nullptr);

		MakeLabel(hwnd, L"Erstellte Keys", 26, 262, 200, 20, nullptr);
		g_hKeys = CreateWindowExW(0, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
			26, 282, 508, 148, hwnd, (HMENU)(INT_PTR)ID_KEYS, nullptr, nullptr);
		SendMessageW(g_hKeys, WM_SETFONT, (WPARAM)g_fontMono, TRUE);
		SendMessageW(g_hKeys, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));

		MakeLabel(hwnd, L"admin.txt = Token, api.txt = Server-URL (optional, neben der exe)", 26, 440, 508, 18, g_fontSmall);

		const std::wstring token = ReadTextFileNextToExe(L"admin.txt");
		if (!token.empty()) SetWindowTextW(g_hToken, token.c_str());

		SetStatus(L"Bereit.", COL_SUB);
		return 0;
	}

	case WM_APP_DONE:
	{
		Result* result = reinterpret_cast<Result*>(lParam);
		if (result) {
			if (result->ok) {
				SetStatus(result->message, COL_OK);
				SetWindowTextW(g_hKeys, result->keys.c_str());
				CopyToClipboard(result->keys);
			}
			else {
				SetStatus(result->message, COL_ERR);
				SetWindowTextW(g_hKeys, L"");
			}
			delete result;
		}
		return 0;
	}

	case WM_CTLCOLORSTATIC:
	{
		HDC dc = (HDC)wParam;
		HWND control = (HWND)lParam;
		SetBkColor(dc, COL_BG);
		SetTextColor(dc, control == g_hStatus ? GetStatusColour() : COL_SUB);
		static HBRUSH bg = CreateSolidBrush(COL_BG);
		return (LRESULT)bg;
	}

	case WM_CTLCOLOREDIT:
	{
		HDC dc = (HDC)wParam;
		SetBkColor(dc, COL_PANEL);
		SetTextColor(dc, COL_TEXT);
		static HBRUSH panel = CreateSolidBrush(COL_PANEL);
		return (LRESULT)panel;
	}

	case WM_ERASEBKGND:
	{
		RECT rc{};
		GetClientRect(hwnd, &rc);
		HBRUSH brush = CreateSolidBrush(COL_BG);
		FillRect((HDC)wParam, &rc, brush);
		DeleteObject(brush);
		return 1;
	}

	case WM_DRAWITEM:
		DrawOwnButton((const DRAWITEMSTRUCT*)lParam);
		return TRUE;

	case WM_COMMAND:
	{
		switch (LOWORD(wParam)) {
		case ID_BTN_CREATE: CreateKeys(hwnd); return 0;
		case ID_BTN_30:     SetWindowTextW(g_hDays, L"30"); return 0;
		case ID_BTN_MONTH:  SetWindowTextW(g_hDays, L"30"); return 0;
		case ID_BTN_LFT:    SetWindowTextW(g_hDays, L"0"); return 0;
		default: break;
		}
		break;
	}

	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
	const wchar_t* class_name = L"PruaAdminPanel";

	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = instance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = CreateSolidBrush(COL_BG);
	wc.lpszClassName = class_name;
	RegisterClassExW(&wc);

	RECT rc{ 0, 0, 560, 470 };
	AdjustWindowRect(&rc, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

	const int width = rc.right - rc.left;
	const int height = rc.bottom - rc.top;
	const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

	HWND hwnd = CreateWindowExW(0, class_name, L"PRUDA ADMIN",
		WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		x, y, width, height, nullptr, nullptr, instance, nullptr);

	if (!hwnd) return 0;

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	MSG msg{};
	while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return 0;
}
