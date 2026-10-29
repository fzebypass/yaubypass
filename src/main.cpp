#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <tchar.h>
#include <winternl.h>

#include <vector>
#include <regex>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>

#include <Gui/Overlay/Overlay.hpp>
#include <Includes/Includes.hpp>
#include <Core/SDK/Memory.hpp>
#include <Core/config.hpp>
#include <Includes/Utils.hpp>
#include <Core/SDK/SDK.hpp>
#include <Core/SDK/visible.hpp>
#include <Core/Core.hpp>
#include <Gui/gui.hpp>
#include <Security/AntiCrack.hpp>
#include <Security/Api/Auth/api/api.hh>
#include <Includes/CustomWidgets/Custom.hpp>
#include <Includes/ImGui/Fonts.hpp>
#include <../Security/api.hpp>
#include <../Globals.hpp>
#include "../Security/xorstr.hpp"
#include "Core/Features/Exploits/Exploits.hpp"
#include <d3d11.h>
#include <dxgi.h>
#include <tlhelp32.h>
#include "prot/protection.hpp"
#include <Core/Features/LicenseGate.hpp>
using namespace Core;

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

LONG WINAPI CrashHandler(EXCEPTION_POINTERS*)
{
    return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI VectoredHandler(EXCEPTION_POINTERS*)
{
    return EXCEPTION_CONTINUE_SEARCH;
}

RECT rc;
void style() {
    ImGuiStyle* style = &ImGui::GetStyle();
    style->AntiAliasedLines = true;
    style->AntiAliasedLinesUseTex = true;
    style->AntiAliasedFill = true;
    style->CurveTessellationTol = 1.10f;
    style->CircleTessellationMaxError = 0.10f;
    style->WindowRounding = 10.0f;
    style->WindowPadding = ImVec2(0.f, 0.f);
    style->WindowBorderSize = 1.f;
    style->Alpha = 1.1f;
    style->Colors[ImGuiCol_WindowBg] = ImVec4(ImColor(17, 17, 21));
    style->Colors[ImGuiCol_ChildBg] = ImVec4(ImColor(255, 255, 255, 255));
    style->Colors[ImGuiCol_Border] = ImVec4(22, 22, 22, 0);
    //style->WindowBorderSize = 0.1f;
}

inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, last - first + 1);
}

ID3D11Device* g_pd3dDevice = NULL;
ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
IDXGISwapChain* g_pSwapChain = NULL;
ID3D11RenderTargetView* g_mainRenderTargetView = NULL;
D3DX11_IMAGE_LOAD_INFO info;
ID3DX11ThreadPump* pump{ nullptr };
HWND hwnd;
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImVec2 window_size = ImVec2(440, 320);

void move_window() {
    ImGui::SetCursorPos(ImVec2(0, 0));
    if (ImGui::InvisibleButton("Move_detector", ImVec2(window_size.x, window_size.y))) {}
    if (ImGui::IsItemActive()) {
        GetWindowRect(hwnd, &rc);
        MoveWindow(hwnd, rc.left + static_cast<int>(ImGui::GetMouseDragDelta().x), rc.top + static_cast<int>(ImGui::GetMouseDragDelta().y),
            static_cast<int>(window_size.x), static_cast<int>(window_size.y), TRUE);
    }
}

bool flag = false;
c_globals globals;
static enum PANELS {
    login_panel,
    register_panel,
} panel;


int gay() {
    while (!Mem.GetMaxPrivileges(GetCurrentProcess())) {
        Sleep(1000);
    }

    for (;;) {
        g_Variables.g_hGameWindow = FindWindowA(xorstr("grcWindow"), nullptr);
        if (g_Variables.g_hGameWindow) {
            auto WindowInfo = Utils::GetWindowPosAndSize(g_Variables.g_hGameWindow);
            g_Variables.g_vGameWindowSize = WindowInfo.second;
            g_Variables.g_vGameWindowPos = WindowInfo.first;
            g_Variables.g_vGameWindowCenter = { g_Variables.g_vGameWindowSize.x / 2, g_Variables.g_vGameWindowSize.y / 2 };
            break;
        }
        Sleep(1000);
    }

    for (;;) {
        DWORD ProcIdFiveM = 0;
        GetWindowThreadProcessId(g_Variables.g_hGameWindow, &ProcIdFiveM);
        if (ProcIdFiveM == 0) {
            g_Variables.g_hGameWindow = nullptr;
            while (!g_Variables.g_hGameWindow) {
                g_Variables.g_hGameWindow = FindWindowA(xorstr("grcWindow"), nullptr);
                if (g_Variables.g_hGameWindow) {
                    auto WindowInfo = Utils::GetWindowPosAndSize(g_Variables.g_hGameWindow);
                    g_Variables.g_vGameWindowSize = WindowInfo.second;
                    g_Variables.g_vGameWindowPos = WindowInfo.first;
                    g_Variables.g_vGameWindowCenter = { g_Variables.g_vGameWindowSize.x / 2, g_Variables.g_vGameWindowSize.y / 2 };
                    break;
                }
                Sleep(1000);
            }
            continue;
        }
        std::string ProcessName = Mem.GetNameByPid(ProcIdFiveM);
        if (ProcessName.empty()) {
            Sleep(1000);
            continue;
        }
        if (!Mem.OpenProc(ProcessName.c_str())) {
            Sleep(1000);
            continue;
        }

        g_Offsets.CurrentBuild = 0;
        if (ProcessName.find(xorstr("FiveM_b2060")) != std::string::npos) g_Offsets.CurrentBuild = 2060;
        else if (ProcessName.find(xorstr("FiveM_b2189")) != std::string::npos) g_Offsets.CurrentBuild = 2189;
        else if (ProcessName.find(xorstr("FiveM_b2372")) != std::string::npos) g_Offsets.CurrentBuild = 2372;
        else if (ProcessName.find(xorstr("FiveM_b2545")) != std::string::npos) g_Offsets.CurrentBuild = 2545;
        else if (ProcessName.find(xorstr("FiveM_b2612")) != std::string::npos) g_Offsets.CurrentBuild = 2612;
        else if (ProcessName.find(xorstr("FiveM_b2699")) != std::string::npos) g_Offsets.CurrentBuild = 2699;
        else if (ProcessName.find(xorstr("FiveM_b2802")) != std::string::npos) g_Offsets.CurrentBuild = 2802;
        else if (ProcessName.find(xorstr("FiveM_b2944")) != std::string::npos) g_Offsets.CurrentBuild = 2944;
        else if (ProcessName.find(xorstr("FiveM_b3095")) != std::string::npos) g_Offsets.CurrentBuild = 3095;
        else if (ProcessName.find(xorstr("FiveM_b3258")) != std::string::npos) g_Offsets.CurrentBuild = 3258;
        else if (ProcessName.find(xorstr("FiveM_b3323")) != std::string::npos) g_Offsets.CurrentBuild = 3323;
        else if (ProcessName.find(xorstr("FiveM_b3407")) != std::string::npos) g_Offsets.CurrentBuild = 3407;
        else if (ProcessName.find(xorstr("FiveM_b1604")) != std::string::npos) g_Offsets.CurrentBuild = 1604;
        else if (ProcessName.find(xorstr("FiveM_b3570")) != std::string::npos) g_Offsets.CurrentBuild = 3570;
        //run_protection();
        auth::SendBypassAction(xorstr("set_connectedcheat"));
        Core::SetupOffsets();
        Mem.AttachProces(ProcIdFiveM);
        Core::Features::g_VisibilityChecker.Initialize();
        //Utils::OpenConsole();
        break;
    }
    return 0;
}

static void setClipboardText(const std::string& utf8text) {
    if (!OpenClipboard(nullptr)) {
        DWORD err = GetLastError();
        return;
    }
    if (!EmptyClipboard()) {
        DWORD err = GetLastError();
    }
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8text.c_str(), -1, nullptr, 0);
    if (wlen <= 0) {
        DWORD err = GetLastError();
        CloseClipboard(); return;
    }
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(wchar_t));
    if (!hMem) {
        DWORD err = GetLastError();
        CloseClipboard(); return;
    }
    wchar_t* wbuf = static_cast<wchar_t*>(GlobalLock(hMem));
    if (!wbuf) {
        DWORD err = GetLastError();
        GlobalFree(hMem); CloseClipboard(); return;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, utf8text.c_str(), -1, wbuf, wlen) == 0) {
        DWORD err = GetLastError();
    }
    GlobalUnlock(hMem);
    if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
        DWORD err = GetLastError();
    }
    CloseClipboard();
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetErrorMode(SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(CrashHandler);
    SetUnhandledExceptionFilter(VectoredHandler);

    // ---- licence gate: the cheat only starts with a valid key -------------
    if (!LicenseGate::Run(hInstance)) {
        return 0;
    }

    WNDCLASSEXW wc;
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = NULL;
    wc.cbWndExtra = NULL;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = L" ";
    wc.lpszClassName = L"StaticOverlayClass";
    wc.hIconSm = LoadIcon(0, IDI_APPLICATION);
    RegisterClassExW(&wc);

    hwnd = CreateWindowExW(NULL, wc.lpszClassName, L" ", WS_POPUP, (GetSystemMetrics(SM_CXSCREEN) / 2) - static_cast<int>(window_size.x / 2),
        (GetSystemMetrics(SM_CYSCREEN) / 2) - static_cast<int>(window_size.y / 2),
        static_cast<int>(window_size.x), static_cast<int>(window_size.y), 0, 0, hInstance, 0);

    SetWindowLongA(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    POINT mouse;
    rc = { 0 };
    GetWindowRect(hwnd, &rc);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext(); {
        ImGui::StyleColorsDark();
        style();
    }
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImFontConfig cfg;
    cfg.FontBuilderFlags = ImGuiBackendFlags_None;
    cfg.FontDataOwnedByAtlas = false;
    globals.open_sans = io.Fonts->AddFontFromMemoryCompressedBase85TTF(open_sans.data(), 18);
    globals.open_sans_sm = io.Fonts->AddFontFromMemoryCompressedBase85TTF(open_sans.data(), 21);

    static const ImWchar icons_ranges[] = { 0xe000, 0xf8ff, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.OversampleH = 3;
    icons_config.OversampleV = 3;
    static const ImWchar FontAwesomeRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    static const ImWchar FontAwesomeRangesBrands[] = { ICON_MIN_FAB, ICON_MAX_FAB, 0 };
    ImFontConfig FontAwesomeConfig;
    FontAwesomeConfig.MergeMode = true;
    FontAwesomeConfig.PixelSnapH = true;
    FontAwesomeConfig.GlyphMinAdvanceX = 17.f;

    io.IniFilename = nullptr;
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    static bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        static bool hide = true;
        static int opticaly = 255;
        /*if ( GetAsyncKeyState( VK_INSERT ) & 0x1 ) hide = !hide;
        if ( GetAsyncKeyState( VK_DELETE ) & 1 ) { exit( 0 ); }*/
        float targetOpacity = (opticaly <= 255 && hide) ? 300.0f : 0.0f;
        opticaly = static_cast<int>(ImLerp(static_cast<float>(opticaly), targetOpacity, ImGui::GetIO().DeltaTime * 8.f));
        if (opticaly > 255) opticaly = 255;

        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), opticaly, LWA_ALPHA);
        ShowWindow(hwnd, opticaly > 0 ? SW_SHOW : SW_HIDE);

        ImGui::NewFrame();
        {
            ImGui::SetNextWindowPos({ 0, 0 }, ImGuiCond_Once);
            ImGui::SetNextWindowSize({ window_size.x, window_size.y }, ImGuiCond_Once);
            ImGui::Begin(" ", &done, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);
            {
               ShowWindow(hwnd, SW_HIDE);
               done = true;

               // The game is attached in the background, the overlay itself starts right
               // away so the menu also opens on the desktop without FiveM running.
               std::thread([] { gay(); }).detach();
               Gui::cOverlay.Render();
                
            }
            ImGui::End();
        }
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0 };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

bool CreateDeviceD3D(HWND hWnd) {
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

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) {
        g_pSwapChain->Release();
        g_pSwapChain = NULL;
    }
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = NULL;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = NULL;
    }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = NULL;
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

std::string hwid;
