// ---------------------------------------------------------------------------
//  PRUDA Admin Panel
//  Enter a Discord ID and a duration -> the API creates the licence (and a key)
//  for it. An ID that is already registered comes back as "Falsche ID".
// ---------------------------------------------------------------------------

#include <windows.h>
#include <string>
#include <thread>
#include <cwctype>

#include "license_client.hpp"

#pragma comment(lib, "comctl32.lib")

static HWND g_hInstanceWnd = nullptr;
static HWND g_hToken = nullptr, g_hId = nullptr, g_hDays = nullptr;
static HWND g_hStatus = nullptr, g_hKeyOut = nullptr;
static HFONT g_font = nullptr, g_fontTitle = nullptr, g_fontSmall = nullptr;

static const COLORREF COL_BG = RGB(8, 9, 13);
static const COLORREF COL_PANEL = RGB(18, 21, 30);
static const COLORREF COL_TEXT = RGB(232, 236, 245);
static const COLORREF COL_SUB = RGB(140, 150, 175);
static const COLORREF COL_ACCENT = RGB(96, 129, 255);
static const COLORREF COL_OK = RGB(107, 219, 140);
static const COLORREF COL_ERR = RGB(235, 92, 92);

static const int ID_TOKEN = 1001, ID_ID = 1002, ID_DAYS = 1003;
static const int ID_BTN_CREATE = 1010, ID_BTN_30 = 1011, ID_BTN_MONTH = 1012, ID_BTN_LFT = 1013;
static const int WM_APP_DONE = WM_APP + 1;

struct Result { bool ok; std::wstring text; std::wstring key; };

// ------------------------------------------------------------------ helpers
static std::string GetTextA(HWND hwnd) {
	wchar_t buf[256]{};
	GetWindowTextW(hwnd, buf, 256);
	return license::narrow(buf);
}

static void SetStatus(const std::wstring& text, COLORREF colour) {
	SetWindowTextW(g_hStatus, text.c_str());
	InvalidateRect(g_hStatus, nullptr, TRUE);
	// colour is read back in WM_CTLCOLORSTATIC
	static COLORREF last = COL_SUB;
	last = colour;
	SetPropW(g_hStatus, L"col", (HANDLE)(INT_PTR)colour);
	InvalidateRect(g_hInstanceWnd, nullptr, FALSE);
}

static COLORREF GetStatusColour() {
	HANDLE h = GetPropW(g_hStatus, L"col");
	return h ? (COLORREF)(INT_PTR)h : COL_SUB;
}

static void CopyToClipboard(const std::wstring& text) {
	if (!OpenClipboard(g_hInstanceWnd)) return;
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
	size_t start = value.find_first_not_of(L" \t");
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

static HWND MakeLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h, HFONT font, int id = -1) {
	HWND c = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
		x, y, w, h, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
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
	if (disabled) bg = RGB(48, 54, 70);
	else if (pressed) bg = RGB(70, 96, 200);

	HBRUSH brush = CreateSolidBrush(bg);
	FillRect(d->hDC, &d->rcItem, brush);
	DeleteObject(brush);

	wchar_t text[64]{};
	GetWindowTextW(d->hwndItem, text, 64);

	SetBkMode(d->hDC, TRANSPARENT);
	SetTextColor(d->hDC, disabled ? RGB(130, 138, 155) : RGB(255, 255, 255));
	HGDIOBJ old = SelectObject(d->hDC, g_font);
	DrawTextW(d->hDC, text, -1, (RECT*)&d->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	SelectObject(d->hDC, old);
}

// ------------------------------------------------------------------- worker
static void CreateLicence(HWND hwnd) {
	const std::string token = GetTextA(g_hToken);
	const std::string id = GetTextA(g_hId);
	const int days = GetDlgItemInt(hwnd, ID_DAYS, nullptr, FALSE);

	if (id.empty()) {
		SetStatus(L"Bitte eine Discord-ID eingeben.", COL_ERR);
		return;
	}
	for (char c : id) {
		if (c < '0' || c > '9') {
			SetStatus(L"Falsche ID - eine Discord-ID besteht nur aus Zahlen.", COL_ERR);
			return;
		}
	}
	if (token.empty()) {
		SetStatus(L"Bitte den Admin-Token eingeben (oder admin.txt anlegen).", COL_ERR);
		return;
	}

	SetStatus(L"Key wird erstellt ...", COL_SUB);
	SetWindowTextW(g_hKeyOut, L"");

	std::thread([hwnd, token, id, days]() {
		license::Response r = license::admin_create(token, id, days);

		auto* result = new Result();
		result->ok = r.ok;
		if (r.ok) {
			std::wstring key = license::widen(r.key);
			std::wstring expires = license::widen(r.expires);
			result->key = key;
			result->text = L"Key erstellt und in die Zwischenablage kopiert.  Ablauf: " + expires;
		}
		else {
			result->text = license::widen(r.message);
		}
		PostMessageW(hwnd, WM_APP_DONE, 0, (LPARAM)result);
		}).detach();
}

// ------------------------------------------------------------------- window
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE:
	{
		g_hInstanceWnd = hwnd;

		NONCLIENTMETRICSW ncm{};
		ncm.cbSize = sizeof(ncm);
		SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);

		g_font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
		g_fontTitle = CreateFontW(-22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
		g_fontSmall = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

		MakeLabel(hwnd, L"PRUDA ADMIN", 26, 18, 300, 32, g_fontTitle);
		MakeLabel(hwnd, L"Lizenz fuer eine Discord-ID anlegen", 26, 50, 380, 20, g_fontSmall);

		MakeLabel(hwnd, L"Admin-Token", 26, 92, 140, 20, nullptr);
		g_hToken = MakeEdit(hwnd, ID_TOKEN, 26, 112, 430, 28, false);
		SendMessageW(g_hToken, EM_SETPASSWORDCHAR, (WPARAM)L'*', 0);

		MakeLabel(hwnd, L"Discord-ID", 26, 152, 140, 20, nullptr);
		g_hId = MakeEdit(hwnd, ID_ID, 26, 172, 200, 28, true);

		MakeLabel(hwnd, L"Tage  (0 = Lifetime)", 240, 152, 200, 20, nullptr);
		g_hDays = MakeEdit(hwnd, ID_DAYS, 240, 172, 70, 28, true);
		SetWindowTextW(g_hDays, L"30");

		MakeButton(hwnd, L"30", ID_BTN_30, 318, 172, 44, 28);
		MakeButton(hwnd, L"1 Monat", ID_BTN_MONTH, 368, 172, 88, 28);
		MakeButton(hwnd, L"LFT", ID_BTN_LFT, 462, 172, 54, 28);

		MakeButton(hwnd, L"KEY ERSTELLEN", ID_BTN_CREATE, 26, 222, 200, 36);

		g_hKeyOut = MakeLabel(hwnd, L"", 240, 228, 280, 24, g_fontSmall);

		g_hStatus = MakeLabel(hwnd, L"Bereit.", 26, 276, 490, 44, nullptr);

		MakeLabel(hwnd, L"admin.txt = Token, api.txt = Server-URL (optional, neben der exe)", 26, 330, 490, 18, g_fontSmall);

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
				SetStatus(result->text, COL_OK);
				SetWindowTextW(g_hKeyOut, result->key.c_str());
				CopyToClipboard(result->key);
			}
			else {
				SetStatus(result->text, COL_ERR);
				SetWindowTextW(g_hKeyOut, L"");
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
		if (control == g_hStatus) SetTextColor(dc, GetStatusColour());
		else if (control == g_hKeyOut) SetTextColor(dc, COL_ACCENT);
		else SetTextColor(dc, COL_SUB);
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
	{
		DrawOwnButton((const DRAWITEMSTRUCT*)lParam);
		return TRUE;
	}

	case WM_COMMAND:
	{
		const int id = LOWORD(wParam);
		switch (id) {
		case ID_BTN_CREATE: CreateLicence(hwnd); return 0;
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

	RECT rc{ 0, 0, 540, 360 };
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
