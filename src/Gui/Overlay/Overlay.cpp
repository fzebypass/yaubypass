#pragma once
#include "Overlay.hpp"
#include <imgui_freetype.h>
#include <Core/Core.hpp>
#include <Gui/Gui.hpp>
#include <slate.h>
#include <Includes/ImGui/Fonts.hpp>
#include <Includes/ImGui/Images.hpp>
#include <Includes/CustomWidgets/Notify.hpp>
#include <cleaners/cleaners.h>
#include <Core/Features/auth.hpp>
#include <dwmapi.h>
#include <windowsx.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <tchar.h>
#include <thread>

#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

//#define FULLSCREEN_EX

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Gui
{
	typedef HWND(WINAPI* CreateWindowInBand)(_In_ DWORD dwExStyle, _In_opt_ ATOM atom, _In_opt_ LPCWSTR lpWindowName, _In_ DWORD dwStyle, _In_ int X, _In_ int Y, _In_ int nWidth, _In_ int nHeight, _In_opt_ HWND hWndParent, _In_opt_ HMENU hMenu, _In_opt_ HINSTANCE hInstance, _In_opt_ LPVOID lpParam, DWORD band);

	inline HHOOK hKeyboardHook;

	namespace
	{
		bool IsCustomInputActive()
		{
			return g_MenuInfo.IsOpen && g_Config.General->CustomInput;
		}

		void DrawCustomInputCursor()
		{
			if (!ImGui::IsMousePosValid())
				return;

			ImDrawList* draw_list = ImGui::GetForegroundDrawList();
			ImVec2 p = ImGui::GetMousePos();

			const float radius = 3.5f;

			ImU32 fill_color = IM_COL32(255, 255, 255, 255);
			ImU32 outline_color = IM_COL32(0, 0, 0, 255);

			// Fill
			draw_list->AddCircleFilled(p, radius, fill_color, 32);

			// Outline
			draw_list->AddCircle(p, radius, outline_color, 32, 1.5f);
		}
	}

	namespace
	{
		// SetForegroundWindow is ignored by Windows unless the calling process owns
		// the current foreground window. Attaching to the foreground thread lifts
		// that restriction, so the menu key opens the menu from any application.
		void ForceForegroundWindow(HWND hwnd)
		{
			if (!hwnd || !IsWindow(hwnd))
				return;

			HWND foreground = GetForegroundWindow();
			if (foreground == hwnd)
			{
				SetFocus(hwnd);
				return;
			}

			const DWORD foreground_thread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
			const DWORD target_thread = GetWindowThreadProcessId(hwnd, nullptr);
			const bool attach = foreground_thread != 0 && foreground_thread != target_thread;

			if (attach)
				AttachThreadInput(foreground_thread, target_thread, TRUE);

			ShowWindow(hwnd, SW_SHOW);
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
			BringWindowToTop(hwnd);
			SetForegroundWindow(hwnd);
			SetFocus(hwnd);
			SetActiveWindow(hwnd);

			if (attach)
				AttachThreadInput(foreground_thread, target_thread, FALSE);
		}
	}

	inline LRESULT CALLBACK KeyboardCallBack(int nCode, WPARAM wParam, LPARAM lParam)
	{
		if (nCode >= 0)
		{
			//	if ( GetForegroundWindow( ) == g_Variables.g_hCheatWindow )
				//{
			KBDLLHOOKSTRUCT* pKeyboard = (KBDLLHOOKSTRUCT*)lParam;
			PostMessage(g_Variables.g_hCheatWindow, wParam, pKeyboard->vkCode, 0);
			//}
		}
		return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
	}

	void Overlay::Render()
	{

#ifdef FULLSCREEN_EX
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		CreateWindowInBand pCreateWindowInBand = reinterpret_cast<CreateWindowInBand>(GetProcAddress(LoadLibraryA(xorstr("user32.dll")), xorstr("CreateWindowInBand")));
		PrepareForUIAccess();
#endif // FULLSCREEN_EX

		WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, Overlay::WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L" ", NULL };
		ATOM RegClass = RegisterClassExW(&wc);

		// Not attached to FiveM yet: the overlay covers the primary monitor so the
		// menu can also be opened on the desktop. As soon as the game window exists,
		// the screen-resolution thread moves and resizes the overlay to match it.
		if (!g_Variables.g_hGameWindow)
		{
			const int screen_w = GetSystemMetrics(SM_CXSCREEN);
			const int screen_h = GetSystemMetrics(SM_CYSCREEN);
			g_Variables.g_vGameWindowPos.x = 0.0f;
			g_Variables.g_vGameWindowPos.y = 0.0f;
			g_Variables.g_vGameWindowSize.x = static_cast<float>(screen_w);
			g_Variables.g_vGameWindowSize.y = static_cast<float>(screen_h);
			g_Variables.g_vGameWindowCenter.x = screen_w / 2.0f;
			g_Variables.g_vGameWindowCenter.y = screen_h / 2.0f;
		}

#ifdef FULLSCREEN_EX
		g_Variables.g_hCheatWindow = pCreateWindowInBand(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE, RegClass, L" ", WS_POPUP, static_cast<int>(g_Variables.g_vGameWindowPos.x), static_cast<int>(g_Variables.g_vGameWindowPos.y), static_cast<int>(g_Variables.g_vGameWindowSize.x), static_cast<int>(g_Variables.g_vGameWindowSize.y), NULL, NULL, wc.hInstance, NULL, 2/*ZBID_UIACCESS*/);
#else
		// No WS_EX_LAYERED: a layered window cannot carry a display affinity, which is
		// what hides the overlay from NVIDIA/OBS capture. Transparency now comes from
		// the premultiplied DirectComposition swapchain created in CreateDeviceD3D.
		g_Variables.g_hCheatWindow = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT, wc.lpszClassName, L" ", WS_POPUP, g_Variables.g_vGameWindowPos.x, g_Variables.g_vGameWindowPos.y, g_Variables.g_vGameWindowSize.x, g_Variables.g_vGameWindowSize.y, NULL, NULL, wc.hInstance, NULL);
#endif // FULLSCREEN_EX		
		MARGINS Margin = { g_Variables.g_vGameWindowPos.x, g_Variables.g_vGameWindowPos.y, g_Variables.g_vGameWindowSize.x,g_Variables.g_vGameWindowSize.y };
		DwmExtendFrameIntoClientArea(g_Variables.g_hCheatWindow, &Margin);

		if (!CreateDeviceD3D(g_Variables.g_hCheatWindow))
		{
			CleanupDeviceD3D();
			UnregisterClassW(wc.lpszClassName, wc.hInstance);
			return;
		}


		//SetWindowDisplayAffinity( g_Variables.g_hCheatWindow, WDA_EXCLUDEFROMCAPTURE );

		ShowWindow(g_Variables.g_hCheatWindow, SW_SHOWDEFAULT);
		UpdateWindow(g_Variables.g_hCheatWindow);


		ImGui::CreateContext();

		Style();
		Fonts();

		ImGui_ImplWin32_Init(g_Variables.g_hCheatWindow);
		ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

		slate->initialize(g_pd3dDevice, g_pd3dDeviceContext, g_pSwapChain);

#ifdef FULLSCREEN_EX
		hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardCallBack, NULL, 0);
#endif // FULLSCREEN_EX

		Core::StartThreads();
		if (!Core::ThreadsStarted) return;

		static RECT old_rc;
		static ULONGLONG menu_focus_deadline = 0;
		ZeroMemory(&Message, sizeof(MSG));

		auto set_overlay_style = [&](bool menu_open) {
#ifdef FULLSCREEN_EX
			SetWindowLong(g_Variables.g_hCheatWindow, GWL_EXSTYLE, menu_open
				? WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE
				: WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
#else
			SetWindowLong(g_Variables.g_hCheatWindow, GWL_EXSTYLE, menu_open
				? WS_EX_TOPMOST | WS_EX_TOOLWINDOW
				: WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT);
#endif // FULLSCREEN_EX
			};

		g_MenuInfo.IsOpen = true;
		set_overlay_style(true);
		menu_focus_deadline = GetTickCount64() + 2500;
		ShowWindow(g_Variables.g_hCheatWindow, SW_SHOW);
		SetForegroundWindow(g_Variables.g_hCheatWindow);
		SetFocus(g_Variables.g_hCheatWindow);
		SetActiveWindow(g_Variables.g_hCheatWindow);
		{
			RECT rect;
			GetWindowRect(g_Variables.g_hCheatWindow, &rect);
			const int centerX = rect.left + (rect.right - rect.left) / 2;
			const int centerY = rect.top + (rect.bottom - rect.top) / 2;
			SetCursorPos(centerX, centerY);
		}

		while (Message.message != WM_QUIT)
		{
			while (PeekMessage(&Message, g_Variables.g_hCheatWindow, 0, 0, PM_REMOVE)) {
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			}

			// Destruct runs invisible: the menu closes and the overlay disappears from
			// the screen immediately while the cleanup thread finishes its work.
			if (IsDestructing())
			{
				g_MenuInfo.IsOpen = false;
				ShowWindow(g_Variables.g_hCheatWindow, SW_HIDE);
				std::this_thread::sleep_for(std::chrono::milliseconds(15));
				continue;
			}

			HWND ActiveWindow = GetForegroundWindow();
			const bool gameFocused = ActiveWindow == g_Variables.g_hGameWindow;
			const bool cheatFocused = ActiveWindow == g_Variables.g_hCheatWindow;
			const bool renderOverlay = gameFocused || cheatFocused || g_MenuInfo.IsOpen;
			const bool lowPowerOverlay = !g_MenuInfo.IsOpen && !gameFocused;
			int menuToggleKey = g_Variables.g_MenuKeyOverride >= 0 ? g_Variables.g_MenuKeyOverride : g_Config.General->MenuKey;
			if (menuToggleKey <= 0)
				menuToggleKey = VK_F1; // never leave the menu without a working key

			// Windows blocks foreground changes unless the caller owns the current
			// foreground window, so the first attempt can fail (keyboard input and the
			// mouse wheel need real focus). Retry briefly after opening.
			if (g_MenuInfo.IsOpen && GetTickCount64() < menu_focus_deadline && GetForegroundWindow() != g_Variables.g_hCheatWindow)
			{
				static ULONGLONG last_focus_try = 0;
				const ULONGLONG now = GetTickCount64();
				if (now - last_focus_try >= 200)
				{
					last_focus_try = now;
					ForceForegroundWindow(g_Variables.g_hCheatWindow);
				}
			}

			// The menu key is global: it opens and closes the menu no matter which
			// window currently has focus (game, desktop, browser, Discord, ...).
			if (menuToggleKey > 0 && (GetAsyncKeyState(menuToggleKey) & 1))
			{
				if (!g_MenuInfo.IsOpen)
				{
					g_MenuInfo.IsOpen = true;
					set_overlay_style(true);
					ForceForegroundWindow(g_Variables.g_hCheatWindow);
					menu_focus_deadline = GetTickCount64() + 2500;

					RECT rect{};
					GetWindowRect(g_Variables.g_hCheatWindow, &rect);
					SetCursorPos(rect.left + (rect.right - rect.left) / 2, rect.top + (rect.bottom - rect.top) / 2);
				}
				else
				{
					g_MenuInfo.IsOpen = false;
					set_overlay_style(false);

					// Hand focus back so mouse look and keyboard reach the game again.
					if (g_Variables.g_hGameWindow && IsWindow(g_Variables.g_hGameWindow))
						ForceForegroundWindow(g_Variables.g_hGameWindow);
				}
			}

			if (!renderOverlay)
			{
				ImGui::GetIO().MouseDrawCursor = false;
				ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
				ShowWindow(g_Variables.g_hCheatWindow, SW_HIDE);
				std::this_thread::sleep_for(std::chrono::milliseconds(35));
				continue;
			}

			ShowWindow(g_Variables.g_hCheatWindow, SW_SHOWNA);

			// NVIDIA/Steam/Discord overlays also live in the topmost band. Without a
			// periodic re-assert they end up above us and swallow every click, so the
			// menu stays visible but stops reacting.
			if (g_MenuInfo.IsOpen)
			{
				static ULONGLONG last_topmost_assert = 0;
				const ULONGLONG now = GetTickCount64();
				if (now - last_topmost_assert >= 400)
				{
					last_topmost_assert = now;
					SetWindowPos(g_Variables.g_hCheatWindow, HWND_TOPMOST, 0, 0, 0, 0,
						SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
				}
			}

			const bool customInputActive = IsCustomInputActive();
			ImGui::GetIO().MouseDrawCursor = customInputActive;
			if (!customInputActive)
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
			}

			static float BgAlpha = 1.f;
			BgAlpha = ImLerp(BgAlpha, g_MenuInfo.IsOpen ? 1.f : 0.f, ImGui::GetIO().DeltaTime * 8);

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();

			// Fallback: the Win32 backend only forwards mouse data while this window is the
			// foreground window. Another topmost overlay or a focus change then makes the
			// menu look alive but ignore every click, so the state is polled globally too.
			// Only when this window is not the foreground window: otherwise the Win32
			// backend already feeds the same events and every click would double up.
			if (g_MenuInfo.IsOpen && GetForegroundWindow() != g_Variables.g_hCheatWindow)
			{
				POINT cursor{};
				if (GetCursorPos(&cursor) && ScreenToClient(g_Variables.g_hCheatWindow, &cursor))
				{
					ImGuiIO& io = ImGui::GetIO();
					io.AddMousePosEvent(static_cast<float>(cursor.x), static_cast<float>(cursor.y));
					io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
					io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
					io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);
				}
			}

			ImGui::NewFrame();
			{


				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, BgAlpha);

				//ImGui::GetBackgroundDrawList( )->AddRectFilled( ImVec2( 0, 0 ), ImVec2( g_Variables.g_vGameWindowSize.x, g_Variables.g_vGameWindowSize.y ), ImColor( 0.f, 0.f, 0.f, BgAlpha >= 0.4f ? 0.4f : BgAlpha ) );

				Gui::Rendering();

				ImGui::PopStyleVar();

				slate->draw_pinned_overlays();

				if (customInputActive)
				{
					ImGui::SetMouseCursor(ImGuiMouseCursor_None);
					DrawCustomInputCursor();
				}
			}
			ImGui::EndFrame();

			const float ClearColor[4] = { 0 };
			g_pd3dDeviceContext->OMSetRenderTargets(1U, &g_mainRenderTargetView, NULL);
			g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, ClearColor);

			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

			g_pSwapChain->Present(true, 0U); //VSync

			static bool unloadKeyDown = false;
			if (g_Config.General->UnloadKey != 0 && (GetAsyncKeyState(g_Config.General->UnloadKey) & 0x8000))
			{
				if (!unloadKeyDown)
				{
					unloadKeyDown = true;
					Core::RequestShutDown();
				}
			}
			else
			{
				unloadKeyDown = false;
			}

			if (g_Config.General->ShutDown)
			{
				static bool shutdown_cleanup_done = false;
				if (!shutdown_cleanup_done)
				{
					shutdown_cleanup_done = true;
					Core::PerformShutDownCleanup();
					Gui::ShutdownStreamProofForUnload();
					auth::SendBypassAction(xorstr("reset_connectedcheat"));
				}
				break;
			}

			if (lowPowerOverlay)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			}
		}

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::GetIO().MouseDrawCursor = false;
		ImGui::DestroyContext();

		CleanupDeviceD3D();
		DestroyWindow(g_Variables.g_hCheatWindow);
		UnregisterClassW(wc.lpszClassName, wc.hInstance);

#ifdef FULLSCREEN_EX
		UnhookWindowsHookEx(hKeyboardHook);
#endif // FULLSCREEN_EX
		return;
	}

	void Overlay::Fonts()
	{
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		io.IniFilename = nullptr;
		io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
		io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_LightHinting;
		io.Fonts->TexGlyphPadding = ImMax(io.Fonts->TexGlyphPadding, 2);

		ImFontConfig Cfg;
		Cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_LightHinting | ImGuiFreeTypeBuilderFlags_LoadColor;
		Cfg.FontDataOwnedByAtlas = false;
		Cfg.RasterizerDensity = 1.25f;
		Cfg.RasterizerMultiply = 1.05f;

		ImFontConfig DrawCfg;
		DrawCfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_LightHinting;
		DrawCfg.FontDataOwnedByAtlas = false;
		DrawCfg.PixelSnapH = true;
		DrawCfg.OversampleH = 3;
		DrawCfg.OversampleV = 2;
		DrawCfg.RasterizerDensity = 1.30f;
		DrawCfg.RasterizerMultiply = 1.08f;


		g_Variables.m_FontBig = io.Fonts->AddFontFromMemoryTTF(InterSemiBold, sizeof(InterSemiBold), 20.f, &Cfg, io.Fonts->GetGlyphRangesCyrillic());
		g_Variables.m_FontBigSmall = io.Fonts->AddFontFromMemoryTTF(InterSemiBold, sizeof(InterSemiBold), 18.f, &Cfg, io.Fonts->GetGlyphRangesCyrillic());
		g_Variables.m_FontNormal = io.Fonts->AddFontFromMemoryTTF(InterSemiBold, sizeof(InterSemiBold), 18.f, &Cfg, io.Fonts->GetGlyphRangesCyrillic());
		g_Variables.m_FontSecundary = io.Fonts->AddFontFromMemoryTTF(InterSemiBold, sizeof(InterSemiBold), 16.f, &Cfg, io.Fonts->GetGlyphRangesCyrillic());
		g_Variables.m_FontSmaller = io.Fonts->AddFontFromMemoryTTF(InterSemiBold, sizeof(InterSemiBold), 14.f, &Cfg, io.Fonts->GetGlyphRangesCyrillic());
		g_Variables.m_FontSmaller2 = io.Fonts->AddFontFromMemoryTTF(MontSerratEB, sizeof(MontSerratEB), 14.f, &Cfg, io.Fonts->GetGlyphRangesCyrillic());

		if (!g_Variables.m_FontNormal) g_Variables.m_FontNormal = io.Fonts->AddFontDefault();
		if (!g_Variables.m_FontBig) g_Variables.m_FontBig = g_Variables.m_FontNormal;
		if (!g_Variables.m_FontBigSmall) g_Variables.m_FontBigSmall = g_Variables.m_FontNormal;
		if (!g_Variables.m_FontSecundary) g_Variables.m_FontSecundary = g_Variables.m_FontNormal;
		if (!g_Variables.m_FontSmaller) g_Variables.m_FontSmaller = g_Variables.m_FontNormal;
		if (!g_Variables.m_FontSmaller2) g_Variables.m_FontSmaller2 = g_Variables.m_FontNormal;

		g_Variables.m_DrawFont = io.Fonts->AddFontFromMemoryTTF(InterBold, sizeof(InterBold), 14.f, &DrawCfg, io.Fonts->GetGlyphRangesCyrillic());
		g_Variables.m_DrawFont2 = io.Fonts->AddFontFromMemoryTTF(InterBold, sizeof(InterBold), 13.f, &DrawCfg, io.Fonts->GetGlyphRangesCyrillic());
		g_Variables.m_DrawFont3 = io.Fonts->AddFontFromMemoryTTF(InterBold, sizeof(InterBold), 12.f, &DrawCfg, io.Fonts->GetGlyphRangesCyrillic());

		static const ImWchar FontAwesomeRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		static const ImWchar FontAwesomeRangesBrands[] = { ICON_MIN_FAB, ICON_MAX_FAB, 0 };

		ImFontConfig FontAwesomeConfig;
		FontAwesomeConfig.PixelSnapH = true;
		FontAwesomeConfig.GlyphMinAdvanceX = 19;

		g_Variables.FontAwesomeSolid = io.Fonts->AddFontFromMemoryCompressedTTF(FontAwesome6Solid_compressed_data, FontAwesome6Solid_compressed_size, 19, &FontAwesomeConfig, FontAwesomeRanges);
		g_Variables.FontAwesomeSolidSmall = io.Fonts->AddFontFromMemoryCompressedTTF(FontAwesome6Solid_compressed_data, FontAwesome6Solid_compressed_size, 16, &FontAwesomeConfig, FontAwesomeRanges);
		g_Variables.FontAwesomeRegular = io.Fonts->AddFontFromMemoryCompressedTTF(FontAwesome6Regular_compressed_data, FontAwesome6Regular_compressed_size, 19, &FontAwesomeConfig, FontAwesomeRanges);

		g_Variables.m_Expand = io.Fonts->AddFontFromMemoryTTF(expand_binary, sizeof(expand_binary), 11.f, &Cfg, io.Fonts->GetGlyphRangesCyrillic());
	}

	void Overlay::Style()
	{
		ImGui::StyleColorsDark();

		ImGuiStyle* style = &ImGui::GetStyle();
		style->AntiAliasedLines = true;
		style->AntiAliasedFill = true;
		{
			style->WindowPadding = ImVec2(0, 0);
			style->WindowBorderSize = 0.f;
			style->ItemSpacing = ImVec2(12, 12);
			style->ScrollbarSize = 4.f;
		}

		if (g_Variables.Logo == nullptr)
		{
			D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, logo_binary, sizeof(logo_binary), &Info, Pump, &g_Variables.Logo, 0);
			//WeaponsIcons
			//Overlay::LoadWeaponIcons( );
		}
	}

	namespace
	{
		IDCompositionDevice* g_dcompDevice = nullptr;
		IDCompositionTarget* g_dcompTarget = nullptr;
		IDCompositionVisual* g_dcompVisual = nullptr;

		void ReleaseComposition()
		{
			if (g_dcompVisual) { g_dcompVisual->Release(); g_dcompVisual = nullptr; }
			if (g_dcompTarget) { g_dcompTarget->Release(); g_dcompTarget = nullptr; }
			if (g_dcompDevice) { g_dcompDevice->Release(); g_dcompDevice = nullptr; }
		}

		// Premultiplied DirectComposition swapchain: gives the overlay per-pixel alpha
		// without WS_EX_LAYERED, so a display affinity is allowed on the window.
		bool CreateCompositionSwapChain(HWND hWnd)
		{
			UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
			const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
			D3D_FEATURE_LEVEL featureLevel;
			if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext)))
				return false;

			IDXGIDevice* dxgiDevice = nullptr;
			IDXGIAdapter* adapter = nullptr;
			IDXGIFactory2* factory = nullptr;
			IDXGISwapChain1* swapChain = nullptr;
			bool ok = false;

			if (SUCCEEDED(g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))
				&& SUCCEEDED(dxgiDevice->GetAdapter(&adapter))
				&& SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory))))
			{
				RECT client{};
				GetClientRect(hWnd, &client);

				DXGI_SWAP_CHAIN_DESC1 desc{};
				desc.Width = (client.right - client.left) > 0 ? static_cast<UINT>(client.right - client.left) : 1u;
				desc.Height = (client.bottom - client.top) > 0 ? static_cast<UINT>(client.bottom - client.top) : 1u;
				desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				desc.SampleDesc.Count = 1;
				desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
				desc.BufferCount = 2;
				desc.Scaling = DXGI_SCALING_STRETCH;
				desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
				desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

				if (SUCCEEDED(factory->CreateSwapChainForComposition(g_pd3dDevice, &desc, nullptr, &swapChain)))
				{
					IDCompositionDevice* dcomp = nullptr;
					if (SUCCEEDED(DCompositionCreateDevice(dxgiDevice, IID_PPV_ARGS(&dcomp))))
					{
						IDCompositionTarget* target = nullptr;
						IDCompositionVisual* visual = nullptr;

						if (SUCCEEDED(dcomp->CreateTargetForHwnd(hWnd, FALSE, &target))
							&& SUCCEEDED(dcomp->CreateVisual(&visual))
							&& SUCCEEDED(visual->SetContent(swapChain))
							&& SUCCEEDED(target->SetRoot(visual))
							&& SUCCEEDED(dcomp->Commit()))
						{
							g_dcompDevice = dcomp;
							g_dcompTarget = target;
							g_dcompVisual = visual;
							g_pSwapChain = swapChain;
							ok = true;
						}
						else
						{
							if (visual) visual->Release();
							if (target) target->Release();
							dcomp->Release();
						}
					}
				}
			}

			if (!ok)
			{
				if (swapChain) swapChain->Release();
				if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
				if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
			}

			if (factory) factory->Release();
			if (adapter) adapter->Release();
			if (dxgiDevice) dxgiDevice->Release();

			return ok;
		}
	}

	bool Overlay::CreateDeviceD3D(HWND hWnd)
	{
		if (CreateCompositionSwapChain(hWnd))
		{
			CreateRenderTarget();
			return true;
		}

		// Fallback: a layered window renders too, but Windows rejects display affinity on it.
		OutputDebugStringA("[overlay] DirectComposition failed, falling back to layered window\n");
		SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
		SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 255, LWA_ALPHA);

		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 2;
		sd.BufferDesc.Width = 0;
		sd.BufferDesc.Height = 0;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		UINT createDeviceFlags = 0;
		//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
		D3D_FEATURE_LEVEL featureLevel;
		const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
		if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
			return false;

		CreateRenderTarget();
		return true;
	}

	void Overlay::CleanupDeviceD3D()
	{
		CleanupRenderTarget();
		ReleaseComposition();

		if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
		if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
		if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
	}

	void Overlay::CreateRenderTarget()
	{
		ID3D11Texture2D* pBackBuffer;
		g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
		pBackBuffer->Release();
	}

	void Overlay::CleanupRenderTarget()
	{
		if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
	}

	LRESULT CALLBACK Overlay::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return true;

	switch (msg) {
	case WM_NCHITTEST:
	{
		// The overlay spans the whole game window. Only the menu and its side panels
		// may take clicks; everywhere else the click has to fall through to the game
		// or the desktop, otherwise nothing is clickable while the menu is open.
		if (!g_MenuInfo.IsOpen)
			return HTTRANSPARENT;

		POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		ScreenToClient(hWnd, &point);

		ImGuiContext* context = ImGui::GetCurrentContext();
		if (!context)
			return HTTRANSPARENT;

		for (ImGuiWindow* window : context->Windows)
		{
			// Only the top-level menu panels are interactive. Child windows, tooltips
			// and full-size background helpers must never swallow the click.
			if (!window || window->ParentWindow != nullptr)
				continue;
			if (!window->WasActive || window->Hidden || window->SkipItems)
				continue;
			if (window->Flags & (ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMouseInputs))
				continue;

			const ImVec2 min = window->Pos;
			const ImVec2 max = window->Pos + window->Size;
			if (static_cast<float>(point.x) >= min.x && static_cast<float>(point.x) < max.x
				&& static_cast<float>(point.y) >= min.y && static_cast<float>(point.y) < max.y)
				return HTCLIENT;
		}

		return HTTRANSPARENT;
	}
	case WM_SIZE:
			if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
				Overlay::CleanupRenderTarget();
				g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
				Overlay::CreateRenderTarget();
			}
			return 0;
		case WM_DESTROY:
			::PostQuitMessage(0);
			return 0;
			break;
		}

		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	DWORD Overlay::GetWinLogonToken(DWORD dwSessionId, DWORD dwDesiredAccess, PHANDLE phToken)
	{
		DWORD dwErr;
		PRIVILEGE_SET ps;

		ps.PrivilegeCount = 1;
		ps.Control = PRIVILEGE_SET_ALL_NECESSARY;

		if (LookupPrivilegeValue(NULL, SE_TCB_NAME, &ps.Privilege[0].Luid)) {
			HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
			if (INVALID_HANDLE_VALUE != hSnapshot) {
				BOOL bCont, bFound = FALSE;
				PROCESSENTRY32 pe;

				pe.dwSize = sizeof(pe);
				dwErr = ERROR_NOT_FOUND;

				for (bCont = Process32First(hSnapshot, &pe); bCont; bCont = Process32Next(hSnapshot, &pe)) {
					HANDLE hProcess;

					if (0 != _tcsicmp(pe.szExeFile, xorstr("winlogon.exe"))) {
						continue;
					}

					hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
					if (hProcess) {
						HANDLE hToken;
						DWORD dwRetLen, sid;

						if (OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_DUPLICATE, &hToken)) {
							BOOL fTcb;

							if (PrivilegeCheck(hToken, &ps, &fTcb) && fTcb) {
								if (GetTokenInformation(hToken, TokenSessionId, &sid, sizeof(sid), &dwRetLen) && sid == dwSessionId) {
									bFound = TRUE;
									if (DuplicateTokenEx(hToken, dwDesiredAccess, NULL, SecurityImpersonation, TokenImpersonation, phToken)) {
										dwErr = ERROR_SUCCESS;
									}
									else {
										dwErr = GetLastError();
									}
								}
							}
							CloseHandle(hToken);
						}
						CloseHandle(hProcess);
					}

					if (bFound) break;
				}

				CloseHandle(hSnapshot);
			}
			else {
				dwErr = GetLastError();
			}
		}
		else {
			dwErr = GetLastError();
		}


		return dwErr;
	}

	DWORD Overlay::CreateUIAccessToken(PHANDLE phToken)
	{
		DWORD dwErr;
		HANDLE hTokenSelf;

		if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &hTokenSelf)) {
			DWORD dwSessionId, dwRetLen;

			if (GetTokenInformation(hTokenSelf, TokenSessionId, &dwSessionId, sizeof(dwSessionId), &dwRetLen)) {
				HANDLE hTokenSystem;

				dwErr = GetWinLogonToken(dwSessionId, TOKEN_IMPERSONATE, &hTokenSystem);
				if (ERROR_SUCCESS == dwErr) {
					if (SetThreadToken(NULL, hTokenSystem)) {
						if (DuplicateTokenEx(hTokenSelf, TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_DEFAULT, NULL, SecurityAnonymous, TokenPrimary, phToken)) {
							BOOL bUIAccess = TRUE;

							if (!SetTokenInformation(*phToken, TokenUIAccess, &bUIAccess, sizeof(bUIAccess))) {
								dwErr = GetLastError();
								CloseHandle(*phToken);
							}
						}
						else {
							dwErr = GetLastError();
						}
						RevertToSelf();
					}
					else {
						dwErr = GetLastError();
					}
					CloseHandle(hTokenSystem);
				}
			}
			else {
				dwErr = GetLastError();
			}

			CloseHandle(hTokenSelf);
		}
		else {
			dwErr = GetLastError();
		}

		return dwErr;
	}

	BOOL Overlay::CheckForUIAccess(DWORD* pdwErr, DWORD* pfUIAccess)
	{
		BOOL result = FALSE;
		HANDLE hToken;

		if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
			DWORD dwRetLen;

			if (GetTokenInformation(hToken, TokenUIAccess, pfUIAccess, sizeof(*pfUIAccess), &dwRetLen)) {
				result = TRUE;
			}
			else {
				*pdwErr = GetLastError();
			}
			CloseHandle(hToken);
		}
		else {
			*pdwErr = GetLastError();
		}

		return result;
	}

	DWORD Overlay::PrepareForUIAccess()
	{
		DWORD dwErr;
		HANDLE hTokenUIAccess;
		DWORD fUIAccess;

		if (CheckForUIAccess(&dwErr, &fUIAccess)) {
			if (fUIAccess) {
				dwErr = ERROR_SUCCESS;
			}
			else {
				dwErr = CreateUIAccessToken(&hTokenUIAccess);
				if (ERROR_SUCCESS == dwErr) {
					STARTUPINFO si;
					PROCESS_INFORMATION pi;

					GetStartupInfo(&si);
					if (CreateProcessAsUser(hTokenUIAccess, NULL, GetCommandLine(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
						CloseHandle(pi.hProcess), CloseHandle(pi.hThread);
						ExitProcess(0);
					}
					else {
						dwErr = GetLastError();
					}

					CloseHandle(hTokenUIAccess);
				}
			}
		}

		return dwErr;
	}
}
