// ---------------------------------------------------------------------------
//  PRUDA licence gate - key window shown before the cheat starts.
// ---------------------------------------------------------------------------

#include "LicenseGate.hpp"

#include <objidl.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <string>
#include <thread>

#include "license_client.hpp"
#include "../../Resources/logo.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

namespace LicenseGate {

	namespace {

		const COLORREF COL_BG = RGB(9, 8, 7);
		const COLORREF COL_PANEL = RGB(20, 19, 16);
		const COLORREF COL_TEXT = RGB(236, 234, 228);
		const COLORREF COL_SUB = RGB(150, 133, 96);
		const COLORREF COL_ACCENT = RGB(226, 186, 96);
		const COLORREF COL_OK = RGB(163, 226, 120);
		const COLORREF COL_ERR = RGB(235, 92, 92);

		const int ID_KEY = 2001;
		const int ID_BTN = 2002;
		const int WM_GATE_DONE = WM_APP + 42;

		HWND g_hwnd = nullptr;
		HWND g_hKey = nullptr;
		HWND g_hStatus = nullptr;
		HFONT g_font = nullptr, g_fontTitle = nullptr, g_fontSmall = nullptr;
		Gdiplus::Image* g_logo = nullptr;
		ULONG_PTR g_gdiplus = 0;
		bool g_ok = false;
		bool g_busy = false;

		struct GateResult {
			bool ok = false;
			std::wstring message;
		};

		// -----------------------------------------------------------------
		Gdiplus::Image* load_logo() {
			HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sizeof(prua_logo));
			if (!mem) return nullptr;

			void* dst = GlobalLock(mem);
			if (!dst) { GlobalFree(mem); return nullptr; }
			memcpy(dst, prua_logo, sizeof(prua_logo));
			GlobalUnlock(mem);

			IStream* stream = nullptr;
			if (FAILED(CreateStreamOnHGlobal(mem, TRUE, &stream)) || !stream) {
				GlobalFree(mem);
				return nullptr;
			}

			Gdiplus::Image* image = Gdiplus::Image::FromStream(stream);
			stream->Release();
			return image;
		}

		void SetStatus(const std::wstring& text, COLORREF colour) {
			if (!g_hStatus) return;
			SetWindowTextW(g_hStatus, text.c_str());
			SetPropW(g_hStatus, L"col", (HANDLE)(INT_PTR)colour);
			InvalidateRect(g_hwnd, nullptr, FALSE);
		}

		COLORREF StatusColour() {
			HANDLE h = GetPropW(g_hStatus, L"col");
			return h ? (COLORREF)(INT_PTR)h : COL_SUB;
		}

		// -----------------------------------------------------------------
		void Submit() {
			if (g_busy || !g_hKey) return;

			wchar_t buffer[128]{};
			GetWindowTextW(g_hKey, buffer, 128);
			const std::string key = license::narrow(buffer);

			if (license::normalize_key(key).empty()) {
				SetStatus(L"Bitte Key eingeben.", COL_ERR);
				return;
			}

			g_busy = true;
			SetStatus(L"Wird geprueft ...", COL_SUB);

			std::thread([key]() {
				license::Response response = license::login(key);

				auto* result = new GateResult();
				result->ok = response.ok;
				result->message = license::widen(response.ok
					? (response.expires == "LIFETIME"
						? std::string("Lizenz aktiv - Lifetime")
						: std::string("Lizenz aktiv bis ") + response.expires)
					: response.message);

				if (g_hwnd) PostMessageW(g_hwnd, WM_GATE_DONE, 0, (LPARAM)result);
				}).detach();
		}

		// -----------------------------------------------------------------
		void DrawButton(const DRAWITEMSTRUCT* d) {
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

		LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
			UINT_PTR /*id*/, DWORD_PTR /*ref*/) {
			if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
				Submit();
				return 0;
			}
			return DefSubclassProc(hwnd, msg, wParam, lParam);
		}

		// -----------------------------------------------------------------
		LRESULT CALLBACK GateProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
			switch (msg) {
			case WM_CREATE:
			{
				g_hwnd = hwnd;

				g_font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
					OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
				g_fontTitle = CreateFontW(-30, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
					OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
				g_fontSmall = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
					OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

				g_hKey = CreateWindowExW(0, L"EDIT", L"",
					WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL | ES_CENTER | WS_TABSTOP,
					26, 150, 388, 32, hwnd, (HMENU)(INT_PTR)ID_KEY, nullptr, nullptr);
				SendMessageW(g_hKey, WM_SETFONT, (WPARAM)g_font, TRUE);
				SendMessageW(g_hKey, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));
				SetWindowSubclass(g_hKey, EditProc, 1, 0);

				HWND button = CreateWindowExW(0, L"BUTTON", L"ANMELDEN",
					WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 26, 196, 388, 38, hwnd, (HMENU)(INT_PTR)ID_BTN, nullptr, nullptr);
				SendMessageW(button, WM_SETFONT, (WPARAM)g_font, TRUE);

				g_hStatus = CreateWindowExW(0, L"STATIC", L"Key eingeben",
					WS_CHILD | WS_VISIBLE | SS_CENTER, 26, 244, 388, 22, hwnd, nullptr, nullptr, nullptr);
				SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_font, TRUE);
				SetPropW(g_hStatus, L"col", (HANDLE)(INT_PTR)COL_SUB);

				HWND hint = CreateWindowExW(0, L"STATIC", L"Key aus dem Admin-Panel",
					WS_CHILD | WS_VISIBLE | SS_CENTER, 26, 272, 388, 20, hwnd, nullptr, nullptr, nullptr);
				SendMessageW(hint, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);

				SetFocus(g_hKey);
				return 0;
			}

			case WM_GATE_DONE:
			{
				GateResult* result = reinterpret_cast<GateResult*>(lParam);
				g_busy = false;
				if (result) {
					if (result->ok) {
						g_ok = true;
						SetStatus(result->message, COL_OK);
						SetTimer(hwnd, 1, 450, nullptr);   // let the user see it, then continue
					}
					else {
						SetStatus(result->message, COL_ERR);
					}
					delete result;
				}
				return 0;
			}

			case WM_TIMER:
				KillTimer(hwnd, 1);
				DestroyWindow(hwnd);
				return 0;

			case WM_COMMAND:
				if (LOWORD(wParam) == ID_BTN) { Submit(); return 0; }
				break;

			case WM_DRAWITEM:
				DrawButton((const DRAWITEMSTRUCT*)lParam);
				return TRUE;

			case WM_CTLCOLORSTATIC:
			{
				HDC dc = (HDC)wParam;
				SetBkColor(dc, COL_BG);
				SetTextColor(dc, (HWND)lParam == g_hStatus ? StatusColour() : COL_SUB);
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

			case WM_PAINT:
			{
				PAINTSTRUCT ps{};
				HDC dc = BeginPaint(hwnd, &ps);

				if (g_logo) {
					Gdiplus::Graphics graphics(dc);
					graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
					graphics.DrawImage(g_logo, 26, 28, 96, 96);
				}

				SetBkMode(dc, TRANSPARENT);
				SetTextColor(dc, COL_TEXT);
				HGDIOBJ old = SelectObject(dc, g_fontTitle);
				TextOutW(dc, 138, 44, L"PRUDA", 5);

				SelectObject(dc, g_fontSmall);
				SetTextColor(dc, COL_SUB);
				TextOutW(dc, 140, 86, L"Zugang nur mit gueltigem Key", 28);

				SelectObject(dc, old);
				EndPaint(hwnd, &ps);
				return 0;
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

	} // namespace

	bool Run(HINSTANCE instance) {
		Gdiplus::GdiplusStartupInput startup;
		if (Gdiplus::GdiplusStartup(&g_gdiplus, &startup, nullptr) != Gdiplus::Ok) {
			g_gdiplus = 0;
		}
		g_logo = load_logo();

		const wchar_t* class_name = L"PruaLicenseGate";

		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = GateProc;
		wc.hInstance = instance;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = CreateSolidBrush(COL_BG);
		wc.lpszClassName = class_name;
		RegisterClassExW(&wc);

		RECT rect{ 0, 0, 440, 320 };
		AdjustWindowRect(&rect, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

		const int width = rect.right - rect.left;
		const int height = rect.bottom - rect.top;
		const int pos_x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
		const int pos_y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

		HWND hwnd = CreateWindowExW(WS_EX_TOPMOST, class_name, L"PRUDA",
			WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
			pos_x, pos_y, width, height, nullptr, nullptr, instance, nullptr);

		if (!hwnd) {
			if (g_gdiplus) Gdiplus::GdiplusShutdown(g_gdiplus);
			return false;
		}

		ShowWindow(hwnd, SW_SHOW);
		UpdateWindow(hwnd);
		SetForegroundWindow(hwnd);
		SetFocus(g_hKey);

		MSG msg{};
		while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		const bool ok = g_ok;

		if (g_logo) { delete g_logo; g_logo = nullptr; }
		if (g_gdiplus) Gdiplus::GdiplusShutdown(g_gdiplus);

		return ok;
	}

} // namespace LicenseGate
