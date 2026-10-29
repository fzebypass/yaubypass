#pragma once
#include <Includes/Includes.hpp>
#include <Includes/Utils.hpp>
#include <Core/Lifecycle.hpp>

#include <Windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <regex>

namespace Core
{
    namespace Threads
    {
        class cScreenResolution
        {
        public:
            HWND g_hMirrorWindow = nullptr;

            void SyncCheatWindowToBounds(const POINT& pos, const POINT& size)
            {
                if (!g_Variables.g_hCheatWindow)
                    return;

                const int width = size.x > 0 ? size.x : 1;
                const int height = size.y > 0 ? size.y : 1;

                SetWindowPos(
                    g_Variables.g_hCheatWindow,
                    HWND_TOPMOST,
                    pos.x,
                    pos.y,
                    width,
                    height,
                    SWP_NOACTIVATE | SWP_SHOWWINDOW
                );
            }

            static LRESULT CALLBACK MirrorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
            {
                switch (msg)
                {
                case WM_LBUTTONDOWN:
                case WM_RBUTTONDOWN:
                case WM_MOUSEMOVE:
                    return 0;

                case WM_PAINT:
                {
                    PAINTSTRUCT ps;
                    HDC hdc = BeginPaint(hwnd, &ps);
                    EndPaint(hwnd, &ps);
                    return 0;
                }

                default:
                    return DefWindowProc(hwnd, msg, wParam, lParam);
                }
            }

            void MoveAndSetResolution()
            {
                if (!g_Variables.g_hGameWindow)
                    return;

                POINT monitorPos;
                POINT monitorSize;

                GetTargetMonitorInfo(g_Config.General->SecondMonitor, monitorPos, monitorSize);

                g_Variables.g_vGameWindowPos = { static_cast<float>(monitorPos.x), static_cast<float>(monitorPos.y) };
                g_Variables.g_vGameWindowSize = { static_cast<float>(monitorSize.x), static_cast<float>(monitorSize.y) };
                g_Variables.g_vGameWindowCenter = { g_Variables.g_vGameWindowSize.x / 2.0f, g_Variables.g_vGameWindowSize.y / 2.0f };

                if (g_Variables.g_hCheatWindow)
                {
                    SyncCheatWindowToBounds(monitorPos, monitorSize);
                }
            }
            void UpdateGameWindowMetrics()
            {
                if (!g_Variables.g_hGameWindow)
                    return;

                const auto windowInfo = Utils::GetWindowPosAndSize(g_Variables.g_hGameWindow);
                g_Variables.g_vGameWindowPos = windowInfo.first;
                g_Variables.g_vGameWindowSize = windowInfo.second;
                g_Variables.g_vGameWindowCenter = {
                    g_Variables.g_vGameWindowSize.x / 2.0f,
                    g_Variables.g_vGameWindowSize.y / 2.0f
                };
            }
            static void GetTargetMonitorInfo(bool useSecondMonitor, POINT& pos, POINT& size)
            {
                HMONITOR targetMonitor = useSecondMonitor ? GetSecondaryMonitor() : GetPrimaryMonitor();
                if (targetMonitor)
                {
                    auto [monitorPos, monitorSize] = GetMonitorDimensions(targetMonitor);
                    pos = monitorPos;
                    size = monitorSize;
                }
                else
                {
                    pos = { static_cast<LONG>(g_Variables.g_vGameWindowPos.x), static_cast<LONG>(g_Variables.g_vGameWindowPos.y) };
                    size = { static_cast<LONG>(g_Variables.g_vGameWindowSize.x), static_cast<LONG>(g_Variables.g_vGameWindowSize.y) };
                }
            }
            void CreateMirrorWindow()
            {
                if (!g_Variables.g_hGameWindow || g_hMirrorWindow || !g_Config.General->SecondMonitor)
                    return;

                static bool registered = false;
                if (!registered)
                {
                    WNDCLASS wc = {};
                    wc.lpfnWndProc = MirrorWndProc;
                    wc.hInstance = GetModuleHandle(nullptr);
                    wc.lpszClassName = xorstr("MirrorWindowClass");
                    RegisterClass(&wc);
                    registered = true;
                }

                POINT pos, size;
                GetTargetMonitorInfo(true, pos, size);

                g_hMirrorWindow = CreateWindowExA(
                    WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
                    xorstr("MirrorWindowClass"),
                    xorstr("FiveM Mirror"),
                    WS_POPUP | WS_VISIBLE,
                    pos.x, pos.y,
                    size.x, size.y,
                    nullptr,
                    nullptr,
                    GetModuleHandle(nullptr),
                    nullptr
                );

                SetLayeredWindowAttributes(g_hMirrorWindow, 0, 255, LWA_ALPHA);

                if (g_Variables.g_hCheatWindow)
                {
                    SetWindowPos(
                        g_hMirrorWindow,
                        g_Variables.g_hCheatWindow,
                        pos.x, pos.y, size.x, size.y,
                        SWP_SHOWWINDOW
                    );
                }
                else
                {
                    ShowWindow(g_hMirrorWindow, SW_SHOW);
                }

                UpdateWindow(g_hMirrorWindow);
            }
            void MirrorGameWindow()
            {
                if (!g_Variables.g_hGameWindow || !g_hMirrorWindow)
                    return;

                RECT rc;
                GetClientRect(g_Variables.g_hGameWindow, &rc);

                HDC hdcWindow = GetDC(g_Variables.g_hGameWindow);
                if (!hdcWindow) return;

                HDC hdcMem = CreateCompatibleDC(hdcWindow);
                if (!hdcMem)
                {
                    ReleaseDC(g_Variables.g_hGameWindow, hdcWindow);
                    return;
                }

                HBITMAP hbm = CreateCompatibleBitmap(hdcWindow, rc.right, rc.bottom);
                if (!hbm)
                {
                    DeleteDC(hdcMem);
                    ReleaseDC(g_Variables.g_hGameWindow, hdcWindow);
                    return;
                }

                SelectObject(hdcMem, hbm);
                BitBlt(hdcMem, 0, 0, rc.right, rc.bottom, hdcWindow, 0, 0, SRCCOPY);

                HDC hdcMirror = GetDC(g_hMirrorWindow);
                if (hdcMirror)
                {
                    HDC hdcMirror = GetDC(g_hMirrorWindow);
                    if (hdcMirror)
                    {
                        RECT rcMirror;
                        GetClientRect(g_hMirrorWindow, &rcMirror);

                        SetStretchBltMode(hdcMirror, HALFTONE);

                        StretchBlt(
                            hdcMirror,
                            0, 0, rcMirror.right, rcMirror.bottom,
                            hdcMem,
                            0, 0, rc.right, rc.bottom,
                            SRCCOPY
                        );

                        ReleaseDC(g_hMirrorWindow, hdcMirror);
                    }
                    ReleaseDC(g_hMirrorWindow, hdcMirror);
                }

                DeleteObject(hbm);
                DeleteDC(hdcMem);
                ReleaseDC(g_Variables.g_hGameWindow, hdcWindow);
            }

            void Update()
            {
                static int monitor_check_counter = 0;
                static int failed_checks = 0;
                static bool had_game_window = false;

                while (!Core::IsShutDownRequested())
                {
                    const bool mirrorActive = g_Config.General->SecondMonitor && g_Config.General->MirrorFivem;
                    try
                    {
                        g_Variables.g_hGameWindow = FindWindowA(xorstr("grcWindow"), nullptr);
                        if (!g_Variables.g_hGameWindow) {
                            if (g_hMirrorWindow)
                            {
                                DestroyWindow(g_hMirrorWindow);
                                g_hMirrorWindow = nullptr;
                            }

                            if (had_game_window)
                            {
                                had_game_window = false;
                                Core::RequestOffsetsRefresh();
                            }

                            std::this_thread::sleep_for(std::chrono::milliseconds(mirrorActive ? 40 : 250));
                            continue;
                        }

                        had_game_window = true;
                        UpdateGameWindowMetrics();

                        if (g_Config.General->SecondMonitor && g_Variables.g_hGameWindow)
                        {
                            MoveAndSetResolution();
                            if (g_Config.General->MirrorFivem)
                            {
                                CreateMirrorWindow();
                                MirrorGameWindow();
                            }
                            else if (g_hMirrorWindow)
                            {
                                DestroyWindow(g_hMirrorWindow);
                                g_hMirrorWindow = nullptr;
                            }
                        }
                        else
                        {
                            if (g_hMirrorWindow)
                            {
                                DestroyWindow(g_hMirrorWindow);
                                g_hMirrorWindow = nullptr;
                            }

                            if (g_Variables.g_hCheatWindow)
                            {
                                POINT pos{
                                    static_cast<LONG>(g_Variables.g_vGameWindowPos.x),
                                    static_cast<LONG>(g_Variables.g_vGameWindowPos.y)
                                };
                                POINT size{
                                    static_cast<LONG>(g_Variables.g_vGameWindowSize.x),
                                    static_cast<LONG>(g_Variables.g_vGameWindowSize.y)
                                };
                                SyncCheatWindowToBounds(pos, size);
                            }
                        }

                        if (monitor_check_counter % 34 == 0)
                        {
                            if (g_Config.General->SecondMonitor)
                            {
                                auto monitors = GetAllMonitors();
                                bool has_second_monitor = monitors.size() >= 2;

                                if (!has_second_monitor)
                                {
                                    failed_checks++;
                                    if (failed_checks >= 2)
                                    {
                                        g_Config.General->MirrorFivem = false;
                                        if (g_hMirrorWindow)
                                        {
                                            DestroyWindow(g_hMirrorWindow);
                                            g_hMirrorWindow = nullptr;
                                        }
                                        g_Config.General->SecondMonitor = false;
                                        failed_checks = 0;
                                    }
                                }
                                else
                                {
                                    failed_checks = 0;
                                }
                            }
                        }

                        monitor_check_counter++;
                    }
                    catch (...) {}

                    std::this_thread::sleep_for(std::chrono::milliseconds(mirrorActive ? 40 : 250));
                }
            }

            static std::vector<HMONITOR> GetAllMonitors()
            {
                std::vector<HMONITOR> monitors;
                EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
                    auto* pMonitors = reinterpret_cast<std::vector<HMONITOR>*>(lParam);
                    pMonitors->push_back(hMonitor);
                    return TRUE;
                    }, reinterpret_cast<LPARAM>(&monitors));
                return monitors;
            }

            static std::pair<POINT, POINT> GetMonitorDimensions(HMONITOR hMonitor)
            {
                MONITORINFO mi = { sizeof(MONITORINFO) };
                if (GetMonitorInfo(hMonitor, &mi))
                {
                    POINT pos = { mi.rcMonitor.left, mi.rcMonitor.top };
                    POINT size = { mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top };
                    return std::make_pair(pos, size);
                }

                DEVMODE dm = {};
                dm.dmSize = sizeof(dm);
                if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm))
                {
                    POINT pos = { 0, 0 };
                    POINT size = { dm.dmPelsWidth, dm.dmPelsHeight };
                    return std::make_pair(pos, size);
                }

                POINT pos = { 0, 0 };
                POINT size = { 1920, 1080 };
                return std::make_pair(pos, size);
            }
            static HMONITOR GetSecondaryMonitor()
            {
                HMONITOR secondary = NULL;
                EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
                    MONITORINFO mi = { sizeof(MONITORINFO) };
                    if (GetMonitorInfo(hMonitor, &mi))
                    {
                        if (!(mi.dwFlags & MONITORINFOF_PRIMARY))
                        {
                            *(HMONITOR*)lParam = hMonitor;
                            return FALSE;
                        }
                    }
                    return TRUE;
                    }, (LPARAM)&secondary);
                return secondary;
            }

            static HMONITOR GetPrimaryMonitor()
            {
                HMONITOR primary = NULL;
                EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
                    MONITORINFO mi = { sizeof(MONITORINFO) };
                    if (GetMonitorInfo(hMonitor, &mi))
                    {
                        if (mi.dwFlags & MONITORINFOF_PRIMARY)
                        {
                            *(HMONITOR*)lParam = hMonitor;
                            return FALSE;
                        }
                    }
                    return TRUE;
                    }, (LPARAM)&primary);
                return primary;
            }
        };

        inline cScreenResolution g_ScreenResolution;
    }
}
