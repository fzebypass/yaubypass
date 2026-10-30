// PRUDA Loader
// Standalone host for the reference ImGui build: borderless DX11 window, the new
// logo, a key field and a start action that launches the cheat next to this exe.
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_settings.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <d3d11.h>
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <string>
#include <cstdio>
#include <cctype>

#include "logo_rgba.h"
#include "imgui/reference_font.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace font
{
    ImFont* inter_bold = nullptr;
    ImFont* inter_child = nullptr;
    ImFont* manrope_widgets = nullptr;
    ImFont* icon_font = nullptr;
}

namespace loader
{
    constexpr int   window_width = 460;
    constexpr int   window_height = 600;
    constexpr char  cheat_name[] = "Wizard External.exe";

    static ID3D11ShaderResourceView* logo_texture = nullptr;
    static std::string key_input;
    static char        key_buffer[64] = {};
    static std::string status = "Enter your key";
    static bool        status_error = false;
    static bool        unlocked = false;
    static bool        remember_key = true;

    bool CreateLogoTexture(ID3D11Device* device)
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = logo_width;
        desc.Height = logo_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = logo_rgba;
        data.SysMemPitch = logo_width * 4;

        ID3D11Texture2D* texture = nullptr;
        if (FAILED(device->CreateTexture2D(&desc, &data, &texture)))
            return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = desc.Format;
        view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        view.Texture2D.MipLevels = 1;

        const HRESULT hr = device->CreateShaderResourceView(texture, &view, &logo_texture);
        texture->Release();
        return SUCCEEDED(hr);
    }

    // Only the shape is checked here: the licence server was removed from the project,
    // so the loader validates the format and lets the cheat itself decide.
    bool ValidateKey(const std::string& key)
    {
        int clean = 0;
        for (char c : key)
            if (!isxdigit(static_cast<unsigned char>(c)) && !isalpha(static_cast<unsigned char>(c))) continue; else clean++;
        return clean >= 8;
    }

    void LaunchCheat()
    {
        char path[MAX_PATH]{};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string dir(path);
        const size_t slash = dir.find_last_of("\\/");
        if (slash != std::string::npos) dir = dir.substr(0, slash + 1);

        const std::string target = dir + cheat_name;
        if (GetFileAttributesA(target.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            status = std::string("Not found: ") + cheat_name;
            status_error = true;
            return;
        }

        // The cheat needs admin rights for its registry work.
        ShellExecuteA(nullptr, "runas", target.c_str(), nullptr, dir.c_str(), SW_SHOWNORMAL);
        status = "Launching...";
        status_error = false;
    }

    void Submit()
    {
        key_input = key_buffer;
        if (key_input.empty())
        {
            status = "Enter a key first";
            status_error = true;
            return;
        }
        if (!ValidateKey(key_input))
        {
            status = "Invalid key";
            status_error = true;
            return;
        }

        unlocked = true;
        status = "Key accepted";
        status_error = false;
    }

    void Draw(ID3D11ShaderResourceView* logo)
    {
        ImGuiIO& io = ImGui::GetIO();

        // This build has no alpha-multiplier overload of GetColorU32.
        const auto with_alpha = [](const ImVec4& color, float alpha) {
            ImVec4 c = color;
            c.w *= alpha;
            return ImGui::GetColorU32(c);
        };

        if (GetAsyncKeyState(VK_ESCAPE) & 0x01)
            PostQuitMessage(0);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(window_width), static_cast<float>(window_height)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::Begin("##loader", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);
        {
            ImGui::PopStyleVar(2);

            const ImVec2 pos = ImGui::GetWindowPos();
            const ImVec2 size = ImGui::GetWindowSize();
            ImDrawList* draw = ImGui::GetWindowDrawList();

            draw->AddRectFilled(pos, pos + size, ImGui::GetColorU32(c::bg::background), c::bg::rounding);
            draw->AddRect(pos, pos + size, ImGui::GetColorU32(c::child::border), c::bg::rounding, 0, 1.5f);

            // Close button
            {
                const ImVec2 close_pos = pos + ImVec2(size.x - 30, 14);
                ImGui::SetCursorScreenPos(close_pos);
                ImGui::InvisibleButton("##close", ImVec2(16, 16));
                if (ImGui::IsItemClicked())
                    PostQuitMessage(0);
                const bool hovered = ImGui::IsItemHovered();
                draw->AddText(close_pos + ImVec2(2, -1), with_alpha(c::text::text_hov, hovered ? 1.f : 0.6f), "x");
            }

            // Logo
            const float logo_w = 150.f;
            const float logo_h = logo_w * (static_cast<float>(logo_height) / static_cast<float>(logo_width));
            const ImVec2 logo_min = pos + ImVec2((size.x - logo_w) * 0.5f, 34.f);
            if (logo)
                draw->AddImage((ImTextureID)logo, logo_min, logo_min + ImVec2(logo_w, logo_h));

            // Title
            ImGui::PushFont(font::inter_child);
            {
                const char* title = "PRUDA";
                const float tw = ImGui::CalcTextSize(title).x;
                draw->AddText(pos + ImVec2((size.x - tw) * 0.5f, logo_min.y + logo_h + 18.f), ImGui::GetColorU32(c::accent_color), title);
            }
            ImGui::PopFont();

            ImGui::PushFont(font::inter_bold);
            {
                const char* sub = unlocked ? "KEY ACCEPTED" : "ENTER YOUR KEY";
                const float sw = ImGui::CalcTextSize(sub).x;
                draw->AddText(pos + ImVec2((size.x - sw) * 0.5f, logo_min.y + logo_h + 44.f), ImGui::GetColorU32(c::text::text), sub);
            }
            ImGui::PopFont();

            const float content_w = size.x - 72.f;
            ImGui::SetCursorPos(ImVec2(36.f, logo_min.y + logo_h + 84.f));
            ImGui::BeginGroup();
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 14));

                if (!unlocked)
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, c::input::rounding);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, c::input::background);
                    ImGui::PushStyleColor(ImGuiCol_Border, c::input::border);
                    ImGui::SetNextItemWidth(content_w);
                    const bool submitted = ImGui::InputTextEx("##key", "XXXX-XXXX-XXXX-XXXX", key_buffer, sizeof(key_buffer),
                        ImVec2(content_w, 34.f), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar();

                    if (submitted)
                        Submit();
                }
                else
                {
                    if (ImGui::Button("Start", ImVec2(content_w, 34.f)))
                        LaunchCheat();
                }

                if (ImGui::Button(unlocked ? "Back" : "Login", ImVec2(content_w, 34.f)))
                {
                    if (unlocked)
                    {
                        unlocked = false;
                        status = "Enter your key";
                        status_error = false;
                    }
                    else
                    {
                        Submit();
                    }
                }

                ImGui::PopStyleVar();
            }
            ImGui::EndGroup();

            // Status line
            ImGui::PushFont(font::inter_bold);
            {
                const float sy = logo_min.y + logo_h + 84.f + 96.f;
                ImVec4 col = status_error ? ImVec4(1.f, 0.45f, 0.45f, 1.f) : c::text::text_hov;
                draw->AddText(pos + ImVec2(36.f, sy), ImGui::GetColorU32(col), status.c_str());
            }
            ImGui::PopFont();

            const char* hint = "F1 opens the menu in game";
            ImGui::PushFont(font::inter_bold);
            {
                const float hw = ImGui::CalcTextSize(hint).x;
                draw->AddText(pos + ImVec2((size.x - hw) * 0.5f, size.y - 34.f), with_alpha(c::text::text, 0.7f), hint);
            }
            ImGui::PopFont();

            (void)io;
        }
        ImGui::End();
    }
}

// ---------------------------------------------------------------------------
// Win32 / D3D11 host
// ---------------------------------------------------------------------------
static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;
static IDXGISwapChain* g_swapchain = nullptr;
static ID3D11RenderTargetView* g_target = nullptr;
static bool g_resize_pending = false;

static void CreateRenderTarget()
{
    ID3D11Texture2D* back = nullptr;
    g_swapchain->GetBuffer(0, IID_PPV_ARGS(&back));
    g_device->CreateRenderTargetView(back, nullptr, &g_target);
    back->Release();
}

static void CleanupRenderTarget()
{
    if (g_target) { g_target->Release(); g_target = nullptr; }
}

static bool CreateDevice(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
        &sd, &g_swapchain, &g_device, &level, &g_context) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;

    switch (msg)
    {
    case WM_NCHITTEST:
    {
        // The whole window is borderless; the top strip drags it.
        POINT pt{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
        ScreenToClient(hwnd, &pt);
        if (pt.y >= 0 && pt.y < 32 && pt.x < loader::window_width - 40)
            return HTCAPTION;
        return HTCLIENT;
    }
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) g_resize_pending = true;
        break;
    case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"PruaLoaderWindow";
    RegisterClassExW(&wc);

    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExW(WS_EX_LAYERED, wc.lpszClassName, L"PRUDA Loader", WS_POPUP,
        (screen_w - loader::window_width) / 2, (screen_h - loader::window_height) / 2,
        loader::window_width, loader::window_height, nullptr, nullptr, instance, nullptr);

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
    MARGINS margins{ -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    if (!CreateDevice(hwnd))
    {
        UnregisterClassW(wc.lpszClassName, instance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;

    font::manrope_widgets = io.Fonts->AddFontFromMemoryTTF(manrope_medium, sizeof(manrope_medium), 18.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    font::inter_child = io.Fonts->AddFontFromMemoryTTF(inter_bold, sizeof(inter_bold), 22.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    font::inter_bold = io.Fonts->AddFontFromMemoryTTF(inter_bold, sizeof(inter_bold), 14.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    font::icon_font = io.Fonts->AddFontFromMemoryTTF(icon_font, sizeof(icon_font), 17.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    if (!font::manrope_widgets) font::manrope_widgets = io.Fonts->AddFontDefault();
    if (!font::inter_child) font::inter_child = font::manrope_widgets;
    if (!font::inter_bold) font::inter_bold = font::manrope_widgets;
    if (!font::icon_font) font::icon_font = font::manrope_widgets;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    loader::CreateLogoTexture(g_device);

    bool running = true;
    while (running)
    {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                running = false;
        }
        if (!running)
            break;

        if (g_resize_pending)
        {
            CleanupRenderTarget();
            g_swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
            g_resize_pending = false;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        loader::Draw(loader::logo_texture);
        ImGui::Render();

        const float clear[4] = { 0.f, 0.f, 0.f, 0.f };
        g_context->OMSetRenderTargets(1, &g_target, nullptr);
        g_context->ClearRenderTargetView(g_target, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_swapchain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (loader::logo_texture) loader::logo_texture->Release();
    CleanupRenderTarget();
    if (g_swapchain) g_swapchain->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, instance);
    return 0;
}
