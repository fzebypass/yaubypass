#include <slate.h>
#include "../resources/fonts.h"
#include "../resources/images.h"
#include "../resources/setup_frames.h"
#include "Core/config.hpp"
#include "Core/Core.hpp"
#include "cleaners/cleaners.h"
#include "Core/features/exploits/exploits.hpp"
#include "Core/features/auth.hpp"
#include "Core/features/WizardVisuals.hpp"
#include "Core/features/nvidia.hpp"
#include "Core/features/NVIDIA/ShadowPlayPatch.hpp"
#include "Core/features/auth.hpp"
#include <Core/SDK/SDK.hpp>
#include "includes/includes.hpp"
#include <Includes/CustomWidgets/Notify.hpp>
#include <Includes/CustomWidgets/Preview.hpp>
#include <Gui/Gui.hpp>
#include <Gui/Crosshair.hpp>
#include <atomic>
#include <algorithm>
#include <shellapi.h>
#include <array>
#include <cctype>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
using namespace Core;

using namespace ImGui;

// Temporary placeholders: all six seasonal slots use the current embedded logo
// until separate byte arrays are added.
c_image logo_spring;
c_image logo_summer;
c_image logo_autumn;
c_image logo_winter;
c_image logo_christmas;
c_image logo_easter;
inline void SoloSession(bool toggle) {
    static uintptr_t solo_session_start = 0;
    static uintptr_t solo_session_end = 0;
    static uintptr_t is_network_tutorial = 0;

    if (solo_session_start == 0) {
        solo_session_start = Mem.FindSignatureStr("48 83 EC ? E8 ? ? ? ? 48 85 C0 74 ? 48 8B 88 ? ? ? ? 48 85 C9 74 ? E8 ? ? ? ? 48 83 C4");
    }

    if (solo_session_end == 0) {
        solo_session_end = Mem.FindSignatureStr("48 83 EC ? E8 ? ? ? ? 48 85 C0 74 ? 48 8B 88 ? ? ? ? 48 85 C9 74 ? 80 B9");
    }

    if (is_network_tutorial == 0) {
        is_network_tutorial = Mem.FindSignatureStr("40 53 48 83 EC ? 33 DB E8 ? ? ? ? 48 85 C0 74 ? 48 8B 88 ? ? ? ? 48 85 C9 74 ? 80 B9");
        if (!is_network_tutorial) {
            is_network_tutorial = Mem.FindSignatureStr("40 53 48 83 EC ? 33 DB E8 ? ? ? ? 48 85 C0 74 ? 48 8B 80 ? ? ? ? 48 85 C0 74 ? 80 B8");
        }
    }

    auto ExecuteShellcode = [](uintptr_t func_addr) {
        if (!func_addr) return;

        unsigned char shellcode[] = {
            0x48, 0x83, 0xEC, 0x28, 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xD0, 0x48, 0x83, 0xC4, 0x28, 0xC3
        };
        *(uintptr_t*)(shellcode + 6) = func_addr;

        LPVOID remote_mem = VirtualAllocEx(Mem.ProcHandle, NULL, sizeof(shellcode), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (remote_mem) {
            WriteProcessMemory(Mem.ProcHandle, remote_mem, shellcode, sizeof(shellcode), NULL);
            HANDLE hThread = CreateRemoteThread(Mem.ProcHandle, NULL, 0, (LPTHREAD_START_ROUTINE)remote_mem, NULL, 0, NULL);
            if (hThread) {
                WaitForSingleObject(hThread, 5000);
                CloseHandle(hThread);
            }
            VirtualFreeEx(Mem.ProcHandle, remote_mem, 0, MEM_RELEASE);
        }
        };
}

class Spinbot {
private:
    float angle = 0.0f;
    DWORD lastTime = GetTickCount();

public:


    void Apply(CPed* ped) {
        if (!ped) {
            return;
        }

        DWORD now = GetTickCount();
        angle += (now - lastTime) * ((g_Config.Player->SpinbotSpeed) / 1000.0f);
        lastTime = now;

        if (angle > 360.0f) {
            angle = std::fmod(angle, 360.0f);
        }

        float rad = angle * 0.01745329251f;

        D3DXVECTOR3 forward(sin(rad), cos(rad), 0.0f);
        D3DXVECTOR3 right(cos(rad), -sin(rad), 0.0f);
        D3DXVECTOR3 up(0.0f, 0.0f, 1.0f);

        uintptr_t addr = (uintptr_t)ped;

        Core::Mem.Write<D3DXVECTOR3>(addr + 0x60, right);
        Core::Mem.Write<D3DXVECTOR3>(addr + 0x70, forward);
        Core::Mem.Write<D3DXVECTOR3>(addr + 0x80, up);
    }
};

Spinbot g_Spinbot;

void UpdateSpinbot() {
    auto ped = Core::SDK::Pointers::pLocalPlayer;
    g_Spinbot.Apply(ped);
}

namespace {
    ULONGLONG menu_opened_at = 0;
    bool last_menu_open = false;
    struct config_browser_ui_state_t;
    config_browser_ui_state_t& get_config_browser_ui_state();

    void UpdatePersistentLocalFeatures() {
        Core::SetFreezeAmmoEnabled(g_Config.Player->InfiniteAmmoEnabled, true);

        if (g_Config.Player->Spinbot) {
            UpdateSpinbot();
        }
    }
    ULONGLONG filetime_to_ull(const FILETIME& file_time) {
        return (static_cast<ULONGLONG>(file_time.dwHighDateTime) << 32) | file_time.dwLowDateTime;
    }

    std::string format_runtime_duration(const std::chrono::seconds duration) {
        const auto total_seconds = static_cast<unsigned long long>(duration.count());
        const auto hours = total_seconds / 3600ULL;
        const auto minutes = (total_seconds % 3600ULL) / 60ULL;
        const auto seconds = total_seconds % 60ULL;

        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%02llu:%02llu:%02llu", hours, minutes, seconds);
        return buffer;
    }

    enum class menu_season_t {
        Spring,
        Summer,
        Autumn,
        Winter,
        Christmas,
        Easter
    };

    struct seasonal_menu_palette_t {
        const char* label = "Winter";
        pcolor accent;
        pcolor accent_soft;
        pcolor bright;
        pcolor text;
        pcolor text_dim;
        pcolor text_faint;
        pcolor border;
        pcolor separator;
        pcolor backdrop;
        pcolor badge_fill;
        pcolor badge_detail;
    };

    pcolor with_alpha(const pcolor& color, const float alpha) {
        return pcolor{ color.r, color.g, color.b, alpha };
    }

    pcolor blend_color(const pcolor& from, const pcolor& to, float t) {
        t = ImClamp(t, 0.0f, 1.0f);
        return pcolor{
            ImLerp(from.r, to.r, t),
            ImLerp(from.g, to.g, t),
            ImLerp(from.b, to.b, t),
            ImLerp(from.a, to.a, t)
        };
    }

    std::tm get_local_menu_time() {
        const std::time_t now = std::time(nullptr);
        std::tm local_time{};
        localtime_s(&local_time, &now);
        return local_time;
    }

    int day_of_year(const int year, const int month, const int day) {
        std::tm date{};
        date.tm_year = year - 1900;
        date.tm_mon = month - 1;
        date.tm_mday = day;
        date.tm_isdst = -1;
        std::mktime(&date);
        return date.tm_yday;
    }

    int easter_day_of_year(const int year) {
        const int a = year % 19;
        const int b = year / 100;
        const int c = year % 100;
        const int d = b / 4;
        const int e = b % 4;
        const int f = (b + 8) / 25;
        const int g = (b - f + 1) / 3;
        const int h = (19 * a + b - d - g + 15) % 30;
        const int i = c / 4;
        const int k = c % 4;
        const int l = (32 + 2 * e + 2 * i - h - k) % 7;
        const int m = (a + 11 * h + 22 * l) / 451;
        const int month = (h + l - 7 * m + 114) / 31;
        const int day = ((h + l - 7 * m + 114) % 31) + 1;
        return day_of_year(year, month, day);
    }

    bool is_christmas_window(const std::tm& local_time) {
        return (local_time.tm_mon == 11 && local_time.tm_mday >= 20) || (local_time.tm_mon == 0 && local_time.tm_mday <= 2);
    }

    bool is_easter_window(const std::tm& local_time) {
        const int easter = easter_day_of_year(local_time.tm_year + 1900);
        return local_time.tm_yday >= easter - 2 && local_time.tm_yday <= easter + 1;
    }

    menu_season_t get_active_menu_season() {
        const std::tm local_time = get_local_menu_time();
        if (is_christmas_window(local_time)) {
            return menu_season_t::Christmas;
        }
        if (is_easter_window(local_time)) {
            return menu_season_t::Easter;
        }

        switch (local_time.tm_mon) {
        case 2:
        case 3:
        case 4:
            return menu_season_t::Spring;
        case 5:
        case 6:
        case 7:
            return menu_season_t::Summer;
        case 8:
        case 9:
        case 10:
            return menu_season_t::Autumn;
        default:
            return menu_season_t::Winter;
        }
    }

    const seasonal_menu_palette_t& get_menu_palette(const menu_season_t) {
        // The menu palette follows the animated infinity mark: a white core inside a
        // saturated violet halo, sitting on a near-black violet-tinted surface.
        static const seasonal_menu_palette_t logo{
            "Dynamic",
            pcolor{ 139, 92, 246, 1.0f },
            pcolor{ 186, 148, 255, 1.0f },
            pcolor{ 238, 232, 255, 1.0f },
            pcolor{ 240, 236, 255, 1.0f },
            pcolor{ 158, 150, 190, 1.0f },
            pcolor{ 158, 150, 190, 0.60f },
            pcolor{ 255, 255, 255, 0.06f },
            pcolor{ 255, 255, 255, 0.04f },
            pcolor{ 58, 32, 110, 0.22f },
            pcolor{ 139, 92, 246, 1.0f },
            pcolor{ 240, 232, 255, 1.0f }
        };

        return logo;
    }

    const seasonal_menu_palette_t& get_active_menu_palette() {
        return get_menu_palette(get_active_menu_season());
    }

    enum class menu_theme_mode_t {
        Dynamic,
        White,
        Black
    };

    menu_theme_mode_t get_menu_theme_mode() {
        switch (g_Config.General->MenuThemeMode) {
        case 1:
            return menu_theme_mode_t::White;
        case 2:
            return menu_theme_mode_t::Black;
        default:
            return menu_theme_mode_t::Dynamic;
        }
    }

    struct seasonal_logo_binary_t {
        unsigned char* data = nullptr;
        size_t size = 0;
    };

    const seasonal_logo_binary_t logo_spring_binary{ logobinary, sizeof(logobinary) };
    const seasonal_logo_binary_t logo_summer_binary{ logobinary, sizeof(logobinary) };
    const seasonal_logo_binary_t logo_autumn_binary{ logobinary, sizeof(logobinary) };
    const seasonal_logo_binary_t logo_winter_binary{ logobinary, sizeof(logobinary) };
    const seasonal_logo_binary_t logo_christmas_binary{ logobinary, sizeof(logobinary) };
    const seasonal_logo_binary_t logo_easter_binary{ logobinary, sizeof(logobinary) };

    c_image& get_menu_logo_texture(const menu_season_t season) {
        switch (season) {
        case menu_season_t::Spring:
            return logo_spring;
        case menu_season_t::Summer:
            return logo_summer;
        case menu_season_t::Autumn:
            return logo_autumn;
        case menu_season_t::Christmas:
            return logo_christmas;
        case menu_season_t::Easter:
            return logo_easter;
        default:
            return logo_winter;
        }
    }

    const seasonal_logo_binary_t& get_menu_logo_binary(const menu_season_t season) {
        switch (season) {
        case menu_season_t::Spring:
            return logo_spring_binary;
        case menu_season_t::Summer:
            return logo_summer_binary;
        case menu_season_t::Autumn:
            return logo_autumn_binary;
        case menu_season_t::Christmas:
            return logo_christmas_binary;
        case menu_season_t::Easter:
            return logo_easter_binary;
        default:
            return logo_winter_binary;
        }
    }

    c_image& get_active_menu_logo_texture() {
        return get_menu_logo_texture(get_active_menu_season());
    }

    void load_menu_logo_texture(ID3D11Device* device, const menu_season_t season) {
        auto& texture = get_menu_logo_texture(season);
        const auto& binary = get_menu_logo_binary(season);
        texture.reset();
        texture.load(device, binary.data, binary.size);
        Core::Features::WizardVisuals::SetLogoTexture(reinterpret_cast<void*>(static_cast<ImTextureID>(texture)));
    }

    void sync_imgui_theme_colors() {
        GImGui->Style.Colors[ImGuiCol_Text] = g_style->colors[pcol_text];
        GImGui->Style.Colors[ImGuiCol_TextDisabled] = g_style->colors[pcol_text2];
        GImGui->Style.Colors[ImGuiCol_WindowBg] = g_style->colors[pcol_bg];
        GImGui->Style.Colors[ImGuiCol_ChildBg] = g_style->colors[pcol_bg2];
        GImGui->Style.Colors[ImGuiCol_Border] = g_style->colors[pcol_border];
        GImGui->Style.Colors[ImGuiCol_TextSelectedBg] = with_alpha(g_style->colors[pcol_scheme], 0.30f);
        GImGui->Style.Colors[ImGuiCol_WindowShadow] = pcolor{ 0.0f, 0.0f, 0.0f, 0.0f };
    }

    void sync_configured_menu_theme_base() {
        switch (get_menu_theme_mode()) {
        case menu_theme_mode_t::Dynamic:
            g_style->theme = 0;
            break;
        
            g_style->theme = 1;
            break;
       
            g_style->theme = 0;
            break;
        }
    }

    void apply_seasonal_menu_theme() {
        const auto& palette = get_active_menu_palette();
        auto& colors = g_style->colors;

        if (g_style->theme == 0) {
            colors[pcol_scheme] = palette.accent;
            colors[pcol_text] = palette.text;
            colors[pcol_text2] = palette.text_dim;
            colors[pcol_text3] = palette.text_faint;
            colors[pcol_bg5] = blend_color(palette.bright, palette.accent_soft, 0.12f);
            colors[pcol_border] = palette.border;
            colors[pcol_separator] = palette.separator;
            colors[pcol_backdrop] = palette.backdrop;
        }
        else {
            const pcolor light_accent = blend_color(palette.accent, pcolor{ 21, 21, 21, 1.0f }, 0.28f);
            colors[pcol_scheme] = light_accent;
            colors[pcol_text2] = blend_color(colors[pcol_text2], light_accent, 0.12f);
            colors[pcol_text3] = with_alpha(colors[pcol_text2], 0.60f);
            colors[pcol_bg5] = blend_color(palette.bright, pcolor{ 255, 255, 255, 1.0f }, 0.28f);
            colors[pcol_border] = with_alpha(light_accent, 0.14f);
            colors[pcol_separator] = with_alpha(palette.accent_soft, 0.08f);
            colors[pcol_backdrop] = with_alpha(light_accent, 0.10f);
        }

        sync_imgui_theme_colors();
    }

    void apply_configured_menu_theme() {
        // Dynamic is the only menu theme; it follows the logo colours.
        g_Config.General->MenuThemeMode = 0;

        if (get_menu_theme_mode() == menu_theme_mode_t::Dynamic) {
            apply_seasonal_menu_theme();
            return;
        }

        sync_imgui_theme_colors();
    }

    void draw_seasonal_logo(ImDrawList* draw, const ImVec2& min, const ImVec2& max, ImU32 tint) {
        auto& texture = get_active_menu_logo_texture();
        Core::Features::WizardVisuals::SetLogoTexture(reinterpret_cast<void*>(static_cast<ImTextureID>(texture)));

        // Fit the logo into the given box without distorting its aspect ratio.
        ImVec2 image_min = min;
        ImVec2 image_max = max;
        if (texture.w > 0.f && texture.h > 0.f) {
            const float scale = ImMin((max.x - min.x) / texture.w, (max.y - min.y) / texture.h);
            const ImVec2 size{ texture.w * scale, texture.h * scale };
            const ImVec2 center{ (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };
            image_min = ImVec2{ center.x - size.x * 0.5f, center.y - size.y * 0.5f };
            image_max = ImVec2{ center.x + size.x * 0.5f, center.y + size.y * 0.5f };
        }

        draw->AddImage((ImTextureID)texture, image_min, image_max, { 0, 0 }, { 1, 1 }, tint);
    }

    void draw_seasonal_logo(ImDrawList* draw, const ImVec2& min, const ImVec2& max) {
        draw_seasonal_logo(draw, min, max, IM_COL32_WHITE);
    }

    // Animated infinity mark: the outline is a Gerono lemniscate, a bright head runs
    // along the path and leaves a comet tail behind it. Drawn procedurally instead of
    // blitting the bitmap logo, so it animates and stays crisp at any size.
    void draw_infinity_logo(ImDrawList* draw, const ImVec2& box_min, const ImVec2& box_max,
        const pcolor& core, const pcolor& halo, const float alpha)
    {
        const ImVec2 center{ (box_min.x + box_max.x) * 0.5f, (box_min.y + box_max.y) * 0.5f };
        const float half_w = (box_max.x - box_min.x) * 0.44f;
        const float half_h = ImMin((box_max.y - box_min.y) * 0.92f, half_w * 0.52f);

        constexpr int segments = 128;
        static std::array<ImVec2, segments + 1> path{};
        for (int i = 0; i <= segments; ++i) {
            const float t = (static_cast<float>(i) / static_cast<float>(segments)) * (IM_PI * 2.f);
            path[i] = ImVec2{ center.x + std::cos(t) * half_w, center.y + std::sin(t) * std::cos(t) * half_h };
        }

        const float time = static_cast<float>(ImGui::GetTime());
        const float pulse = 0.84f + 0.16f * std::sin(time * 2.1f);
        const float strength = ImClamp(alpha * pulse, 0.f, 1.f);
        const auto blend = [&](const pcolor& c, const float a) {
            return ImGui::GetColorU32(ImVec4(c.r, c.g, c.b, ImClamp(a, 0.f, 1.f)));
        };

        // Body: dim outline plus two soft halo passes.
        draw->AddPolyline(path.data(), segments + 1, blend(halo, 0.14f * strength), 0, 7.f ds);
        draw->AddPolyline(path.data(), segments + 1, blend(halo, 0.32f * strength), 0, 2.8f ds);

        // Comet head with a tapering tail.
        const float head = std::fmod(time * 0.62f, 1.f) * static_cast<float>(segments);
        constexpr int trail = 34;
        const auto wrap = [](int v) { return ((v % segments) + segments) % segments; };
        for (int i = 0; i < trail; ++i) {
            const int from = static_cast<int>(head) - i;
            const float k = 1.f - static_cast<float>(i) / static_cast<float>(trail);
            const float falloff = k * k;
            draw->AddLine(path[wrap(from)], path[wrap(from + 1)], blend(halo, 0.85f * falloff * strength), (1.1f + 2.4f * falloff) ds);
            if (i % 2 == 0) {
                draw->AddLine(path[wrap(from)], path[wrap(from + 1)], blend(core, 0.90f * falloff * strength), (0.4f + 1.3f * falloff) ds);
            }
        }

        const ImVec2 head_pos = path[wrap(static_cast<int>(head))];
        draw->AddCircleFilled(head_pos, 8.f ds, blend(halo, 0.10f * strength), 24);
        draw->AddCircleFilled(head_pos, 3.6f ds, blend(halo, 0.32f * strength), 24);
        draw->AddCircleFilled(head_pos, 1.6f ds, blend(core, strength), 16);
    }

    // Real frames of the setup animation (see resources/setup_frames.h).
    std::array<c_image, setup_animation::frame_count> setup_frames;
    bool setup_frames_loaded = false;

    // ---- First-run setup screen --------------------------------------------
    // Shown once instead of the menu: fullscreen black frame, the animated mark,
    // then an automatic reboot after the countdown. Esc skips the screen.
    bool setup_screen_done() {
        HKEY key{};
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\WiZardExternal", 0, KEY_READ, &key) != ERROR_SUCCESS)
            return false;

        DWORD value = 0;
        DWORD size = sizeof(value);
        DWORD type = 0;
        const bool done = RegQueryValueExW(key, L"SetupDone", nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS
            && type == REG_DWORD && value != 0;
        RegCloseKey(key);
        return done;
    }

    void mark_setup_screen_done() {
        HKEY key{};
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\WiZardExternal", 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
            return;

        const DWORD value = 1;
        RegSetValueExW(key, L"SetupDone", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
    }

    void draw_setup_screen() {
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* draw = ImGui::GetBackgroundDrawList();

        static ULONGLONG started_at = GetTickCount64();
        static bool finished = false;
        static constexpr float setup_seconds = 8.f;

        draw->AddRectFilled(ImVec2{ 0, 0 }, io.DisplaySize, IM_COL32(0, 0, 0, 255));

        // 1:1 the original animation: the embedded frames, advanced with the GIF timing.
        static int frame_index = 0;
        static double frame_started = ImGui::GetTime();
        const double frame_seconds = static_cast<double>(setup_animation::frame_delay_ms) / 1000.0;
        const double now = ImGui::GetTime();
        while (now - frame_started >= frame_seconds) {
            frame_started += frame_seconds;
            frame_index = (frame_index + 1) % setup_animation::frame_count;
        }

        // Reserve the lower quarter for the text, so the frame never covers it.
        const float frame_box_height = io.DisplaySize.y * 0.74f;
        float image_bottom = io.DisplaySize.y * 0.74f;
        if (setup_frames_loaded) {
            const auto& frame_image = setup_frames[frame_index];
            if (frame_image.w > 0.f && frame_image.h > 0.f) {
                const float scale = ImMin(io.DisplaySize.x / frame_image.w, frame_box_height / frame_image.h);
                const ImVec2 draw_size{ frame_image.w * scale, frame_image.h * scale };
                const ImVec2 draw_pos{ (io.DisplaySize.x - draw_size.x) * 0.5f,
                                       (io.DisplaySize.y * 0.48f) - draw_size.y * 0.5f };
                image_bottom = draw_pos.y + draw_size.y;
                draw->AddImage((ImTextureID)frame_image, draw_pos, draw_pos + draw_size);
            }
        }
        else {
            const auto& palette = get_active_menu_palette();
            const float mark_w = ImMin(io.DisplaySize.x * 0.40f, io.DisplaySize.y * 1.05f);
            const float mark_h = mark_w * 0.52f;
            const ImVec2 mark_center{ io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.43f };
            image_bottom = mark_center.y + mark_h * 0.5f;
            draw_infinity_logo(draw, ImVec2{ mark_center.x - mark_w * 0.5f, mark_center.y - mark_h * 0.5f },
                ImVec2{ mark_center.x + mark_w * 0.5f, mark_center.y + mark_h * 0.5f }, palette.bright, palette.accent, 1.f);
        }

        const auto& palette = get_active_menu_palette();
        ImFont* title_font = fonts[font].get(24);
        ImFont* text_font = fonts[font].get(15);
        if (!title_font) title_font = GetFont();
        if (!text_font) text_font = GetFont();

        const auto centered = [&](ImFont* f, float size, float y, ImU32 col, const std::string& text) {
            const ImVec2 extent = f->CalcTextSizeA(size, FLT_MAX, 0.f, text.c_str());
            draw->AddText(f, size, ImVec2{ (io.DisplaySize.x - extent.x) * 0.5f, y }, col, text.c_str());
        };

        const float elapsed = static_cast<float>(GetTickCount64() - started_at) / 1000.f;
        const int remaining = static_cast<int>(std::ceil(setup_seconds - elapsed));

        const float text_y = ImMin(image_bottom + 22 ds, io.DisplaySize.y - 120 ds);
        centered(title_font, 24.f, text_y, palette.bright, "Setup abgeschlossen");
        if (remaining > 0) {
            centered(text_font, 15.f, text_y + 40 ds,
                IM_COL32(190, 182, 210, 200), "Starte deinen PC neu in " + std::to_string(remaining) + " s");
        }
        else {
            centered(text_font, 15.f, text_y + 40 ds,
                IM_COL32(190, 182, 210, 200), "Starte deinen PC neu");

            if (!finished) {
                finished = true;
                mark_setup_screen_done();
                WinExec("shutdown /r /t 0", SW_HIDE);
            }
        }

        centered(text_font, 13.f, io.DisplaySize.y - 48 ds, IM_COL32(120, 112, 140, 190), "Esc zum Ueberspringen");

        if (GetAsyncKeyState(VK_ESCAPE) & 1) {
            mark_setup_screen_done();
            started_at = GetTickCount64();
            g_MenuInfo.IsOpen = true;
        }
    }

    void draw_seasonal_logo_widget(const ImVec2& size) {
        const ImVec2 min = GetCursorScreenPos();
        Dummy(size);
        const auto& palette = get_active_menu_palette();
        draw_infinity_logo(GetWindowDrawList(), min, min + size, palette.bright, palette.accent, 1.f);
    }

    enum class keybind_overlay_mode_t {
        Toggle,
        Hold,
        Press,
        Always
    };

    struct keybind_overlay_entry_t {
        const char* name = "";
        std::string bind_label;
        bool active = false;
    };

    bool is_key_down(const int key) {
        return key > 0 && (GetAsyncKeyState(key) & 0x8000) != 0;
    }

    std::string format_virtual_key_name(const int key) {
        switch (key) {
        case 0: return "Always";
        case VK_LBUTTON: return "Mouse 1";
        case VK_RBUTTON: return "Mouse 2";
        case VK_MBUTTON: return "Mouse 3";
        case VK_XBUTTON1: return "Mouse 4";
        case VK_XBUTTON2: return "Mouse 5";
        case VK_LSHIFT: return "L Shift";
        case VK_RSHIFT: return "R Shift";
        case VK_LCONTROL: return "L Ctrl";
        case VK_RCONTROL: return "R Ctrl";
        case VK_LMENU: return "L Alt";
        case VK_RMENU: return "R Alt";
        default:
            break;
        }

        UINT scan_code = MapVirtualKeyA(static_cast<UINT>(key), MAPVK_VK_TO_VSC);
        switch (key) {
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_INSERT:
        case VK_DELETE:
        case VK_DIVIDE:
        case VK_NUMLOCK:
        case VK_RCONTROL:
        case VK_RMENU:
            scan_code |= KF_EXTENDED;
            break;
        default:
            break;
        }

        char buffer[64]{};
        if (scan_code != 0 && GetKeyNameTextA(static_cast<LONG>(scan_code << 16), buffer, sizeof(buffer)) > 0) {
            return buffer;
        }

        char fallback[16]{};
        std::snprintf(fallback, sizeof(fallback), "0x%02X", key);
        return fallback;
    }

    const char* keybind_mode_suffix(const keybind_overlay_mode_t mode) {
        switch (mode) {
        case keybind_overlay_mode_t::Toggle:
            return "Toggle";
        case keybind_overlay_mode_t::Hold:
            return "Hold";
        case keybind_overlay_mode_t::Press:
            return "Press";
        case keybind_overlay_mode_t::Always:
            return "";
        }

        return "";
    }

    std::string format_keybind_label(const int key, const keybind_overlay_mode_t mode) {
        std::string label = "[";
        label += format_virtual_key_name(key);
        label += "]";

        const char* suffix = keybind_mode_suffix(mode);
        if (suffix[0] != '\0') {
            label += ' ';
            label += suffix;
        }

        return label;
    }

    keybind_overlay_mode_t overlay_mode_from_config(const int mode) {
        return mode == 1 ? keybind_overlay_mode_t::Hold : keybind_overlay_mode_t::Toggle;
    }

    void add_keybind_overlay_entry(
        std::vector<keybind_overlay_entry_t>& entries,
        const char* name,
        const bool visible,
        const bool active,
        const int key,
        const keybind_overlay_mode_t mode
    ) {
        if (!visible) {
            return;
        }

        if (key == 0 && mode != keybind_overlay_mode_t::Always) {
            return;
        }

        entries.push_back({ name, format_keybind_label(key, mode), active });
    }

    std::vector<keybind_overlay_entry_t> collect_keybind_overlay_entries() {
        std::vector<keybind_overlay_entry_t> entries;
        entries.reserve(10);

        add_keybind_overlay_entry(
            entries,
            "Aimbot",
            g_Config.Aimbot->Enabled,
            g_Config.Aimbot->KeyBindMode == 1 ? is_key_down(g_Config.Aimbot->KeyBind) : g_Config.Aimbot->KeyBindToggled,
            g_Config.Aimbot->KeyBind,
            overlay_mode_from_config(g_Config.Aimbot->KeyBindMode)
        );

        add_keybind_overlay_entry(
            entries,
            "Silent Aim",
            g_Config.SilentAim->Enabled,
            g_Config.SilentAim->KeyBindMode == 1 ? is_key_down(g_Config.SilentAim->KeyBind) : g_Config.SilentAim->KeyBindToggled,
            g_Config.SilentAim->KeyBind,
            overlay_mode_from_config(g_Config.SilentAim->KeyBindMode)
        );

        add_keybind_overlay_entry(
            entries,
            "Triggerbot",
            g_Config.TriggerBot->Enabled,
            g_Config.TriggerBot->KeyBindMode == 1 ? is_key_down(g_Config.TriggerBot->KeyBind) : g_Config.TriggerBot->KeyBindToggled,
            g_Config.TriggerBot->KeyBind,
            overlay_mode_from_config(g_Config.TriggerBot->KeyBindMode)
        );

        add_keybind_overlay_entry(
            entries,
            "Immortality",
            g_Config.Player->Immortality,
            g_Config.Player->ImmortalityKeyBind == 0
                ? true
                : (g_Config.Player->ImmortalityKeyBindMode == 1
                    ? is_key_down(g_Config.Player->ImmortalityKeyBind)
                    : g_Config.Player->Toggled != 0),
            g_Config.Player->ImmortalityKeyBind,
            g_Config.Player->ImmortalityKeyBind == 0
                ? keybind_overlay_mode_t::Always
                : overlay_mode_from_config(g_Config.Player->ImmortalityKeyBindMode)
        );

        add_keybind_overlay_entry(
            entries,
            "Fake Lag",
            g_Config.Player->FakeLag,
            g_Config.Player->FakeLagKey == 0
                ? true
                : (g_Config.Player->FakeLagMode == 1
                    ? is_key_down(g_Config.Player->FakeLagKey)
                    : g_Config.Player->FakelagToggle),
            g_Config.Player->FakeLagKey,
            g_Config.Player->FakeLagKey == 0
                ? keybind_overlay_mode_t::Always
                : overlay_mode_from_config(g_Config.Player->FakeLagMode)
        );

        add_keybind_overlay_entry(
            entries,
            "God Mode",
            g_Config.Player->GodModeKey != 0,
            g_Config.Player->EnableGodMode,
            g_Config.Player->GodModeKey,
            keybind_overlay_mode_t::Press
        );

        add_keybind_overlay_entry(
            entries,
            "NoClip",
            g_Config.Player->NoClipKey != 0,
            g_Config.Player->NoClipEnabled,
            g_Config.Player->NoClipKey,
            keybind_overlay_mode_t::Press
        );

        add_keybind_overlay_entry(
            entries,
            "Fast Run",
            g_Config.Player->FastRunKey != 0,
            g_Config.Player->FastRun,
            g_Config.Player->FastRunKey,
            keybind_overlay_mode_t::Press
        );

        add_keybind_overlay_entry(
            entries,
            "Friend Marker",
            g_Config.ESP->FriendsMarker && g_Config.ESP->FriendsMarkerBind != 0,
            is_key_down(g_Config.ESP->FriendsMarkerBind),
            g_Config.ESP->FriendsMarkerBind,
            keybind_overlay_mode_t::Press
        );

        std::stable_sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
            return left.active && !right.active;
            });

        return entries;
    }

    struct watermark_metrics_t {
        std::string uptime_text;
        std::string cpu_text;
    };


    struct movable_overlay_state_t {
        ImVec2 pos{};
        bool initialized = false;
        bool user_positioned = false;
    };

    struct process_metrics_sampler_t {
        DWORD process_id = GetCurrentProcessId();
        HANDLE process_handle = nullptr;
        ULONGLONG process_start_filetime = 0;
        ULONGLONG last_process_time = 0;
        std::chrono::steady_clock::time_point last_sample_time{};
        float displayed_cpu_percent = 0.0f;
        float target_cpu_percent = 0.0f;
        DWORD logical_processor_count = 1;
        bool initialized = false;

        void initialize() {
            if (initialized) {
                return;
            }

            process_handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
            if (!process_handle) {
                process_handle = GetCurrentProcess();
            }

            SYSTEM_INFO system_info{};
            GetSystemInfo(&system_info);
            logical_processor_count = system_info.dwNumberOfProcessors ? system_info.dwNumberOfProcessors : 1;

            FILETIME creation_time{};
            FILETIME exit_time{};
            FILETIME kernel_time{};
            FILETIME user_time{};
            FILETIME now_time{};
            GetSystemTimeAsFileTime(&now_time);

            if (GetProcessTimes(process_handle, &creation_time, &exit_time, &kernel_time, &user_time)) {
                process_start_filetime = filetime_to_ull(creation_time);
                last_process_time = filetime_to_ull(kernel_time) + filetime_to_ull(user_time);
            }
            else {
                process_start_filetime = filetime_to_ull(now_time);
                last_process_time = 0;
            }

            last_sample_time = std::chrono::steady_clock::now();
            initialized = true;
        }
    };

    watermark_metrics_t get_watermark_metrics() {
        static process_metrics_sampler_t sampler;
        sampler.initialize();

        FILETIME now_time{};
        GetSystemTimeAsFileTime(&now_time);
        const auto now_steady = std::chrono::steady_clock::now();
        const auto uptime = std::chrono::seconds((filetime_to_ull(now_time) - sampler.process_start_filetime) / 10000000ULL);

        FILETIME creation_time{};
        FILETIME exit_time{};
        FILETIME kernel_time{};
        FILETIME user_time{};

        if (now_steady - sampler.last_sample_time >= std::chrono::milliseconds(250) &&
            GetProcessTimes(sampler.process_handle, &creation_time, &exit_time, &kernel_time, &user_time)) {
            const ULONGLONG current_process_time = filetime_to_ull(kernel_time) + filetime_to_ull(user_time);
            const double elapsed_seconds = std::chrono::duration<double>(now_steady - sampler.last_sample_time).count();

            if (elapsed_seconds > 0.0 && current_process_time >= sampler.last_process_time) {
                const double process_delta_seconds = static_cast<double>(current_process_time - sampler.last_process_time) / 10000000.0;
                const double raw_cpu_percent = (process_delta_seconds / (elapsed_seconds * static_cast<double>(sampler.logical_processor_count))) * 100.0;
                sampler.target_cpu_percent = static_cast<float>(ImClamp(raw_cpu_percent, 0.0, 100.0));
            }

            sampler.last_process_time = current_process_time;
            sampler.last_sample_time = now_steady;
        }

        const float delta_time = ImMax(ImGui::GetIO().DeltaTime, 0.0001f);
        const float smoothing = ImClamp(1.0f - std::exp(-6.0f * delta_time), 0.0f, 1.0f);
        sampler.displayed_cpu_percent = ImLerp(sampler.displayed_cpu_percent, sampler.target_cpu_percent, smoothing);

        char cpu_buffer[32];
        std::snprintf(cpu_buffer, sizeof(cpu_buffer), "%.1f%% CPU", sampler.displayed_cpu_percent);

        return {
            format_runtime_duration(uptime),
            cpu_buffer
        };
    }

    ImVec2 clamp_overlay_position(const ImVec2& pos, const ImVec2& size, float margin) {
        const ImVec2 display_size = ImGui::GetIO().DisplaySize;
        const float max_x = ImMax(margin, display_size.x - size.x - margin);
        const float max_y = ImMax(margin, display_size.y - size.y - margin);

        return ImVec2{
            ImClamp(pos.x, margin, max_x),
            ImClamp(pos.y, margin, max_y)
        };
    }

    void begin_movable_overlay_window(
        const char* name,
        movable_overlay_state_t& state,
        const ImVec2& default_pos,
        const ImVec2& default_pivot,
        const ImVec2& padding,
        ImGuiWindowFlags flags,
        bool interactive
    ) {
        if (state.initialized) {
            SetNextWindowPos(state.pos, ImGuiCond_Always);
        }
        else {
            SetNextWindowPos(default_pos, ImGuiCond_Always, default_pivot);
        }

        if (!interactive) {
            flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
        }

        SetNextWindowBgAlpha(1.0f);
        PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
        PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
        PushStyleColor(ImGuiCol_Text, g_style->col(pcol_text).vec4());
        PushStyleColor(ImGuiCol_TextDisabled, g_style->col(pcol_text2).vec4());
        widgets->window.begin(name, { 0, 0 }, flags);
    }

    void draw_fixed_width_overlay_text(const char* text, const char* basis_text) {
        const ImVec2 pos = GetCursorScreenPos();
        const float reserved_width = CalcTextSize(basis_text).x;

        GetWindowDrawList()->AddText(GetFont(), GetFontSize(), pos, GetColorU32(ImGuiCol_Text), text);
        Dummy(ImVec2{ reserved_width, GetTextLineHeight() });
    }

    void end_movable_overlay_window(
        movable_overlay_state_t& state,
        const ImVec2& default_pos,
        const ImVec2& default_pivot,
        float margin,
        bool interactive
    ) {
        auto* window = GetCurrentWindow();
        if (interactive && GImGui->MovingWindow == window) {
            state.user_positioned = true;
        }

        ImVec2 target_pos = GetWindowPos();
        if (!state.user_positioned) {
            target_pos = ImVec2{
                default_pos.x - GetWindowSize().x * default_pivot.x,
                default_pos.y - GetWindowSize().y * default_pivot.y
            };
        }

        const ImVec2 clamped_pos = clamp_overlay_position(target_pos, GetWindowSize(), margin);
        state.pos = clamped_pos;
        state.initialized = true;

        if (clamped_pos.x != GetWindowPos().x || clamped_pos.y != GetWindowPos().y) {
            SetWindowPos(clamped_pos);
        }

        widgets->window.end();
        PopStyleColor(2);
        PopStyleVar(2);
    }

    bool is_placeholder_player_name(std::string_view name) {
        if (name.empty()) {
            return true;
        }

        std::string lower;
        lower.reserve(name.size());
        for (const unsigned char ch : name) {
            lower.push_back(static_cast<char>(std::tolower(ch)));
        }

        return lower == "npc" || lower == "unknown" || lower == "unknown player";
    }

    std::string get_player_display_name(const Core::SDK::Game::EntityStruct& entity) {

        if (!entity.NetworkInfo.UserName.empty()) {
            const std::string sanitized = Utils::ValidateTextForRendering(entity.NetworkInfo.UserName);
            if (!is_placeholder_player_name(sanitized)) {
                return sanitized;
            }
        }

        if (entity.Id > 0) {
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "Player #%d", entity.Id);
            return buffer;
        }

        return "Unknown Player";
    }

    std::string get_player_weapon_name(const Core::SDK::Game::EntityStruct& entity) {
        if (entity.WeaponName.empty()) {
            return "Unknown";
        }

        const std::string sanitized = Utils::ValidateTextForRendering(entity.WeaponName);
        return sanitized.empty() ? "Unknown" : sanitized;
    }

    std::string format_player_distance(const float distance) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.0fm", ImMax(distance, 0.0f));
        return buffer;
    }

    std::string make_safe_ui_label(std::string_view value, const char* fallback = "Unknown") {
        std::string result;
        result.reserve(ImMin(static_cast<int>(value.size()), 64));

        for (const unsigned char ch : value) {
            if (ch >= 32 && ch <= 126) {
                result.push_back(static_cast<char>(ch));
            }
            else if (std::isspace(ch)) {
                result.push_back(' ');
            }
            else {
                result.push_back('?');
            }

            if (result.size() >= 64) {
                break;
            }
        }

        const auto first_non_space = result.find_first_not_of(' ');
        if (first_non_space == std::string::npos) {
            return fallback;
        }

        const auto last_non_space = result.find_last_not_of(' ');
        result = result.substr(first_non_space, last_non_space - first_non_space + 1);
        return result.empty() ? fallback : result;
    }

    struct account_sidebar_state_t {
        std::mutex mutex;
        std::atomic<bool> fetch_in_flight{ false };
        std::atomic<ULONGLONG> refresh_block_until{ 0 };
        auth::AccountProfile profile{};
        bool has_profile = false;
        std::string avatar_loaded_url;
        std::string avatar_attempted_url;
    };

    account_sidebar_state_t account_sidebar_state;
    bool wizard_visuals_enabled = false;

    struct wizard_names_state_t {
        std::atomic<bool> fetch_in_flight{ false };
        std::atomic<ULONGLONG> refresh_block_until{ 0 };
    };

    wizard_names_state_t wizard_names_state;

    std::string trim_copy(std::string value) {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return "";
        }

        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    std::string format_account_until_text(const auth::AccountProfile& profile) {
        const std::string raw = trim_copy(!profile.expires_at.empty() ? profile.expires_at : profile.expiredate);
        if (raw.empty() || raw == "0000-00-00 00:00:00") {
            return "slotted";
        }

        if (raw.size() >= 10 && raw[4] == '-' && raw[7] == '-') {
            try {
                if (std::stoi(raw.substr(0, 4)) >= 2099) {
                    return "slotted";
                }
            }
            catch (...) {
            }

            return std::string("until ") + raw.substr(8, 2) + "." + raw.substr(5, 2) + "." + raw.substr(0, 4);
        }

        return std::string("until ") + raw;
    }

    void request_account_sidebar_refresh(bool force = false) {
        const ULONGLONG now = GetTickCount64();
        if (!force && account_sidebar_state.refresh_block_until.load(std::memory_order_acquire) > now) {
            return;
        }

        if (account_sidebar_state.fetch_in_flight.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        account_sidebar_state.refresh_block_until.store(now + (force ? 3000 : 15000), std::memory_order_release);

        std::thread([] {
            auth::AccountProfile fetched_profile{};
            std::string raw;
            const bool ok = auth::LoadCurrentAccountProfile(fetched_profile, &raw);
            if (ok && !fetched_profile.discord_id.empty()) {
                std::string discord_raw;
                auth::PopulateDiscordProfile(fetched_profile, &discord_raw);
            }

            {
                std::lock_guard<std::mutex> lock(account_sidebar_state.mutex);
                if (ok) {
                    account_sidebar_state.profile = std::move(fetched_profile);
                    account_sidebar_state.has_profile = true;
                    const std::string avatar_url = trim_copy(account_sidebar_state.profile.discord_avatar_url);
                    if (avatar_url != account_sidebar_state.avatar_loaded_url) {
                        account_sidebar_state.avatar_attempted_url.clear();
                    }
                }
            }

            account_sidebar_state.fetch_in_flight.store(false, std::memory_order_release);
        }).detach();
    }

    bool current_account_has_staff_tools() {
        std::lock_guard<std::mutex> lock(account_sidebar_state.mutex);
        return account_sidebar_state.has_profile && (account_sidebar_state.profile.admin || account_sidebar_state.profile.staff);
    }

    void request_wizard_names_refresh() {
        const ULONGLONG now = GetTickCount64();
        if (wizard_names_state.refresh_block_until.load(std::memory_order_acquire) > now) {
            return;
        }

        if (wizard_names_state.fetch_in_flight.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        wizard_names_state.refresh_block_until.store(now + 30000, std::memory_order_release);

        std::thread([] {
            std::vector<std::string> names;
            std::string raw;
            const bool ok = auth::LoadWizardIngameNames(names, &raw);

            if (ok) {
                Core::Features::WizardVisuals::SetNames(std::move(names));
            }

            wizard_names_state.fetch_in_flight.store(false, std::memory_order_release);
        }).detach();
    }

    void pump_account_sidebar_avatar(c_slate* instance, ID3D11Device* device) {
        std::string avatar_url;
        bool has_profile = false;
        bool had_loaded_avatar = false;
        {
            std::lock_guard<std::mutex> lock(account_sidebar_state.mutex);
            has_profile = account_sidebar_state.has_profile;
            avatar_url = trim_copy(account_sidebar_state.profile.discord_avatar_url);
            had_loaded_avatar = !account_sidebar_state.avatar_loaded_url.empty();

            if (!has_profile) {
                if (had_loaded_avatar) {
                    account_sidebar_state.avatar_loaded_url.clear();
                    account_sidebar_state.avatar_attempted_url.clear();
                }
                else {
                    return;
                }
            }

            if (avatar_url.empty()) {
                if (had_loaded_avatar) {
                    account_sidebar_state.avatar_loaded_url.clear();
                    account_sidebar_state.avatar_attempted_url.clear();
                }
                else {
                    return;
                }
            }

            if (avatar_url.empty() ||
                account_sidebar_state.avatar_loaded_url == avatar_url ||
                account_sidebar_state.avatar_attempted_url == avatar_url) {
                if (!avatar_url.empty()) {
                    return;
                }
            }

            account_sidebar_state.avatar_attempted_url = avatar_url;
        }

        if (avatar_url.empty()) {
            instance->avatar.reset();
            return;
        }

        instance->avatar.reset();
        try {
            instance->avatar.load(device, avatar_url);
            std::lock_guard<std::mutex> lock(account_sidebar_state.mutex);
            account_sidebar_state.avatar_loaded_url = avatar_url;
        }
        catch (...) {
        }
    }

    void draw_account_sidebar_card(c_slate* instance) {
        auth::AccountProfile profile{};
        bool has_profile = false;
        std::string loaded_avatar_url;
        {
            std::lock_guard<std::mutex> lock(account_sidebar_state.mutex);
            profile = account_sidebar_state.profile;
            has_profile = account_sidebar_state.has_profile;
            loaded_avatar_url = account_sidebar_state.avatar_loaded_url;
        }

        const std::string preferred_name = !profile.discord_display_name.empty() ? profile.discord_display_name : profile.user_name;
        const std::string validated_name = has_profile ? Utils::ValidateTextForRendering(preferred_name) : std::string{};
        const std::string account_name = make_safe_ui_label(validated_name.empty() ? "@bloodluvrs" : validated_name, "@bloodluvrs");
        const std::string until_text = format_account_until_text(profile);

        const ImVec2 card_pos = GetWindowPos() + ImVec2{ 14 ds, GetWindowHeight() - 58 ds };
        const ImVec2 card_size = { GetWindowWidth() - 28 ds, 44 ds };
        const ImVec2 avatar_min = card_pos + ImVec2{ 10 ds, 10 ds };
        const ImVec2 avatar_max = avatar_min + ImVec2{ 24 ds, 24 ds };
        const ImVec2 icon_min = avatar_min + ImVec2{ 3 ds, 3 ds };
        const ImVec2 icon_max = avatar_max - ImVec2{ 3 ds, 3 ds };
        const float text_x = avatar_max.x + 10 ds;
        const float text_width = card_pos.x + card_size.x - text_x - 10 ds;

        auto* draw = GetWindowDrawList();
        draw->AddRectFilled(card_pos, card_pos + card_size, g_style->col(pcol_bg3), 10 ds);
        draw->AddRect(card_pos, card_pos + card_size, g_style->col(pcol_border), 10 ds);

        if (!loaded_avatar_url.empty()) {
            draw->AddImageRounded((ImTextureID)instance->avatar, avatar_min, avatar_max, { 0, 0 }, { 1, 1 }, IM_COL32_WHITE, 12 ds);
        }
        else {
            draw->AddCircleFilled((avatar_min + avatar_max) * 0.5f, 12 ds, g_style->col(pcol_text));
            draw_seasonal_logo(draw, icon_min, icon_max, g_style->col(pcol_bg2));
        }

        ImFont* name_font = fonts[font].get(14);
        ImFont* meta_font = fonts[font].get(12);
        if (!name_font) {
            name_font = GetFont();
        }
        if (!meta_font) {
            meta_font = GetFont();
        }

        const std::string clipped_name = account_name.substr(0, 48);
        draw->AddText(name_font, 14 ds, { text_x, card_pos.y + 7 ds }, g_style->col(pcol_text), clipped_name.c_str(), nullptr, text_width);
        draw->AddText(meta_font, 12 ds, { text_x, card_pos.y + 23 ds }, g_style->col(pcol_text2), until_text.c_str(), nullptr, text_width);
    }

    bool contains_case_insensitive(std::string_view haystack, std::string_view needle) {
        if (needle.empty()) {
            return true;
        }

        const auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
            [](const char lhs, const char rhs) {
                return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
            });

        return it != haystack.end();
    }

    void draw_player_empty_state(const char* icon, const char* title, const char* subtitle) {
        const ImVec2 area = GetContentRegionAvail();
        const ImVec2 start = GetCursorScreenPos();
        const ImVec2 icon_size = CalcTextSize(icon);
        const ImVec2 title_size = CalcTextSize(title);
        const ImVec2 subtitle_size = CalcTextSize(subtitle);
        const float total_height = icon_size.y + title_size.y + subtitle_size.y + 18 ds;
        const float top = start.y + ImMax(0.0f, (area.y - total_height) * 0.5f);

        SetCursorScreenPos({ start.x, top });
        PushFont(fonts[icons].get(18));
        SetCursorPosX(GetCursorPosX() + ImMax(0.0f, (area.x - icon_size.x) * 0.5f));
        TextDisabled(icon);
        PopFont();

        SetCursorPosX(GetCursorPosX() + ImMax(0.0f, (area.x - title_size.x) * 0.5f));
        Text("%s", title);

        SetCursorPosX(GetCursorPosX() + ImMax(0.0f, (area.x - subtitle_size.x) * 0.5f));
        TextDisabled("%s", subtitle);
    }

    bool draw_player_directory_row(const Core::SDK::Game::EntityStruct& entity, bool selected) {
        const std::string anim_id = "player_row_" + std::to_string(reinterpret_cast<uintptr_t>(entity.Ped));
        struct row_anim_t {
            float hover = 0.0f;
            float active = 0.0f;
        };
        auto& anim_state = anim_obj(anim_id.c_str(), 0, row_anim_t{});

        const ImVec2 size{ GetContentRegionAvail().x, 54 ds };
        const ImRect bb{ GetCursorScreenPos(), GetCursorScreenPos() + size };
        InvisibleButton(anim_id.c_str(), size);

        const bool hovered = IsItemHovered();
        const bool pressed = IsItemClicked(ImGuiMouseButton_Left);
        anim_state.hover = anim(anim_state.hover, 0.0f, 1.0f, hovered);
        anim_state.active = anim(anim_state.active, 0.0f, 1.0f, selected);

        const float emphasis = ImClamp(anim_state.hover * 0.45f + anim_state.active * 0.85f, 0.0f, 1.0f);
        const ImVec4 fill = ImLerp(g_style->col(pcol_bg3, 0.94f).vec4(), g_style->col(pcol_bg4, 1.0f).vec4(), emphasis);
        const ImVec4 border = ImLerp(g_style->col(pcol_border, 0.85f).vec4(), g_style->col(pcol_scheme, 0.90f).vec4(), anim_state.active);
        const ImVec4 accent = ImLerp(g_style->col(pcol_text3).vec4(), g_style->col(pcol_scheme).vec4(), ImClamp(anim_state.hover * 0.35f + anim_state.active, 0.0f, 1.0f));

        auto* draw = GetWindowDrawList();
        draw->AddRectFilled(bb.Min, bb.Max, GetColorU32(fill), 8 ds);
        draw->AddRect(bb.Min, bb.Max, GetColorU32(border), 8 ds);
        draw->AddRectFilled(bb.Min, { bb.Min.x + 3 ds, bb.Max.y }, GetColorU32(accent), 8 ds, ImDrawFlags_RoundCornersLeft);
        draw->AddCircleFilled({ bb.Min.x + 16 ds, bb.GetCenter().y }, 4 ds, GetColorU32(entity.IsFriend ? g_style->col(pcol_scheme).vec4() : g_style->col(pcol_text2).vec4()));

        const std::string display_name = get_player_display_name(entity);
        const std::string meta = "#" + std::to_string(entity.Id) + "  |  " + format_player_distance(entity.Distance);
        const std::string badge = entity.IsFriend ? "FRIEND" : "PLAYER";
        const float badge_width = CalcTextSize(badge.c_str()).x + 18 ds;
        const ImRect badge_bb{
            { bb.Max.x - badge_width - 14 ds, bb.Min.y + 10 ds },
            { bb.Max.x - 14 ds, bb.Min.y + 28 ds }
        };

        draw->AddRectFilled(
            badge_bb.Min,
            badge_bb.Max,
            GetColorU32(entity.IsFriend ? g_style->col(pcol_scheme, 0.14f).vec4() : g_style->col(pcol_bg2).vec4()),
            badge_bb.GetHeight() * 0.5f);

        g_draw->text(
            cascadia,
            11,
            { badge_bb.Min.x + 9 ds, badge_bb.GetCenter().y - g_draw->textsize(cascadia, 11, badge.c_str()).y * 0.5f },
            entity.IsFriend ? g_style->col(pcol_scheme) : g_style->col(pcol_text2),
            badge.c_str());

        const float text_width = ImMax(64.0f, badge_bb.Min.x - bb.Min.x - 42 ds);
        const std::string trimmed_name = ptext{ display_name }.trim(text_width);
        const std::string trimmed_meta = ptext{ meta }.trim(text_width);

        g_draw->text(fontb, 16, { bb.Min.x + 28 ds, bb.Min.y + 10 ds }, g_style->col(pcol_text), trimmed_name.c_str());
        g_draw->text(cascadia, 12, { bb.Min.x + 28 ds, bb.Min.y + 30 ds }, g_style->col(pcol_text2), trimmed_meta.c_str());

        return pressed;
    }

    void draw_player_stat_card(const char* str_id, const char* icon, const char* title, const std::string& value, ImVec2 size, bool accent_value = false) {
        const ImRect bb{ GetCursorScreenPos(), GetCursorScreenPos() + size };
        InvisibleButton(str_id, size);

        auto* draw = GetWindowDrawList();
        draw->AddRectFilled(bb.Min, bb.Max, GetColorU32(g_style->col(pcol_bg3).vec4()), 10 ds);
        draw->AddRect(bb.Min, bb.Max, GetColorU32(g_style->col(pcol_border).vec4()), 10 ds);
        draw->AddRectFilled(bb.Min, { bb.Max.x, bb.Min.y + 2 ds }, GetColorU32(g_style->col(pcol_scheme, 0.14f).vec4()), 10 ds, ImDrawFlags_RoundCornersTop);

        PushFont(fonts[icons].get(14));
        g_draw->text(icons, 14, { bb.Min.x + 12 ds, bb.Min.y + 11 ds }, g_style->col(pcol_scheme), icon);
        PopFont();

        g_draw->text(cascadia, 11, { bb.Min.x + 34 ds, bb.Min.y + 12 ds }, g_style->col(pcol_text2), title);
        const std::string trimmed_value = ptext{ value }.trim(bb.GetWidth() - 24 ds);
        g_draw->text(font, 16, { bb.Min.x + 12 ds, bb.Min.y + 32 ds }, accent_value ? g_style->col(pcol_scheme) : g_style->col(pcol_text), trimmed_value.c_str());
    }

    void draw_player_overview_header(const Core::SDK::Game::EntityStruct& entity) {
        const ImVec2 size{ GetContentRegionAvail().x, 82 ds };
        const ImRect bb{ GetCursorScreenPos(), GetCursorScreenPos() + size };
        InvisibleButton("##player_focus_header", size);

        auto* draw = GetWindowDrawList();
        draw->AddRectFilled(bb.Min, bb.Max, GetColorU32(g_style->col(pcol_bg3).vec4()), 12 ds);
        draw->AddRect(bb.Min, bb.Max, GetColorU32(g_style->col(pcol_border).vec4()), 12 ds);
        draw->AddRectFilled(bb.Min, { bb.Min.x + 4 ds, bb.Max.y }, GetColorU32(g_style->col(pcol_scheme).vec4()), 12 ds, ImDrawFlags_RoundCornersLeft);
        draw->AddCircleFilled({ bb.Min.x + 28 ds, bb.GetCenter().y }, 13 ds, GetColorU32(g_style->col(pcol_bg2).vec4()));
        draw->AddCircleFilled({ bb.Min.x + 28 ds, bb.GetCenter().y }, 5 ds, GetColorU32(entity.IsFriend ? g_style->col(pcol_scheme).vec4() : g_style->col(pcol_text2).vec4()));

        const std::string display_name = ptext{ get_player_display_name(entity) }.trim(bb.GetWidth() - 150 ds);
        const std::string subtitle = "Tracked target  |  " + format_player_distance(entity.Distance);
        const char* badge = entity.IsFriend ? "FRIEND" : "LIVE";
        const ImVec2 badge_size{ CalcTextSize(badge).x + 18 ds, 20 ds };
        const ImRect badge_bb{
            { bb.Max.x - badge_size.x - 16 ds, bb.Min.y + 14 ds },
            { bb.Max.x - 16 ds, bb.Min.y + 14 ds + badge_size.y }
        };

        draw->AddRectFilled(
            badge_bb.Min,
            badge_bb.Max,
            GetColorU32(entity.IsFriend ? g_style->col(pcol_scheme, 0.14f).vec4() : g_style->col(pcol_bg2).vec4()),
            badge_bb.GetHeight() * 0.5f);

        g_draw->text(cascadia, 11, { badge_bb.Min.x + 9 ds, badge_bb.GetCenter().y - g_draw->textsize(cascadia, 11, badge).y * 0.5f }, entity.IsFriend ? g_style->col(pcol_scheme) : g_style->col(pcol_text2), badge);
        g_draw->text(fontb, 18, { bb.Min.x + 50 ds, bb.Min.y + 16 ds }, g_style->col(pcol_text), display_name.c_str());
        g_draw->text(cascadia, 12, { bb.Min.x + 50 ds, bb.Min.y + 44 ds }, g_style->col(pcol_text2), subtitle.c_str());
    }

    void copy_player_value_to_clipboard(const std::string& value, const char* success_message, const char* empty_message) {
        if (value.empty()) {
            NotifyManager::Send(empty_message, 220.0f);
            return;
        }

        Utils::PasteClipboard(value.c_str());
        NotifyManager::Send(success_message, 220.0f);
    }

    std::vector<Core::SDK::Game::EntityStruct> collect_player_directory_entries() {
        auto players = Core::SDK::Game::GetEntityListSnapshot();

        players.erase(std::remove_if(players.begin(), players.end(), [](const Core::SDK::Game::EntityStruct& entity) {
            const std::string sanitized_name = Utils::ValidateTextForRendering(entity.NetworkInfo.UserName);
            const bool has_resolved_name = !is_placeholder_player_name(sanitized_name);

            return !entity.Ped ||
                entity.Ped == Core::SDK::Pointers::pLocalPlayer ||
                (entity.Id <= 0 && !has_resolved_name);
            }), players.end());

        std::stable_sort(players.begin(), players.end(), [](const Core::SDK::Game::EntityStruct& lhs, const Core::SDK::Game::EntityStruct& rhs) {
            const std::string lhs_name = get_player_display_name(lhs);
            const std::string rhs_name = get_player_display_name(rhs);
            const bool lhs_has_real_name = lhs_name.rfind("Player #", 0) != 0;
            const bool rhs_has_real_name = rhs_name.rfind("Player #", 0) != 0;

            if (lhs.IsFriend != rhs.IsFriend) {
                return lhs.IsFriend > rhs.IsFriend;
            }

            if (lhs_has_real_name != rhs_has_real_name) {
                return lhs_has_real_name > rhs_has_real_name;
            }

            if (lhs.Distance != rhs.Distance) {
                return lhs.Distance < rhs.Distance;
            }

            return lhs_name < rhs_name;
            });

        return players;
    }

    void filter_player_directory_entries(std::vector<Core::SDK::Game::EntityStruct>& players, std::string_view search_query) {
        if (search_query.empty()) {
            return;
        }

        players.erase(std::remove_if(players.begin(), players.end(), [&](const Core::SDK::Game::EntityStruct& entity) {
            const std::string display_name = get_player_display_name(entity);
            const std::string id_text = entity.Id > 0 ? std::to_string(entity.Id) : std::string{};
            return !contains_case_insensitive(display_name, search_query) && !contains_case_insensitive(id_text, search_query);
            }), players.end());
    }

    void filter_player_directory_entries_by_friend_state(std::vector<Core::SDK::Game::EntityStruct>& players, const bool friends_only) {
        players.erase(std::remove_if(players.begin(), players.end(), [&](const Core::SDK::Game::EntityStruct& entity) {
            return entity.IsFriend != friends_only;
            }), players.end());
    }

    int find_player_selection_index(const std::vector<Core::SDK::Game::EntityStruct>& players, uintptr_t selected_ped_ptr, int selected_player_id) {
        if (selected_ped_ptr != 0) {
            for (int i = 0; i < static_cast<int>(players.size()); ++i) {
                if (reinterpret_cast<uintptr_t>(players[i].Ped) == selected_ped_ptr) {
                    return i;
                }
            }
        }

        if (selected_player_id > 0) {
            for (int i = 0; i < static_cast<int>(players.size()); ++i) {
                if (players[i].Id == selected_player_id) {
                    return i;
                }
            }
        }

        return -1;
    }

    void set_player_friend_state(const Core::SDK::Game::EntityStruct& entity, bool should_friend) {
        if (!entity.Ped) {
            NotifyManager::Send("Player unavailable", 220.0f);
            return;
        }

        if (should_friend) {
            Core::Features::g_Friends.AddFriend(entity.Ped);
            NotifyManager::Send("Player added to friends", 220.0f);
        }
        else {
            Core::Features::g_Friends.RemoveFriend(entity.Ped);
            NotifyManager::Send("Player removed from friends", 220.0f);
        }
    }

    void teleport_to_player(const Core::SDK::Game::EntityStruct& entity) {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        if (!local_player || !entity.Ped) {
            NotifyManager::Send("Player unavailable", 220.0f);
            return;
        }

        D3DXVECTOR3 target_position = entity.Ped->GetPos();
        target_position.z += 0.15f;
        local_player->SetPos(target_position);
        NotifyManager::Send("Teleported to player", 220.0f);
    }

    struct player_directory_ui_state_t {
        char search[64]{};
        uintptr_t selected_player_ptr = 0;
        int selected_player_id = -1;
    };

    void draw_player_directory_page(const bool friends_only, player_directory_ui_state_t& ui_state) {
        const bool wizard_highlight_active = !friends_only && Core::Features::WizardVisuals::IsEnabled() && current_account_has_staff_tools();
        if (wizard_highlight_active) {
            request_wizard_names_refresh();
        }

        auto scoped_players = collect_player_directory_entries();
        filter_player_directory_entries_by_friend_state(scoped_players, friends_only);

        auto filtered_players = scoped_players;
        filter_player_directory_entries(filtered_players, ui_state.search);

        const int selected_index = find_player_selection_index(filtered_players, ui_state.selected_player_ptr, ui_state.selected_player_id);
        Core::SDK::Game::EntityStruct selected_player{};
        const bool has_selected_player = selected_index >= 0;

        if (has_selected_player) {
            selected_player = filtered_players[selected_index];
        }
        else {
            ui_state.selected_player_ptr = 0;
            ui_state.selected_player_id = -1;
        }

        const char* list_title = friends_only ? "Friends" : "Players";
        const char* search_hint = friends_only ? "Search friends..." : "Search players...";
        const char* empty_title = friends_only ? "No friends are currently tracked." : "No remote players are currently tracked.";
        const char* no_match_title = friends_only ? "No friends match your search." : "No players match your search.";

        BeginGroup();
        {
            widgets->child.begin(I_USER__COG, list_title, 1, { 318 ds, 414 ds });
            {
                widgets->textinput("##player_directory_search", ui_state.search, sizeof(ui_state.search), { .icon = I_SEARCH, .hint = search_hint });
                widgets->spacing(6 ds);

                if (filtered_players.empty()) {
                    TextDisabled(scoped_players.empty() ? empty_title : no_match_title);
                }
                else {
                    PushStyleColor(ImGuiCol_Header, ImVec4{ 0.f, 0.f, 0.f, 0.f });
                    PushStyleColor(ImGuiCol_HeaderHovered, ImVec4{ 0.f, 0.f, 0.f, 0.f });
                    PushStyleColor(ImGuiCol_HeaderActive, ImVec4{ 0.f, 0.f, 0.f, 0.f });
                    BeginChild("##player_directory_entries", { 0, 346 ds }, true, ImGuiWindowFlags_NoBackground);
                    for (const auto& entity : filtered_players) {
                        const bool is_selected = has_selected_player &&
                            (selected_player.Id == entity.Id || selected_player.Ped == entity.Ped);
                        const std::string safe_name = make_safe_ui_label(get_player_display_name(entity));
                        const std::string id_suffix = entity.Id > 0 ? " #" + std::to_string(entity.Id) : std::string{};
                        const bool is_friend = entity.IsFriend;
                        const bool is_wizard = wizard_highlight_active &&
                            (Core::Features::WizardVisuals::MatchesName(safe_name) || Core::Features::WizardVisuals::MatchesName(entity.NetworkInfo.UserName));

                        const std::string row_title = ptext{ safe_name + id_suffix }.trim(GetContentRegionAvail().x - 18 ds);
                        const std::string row_label =
                            row_title +
                            "##player_row_" + std::to_string(reinterpret_cast<uintptr_t>(entity.Ped));
                        const ImVec2 row_size{
                            ImMin(GetContentRegionAvail().x, CalcTextSize(row_title.c_str()).x + 8 ds),
                            18 ds
                        };
                        const ImVec4 friend_color = is_selected
                            ? ImVec4{ 0.58f, 0.92f, 0.62f, 1.0f }
                            : ImVec4{ 0.42f, 0.86f, 0.48f, 1.0f };
                        const ImVec4 wizard_color = is_selected
                            ? ImVec4{ 0.42f, 0.76f, 1.0f, 1.0f }
                            : ImVec4{ 0.24f, 0.62f, 1.0f, 1.0f };
                        const ImVec4 row_text_color = is_wizard
                            ? wizard_color
                            : is_friend
                            ? friend_color
                            : (is_selected ? g_style->col(pcol_scheme).vec4() : g_style->col(pcol_text2).vec4());

                        PushStyleColor(ImGuiCol_Text, row_text_color);
                        if (Selectable(row_label.c_str(), is_selected, 0, row_size)) {
                            ui_state.selected_player_ptr = reinterpret_cast<uintptr_t>(entity.Ped);
                            ui_state.selected_player_id = entity.Id;
                        }
                        PopStyleColor();

                        widgets->spacing(2 ds);
                    }
                    EndChild();
                    PopStyleColor(3);
                }
            }
            widgets->child.end();
        }
        EndGroup();

        SameLine(0, 8.f ds);

        BeginGroup();
        {
            widgets->child.begin("", friends_only ? "Selected Friend" : "Selected Player", 2, { 186 ds, 414 ds });
            {
                if (!has_selected_player) {
                    PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 10 ds, 10 ds });
                    BeginChild("##selected_player_preview", { 0, 144 ds }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    {
                        Text("Preview");

                        widgets->spacing(3 ds);
                        widgets->separator();
                        widgets->spacing(6 ds);
                        TextDisabled(friends_only ? "Select a friend from the list." : "Select a player from the list.");
                    }
                    EndChild();
                    PopStyleVar();

                    widgets->spacing(6 ds);

                    PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 10 ds, 10 ds });
                    BeginChild("##selected_player_actions", { 0, 128 ds }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    {
                        Text("Actions");

                        widgets->spacing(3 ds);
                        widgets->separator();
                        widgets->spacing(6 ds);
                        TextDisabled("Choose a player to unlock actions.");
                    }
                    EndChild();
                    PopStyleVar();
                }
                else {
                    const std::string safe_name = make_safe_ui_label(get_player_display_name(selected_player));
                    const std::string preview_name = selected_player.Id > 0
                        ? safe_name + " #" + std::to_string(selected_player.Id)
                        : safe_name;

                    PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 10 ds, 10 ds });
                    BeginChild("##selected_player_preview", { 0, 144 ds }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    {
                        Text("Preview");

                        widgets->spacing(3 ds);
                        widgets->separator();
                        widgets->spacing(6 ds);

                        const std::string title = ptext{ preview_name }.trim(GetContentRegionAvail().x - 4 ds);
                        PushFont(fonts[font].get(16));
                        TextUnformatted(title.c_str());
                        PopFont();
                        PushStyleColor(ImGuiCol_Text, selected_player.IsFriend ? g_style->col(pcol_scheme).vec4() : g_style->col(pcol_text2).vec4());
                        TextUnformatted(selected_player.IsFriend ? "Friend" : "Tracked");
                        PopStyleColor();

                        widgets->spacing(8 ds);
                        TextDisabled("Distance");
                        SameLine(74 ds);
                        TextUnformatted(format_player_distance(selected_player.Distance).c_str());
                        TextDisabled("Weapon");
                        SameLine(74 ds);
                        const std::string trimmed_weapon = ptext{ get_player_weapon_name(selected_player) }.trim(GetContentRegionAvail().x - 78 ds);
                        TextUnformatted(trimmed_weapon.c_str());
                        TextDisabled("Visible");
                        SameLine(74 ds);
                        TextUnformatted(selected_player.IsVisible ? "Yes" : "No");
                        TextDisabled("Friend");
                        SameLine(74 ds);
                        TextUnformatted(selected_player.IsFriend ? "Yes" : "No");
                    }
                    EndChild();
                    PopStyleVar();

                    widgets->spacing(6 ds);

                    PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 10 ds, 10 ds });
                    BeginChild("##selected_player_actions", { 0, 128 ds }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    {
                        Text("Actions");

                        widgets->spacing(3 ds);
                        widgets->separator();
                        widgets->spacing(6 ds);

                        bool friend_state = selected_player.IsFriend;
                        if (widgets->checkbox("", "Friend", &friend_state)) {
                            set_player_friend_state(selected_player, friend_state);
                        }

                        widgets->spacing(6 ds);

                        const float button_width = GetContentRegionAvail().x;
                        PushStyleVar(ImGuiStyleVar_FrameRounding, 7 ds);
                        PushStyleColor(ImGuiCol_Button, g_style->col(pcol_bg3).vec4());
                        PushStyleColor(ImGuiCol_ButtonHovered, g_style->col(pcol_scheme, 0.18f).vec4());
                        PushStyleColor(ImGuiCol_ButtonActive, g_style->col(pcol_scheme, 0.28f).vec4());
                        if (widgets->button("Teleport To Player", { button_width, 32 ds })) {
                            teleport_to_player(selected_player);
                        }

                        widgets->spacing(4 ds);
                        const std::string player_name = selected_player.NetworkInfo.UserName.empty()
                            ? make_safe_ui_label(get_player_display_name(selected_player))
                            : make_safe_ui_label(selected_player.NetworkInfo.UserName);
                        if (widgets->button("Copy Name", { button_width, 32 ds })) {
                            copy_player_value_to_clipboard(player_name, "Player name copied", "Player name unavailable");
                        }
                        PopStyleColor(3);
                        PopStyleVar();
                    }
                    EndChild();
                    PopStyleVar();
                }
            }
            widgets->child.end();
        }
        EndGroup();
    }

    std::string get_vehicle_display_name(const Core::SDK::Game::VehicleStructure& vehicle) {
        if (vehicle.Name.empty()) {
            return "Unknown Vehicle";
        }

        return make_safe_ui_label(vehicle.Name, "Unknown Vehicle");
    }

    std::string get_vehicle_driver_name(const Core::SDK::Game::VehicleStructure& vehicle) {
        if (!vehicle.Driver) {
            return "Unknown";
        }

        if (vehicle.Driver->GetPedType() != 2) {
            return "NPC";
        }

        const std::string raw_name = Utils::ValidateTextForRendering(Core::SDK::Game::GetPedName(vehicle.Driver));
        if (!is_placeholder_player_name(raw_name)) {
            return make_safe_ui_label(raw_name, "Player");
        }

        const int driver_id = vehicle.Driver->GetID();
        if (driver_id > 0) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "Player #%d", driver_id);
            return buffer;
        }

        return "Player";
    }

    float get_vehicle_speed(const Core::SDK::Game::VehicleStructure& vehicle) {
        if (!vehicle.Pointer) {
            return 0.0f;
        }

        const D3DXVECTOR3 velocity = vehicle.Pointer->GetVelocity();
        return std::sqrt(
            velocity.x * velocity.x +
            velocity.y * velocity.y +
            velocity.z * velocity.z);
    }

    std::string format_vehicle_speed(const Core::SDK::Game::VehicleStructure& vehicle) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.1f u/s", get_vehicle_speed(vehicle));
        return buffer;
    }

    bool is_vehicle_locked(const Core::SDK::Game::VehicleStructure& vehicle) {
        return vehicle.Pointer ? vehicle.Pointer->IsLocked() : vehicle.IsLocked;
    }

    std::vector<Core::SDK::Game::VehicleStructure> collect_vehicle_directory_entries() {
        std::lock_guard<std::mutex> lock(Core::Threads::g_VehicleList.vehicleListMutex);
        auto vehicles = Core::SDK::Game::VehicleList;

        vehicles.erase(std::remove_if(vehicles.begin(), vehicles.end(), [](const Core::SDK::Game::VehicleStructure& vehicle) {
            return !vehicle.Pointer;
            }), vehicles.end());

        std::stable_sort(vehicles.begin(), vehicles.end(), [](const Core::SDK::Game::VehicleStructure& lhs, const Core::SDK::Game::VehicleStructure& rhs) {
            if (lhs.Dist != rhs.Dist) {
                return lhs.Dist < rhs.Dist;
            }

            return get_vehicle_display_name(lhs) < get_vehicle_display_name(rhs);
            });

        return vehicles;
    }

    void filter_vehicle_directory_entries(std::vector<Core::SDK::Game::VehicleStructure>& vehicles, std::string_view search_query) {
        if (search_query.empty()) {
            return;
        }

        vehicles.erase(std::remove_if(vehicles.begin(), vehicles.end(), [&](const Core::SDK::Game::VehicleStructure& vehicle) {
            const std::string vehicle_name = get_vehicle_display_name(vehicle);
            const std::string driver_name = get_vehicle_driver_name(vehicle);
            const std::string distance_text = std::to_string(static_cast<int>(vehicle.Dist));
            return !contains_case_insensitive(vehicle_name, search_query) &&
                !contains_case_insensitive(driver_name, search_query) &&
                !contains_case_insensitive(distance_text, search_query);
            }), vehicles.end());
    }

    int find_vehicle_selection_index(const std::vector<Core::SDK::Game::VehicleStructure>& vehicles, uintptr_t selected_vehicle_ptr) {
        if (selected_vehicle_ptr == 0) {
            return -1;
        }

        for (int i = 0; i < static_cast<int>(vehicles.size()); ++i) {
            if (reinterpret_cast<uintptr_t>(vehicles[i].Pointer) == selected_vehicle_ptr) {
                return i;
            }
        }

        return -1;
    }

    bool is_tracked_vehicle(CVehicle* vehicle) {
        if (!vehicle) {
            return false;
        }

        std::lock_guard<std::mutex> lock(Core::Threads::g_VehicleList.vehicleListMutex);
        return std::any_of(Core::SDK::Game::VehicleList.begin(), Core::SDK::Game::VehicleList.end(),
            [vehicle](const Core::SDK::Game::VehicleStructure& entry) {
                return entry.Pointer == vehicle;
            });
    }

    CVehicle* get_active_local_vehicle(CPed* local_player) {
        if (!local_player || !local_player->InVehicle()) {
            return nullptr;
        }

        auto* vehicle = local_player->GetLastVehicle();
        return is_tracked_vehicle(vehicle) ? vehicle : nullptr;
    }

    bool request_vehicle_control(CVehicle* vehicle) {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        auto* local_player_info = local_player ? local_player->GetPlayerInfo() : nullptr;
        if (!vehicle || !local_player_info) {
            return false;
        }

        network_request_control_of_entity(reinterpret_cast<uintptr_t>(vehicle), reinterpret_cast<uintptr_t>(local_player_info));
        return true;
    }

    void set_vehicle_velocity(CVehicle* vehicle, const D3DXVECTOR3& velocity) {
        if (!vehicle) {
            return;
        }

        Mem.Write<D3DXVECTOR3>(reinterpret_cast<uintptr_t>(vehicle) + 0x320, velocity);
    }

    void teleport_vehicle_entity(CVehicle* vehicle, const D3DXVECTOR3& position, bool stop_vehicle = true) {
        if (!vehicle) {
            return;
        }

        const uintptr_t navigation = vehicle->GetNavigation();
        const uintptr_t model_info = static_cast<uintptr_t>(vehicle->GetModelInfo());

        if (navigation && model_info) {
            TeleportToObject(reinterpret_cast<uintptr_t>(vehicle), navigation, model_info, position, position, stop_vehicle);
        }
        else {
            vehicle->SetPos(position);
        }
    }

    void teleport_to_vehicle(const Core::SDK::Game::VehicleStructure& vehicle) {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        if (!local_player || !vehicle.Pointer) {
            NotifyManager::Send("Vehicle unavailable", 220.0f);
            return;
        }

        D3DXVECTOR3 target_position = vehicle.Pointer->GetPos();
        target_position.z += 1.0f;
        local_player->SetPos(target_position);
        NotifyManager::Send("Teleported to vehicle", 220.0f);
    }

    void pull_vehicle_to_local(const Core::SDK::Game::VehicleStructure& vehicle) {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        if (!local_player || !vehicle.Pointer || !request_vehicle_control(vehicle.Pointer)) {
            NotifyManager::Send("Vehicle unavailable", 220.0f);
            return;
        }

        D3DXVECTOR3 target_position = local_player->GetPos();
        target_position.y += 2.5f;
        target_position.z += 0.35f;
        teleport_vehicle_entity(vehicle.Pointer, target_position, true);
        set_vehicle_velocity(vehicle.Pointer, D3DXVECTOR3{ 0.0f, 0.0f, 0.0f });
        NotifyManager::Send("Vehicle pulled", 220.0f);
    }

    void void_vehicle(const Core::SDK::Game::VehicleStructure& vehicle) {
        if (!vehicle.Pointer || !request_vehicle_control(vehicle.Pointer)) {
            NotifyManager::Send("Vehicle unavailable", 220.0f);
            return;
        }

        D3DXVECTOR3 target_position = vehicle.Pointer->GetPos();
        target_position.z = -10.0f;
		vehicle.Pointer->Fix(0);
        teleport_vehicle_entity(vehicle.Pointer, target_position, true);
        set_vehicle_velocity(vehicle.Pointer, D3DXVECTOR3{ 0.0f, 0.0f, 0.0f });
        NotifyManager::Send("Vehicle voided", 220.0f);
    }

    void fling_vehicle(const Core::SDK::Game::VehicleStructure& vehicle) {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        if (!vehicle.Pointer || !request_vehicle_control(vehicle.Pointer)) {
            NotifyManager::Send("Vehicle unavailable", 220.0f);
            return;
        }

        static std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        D3DXVECTOR3 direction{
            dist(rng),
            dist(rng),
            0.0f
        };

        float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length > 0.001f) {
            direction.x /= length;
            direction.y /= length;
        }
        else {
            direction = D3DXVECTOR3{ 0.0f, 1.0f, 0.0f };
        }

        D3DXVECTOR3 position = vehicle.Pointer->GetPos();
        position.z += 0.35f;
        vehicle.Pointer->SetPos(position);

        set_vehicle_velocity(vehicle.Pointer, D3DXVECTOR3{
            direction.x * 55.0f,
            direction.y * 55.0f,
            85.0f
            });

        NotifyManager::Send("Vehicle flung (random direction)", 220.0f);
    }

    void repair_vehicle(const Core::SDK::Game::VehicleStructure& vehicle) {
        if (!vehicle.Pointer || !request_vehicle_control(vehicle.Pointer)) {
            NotifyManager::Send("Vehicle unavailable", 220.0f);
            return;
        }

        vehicle.Pointer->Fix(1000.0f);
        NotifyManager::Send("Vehicle repaired", 220.0f);
    }

    void toggle_vehicle_lock(const Core::SDK::Game::VehicleStructure& vehicle) {
        if (!vehicle.Pointer || !request_vehicle_control(vehicle.Pointer)) {
            NotifyManager::Send("Vehicle unavailable", 220.0f);
            return;
        }

        const bool locked = vehicle.Pointer->IsLocked();
        vehicle.Pointer->DoorState(locked);
        NotifyManager::Send(locked ? "Vehicle unlocked" : "Vehicle locked", 220.0f);
    }

    struct local_vehicle_tuning_state_t {
        uintptr_t vehicle_ptr = 0;
        uintptr_t handling_ptr = 0;
        float gravity = 0.0f;
        float acceleration = 0.0f;
        float drive_force = 0.0f;
        float grip = 0.0f;
        float handbrake_force = 0.0f;
        float m_brake_force = 0.0f;
        float suspension_force = 0.0f;
    };

    local_vehicle_tuning_state_t g_local_vehicle_tuning_state{};

    void reset_local_vehicle_tuning_state() {
        g_local_vehicle_tuning_state = {};
    }

    bool sync_local_vehicle_tuning_state(CVehicle* vehicle) {
        if (!vehicle) {
            reset_local_vehicle_tuning_state();
            return false;
        }

        const uintptr_t vehicle_ptr = reinterpret_cast<uintptr_t>(vehicle);
        const uintptr_t handling_ptr = vehicle->GetHandling();
        if (!handling_ptr) {
            g_local_vehicle_tuning_state.vehicle_ptr = vehicle_ptr;
            g_local_vehicle_tuning_state.handling_ptr = 0;
            return false;
        }

        if (g_local_vehicle_tuning_state.vehicle_ptr == vehicle_ptr &&
            g_local_vehicle_tuning_state.handling_ptr == handling_ptr) {
            return true;
        }

        g_local_vehicle_tuning_state.vehicle_ptr = vehicle_ptr;
        g_local_vehicle_tuning_state.handling_ptr = handling_ptr;
        g_local_vehicle_tuning_state.gravity = g_Offsets.m_VehicleGravity? vehicle->GetGravity() : Mem.Read<float>(handling_ptr + offsetof(CHandlingData, m_downforce_multiplier));
        g_local_vehicle_tuning_state.acceleration = Mem.Read<float>(handling_ptr + offsetof(CHandlingData, m_acceleration));
        g_local_vehicle_tuning_state.drive_force = Mem.Read<float>(handling_ptr + offsetof(CHandlingData, m_initial_drive_force));
        g_local_vehicle_tuning_state.grip = Mem.Read<float>(handling_ptr + offsetof(CHandlingData, m_traction_curve_min));
        g_local_vehicle_tuning_state.handbrake_force = Mem.Read<float>(handling_ptr + offsetof(CHandlingData, m_handbrake_force));
        g_local_vehicle_tuning_state.suspension_force = Mem.Read<float>(handling_ptr + offsetof(CHandlingData, m_suspension_force));
        return true;
    }

    bool write_local_vehicle_handling_value(CVehicle* vehicle, size_t offset, float value) {
        if (!vehicle) {
            return false;
        }

        const uintptr_t handling_ptr = vehicle->GetHandling();
        if (!handling_ptr) {
            return false;
        }

        Mem.Write<float>(handling_ptr + offset, value);
        g_local_vehicle_tuning_state.handling_ptr = handling_ptr;
        return true;
    }

    bool set_local_vehicle_gravity(CVehicle* vehicle, float value) {
        if (!vehicle) {
            return false;
        }

        if (g_Offsets.m_VehicleGravity) {
            vehicle->SetGravity(value);
            return true;
        }

        return write_local_vehicle_handling_value(vehicle, offsetof(CHandlingData, m_downforce_multiplier), value);
    }

    void set_local_godmode(bool enabled) {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        if (!local_player) {
            NotifyManager::Send("Local player unavailable", 220.0f);
            return;
        }

        g_Config.Player->EnableGodMode = enabled;
        local_player->SetGodMode(enabled, g_Offsets.CurrentBuild);
        NotifyManager::Send(enabled ? "Godmode enabled" : "Godmode disabled", 220.0f);
    }

    void set_local_immortality(bool enabled) {
        g_Config.Player->Immortality = enabled;
        if (!enabled) {
            g_Config.Player->Toggled = false;
        }

        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        if (!local_player) {
            NotifyManager::Send("Local player unavailable", 220.0f);
            return;
        }

        if (!enabled) {
            local_player->SetConfigFlag(ePedConfigFlag::DisablePlayerLockon, false);
            local_player->SetConfigFlag(ePedConfigFlag::AllowLockonToFriendlyPlayers, false);
            local_player->SetConfigFlag(ePedConfigFlag::AllowPlayerLockOnIfFriendly, false);
            local_player->SetConfigFlag(ePedConfigFlag::TreatAsFriendlyForTargetingAndDamage, false);
            local_player->SetConfigFlag(ePedConfigFlag::TreatAsFriendlyForTargetingAndDamageNonSynced, false);
            local_player->SetConfigFlag(ePedConfigFlag::DisableLockonToRandomPeds, false);
            local_player->SetConfigFlag(ePedConfigFlag::NeverEverTargetThisPed, false);
            NotifyManager::Send("Passive disabled", 220.0f);
            return;
        }

        NotifyManager::Send(
            g_Config.Player->ImmortalityKeyBind == 0
                ? "Passive enabled (always on)"
                : "Passive enabled",
            220.0f
        );
    }

    void set_local_fake_lag(bool enabled) {
        g_Config.Player->FakeLag = enabled;
        if (!enabled) {
            g_Config.Player->FakelagToggle = false;
            ::RestoreFakeLagPatch();
            NotifyManager::Send("FakeLag disabled", 220.0f);
            return;
        }

        NotifyManager::Send(
            g_Config.Player->FakeLagKey == 0
                ? "FakeLag enabled (always on)"
                : "FakeLag enabled",
            220.0f
        );
    }

    void set_local_noclip(bool enabled) {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        if (!local_player) {
            NotifyManager::Send("Local player unavailable", 220.0f);
            return;
        }

        g_Config.Player->NoClipEnabled = enabled;
        local_player->FreezePed(enabled);
        NotifyManager::Send(enabled ? "NoClip enabled" : "NoClip disabled", 220.0f);
    }

    void set_local_anti_headshot(bool enabled) {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        if (!local_player) {
            NotifyManager::Send("Local player unavailable", 220.0f);
            return;
        }

        g_Config.Player->AntiHSEnabled = enabled;
        local_player->SetConfigFlag(ePedConfigFlag::NoCriticalHits, enabled);
    }

    void set_freeze_ammo(bool enabled, bool suppress_errors = false) {
        if (enabled && !Core::SDK::Pointers::pLocalPlayer && !suppress_errors) {
            NotifyManager::Send("Local player unavailable", 220.0f);
            return;
        }

        if (enabled && (!Core::HasValidCoreOffsets() || !g_Offsets.m_InfiniteAmmo1) && !suppress_errors) {
            NotifyManager::Send("Offsets unavailable", 220.0f);
            return;
        }

        Core::SetFreezeAmmoEnabled(enabled, suppress_errors);
    }

    void set_local_no_ragdoll(bool enabled) {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        if (!local_player) {
            NotifyManager::Send("Local player unavailable", 220.0f);
            return;
        }

        g_Config.Player->NoRagDollEnabled = enabled;
        local_player->NoRagDoll(enabled);
    }

    void set_local_steal_car(bool enabled) {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        if (!local_player) {
            NotifyManager::Send("Local player unavailable", 220.0f);
            return;
        }

        g_Config.Player->StealCarEnabled = enabled;
        local_player->SetConfigFlag(ePedConfigFlag::NotAllowedToJackAnyPlayers, !enabled);
        local_player->SetConfigFlag(ePedConfigFlag::PlayerCanJackFriendlyPlayers, enabled);
        local_player->SetConfigFlag(ePedConfigFlag::WillJackAnyPlayer, enabled);
    }

    void set_local_infinite_combat_roll(bool enabled) {
        g_Config.Player->InfiniteCombatRoll = enabled;

        std::thread([enabled]() {
            if (auto* local_player = Core::SDK::Pointers::pLocalPlayer) {
                local_player->SetInfCombatRoll(enabled);
            }
            }).detach();
    }

    void set_local_force_weapon_wheel(bool enabled) {
        g_Config.Player->ForceWeaponWheel = enabled;

        std::thread([enabled]() {
            if (auto* local_player = Core::SDK::Pointers::pLocalPlayer) {
                local_player->ForceWeaponWheel(enabled);
            }
            }).detach();
    }

    void heal_local_player() {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        if (!local_player) {
            NotifyManager::Send("Local player unavailable", 220.0f);
            return;
        }

        local_player->SetHealth(200.0f);
        g_Config.Player->CurrentHealthValue = 200.0f;
        NotifyManager::Send("Healed", 220.0f);
    }

    void fill_local_armor() {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        if (!local_player) {
            NotifyManager::Send("Local player unavailable", 220.0f);
            return;
        }

        local_player->SetArmor(100.0f);
        g_Config.Player->CurrentArmorValue = 100.0f;
        NotifyManager::Send("Armor filled", 220.0f);
    }

    void teleport_to_waypoint() {
        TpToWaypoint();
        NotifyManager::Send("Teleported to waypoint", 220.0f);
    }

    void set_local_vehicle_seatbelt(bool enabled) {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        if (!local_player || !get_active_local_vehicle(local_player)) {
            g_Config.Player->SeatBelt = false;
            NotifyManager::Send("Enter a vehicle first", 220.0f);
            return;
        }

        g_Config.Player->SeatBelt = enabled;
        std::thread([enabled]() {
            if (auto* player = Core::SDK::Pointers::pLocalPlayer) {
                player->SeatBealt(enabled);
            }
            }).detach();
    }

    bool try_get_closest_troll_target(Core::SDK::Game::EntityStruct& entity) {
        CPed* closest_ped = Core::SDK::Game::GetClosestPed(300, true, true, false, true,false);
        if (!closest_ped) {
            return false;
        }

        const auto entities = Core::SDK::Game::GetEntityListSnapshot();
        for (const auto& candidate : entities) {
            if (candidate.Ped == closest_ped) {
                entity = candidate;
                return true;
            }
        }

        return false;
    }

    void pull_local_vehicle_to_target(const Core::SDK::Game::EntityStruct& entity) {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        auto* vehicle = get_active_local_vehicle(local_player);
        if (!entity.Ped || !vehicle) {
            NotifyManager::Send("Vehicle or target unavailable", 220.0f);
            return;
        }

        const bool previous_vehicle_rain = g_Config.Player->VehicleRain;
        g_Config.Player->VehicleRain = true;
        PullVehicle(entity.Ped);
        g_Config.Player->VehicleRain = previous_vehicle_rain;
        NotifyManager::Send("Vehicle pulled to target", 220.0f);
    }

    void lift_local_vehicle_on_target(const Core::SDK::Game::EntityStruct& entity) {
        auto* local_player = Core::SDK::Pointers::pLocalPlayer;
        auto* vehicle = get_active_local_vehicle(local_player);
        if (!entity.Ped || !vehicle) {
            NotifyManager::Send("Vehicle or target unavailable", 220.0f);
            return;
        }

        Exploit1(entity.Ped);
        NotifyManager::Send("Target lift started", 220.0f);
    }

    struct ped_hitbox_point_t {
        const char* label;
        int value_index;
        float x;
        float y;
    };

    constexpr ped_hitbox_point_t aimbot_hitbox_points[] = {
        { "Head", 0, 0.535f, 0.095f },
        { "Neck", 1, 0.535f, 0.205f },
        { "Chest", 2, 0.535f, 0.355f },
        { "Arms", 3, 0.105f, 0.47f },
        { "Arms", 3, 0.885f, 0.49f },
        { "Legs", 4, 0.365f, 0.645f },
        { "Legs", 4, 0.685f, 0.655f },
        { "Feet", 5, 0.355f, 0.955f },
        { "Feet", 5, 0.645f, 0.965f },
    };

    constexpr ped_hitbox_point_t silentaim_hitbox_points[] = {
        { "Head", 0, 0.535f, 0.095f },
        { "Neck", 1, 0.535f, 0.205f },
        { "Chest", 2, 0.535f, 0.355f },
        { "Arms", 3, 0.105f, 0.47f },
        { "Arms", 3, 0.885f, 0.49f },
        { "Legs", 4, 0.365f, 0.645f },
        { "Legs", 4, 0.685f, 0.655f },
        { "Feet", 5, 0.355f, 0.955f },
        { "Feet", 5, 0.645f, 0.965f },
        { "Wheels", 6, 0.535f, 1.03f },
    };

    c_image ped_hitbox_photo;

    bool load_image_from_file(ID3D11Device* device, c_image& image, const std::filesystem::path& path) {
        std::ifstream file{ path, std::ios::binary | std::ios::ate };
        if (!file) {
            return false;
        }

        const std::streamsize file_size = file.tellg();
        if (file_size <= 0) {
            return false;
        }

        file.seekg(0, std::ios::beg);
        std::vector<unsigned char> buffer(static_cast<size_t>(file_size));
        if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size)) {
            return false;
        }

        image.load(device, buffer.data(), buffer.size());
        return true;
    }

    void draw_ped_hitbox_selector(const char* str_id, bool* values, int value_count, const ped_hitbox_point_t* points, int point_count, bool selector_active) {
        auto* window = GetCurrentWindow();
        auto* draw = window->DrawList;

        const ImVec2 pos = window->DC.CursorPos;
        const ImVec2 avail = GetContentRegionAvail();
        const ImVec2 size{ avail.x, avail.y };
        Dummy(size);
        const ImVec2 post_dummy_cursor = window->DC.CursorPos;

        ImRect bb{ pos, pos + size };
        const float outer_pad = 10 ds;

        g_draw->gradient(draw, bb.Min, bb.Max,
            g_style->col(pcol_bg3),
            g_style->col(pcol_bg2),
            g_style->col(pcol_bg2),
            g_style->col(pcol_bg3),
            12 ds);

        ImRect image_panel{ bb.Min + ImVec2{ outer_pad, outer_pad }, bb.Max - ImVec2{ outer_pad, outer_pad } };
        g_draw->gradient(draw, image_panel.Min, image_panel.Max,
            g_style->col(pcol_bg3),
            g_style->col(pcol_bg7),
            g_style->col(pcol_bg2),
            g_style->col(pcol_bg3),
            10 ds);

        ImRect image_area{ image_panel.Min + ImVec2{ 10 ds, 10 ds }, image_panel.Max - ImVec2{ 10 ds, 10 ds } };
        ImRect ped_rect = image_area;

        // No model artwork any more: the hitbox points sit directly on the panel.
        ped_rect = image_area;

        bool hovered_points[12]{ };
        bool any_hovered = false;
        const ImVec2 point_hitbox{ 22 ds, 22 ds };
        const float point_radius = 6 ds;

        PushID(str_id);
        for (int i = 0; i < point_count; ++i) {
            const ImVec2 point_pos{
                ped_rect.Min.x + ped_rect.GetWidth() * points[i].x,
                ped_rect.Min.y + ped_rect.GetHeight() * points[i].y
            };

            PushID(i);
            SetCursorScreenPos(point_pos - point_hitbox * 0.5f);
            if (InvisibleButton("##hitbox_point", point_hitbox)) {
                values[points[i].value_index] = !values[points[i].value_index];
            }

            hovered_points[i] = IsItemHovered();
            any_hovered |= hovered_points[i];

            if (hovered_points[i]) {
                SetTooltip("%s", points[i].label);
            }
            PopID();
        }
        PopID();

        SetCursorScreenPos(post_dummy_cursor);

        if (any_hovered) {
            SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        const auto& palette = get_active_menu_palette();
        const pcolor disabled_red{ 255, 42, 42, 1.0f };
        const pcolor disabled_red_dim{ 120, 18, 18, 1.0f };
        for (int i = 0; i < point_count; ++i) {
            const ImVec2 point_pos{
                ped_rect.Min.x + ped_rect.GetWidth() * points[i].x,
                ped_rect.Min.y + ped_rect.GetHeight() * points[i].y
            };

            const bool enabled = values[points[i].value_index];
            const bool hovered = hovered_points[i];
            const float radius = point_radius + (hovered ? 1.0f : 0.0f);
            const bool active_enabled = selector_active && enabled;
            const pcolor glow = active_enabled
                ? with_alpha(palette.bright, 0.18f)
                : hovered
                ? with_alpha(disabled_red, 0.13f)
                : pcolor{ 0, 0, 0, 0.0f };
            const pcolor fill = active_enabled
                ? blend_color(palette.bright, palette.accent_soft, 0.08f)
                : hovered
                ? blend_color(disabled_red, pcolor{ 124, 124, 124, 1.0f }, 0.30f)
                : blend_color(disabled_red, disabled_red_dim, 0.38f);

            if (glow.a > 0.0f) {
                draw->AddCircleFilled(point_pos, radius + 5 ds, glow);
            }
            draw->AddCircleFilled(point_pos, radius, fill);
        }
    }

    void draw_esp_preview_ped() {
        auto* window = GetCurrentWindow();
        auto* draw = window->DrawList;

        const ImRect bb{ window->Pos, window->Pos + window->Size };
        const float outer_pad = 10 ds;

        g_draw->gradient(draw, bb.Min, bb.Max,
            g_style->col(pcol_bg3),
            g_style->col(pcol_bg2),
            g_style->col(pcol_bg2),
            g_style->col(pcol_bg3),
            12 ds);

        const ImRect image_panel{ bb.Min + ImVec2{ outer_pad, outer_pad }, bb.Max - ImVec2{ outer_pad, outer_pad } };
        g_draw->gradient(draw, image_panel.Min, image_panel.Max,
            g_style->col(pcol_bg3),
            g_style->col(pcol_bg7),
            g_style->col(pcol_bg2),
            g_style->col(pcol_bg3),
            10 ds);

        const ImRect image_area{ image_panel.Min + ImVec2{ 10 ds, 10 ds }, image_panel.Max - ImVec2{ 10 ds, 10 ds } };
        const ImVec2 preview_size = image_area.GetSize();
        const float preview_box_height = ImClamp(preview_size.y * 0.69f, 258.0f, 318.0f);
        const float preview_box_width = ImMin(preview_box_height * 0.67f, preview_size.x * 0.80f);
        const ImRect preview_box{
            ImVec2{ image_area.GetCenter().x - preview_box_width * 0.5f, image_area.GetCenter().y - preview_box_height * 0.5f },
            ImVec2{ image_area.GetCenter().x + preview_box_width * 0.5f, image_area.GetCenter().y + preview_box_height * 0.5f }
        };

        // No model artwork any more - the animated mark carries the panel instead.
        const auto& preview_palette = get_active_menu_palette();
        draw_infinity_logo(draw, preview_box.Min, preview_box.Max, preview_palette.bright, preview_palette.accent, 0.55f);
    }

    float aimbot_curve_factor(float t, float accel, float decel) {
        accel = ImMax(0.2f, accel);
        decel = ImMax(0.2f, decel);

        float ease_in = powf(t, accel);
        float ease_out = 1.0f - powf(1.0f - t, decel);
        return ImClamp(ease_in + ((ease_out - ease_in) * t), 0.0f, 1.0f);
    }

    float solve_curve_accel(float target_y, float t, float decel) {
        float lo = 0.2f;
        float hi = 8.0f;

        target_y = ImClamp(target_y, 0.02f, 0.98f);

        for (int i = 0; i < 20; ++i) {
            float mid = (lo + hi) * 0.5f;
            float y = aimbot_curve_factor(t, mid, decel);

            if (y > target_y)
                lo = mid;
            else
                hi = mid;
        }

        return (lo + hi) * 0.5f;
    }

    float solve_curve_decel(float target_y, float t, float accel) {
        float lo = 0.2f;
        float hi = 8.0f;

        target_y = ImClamp(target_y, 0.02f, 0.98f);

        for (int i = 0; i < 20; ++i) {
            float mid = (lo + hi) * 0.5f;
            float y = aimbot_curve_factor(t, accel, mid);

            if (y < target_y)
                lo = mid;
            else
                hi = mid;
        }

        return (lo + hi) * 0.5f;
    }

    void draw_dashed_line(ImDrawList* draw, ImVec2 from, ImVec2 to, pcolor color, float dash = 10.f, float gap = 7.f, float thickness = 1.5f) {
        ImVec2 delta = to - from;
        float length = sqrtf(delta.x * delta.x + delta.y * delta.y);

        if (length <= 0.0f) {
            return;
        }

        ImVec2 dir = delta / length;

        for (float traveled = 0.f; traveled < length; traveled += dash + gap) {
            float segment_end = ImMin(traveled + dash, length);
            draw->AddLine(from + dir * traveled, from + dir * segment_end, color, thickness);
        }
    }

    void draw_curve_value_card(const char* str_id, const char* title, float value, ImVec2 size) {
        ImDrawList* draw = GetWindowDrawList();
        ImVec2 pos = GetCurrentWindow()->DC.CursorPos;
        Dummy(size);

        ImRect bb{ pos, pos + size };
        g_draw->gradient(draw, bb.Min, bb.Max,
            g_style->col(pcol_bg3),
            g_style->col(pcol_bg2),
            g_style->col(pcol_bg2),
            g_style->col(pcol_bg3),
            10 ds);
        g_draw->gradientoutline(draw, bb.Min, bb.Max,
            g_style->col(pcol_scheme, 0.20f),
            g_style->col(pcol_border),
            g_style->col(pcol_border),
            g_style->col(pcol_scheme, 0.08f),
            10 ds);

        char value_buf[16];
        ImFormatString(value_buf, sizeof(value_buf), "%.1f", value);

        ImVec2 title_size = g_draw->textsize(cascadia, 13, title);
        ImVec2 value_size = g_draw->textsize(fontb, 16, value_buf);

        g_draw->text(cascadia, 13, { bb.GetCenter().x - title_size.x / 2, bb.Min.y + 10 ds }, g_style->col(pcol_text2), title);
        g_draw->text(fontb, 16, { bb.GetCenter().x - value_size.x / 2, bb.Min.y + 28 ds }, g_style->col(pcol_text), value_buf);
    }

    struct aimbot_curve_editor_state_t {
        float accel_t = 0.28f;
        float decel_t = 0.72f;
        float last_accel = -1.0f;
        float last_decel = -1.0f;
        bool initialized = false;
    };

    aimbot_curve_editor_state_t& get_aimbot_curve_editor_state() {
        static aimbot_curve_editor_state_t state;
        return state;
    }

    void sync_aimbot_curve_editor_state(aimbot_curve_editor_state_t& state, float accel, float decel, bool reset_positions = false) {
        accel = ImClamp(accel, 0.2f, 8.0f);
        decel = ImClamp(decel, 0.2f, 8.0f);

        if (!state.initialized || reset_positions) {
            state.accel_t = 0.28f;
            state.decel_t = 0.72f;
            state.initialized = true;
        }

        state.last_accel = accel;
        state.last_decel = decel;
    }

    void draw_aimbot_curve_editor(const char* str_id, float* accel, float* decel, bool enabled) {
        auto* window = GetCurrentWindow();
        auto* draw = window->DrawList;
        auto& state = get_aimbot_curve_editor_state();

        *accel = ImClamp(*accel, 0.2f, 8.0f);
        *decel = ImClamp(*decel, 0.2f, 8.0f);

        if (!state.initialized) {
            sync_aimbot_curve_editor_state(state, *accel, *decel, true);
        }
        else if (fabsf(state.last_accel - *accel) > 0.001f || fabsf(state.last_decel - *decel) > 0.001f) {
            sync_aimbot_curve_editor_state(state, *accel, *decel);
        }

        const float visual_alpha = enabled ? 1.0f : 0.45f;
        const float handle_radius = 7.f ds;
        const ImVec2 handle_hitbox{ 26 ds, 26 ds };

        ImVec2 pos = window->DC.CursorPos;
        Dummy({ CalcItemWidth(), 190 ds });
        ImVec2 post_dummy_cursor = window->DC.CursorPos;
        ImRect bb{ pos, pos + ImVec2{ CalcItemWidth(), 190 ds } };
        ImRect plot{ bb.Min + ImVec2{ 12 ds, 10 ds }, bb.Max - ImVec2{ 12 ds, 10 ds } };

        g_draw->gradient(draw, plot.Min, plot.Max,
            g_style->col(pcol_bg3),
            g_style->col(pcol_bg7),
            g_style->col(pcol_bg2),
            g_style->col(pcol_bg3),
            12 ds);
        g_draw->gradientoutline(draw, plot.Min, plot.Max,
            g_style->col(pcol_scheme, 0.22f * visual_alpha),
            g_style->col(pcol_border),
            g_style->col(pcol_border),
            g_style->col(pcol_scheme, 0.10f * visual_alpha),
            12 ds);

        g_draw->gradient(draw, plot.Min, { plot.Max.x, plot.Min.y + 54 ds },
            g_style->col(pcol_scheme, 0.11f * visual_alpha),
            g_style->col(pcol_scheme, 0.02f * visual_alpha),
            g_style->col(pcol_scheme, 0.00f),
            g_style->col(pcol_scheme, 0.00f),
            12 ds,
            ImDrawFlags_RoundCornersTop);

        const ImRect graph{ plot.Min + ImVec2{ 16 ds, 16 ds }, plot.Max - ImVec2{ 16 ds, 16 ds } };

        auto graph_pos = [&](float t, float y) {
            return ImVec2{
                ImLerp(graph.Min.x, graph.Max.x, t),
                ImLerp(graph.Max.y, graph.Min.y, y)
            };
            };

        float accel_y = aimbot_curve_factor(state.accel_t, *accel, *decel);
        float decel_y = aimbot_curve_factor(state.decel_t, *accel, *decel);
        ImVec2 accel_handle = graph_pos(state.accel_t, accel_y);
        ImVec2 decel_handle = graph_pos(state.decel_t, decel_y);

        bool accel_hovered = false;
        bool accel_active = false;
        bool decel_hovered = false;
        bool decel_active = false;

        PushID(str_id);
        SetCursorScreenPos(accel_handle - ImVec2{ handle_hitbox.x * 0.5f, handle_hitbox.y * 0.5f });
        InvisibleButton("accel_point", handle_hitbox);
        accel_hovered = IsItemHovered();
        accel_active = IsItemActive();

        SetCursorScreenPos(decel_handle - ImVec2{ handle_hitbox.x * 0.5f, handle_hitbox.y * 0.5f });
        InvisibleButton("decel_point", handle_hitbox);
        decel_hovered = IsItemHovered();
        decel_active = IsItemActive();
        PopID();

        ImVec2 mouse = GetIO().MousePos;
        float mouse_t = ImClamp((mouse.x - graph.Min.x) / graph.GetWidth(), 0.08f, 0.92f);
        float mouse_y = ImClamp(1.0f - ((mouse.y - graph.Min.y) / graph.GetHeight()), 0.04f, 0.98f);

        if (accel_active) {
            state.accel_t = ImClamp(mouse_t, 0.08f, state.decel_t - 0.10f);
            *accel = solve_curve_accel(mouse_y, state.accel_t, *decel);
            *accel = ImClamp(*accel, 0.2f, 8.0f);
            state.last_accel = *accel;
        }

        if (decel_active) {
            state.decel_t = ImClamp(mouse_t, state.accel_t + 0.10f, 0.92f);
            *decel = solve_curve_decel(mouse_y, state.decel_t, *accel);
            *decel = ImClamp(*decel, 0.2f, 8.0f);
            state.last_decel = *decel;
        }

        accel_y = aimbot_curve_factor(state.accel_t, *accel, *decel);
        decel_y = aimbot_curve_factor(state.decel_t, *accel, *decel);
        accel_handle = graph_pos(state.accel_t, accel_y);
        decel_handle = graph_pos(state.decel_t, decel_y);
        state.last_accel = *accel;
        state.last_decel = *decel;

        SetCursorScreenPos(post_dummy_cursor);

        draw->PushClipRect(graph.Min, graph.Max, true);
        for (int i = 1; i < 8; ++i) {
            float x = graph.Min.x + graph.GetWidth() * (i / 8.0f);
            draw->AddLine({ x, graph.Min.y }, { x, graph.Max.y }, g_style->col(pcol_border, 0.35f * visual_alpha));
        }

        for (int i = 1; i < 5; ++i) {
            float y = graph.Min.y + graph.GetHeight() * (i / 5.0f);
            draw->AddLine({ graph.Min.x, y }, { graph.Max.x, y }, g_style->col(pcol_border, 0.28f * visual_alpha));
        }

        ImVec2 curve_points[65];
        for (int i = 0; i <= 64; ++i) {
            float t = i / 64.0f;
            float y = aimbot_curve_factor(t, *accel, *decel);
            curve_points[i] = {
                ImLerp(graph.Min.x, graph.Max.x, t),
                ImLerp(graph.Max.y, graph.Min.y, y)
            };
        }

        draw->AddPolyline(curve_points, 65, g_style->col(pcol_scheme, 0.16f * visual_alpha), false, 8 ds);
        draw->AddPolyline(curve_points, 65, g_style->col(pcol_scheme, 0.32f * visual_alpha), false, 5 ds);
        draw->AddPolyline(curve_points, 65, g_style->col(pcol_text, 0.95f * visual_alpha), false, 2.75f ds);

        ImVec2 start = curve_points[0];
        ImVec2 end = curve_points[64];

        draw_dashed_line(draw, { graph.Min.x, accel_handle.y }, accel_handle, g_style->col(pcol_text2, 0.40f * visual_alpha));
        draw_dashed_line(draw, decel_handle, { graph.Max.x, decel_handle.y }, g_style->col(pcol_text2, 0.40f * visual_alpha));

        draw->AddCircleFilled(start, 7 ds, g_style->col(pcol_text, 0.95f * visual_alpha));
        draw->AddCircle(start, 9 ds, g_style->col(pcol_scheme, 0.45f * visual_alpha), 0, 2 ds);
        draw->AddCircleFilled(end, 7 ds, g_style->col(pcol_text, 0.95f * visual_alpha));
        draw->AddCircle(end, 9 ds, g_style->col(pcol_scheme, 0.45f * visual_alpha), 0, 2 ds);
        draw->PopClipRect();

        //g_draw->text(cascadia, 13, { plot.Min.x + 14 ds, plot.Min.y + 12 ds }, g_style->col(pcol_text2, 0.95f * visual_alpha), "CAMERA START");

        //const char* target_label = "TARGET END";
        //ImVec2 target_label_size = g_draw->textsize(cascadia, 13, target_label);
        //g_draw->text(cascadia, 13, { plot.Max.x - target_label_size.x - 14 ds, plot.Min.y + 12 ds }, g_style->col(pcol_text2, 0.95f * visual_alpha), target_label);

        float accel_outline_alpha = accel_active ? 0.90f : accel_hovered ? 0.72f : 0.55f;
        float decel_outline_alpha = decel_active ? 0.90f : decel_hovered ? 0.72f : 0.55f;
        draw->AddCircleFilled(accel_handle, handle_radius, g_style->col(pcol_bg5, visual_alpha));
        draw->AddCircle(accel_handle, handle_radius + 2 ds, g_style->col(pcol_scheme, accel_outline_alpha * visual_alpha), 0, 2 ds);
        draw->AddCircleFilled(decel_handle, handle_radius, g_style->col(pcol_bg5, visual_alpha));
        draw->AddCircle(decel_handle, handle_radius + 2 ds, g_style->col(pcol_scheme, decel_outline_alpha * visual_alpha), 0, 2 ds);
    }

    enum config_sort_mode_t {
        config_sort_my_configs = 0,
        config_sort_trending,
        config_sort_latest
    };

    struct config_browser_entry_t {
        std::string id;
        std::string name;
        std::string author;
        std::string updated_label;
        int usage_count = 0;
        int trend_score = 0;
        int latest_rank = 0;
        int slot_index = 0;
        bool is_owned = false;
        bool is_private = false;
        bool is_cloud_slot = false;
        bool is_active_slot = false;
        bool streamproof = false;
        bool esp_enabled = false;
        bool silentaim_enabled = false;
        bool aimbot_enabled = false;
        bool triggerbot_enabled = false;
        bool uses_exploits = false;
        std::string config_json;
    };

    std::string format_config_usage_count(int usage_count) {
        char buffer[32];
        if (usage_count >= 1000) {
            std::snprintf(buffer, sizeof(buffer), "%.1fk uses", static_cast<float>(usage_count) / 1000.0f);
        }
        else {
            std::snprintf(buffer, sizeof(buffer), "%d uses", usage_count);
        }
        return buffer;
    }

    std::string make_config_browser_preset_json(
        bool esp_enabled,
        bool aimbot_enabled,
        bool silentaim_enabled,
        bool triggerbot_enabled,
        bool streamproof,
        int aimbot_fov,
        int silentaim_fov,
        int trigger_delay,
        int esp_distance,
        bool only_visible,
        bool highlight_visible,
        bool smart_trigger,
        bool uses_exploits) {
        nlohmann::json cfg;
        auto& general = cfg["General"];
        auto& aimbot = cfg["Aimbot"];
        auto& silentaim = cfg["SilentAim"];
        auto& triggerbot = cfg["TriggerBot"];
        auto& esp = cfg["ESP"];
        auto& vehicle_esp = cfg["VehicleESP"];
        auto& player = cfg["Player"];

        general["StreamProof"] = streamproof;

        aimbot["Enabled"] = aimbot_enabled;
        aimbot["ShowFov"] = aimbot_enabled;
        aimbot["OnlyVisible"] = only_visible;
        aimbot["IgnoreNPCs"] = true;
        aimbot["IgnoreDead"] = true;
        aimbot["Prediction"] = aimbot_enabled;
        aimbot["RandomHitbox"] = false;
        aimbot["LegitMode"] = true;
        aimbot["StickyAim"] = false;
        aimbot["CurvyMode"] = false;
        aimbot["CurvyValue"] = 1.0f;
        aimbot["CurveDecel"] = 3.0f;
        aimbot["CurveAccel"] = 2.0f;
        aimbot["ARMS"] = false;
        aimbot["NECKS"] = false;
        aimbot["CHESTS"] = false;
        aimbot["HEADS"] = true;
        aimbot["LEGS"] = false;
        aimbot["FEET"] = false;
        aimbot["FOV"] = aimbot_fov;
        aimbot["FOVX"] = aimbot_fov;
        aimbot["FOVY"] = aimbot_fov;
        aimbot["MaxDistance"] = aimbot_enabled ? 280 : 160;
        aimbot["SmoothHorizontal"] = aimbot_enabled ? 10 : 12;
        aimbot["SmoothVertical"] = aimbot_enabled ? 10 : 12;
        aimbot["AimSpeed"] = aimbot_enabled ? 16 : 0;
        aimbot["AimbotSpeed"] = aimbot_enabled ? 16 : 0;
        aimbot["KeyBindMode"] = 1;
        aimbot["MissChance"] = aimbot_enabled ? 2 : 0;
        aimbot["FovColor"] = nlohmann::json::array({ 1.0f, 1.0f, 1.0f, 0.92f });
        aimbot["FovThickness"] = 1.8f;
        aimbot["FovFilled"] = false;
        aimbot["FovFillColor"] = nlohmann::json::array({ 1.0f, 1.0f, 1.0f, 0.14f });

        silentaim["Enabled"] = silentaim_enabled;
        silentaim["ShowFov"] = silentaim_enabled;
        silentaim["OnlyVisible"] = only_visible;
        silentaim["IgnoreNPCs"] = true;
        silentaim["IgnoreDead"] = true;
        silentaim["Prediction"] = silentaim_enabled;
        silentaim["FOV"] = silentaim_fov;
        silentaim["FOVX"] = silentaim_fov;
        silentaim["FOVY"] = silentaim_fov;
        silentaim["SilentBypass"] = silentaim_enabled;
        silentaim["BoneGroup_Wheels"] = false;
        silentaim["LEGS"] = false;
        silentaim["FEET"] = false;
        silentaim["ARMS"] = false;
        silentaim["CHESTS"] = false;
        silentaim["NECKS"] = false;
        silentaim["HEADS"] = true;
        silentaim["MaxDistance"] = silentaim_enabled ? 240 : 150;
        silentaim["KeyBindMode"] = 1;
        silentaim["MissChance"] = silentaim_enabled ? 6 : 0;
        silentaim["DynamicMissChance"] = 0;
        silentaim["MagicBullets"] = uses_exploits && silentaim_enabled;
        silentaim["FovColor"] = nlohmann::json::array({ 0.98f, 0.62f, 0.62f, 0.86f });
        silentaim["FovThickness"] = 1.8f;
        silentaim["FovFilled"] = false;
        silentaim["FovFillColor"] = nlohmann::json::array({ 0.98f, 0.62f, 0.62f, 0.12f });

        triggerbot["Enabled"] = triggerbot_enabled;
        triggerbot["ShowFov"] = triggerbot_enabled && !smart_trigger;
        triggerbot["SmartTrigger"] = smart_trigger;
        triggerbot["OnlyVisible"] = only_visible;
        triggerbot["IgnoreNPCs"] = true;
        triggerbot["IgnoreDead"] = true;
        triggerbot["FOV"] = triggerbot_enabled ? 36 : 0;
        triggerbot["FOVX"] = triggerbot_enabled ? 36 : 0;
        triggerbot["FOVY"] = triggerbot_enabled ? 36 : 0;
        triggerbot["MaxDistance"] = triggerbot_enabled ? 140 : 100;
        triggerbot["Delay"] = triggerbot_enabled ? trigger_delay : 0;
        triggerbot["KeyBindMode"] = 1;
        triggerbot["FovColor"] = nlohmann::json::array({ 0.90f, 0.90f, 1.0f, 0.90f });
        triggerbot["FovThickness"] = 1.8f;
        triggerbot["FovFilled"] = false;
        triggerbot["FovFillColor"] = nlohmann::json::array({ 0.90f, 0.90f, 1.0f, 0.12f });

        esp["Enabled"] = esp_enabled;
        esp["Box"] = esp_enabled;
        esp["BoxState"] = 0;
        esp["Skeleton"] = esp_enabled;
        esp["HealthBar"] = esp_enabled;
        esp["HealthBarState"] = 0;
        esp["ArmorBar"] = false;
        esp["ArmorBarState"] = 0;
        esp["WeaponName"] = esp_enabled;
        esp["WeaponNameState"] = 0;
        esp["SnapLines"] = false;
        esp["UserNames"] = esp_enabled;
        esp["UserNamesState"] = 0;
        esp["HeadCircle"] = false;
        esp["IgnoreNPCs"] = true;
        esp["HighlightVisible"] = highlight_visible;
        esp["IgnoreDead"] = true;
        esp["Target"] = false;
        esp["DistanceFromMe"] = esp_enabled;
        esp["DistanceFromMeState"] = 0;
        esp["ShowLocalPlayer"] = false;
        esp["MaxDistance"] = esp_distance;
        esp["FriendsMarker"] = false;
        esp["FriendsMarkerBind"] = 0;

        vehicle_esp["Enabled"] = false;
        vehicle_esp["SnapLines"] = false;
        vehicle_esp["ShowLockUnlock"] = false;
        vehicle_esp["VehName"] = false;
        vehicle_esp["DistanceFromMe"] = false;
        vehicle_esp["MaxDistance"] = 0;

        player["EnableGodMode"] = false;
        player["FakeLagMode"] = uses_exploits ? 1 : 0;
        player["FakeLagKey"] = 0;
        player["FakeLag"] = uses_exploits;
        player["FakeLagDelay"] = uses_exploits ? 850.0f : 1000.0f;

        return cfg.dump();
    }

    config_browser_entry_t make_config_browser_entry(
        const char* id,
        const char* name,
        const char* author,
        const char* updated_label,
        int usage_count,
        int trend_score,
        int latest_rank,
        bool is_owned,
        bool is_private,
        bool streamproof,
        bool esp_enabled,
        bool silentaim_enabled,
        bool aimbot_enabled,
        bool triggerbot_enabled,
        bool uses_exploits,
        std::string config_json) {
        config_browser_entry_t entry;
        entry.id = id;
        entry.name = name;
        entry.author = author;
        entry.updated_label = updated_label;
        entry.usage_count = usage_count;
        entry.trend_score = trend_score;
        entry.latest_rank = latest_rank;
        entry.is_owned = is_owned;
        entry.is_private = is_private;
        entry.streamproof = streamproof;
        entry.esp_enabled = esp_enabled;
        entry.silentaim_enabled = silentaim_enabled;
        entry.aimbot_enabled = aimbot_enabled;
        entry.triggerbot_enabled = triggerbot_enabled;
        entry.uses_exploits = uses_exploits;
        entry.config_json = std::move(config_json);
        return entry;
    }

    const std::vector<config_browser_entry_t>& get_config_browser_catalog() {
        static const std::vector<config_browser_entry_t> catalog = [] {
            std::vector<config_browser_entry_t> entries;

            entries.push_back(make_config_browser_entry(
                "own_streamsafe",
                "StreamSafe Legit",
                "You",
                "Updated 2m ago",
                124,
                86,
                6,
                true,
                true,
                true,
                true,
                false,
                false,
                true,
                false,
                make_config_browser_preset_json(true, false, false, true, true, 0, 0, 18, 260, true, true, true, false)));

            entries.push_back(make_config_browser_entry(
                "own_hybrid",
                "Hybrid Control",
                "You",
                "Updated 18m ago",
                219,
                74,
                5,
                true,
                true,
                true,
                true,
                false,
                true,
                true,
                false,
                make_config_browser_preset_json(true, true, false, true, true, 52, 0, 12, 300, true, true, false, false)));

            entries.push_back(make_config_browser_entry(
                "own_boxes",
                "Box Hunter",
                "You",
                "Updated 1h ago",
                88,
                41,
                4,
                true,
                false,
                false,
                true,
                false,
                false,
                false,
                false,
                make_config_browser_preset_json(true, false, false, false, false, 0, 0, 0, 420, true, true, false, false)));

            entries.push_back(make_config_browser_entry(
                "trend_reaper",
                "Reaper Mix",
                "Reaper",
                "Updated 4m ago",
                2418,
                99,
                9,
                false,
                false,
                false,
                true,
                true,
                true,
                false,
                true,
                make_config_browser_preset_json(true, true, true, false, false, 78, 62, 0, 340, false, true, false, true)));

            entries.push_back(make_config_browser_entry(
                "trend_shadowline",
                "Shadowline",
                "Kairo",
                "Updated 12m ago",
                1682,
                93,
                8,
                false,
                false,
                true,
                true,
                true,
                false,
                true,
                false,
                make_config_browser_preset_json(true, false, true, true, true, 0, 54, 16, 280, true, true, true, false)));

            entries.push_back(make_config_browser_entry(
                "latest_aperture",
                "Aperture ESP",
                "Nova",
                "Updated 1m ago",
                376,
                58,
                12,
                false,
                false,
                true,
                true,
                false,
                false,
                false,
                false,
                make_config_browser_preset_json(true, false, false, false, true, 0, 0, 0, 500, true, true, false, false)));

            entries.push_back(make_config_browser_entry(
                "latest_ghosttap",
                "GhostTap",
                "Astra",
                "Updated 6m ago",
                512,
                67,
                11,
                false,
                true,
                true,
                true,
                false,
                true,
                true,
                true,
                make_config_browser_preset_json(true, true, false, true, true, 46, 0, 8, 260, true, false, false, true)));

            return entries;
        }();

        return catalog;
    }

    std::vector<const config_browser_entry_t*> collect_config_browser_entries(
        const std::vector<config_browser_entry_t>& catalog,
        int sort_mode,
        std::string_view search_query,
        bool filter_esp,
        bool filter_silentaim,
        bool filter_aimbot,
        bool filter_triggerbot) {
        std::vector<const config_browser_entry_t*> entries;

        for (const auto& entry : catalog) {
            if (sort_mode == config_sort_my_configs && !entry.is_owned) {
                continue;
            }

            if (!search_query.empty() && !contains_case_insensitive(entry.name, search_query)) {
                continue;
            }

            if (filter_esp && !entry.esp_enabled) {
                continue;
            }

            if (filter_silentaim && !entry.silentaim_enabled) {
                continue;
            }

            if (filter_aimbot && !entry.aimbot_enabled) {
                continue;
            }

            if (filter_triggerbot && !entry.triggerbot_enabled) {
                continue;
            }

            entries.push_back(&entry);
        }

        std::sort(entries.begin(), entries.end(), [&](const config_browser_entry_t* lhs, const config_browser_entry_t* rhs) {
            if (sort_mode == config_sort_my_configs) {
                if (lhs->is_owned != rhs->is_owned) {
                    return lhs->is_owned > rhs->is_owned;
                }
                return lhs->latest_rank > rhs->latest_rank;
            }

            if (sort_mode == config_sort_latest) {
                if (lhs->latest_rank != rhs->latest_rank) {
                    return lhs->latest_rank > rhs->latest_rank;
                }
                return lhs->trend_score > rhs->trend_score;
            }

            if (lhs->trend_score != rhs->trend_score) {
                return lhs->trend_score > rhs->trend_score;
            }

            return lhs->latest_rank > rhs->latest_rank;
        });

        return entries;
    }

    struct config_browser_ui_state_t {
        char search[64]{};
        int sort_mode = config_sort_trending;
        bool filter_esp = false;
        bool filter_silentaim = false;
        bool filter_aimbot = false;
        bool filter_triggerbot = false;
        bool preview_active = false;
        bool preview_is_cloud_slot = false;
        std::string preview_config_id;
        std::string preview_config_name;
        std::string main_config_snapshot;
    };

    config_browser_ui_state_t& get_config_browser_ui_state() {
        static config_browser_ui_state_t state{};
        return state;
    }

    struct config_browser_cloud_state_t {
        std::mutex mutex;
        std::vector<config_browser_entry_t> entries;
        bool ready = false;
        bool loading = false;
        int active_slot = 0;
        ULONGLONG next_refresh_at = 0;
        std::string error;
    };

    config_browser_cloud_state_t& get_config_browser_cloud_state() {
        static config_browser_cloud_state_t state{};
        return state;
    }

    const nlohmann::json* json_member(const nlohmann::json& object, const char* key) {
        if (!object.is_object()) {
            return nullptr;
        }

        const auto it = object.find(key);
        if (it == object.end()) {
            return nullptr;
        }

        return &(*it);
    }

    std::string json_string_or(const nlohmann::json& value, const std::string& fallback = {}) {
        if (value.is_string()) {
            return value.get<std::string>();
        }
        if (value.is_boolean()) {
            return value.get<bool>() ? "true" : "false";
        }
        if (value.is_number_integer()) {
            return std::to_string(value.get<long long>());
        }
        if (value.is_number_unsigned()) {
            return std::to_string(value.get<unsigned long long>());
        }

        return fallback;
    }

    int json_int_or(const nlohmann::json& value, int fallback = 0) {
        if (value.is_number_integer()) {
            const long long raw = value.get<long long>();
            if (raw < static_cast<long long>(INT_MIN)) {
                return INT_MIN;
            }
            if (raw > static_cast<long long>(INT_MAX)) {
                return INT_MAX;
            }
            return static_cast<int>(raw);
        }

        if (value.is_number_unsigned()) {
            const unsigned long long raw = value.get<unsigned long long>();
            if (raw > static_cast<unsigned long long>(INT_MAX)) {
                return INT_MAX;
            }
            return static_cast<int>(raw);
        }

        if (value.is_number_float()) {
            const double raw = value.get<double>();
            if (!std::isfinite(raw)) {
                return fallback;
            }
            if (raw < static_cast<double>(INT_MIN)) {
                return INT_MIN;
            }
            if (raw > static_cast<double>(INT_MAX)) {
                return INT_MAX;
            }
            return static_cast<int>(raw);
        }

        if (value.is_boolean()) {
            return value.get<bool>() ? 1 : 0;
        }

        return fallback;
    }

    bool json_bool_or(const nlohmann::json& value, bool fallback = false) {
        if (value.is_boolean()) {
            return value.get<bool>();
        }
        if (value.is_number_integer() || value.is_number_unsigned()) {
            return json_int_or(value, fallback ? 1 : 0) != 0;
        }
        if (value.is_string()) {
            std::string text = value.get<std::string>();
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            if (text == "1" || text == "true" || text == "yes" || text == "on") {
                return true;
            }
            if (text == "0" || text == "false" || text == "no" || text == "off") {
                return false;
            }
        }
        return fallback;
    }

    std::string json_member_string_or(const nlohmann::json& object, const char* key, const std::string& fallback = {}) {
        const auto* value = json_member(object, key);
        return value ? json_string_or(*value, fallback) : fallback;
    }

    int json_member_int_or(const nlohmann::json& object, const char* key, int fallback = 0) {
        const auto* value = json_member(object, key);
        return value ? json_int_or(*value, fallback) : fallback;
    }

    bool json_member_bool_or(const nlohmann::json& object, const char* key, bool fallback = false) {
        const auto* value = json_member(object, key);
        return value ? json_bool_or(*value, fallback) : fallback;
    }

    nlohmann::json json_object_or_empty(const nlohmann::json& value) {
        if (value.is_object()) {
            return value;
        }

        if (value.is_string()) {
            nlohmann::json parsed = nlohmann::json::parse(value.get<std::string>(), nullptr, false);
            if (parsed.is_object()) {
                return parsed;
            }
        }

        return nlohmann::json::object();
    }

    nlohmann::json json_member_object_or_empty(const nlohmann::json& object, const char* key) {
        const auto* value = json_member(object, key);
        return value ? json_object_or_empty(*value) : nlohmann::json::object();
    }

    bool config_section_bool(const nlohmann::json& config, const char* section_name, const char* key_name, bool fallback = false) {
        if (!config.is_object()) {
            return fallback;
        }

        const auto section_it = config.find(section_name);
        if (section_it == config.end() || !section_it->is_object()) {
            return fallback;
        }

        const auto value_it = section_it->find(key_name);
        if (value_it == section_it->end()) {
            return fallback;
        }

        return json_bool_or(*value_it, fallback);
    }

    bool preview_bool_or(const nlohmann::json& preview, const char* key_name, bool fallback = false) {
        if (!preview.is_object()) {
            return fallback;
        }

        const auto value_it = preview.find(key_name);
        if (value_it == preview.end()) {
            return fallback;
        }

        return json_bool_or(*value_it, fallback);
    }

    nlohmann::json slot_config_object(const nlohmann::json& slot) {
        return json_member_object_or_empty(slot, "config");
    }

    bool config_has_exploits(const nlohmann::json& preview, const nlohmann::json& config) {
        if (preview.is_object()) {
            const auto exploits_it = preview.find("exploits");
            if (exploits_it != preview.end() && exploits_it->is_array() && !exploits_it->empty()) {
                return true;
            }
        }

        return config_section_bool(config, "SilentAim", "MagicBullets") ||
            config_section_bool(config, "Player", "FakeLag") ||
            config_section_bool(config, "Player", "EnableGodMode") ||
            config_section_bool(config, "Player", "VehicleGodMode") ||
            config_section_bool(config, "Player", "StealCarEnabled");
    }

    config_browser_entry_t make_cloud_config_browser_entry(const nlohmann::json& slot, int active_slot) {
        const int slot_index = json_member_int_or(slot, "slot_index", 0);
        const nlohmann::json config = slot_config_object(slot);
        const nlohmann::json preview = json_member_object_or_empty(slot, "preview");
        const bool is_active = json_member_bool_or(slot, "is_active", false) || (slot_index > 0 && slot_index == active_slot);
        const bool is_public = json_member_bool_or(slot, "is_public", false);

        config_browser_entry_t entry;
        entry.slot_index = slot_index;
        entry.id = "slot_" + std::to_string(slot_index);
        entry.name = json_member_string_or(slot, "config_name", std::string("Config ") + std::to_string(slot_index));
        entry.author = "You";
        entry.updated_label = is_active ? "Using on site" : "Site slot " + std::to_string(slot_index);
        entry.usage_count = json_member_int_or(slot, "copy_count", 0);
        entry.trend_score = is_active ? 100000 : entry.usage_count;
        entry.latest_rank = is_active ? 100000 : (1000 - slot_index);
        entry.is_owned = true;
        entry.is_private = !is_public;
        entry.is_cloud_slot = true;
        entry.is_active_slot = is_active;
        entry.streamproof = preview_bool_or(preview, "streamproof", config_section_bool(config, "General", "StreamProof"));
        entry.esp_enabled = preview_bool_or(preview, "esp_enabled", config_section_bool(config, "ESP", "Enabled"));
        entry.silentaim_enabled = preview_bool_or(preview, "silentaim_enabled", config_section_bool(config, "SilentAim", "Enabled"));
        entry.aimbot_enabled = preview_bool_or(preview, "aimbot_enabled", config_section_bool(config, "Aimbot", "Enabled"));
        entry.triggerbot_enabled = preview_bool_or(preview, "triggerbot_enabled", config_section_bool(config, "TriggerBot", "Enabled"));
        entry.uses_exploits = config_has_exploits(preview, config);
        entry.config_json = config.dump();
        return entry;
    }

    bool parse_cloud_config_slots_response(
        const std::string& body,
        std::vector<config_browser_entry_t>& entries,
        int& active_slot,
        std::string& error) {
        try {
            nlohmann::json root = nlohmann::json::parse(body, nullptr, false);
            if (root.is_discarded() || !root.is_object()) {
                error = "bad_json";
                return false;
            }

            if (!json_member_bool_or(root, "ok", false)) {
                error = json_member_string_or(root, "error", "server_error");
                return false;
            }

            active_slot = json_member_int_or(root, "active_slot", json_member_int_or(root, "slot_index", 0));
            const auto* slots_it = json_member(root, "slots");
            if (!slots_it || !slots_it->is_array()) {
                error = "missing_slots";
                return false;
            }

            std::vector<config_browser_entry_t> parsed_entries;
            for (const auto& slot : *slots_it) {
                if (!slot.is_object()) {
                    continue;
                }

                const int slot_index = json_member_int_or(slot, "slot_index", 0);
                if (slot_index <= 0) {
                    continue;
                }

                parsed_entries.push_back(make_cloud_config_browser_entry(slot, active_slot));
            }

            entries = std::move(parsed_entries);
            error.clear();
            return true;
        }
        catch (const std::exception& ex) {
            error = std::string("catalog_parse_exception:") + ex.what();
            return false;
        }
        catch (...) {
            error = "catalog_parse_exception";
            return false;
        }
    }

    ULONGLONG parse_retry_after_ms(const std::string& error) {
        const std::string marker = "retry_after_ms:";
        const size_t marker_pos = error.find(marker);
        if (marker_pos == std::string::npos) {
            return 0;
        }

        ULONGLONG value = 0;
        bool found_digit = false;
        for (size_t i = marker_pos + marker.size(); i < error.size(); ++i) {
            const unsigned char c = static_cast<unsigned char>(error[i]);
            if (!std::isdigit(c)) {
                break;
            }

            found_digit = true;
            value = (value * 10) + static_cast<ULONGLONG>(error[i] - '0');
            if (value > 60000) {
                return 60000;
            }
        }

        return found_digit ? value : 0;
    }

    ULONGLONG cloud_config_error_backoff_ms(const std::string& error) {
        constexpr ULONGLONG kMinRetryMs = 2500;
        constexpr ULONGLONG kDefaultRetryMs = 8000;
        constexpr ULONGLONG kAuthRetryMs = 15000;
        constexpr ULONGLONG kMaxRetryMs = 60000;

        const ULONGLONG retry_after = parse_retry_after_ms(error);
        if (retry_after > 0) {
            const ULONGLONG retry_delay = retry_after + 1000;
            if (retry_delay < kMinRetryMs) {
                return kMinRetryMs;
            }
            if (retry_delay > kMaxRetryMs) {
                return kMaxRetryMs;
            }
            return retry_delay;
        }

        if (error == "unauthorized" || error == "hwid_not_linked") {
            return kAuthRetryMs;
        }

        return kDefaultRetryMs;
    }

    std::string format_cloud_config_error(const std::string& error) {
        if (error == "hwid_not_linked" || error == "unauthorized") {
            return "HWID is not linked on the site";
        }
        if (error.find("unexpected_html_response") != std::string::npos ||
            error == "bad_json" ||
            error.find("catalog_parse_exception") != std::string::npos ||
            error.find("catalog_thread_exception") != std::string::npos) {
            return "Unexpected API response";
        }
        if (error.find("rate_limited") != std::string::npos || error.find("retry_after_ms:") != std::string::npos) {
            return "API cooldown, retrying soon";
        }
        return error;
    }

    bool is_cloud_config_auth_error(const std::string& error) {
        return error == "unauthorized" || error == "hwid_not_linked";
    }

    void store_cloud_config_catalog(std::vector<config_browser_entry_t> entries, int active_slot, const std::string& error, bool ok) {
        auto& cloud = get_config_browser_cloud_state();
        std::lock_guard<std::mutex> lock(cloud.mutex);
        if (ok) {
            cloud.entries = std::move(entries);
            cloud.active_slot = active_slot;
            cloud.ready = true;
            cloud.error.clear();
            cloud.next_refresh_at = GetTickCount64() + 2500;
        }
        else {
            cloud.entries.clear();
            cloud.active_slot = 0;
            cloud.ready = false;
            cloud.error = error.empty() ? "load_failed" : error;
            cloud.next_refresh_at = GetTickCount64() + cloud_config_error_backoff_ms(cloud.error);
        }
        cloud.loading = false;
    }

    void refresh_config_browser_cloud_catalog(bool force = false) {
        auto& cloud = get_config_browser_cloud_state();
        const ULONGLONG now = GetTickCount64();
        {
            std::lock_guard<std::mutex> lock(cloud.mutex);
            if (cloud.loading) {
                return;
            }
            if (!force && cloud.next_refresh_at > now) {
                return;
            }
            cloud.loading = true;
        }

        std::thread([] {
            std::vector<config_browser_entry_t> entries;
            int active_slot = 0;
            std::string error;
            bool ok = false;

            try {
                const std::string payload =
                    std::string("{\"hwid\":\"") + auth::json_escape(auth::get_hwid()) +
                    "\",\"action\":\"list_slots\"}";

                std::string body;
                ok = auth::http_post_json(auth::CONFIG_API_URL, payload, &body);
                if (!ok) {
                    error = body.empty() ? "network_error" : body;
                }
                else {
                    ok = parse_cloud_config_slots_response(body, entries, active_slot, error);
                }
            }
            catch (const std::exception& ex) {
                error = std::string("catalog_thread_exception:") + ex.what();
                ok = false;
            }
            catch (...) {
                error = "catalog_thread_exception";
                ok = false;
            }

            store_cloud_config_catalog(std::move(entries), active_slot, error, ok);
        }).detach();
    }

    std::vector<config_browser_entry_t> get_config_browser_catalog_snapshot() {
        auto& cloud = get_config_browser_cloud_state();
        std::lock_guard<std::mutex> lock(cloud.mutex);
        return cloud.entries;
    }

    bool get_config_browser_cloud_status(bool& loading, bool& ready, std::string& error) {
        auto& cloud = get_config_browser_cloud_state();
        std::lock_guard<std::mutex> lock(cloud.mutex);
        loading = cloud.loading;
        ready = cloud.ready;
        error = cloud.error;
        return cloud.ready;
    }

    int get_config_browser_active_filter_count(const config_browser_ui_state_t& state) {
        return static_cast<int>(state.filter_esp) +
            static_cast<int>(state.filter_silentaim) +
            static_cast<int>(state.filter_aimbot) +
            static_cast<int>(state.filter_triggerbot);
    }

    bool save_config_to_active_cloud_slot(const std::string& config_json, std::string* raw = nullptr) {
        const std::string payload =
            std::string("{\"hwid\":\"") + auth::json_escape(auth::get_hwid()) +
            "\",\"action\":\"save\",\"config\":\"" + auth::json_escape(config_json) + "\"}";

        std::string body;
        if (!auth::http_post_json(auth::CONFIG_API_URL, payload, &body)) {
            if (raw) *raw = body.empty() ? "network_error" : body;
            return false;
        }

        if (raw) *raw = body;

        nlohmann::json root = nlohmann::json::parse(body, nullptr, false);
        return !root.is_discarded() && root.is_object() && json_member_bool_or(root, "ok", false);
    }

    bool activate_cloud_config_slot(config_browser_ui_state_t& state, const config_browser_entry_t& entry) {
        try {
            if (!entry.is_cloud_slot || entry.slot_index <= 0) {
                return false;
            }

            const std::string payload =
                std::string("{\"hwid\":\"") + auth::json_escape(auth::get_hwid()) +
                "\",\"action\":\"set_active\",\"slot_index\":" + std::to_string(entry.slot_index) + "}";

            std::string body;
            if (!auth::http_post_json(auth::CONFIG_API_URL, payload, &body)) {
                NotifyManager::Send("Failed to use site config", 220.0f);
                return false;
            }

            nlohmann::json root = nlohmann::json::parse(body, nullptr, false);
            if (root.is_discarded() || !root.is_object() || !json_member_bool_or(root, "ok", false)) {
                NotifyManager::Send("Site config was rejected", 220.0f);
                return false;
            }

            std::string config_json;
            const auto* config_it = json_member(root, "config");
            if (config_it) {
                if (config_it->is_string()) {
                    config_json = config_it->get<std::string>();
                }
                else if (config_it->is_object()) {
                    config_json = config_it->dump();
                }
            }
            if (config_json.empty()) {
                config_json = entry.config_json;
            }

            if (!g_Config.ApplyConfigJsonString(config_json)) {
                NotifyManager::Send("Failed to apply site config", 220.0f);
                return false;
            }

            std::vector<config_browser_entry_t> entries;
            int active_slot = entry.slot_index;
            std::string error;
            if (parse_cloud_config_slots_response(body, entries, active_slot, error)) {
                store_cloud_config_catalog(std::move(entries), active_slot, "", true);
            }
            else {
                refresh_config_browser_cloud_catalog(true);
            }

            state.preview_active = true;
            state.preview_is_cloud_slot = true;
            state.preview_config_id = entry.id;
            state.preview_config_name = entry.name;
            state.main_config_snapshot.clear();
            NotifyManager::Send("Site config loaded", 220.0f);
            return true;
        }
        catch (const std::exception&) {
            NotifyManager::Send("Config action failed", 220.0f);
            return false;
        }
        catch (...) {
            NotifyManager::Send("Config action failed", 220.0f);
            return false;
        }
    }

    bool save_current_config_to_cloud_slot(config_browser_ui_state_t& state, const config_browser_entry_t& entry) {
        try {
            if (!entry.is_cloud_slot || entry.slot_index <= 0) {
                return false;
            }

            const std::string config_json = g_Config.ExportCurrentConfigJsonString();
            if (config_json.empty()) {
                NotifyManager::Send("Failed to capture current config", 220.0f);
                return false;
            }

            const std::string select_payload =
                std::string("{\"hwid\":\"") + auth::json_escape(auth::get_hwid()) +
                "\",\"action\":\"set_active\",\"slot_index\":" + std::to_string(entry.slot_index) + "}";

            std::string select_body;
            if (!auth::http_post_json(auth::CONFIG_API_URL, select_payload, &select_body)) {
                NotifyManager::Send("Failed to select site config", 220.0f);
                return false;
            }

            nlohmann::json root = nlohmann::json::parse(select_body, nullptr, false);
            if (root.is_discarded() || !root.is_object() || !json_member_bool_or(root, "ok", false)) {
                NotifyManager::Send("Site config was rejected", 220.0f);
                return false;
            }

            std::string raw;
            if (!save_config_to_active_cloud_slot(config_json, &raw)) {
                refresh_config_browser_cloud_catalog(true);
                NotifyManager::Send("Failed to save site config", 220.0f);
                return false;
            }

            refresh_config_browser_cloud_catalog(true);

            state.preview_active = true;
            state.preview_is_cloud_slot = true;
            state.preview_config_id = entry.id;
            state.preview_config_name = entry.name;
            state.main_config_snapshot.clear();
            NotifyManager::Send("Current config saved to site", 220.0f);
            return true;
        }
        catch (const std::exception&) {
            NotifyManager::Send("Config save failed", 220.0f);
            return false;
        }
        catch (...) {
            NotifyManager::Send("Config save failed", 220.0f);
            return false;
        }
    }

    bool apply_config_browser_config(config_browser_ui_state_t& state, const config_browser_entry_t& entry) {
        if (entry.is_cloud_slot) {
            return activate_cloud_config_slot(state, entry);
        }

        const bool had_preview = state.preview_active;
        if (!had_preview || state.preview_is_cloud_slot) {
            state.main_config_snapshot = g_Config.ExportCurrentConfigJsonString();
            if (state.main_config_snapshot.empty()) {
                NotifyManager::Send("Failed to capture main config", 220.0f);
                return false;
            }
        }

        if (!g_Config.ApplyConfigJsonString(entry.config_json)) {
            if (!had_preview) {
                state.main_config_snapshot.clear();
            }
            NotifyManager::Send("Failed to apply config preview", 220.0f);
            return false;
        }

        state.preview_active = true;
        state.preview_is_cloud_slot = false;
        state.preview_config_id = entry.id;
        state.preview_config_name = entry.name;
        NotifyManager::Send("Config preview loaded", 220.0f);
        return true;
    }

    bool restore_config_browser_main(config_browser_ui_state_t& state) {
        if (!state.preview_active || state.main_config_snapshot.empty()) {
            return false;
        }

        if (!g_Config.ApplyConfigJsonString(state.main_config_snapshot)) {
            NotifyManager::Send("Failed to restore main config", 220.0f);
            return false;
        }

        state.preview_active = false;
        state.preview_is_cloud_slot = false;
        state.preview_config_id.clear();
        state.preview_config_name.clear();
        state.main_config_snapshot.clear();
        NotifyManager::Send("Main config restored", 220.0f);
        return true;
    }

    void open_config_exploit_warning(config_browser_ui_state_t& state, const config_browser_entry_t& entry) {
        auto* state_ptr = &state;
        config_browser_entry_t entry_copy = entry;
        widgets->modal.add([state_ptr, entry_copy] {
            const ImVec2 window_pos = GetWindowPos();
            const ImVec2 window_size = GetWindowSize();
            const ImVec2 center{
                window_pos.x + window_size.x * 0.5f,
                window_pos.y + window_size.y * 0.5f
            };

            const char* title = "this config uses exploits";
            const char* description = "Click Okay to continue.";
            const ImVec2 icon_size = g_draw->textsize(icons, 34, I_ALERT__TRIANGLE);
            const ImVec2 title_size = CalcTextSize(title);
            const ImVec2 description_size = CalcTextSize(description);

            g_draw->text(
                icons,
                34,
                { center.x - icon_size.x * 0.5f, center.y - 92 ds },
                g_style->col(pcol_scheme),
                I_ALERT__TRIANGLE
            );

            GetWindowDrawList()->AddText(
                { center.x - title_size.x * 0.5f, center.y - 30 ds },
                g_style->col(pcol_text),
                title
            );

            GetWindowDrawList()->AddText(
                { center.x - description_size.x * 0.5f, center.y - 8 ds },
                g_style->col(pcol_text2),
                description
            );

            const float button_width = 118 ds;
            SetCursorPos({ window_size.x * 0.5f - button_width * 0.5f, window_size.y * 0.5f + 34 ds });
            if (widgets->button("Okay", { button_width, 32 ds })) {
                if (apply_config_browser_config(*state_ptr, entry_copy)) {
                    widgets->modal.close();
                }
            }
        });
    }

    float draw_config_badge(ImDrawList* draw, ImVec2 pos, const std::string& label, pcolor fill_col, pcolor text_col) {
        const float badge_height = 20 ds;
        const float pad_x = 8 ds;
        const ImVec2 text_size = g_draw->textsize(cascadia, 11, label.c_str());
        const float badge_width = text_size.x + pad_x * 2.0f;
        const ImRect bb{ pos, pos + ImVec2{ badge_width, badge_height } };

        draw->AddRectFilled(bb.Min, bb.Max, fill_col, badge_height * 0.5f);
        g_draw->text(cascadia, 11, { bb.Min.x + pad_x, bb.GetCenter().y - text_size.y * 0.5f }, text_col, label.c_str());

        return badge_width;
    }

    void draw_config_badge_wrap(const config_browser_entry_t& entry, float max_width) {
        struct badge_spec_t {
            std::string label;
            pcolor fill;
            pcolor text;
        };

        std::vector<badge_spec_t> badges;
        if (entry.is_active_slot) {
            badges.push_back({ "ACTIVE", g_style->col(pcol_scheme, 0.22f), g_style->col(pcol_scheme) });
        }
        if (entry.is_owned) {
            badges.push_back({ "OWN", g_style->col(pcol_scheme, 0.16f), g_style->col(pcol_scheme) });
        }

        badges.push_back({
            entry.is_private ? "PRIVATE" : "PUBLIC",
            entry.is_private ? g_style->col(pcol_bg4) : g_style->col(pcol_bg3),
            entry.is_private ? g_style->col(pcol_text) : g_style->col(pcol_text2)
            });

        badges.push_back({
            entry.streamproof ? "SP ON" : "SP OFF",
            entry.streamproof ? g_style->col(pcol_scheme, 0.16f) : g_style->col(pcol_bg4),
            entry.streamproof ? g_style->col(pcol_scheme) : g_style->col(pcol_text2)
            });

        if (entry.uses_exploits) {
            badges.push_back({ "EXPLOITS", g_style->col(pcol_scheme, 0.18f), g_style->col(pcol_scheme) });
        }

        if (entry.esp_enabled) {
            badges.push_back({ "ESP", g_style->col(pcol_scheme, 0.10f), g_style->col(pcol_text) });
        }
        if (entry.silentaim_enabled) {
            badges.push_back({ "SILENTAIM", g_style->col(pcol_scheme, 0.10f), g_style->col(pcol_text) });
        }
        if (entry.aimbot_enabled) {
            badges.push_back({ "AIMBOT", g_style->col(pcol_scheme, 0.10f), g_style->col(pcol_text) });
        }
        if (entry.triggerbot_enabled) {
            badges.push_back({ "TRIGGER", g_style->col(pcol_scheme, 0.10f), g_style->col(pcol_text) });
        }

        const ImVec2 origin = GetCursorScreenPos();
        ImVec2 cursor = origin;
        const float badge_gap = 6 ds;
        const float badge_height = 20 ds;
        auto* draw = GetWindowDrawList();

        for (const auto& badge : badges) {
            const ImVec2 text_size = g_draw->textsize(cascadia, 11, badge.label.c_str());
            const float badge_width = text_size.x + 16 ds;

            if (cursor.x > origin.x && cursor.x + badge_width > origin.x + max_width) {
                cursor.x = origin.x;
                cursor.y += badge_height + badge_gap;
            }

            draw_config_badge(draw, cursor, badge.label, badge.fill, badge.text);
            cursor.x += badge_width + badge_gap;
        }

        Dummy(ImVec2{ max_width, (cursor.y - origin.y) + badge_height });
    }

    enum class config_entry_action_t {
        none,
        import_slot,
        save_slot
    };

    config_entry_action_t draw_config_entry_card(const config_browser_entry_t& entry, bool is_previewing) {
        config_entry_action_t action = config_entry_action_t::none;
        const float button_width = entry.is_cloud_slot ? 80 ds : 76 ds;
        const float card_height = entry.is_cloud_slot ? 108 ds : 92 ds;

        PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 12, 10 });
        BeginChild("catalog_entry", { 0, card_height }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            auto* draw = GetWindowDrawList();
            if (is_previewing) {
                draw->AddRectFilled(GetWindowPos(), GetWindowPos() + ImVec2{ 4 ds, GetWindowHeight() }, g_style->col(pcol_scheme), 8 ds, ImDrawFlags_RoundCornersLeft);
            }

            const float content_width = GetContentRegionAvail().x;
            const float text_width = ImMax(100.0f, content_width - button_width - 18 ds);
            const std::string title = ptext{ entry.name }.trim(text_width);
            const std::string meta = entry.author + "  |  " + entry.updated_label + "  |  " + format_config_usage_count(entry.usage_count);
            const std::string trimmed_meta = ptext{ meta }.trim(text_width);

            PushFont(fonts[fontb].get(16));
            TextUnformatted(title.c_str());
            PopFont();

            PushStyleColor(ImGuiCol_Text, g_style->col(pcol_text2).vec4());
            TextUnformatted(trimmed_meta.c_str());
            PopStyleColor();

            widgets->spacing(4 ds);
            draw_config_badge_wrap(entry, text_width);

            const ImVec2 saved_cursor = GetCursorPos();
            const float button_x = GetWindowWidth() - button_width - GImGui->Style.WindowPadding.x;
            SetCursorPos(ImVec2{ button_x, entry.is_cloud_slot ? 18 ds : 24 ds });
            PushStyleVar(ImGuiStyleVar_FrameRounding, 7 ds);
            PushStyleColor(ImGuiCol_Button, is_previewing ? g_style->col(pcol_scheme, 0.28f).vec4() : g_style->col(pcol_bg3).vec4());
            PushStyleColor(ImGuiCol_ButtonHovered, g_style->col(pcol_scheme, 0.18f).vec4());
            PushStyleColor(ImGuiCol_ButtonActive, g_style->col(pcol_scheme, 0.30f).vec4());
            if (widgets->button(entry.is_cloud_slot ? (is_previewing ? "Active" : "Import") : (is_previewing ? "Active" : "Use"), { button_width, 30 ds })) {
                action = config_entry_action_t::import_slot;
            }
            PopStyleColor(3);
            PopStyleVar();

            if (entry.is_cloud_slot) {
                SetCursorPos(ImVec2{ button_x, 54 ds });
                PushStyleVar(ImGuiStyleVar_FrameRounding, 7 ds);
                PushStyleColor(ImGuiCol_Button, g_style->col(pcol_bg3).vec4());
                PushStyleColor(ImGuiCol_ButtonHovered, g_style->col(pcol_scheme, 0.18f).vec4());
                PushStyleColor(ImGuiCol_ButtonActive, g_style->col(pcol_scheme, 0.30f).vec4());
                if (widgets->button("Save", { button_width, 26 ds })) {
                    action = config_entry_action_t::save_slot;
                }
                PopStyleColor(3);
                PopStyleVar();
            }

            SetCursorPos(saved_cursor);
        }
        EndChild();
        PopStyleVar();

        return action;
    }

    [[maybe_unused]] void clear_custom_crosshair_pixels(Gui::Crosshair::custom_pixels_t& pixels) {
        pixels.fill(0);
    }

    [[maybe_unused]] void apply_custom_crosshair_dot_preset(Gui::Crosshair::custom_pixels_t& pixels) {
        clear_custom_crosshair_pixels(pixels);

        constexpr int center = Core::Config::General::CrosshairCustomGridSize / 2;
        for (int row = center - 1; row <= center + 1; ++row) {
            for (int col = center - 1; col <= center + 1; ++col) {
                pixels[static_cast<std::size_t>(row * Core::Config::General::CrosshairCustomGridSize + col)] = 1;
            }
        }
    }

    [[maybe_unused]] void apply_custom_crosshair_plus_preset(Gui::Crosshair::custom_pixels_t& pixels) {
        clear_custom_crosshair_pixels(pixels);

        constexpr int center = Core::Config::General::CrosshairCustomGridSize / 2;
        for (int offset = -4; offset <= 4; ++offset) {
            pixels[static_cast<std::size_t>(center * Core::Config::General::CrosshairCustomGridSize + (center + offset))] = 1;
            pixels[static_cast<std::size_t>((center + offset) * Core::Config::General::CrosshairCustomGridSize + center)] = 1;
        }
    }

    [[maybe_unused]] bool draw_crosshair_style_selector(const char* label, int* current_style) {
        bool changed = false;
        const int active_style = Gui::Crosshair::GetActiveStyle(*current_style);

        TextUnformatted(label);
        widgets->spacing(4 ds);

        PushStyleColor(ImGuiCol_ChildBg, g_style->col(pcol_bg3).vec4());
        PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 4, 4 });
        PushStyleVar(ImGuiStyleVar_ChildRounding, 3 ds);
        BeginChild("##crosshair_style_selector", { 0, 72 ds }, ImGuiChildFlags_Border | ImGuiChildFlags_AlwaysUseWindowPadding);
        {
            const float gap = 6 ds;
            const float item_width = (GetContentRegionAvail().x - gap * 2.0f) / 3.0f;

            PushID("crosshair_style_selector_items");
            for (int i = 0; i < Gui::Crosshair::kStyleCount; ++i) {
                PushID(i);
                if (widgets->selectable(Gui::Crosshair::kStyleLabels[i], active_style == i, { item_width, 28 ds }, { .xpad = 6, .rounding = 3, .checkmark = false })) {
                    *current_style = i;
                    Gui::Crosshair::SetStyleOverride(i);
                    changed = true;
                }
                PopID();

                if ((i % 3) != 2) {
                    SameLine(0, gap);
                }
            }
            PopID();
        }
        EndChild();
        PopStyleVar(2);
        PopStyleColor();

        return changed;
    }

    [[maybe_unused]] bool draw_crosshair_custom_editor(const char* str_id, Gui::Crosshair::custom_pixels_t& pixels) {
        constexpr int grid_size = Core::Config::General::CrosshairCustomGridSize;
        const float cell_size = 16 ds;
        const float spacing = 3 ds;
        const float stride = cell_size + spacing;
        const ImVec2 canvas_size{
            (cell_size * grid_size) + (spacing * (grid_size - 1)),
            (cell_size * grid_size) + (spacing * (grid_size - 1))
        };

        const ImVec2 canvas_pos = GetCursorScreenPos();
        InvisibleButton(str_id, canvas_size);

        const ImRect bb{ canvas_pos, canvas_pos + canvas_size };
        auto* draw = GetWindowDrawList();
        const ImVec2 mouse = GetIO().MousePos;
        const bool hovered = IsItemHovered();
        int hovered_index = -1;
        bool changed = false;

        draw->AddRectFilled(bb.Min, bb.Max, g_style->col(pcol_bg3), 8 ds);
        draw->AddRect(bb.Min, bb.Max, g_style->col(pcol_border), 8 ds);

        for (int row = 0; row < grid_size; ++row) {
            for (int col = 0; col < grid_size; ++col) {
                const int index = row * grid_size + col;
                const ImVec2 cell_min = bb.Min + ImVec2{ col * stride, row * stride };
                const ImRect cell_bb{ cell_min, cell_min + ImVec2{ cell_size, cell_size } };

                if (cell_bb.Contains(mouse)) {
                    hovered_index = index;
                }

                const bool center_axis = row == grid_size / 2 || col == grid_size / 2;
                const pcolor fill_color = pixels[static_cast<std::size_t>(index)]
                    ? g_style->col(pcol_scheme, 0.95f)
                    : g_style->col(center_axis ? pcol_bg4 : pcol_bg3);
                const pcolor border_color = hovered_index == index ? g_style->col(pcol_text, 0.65f) : g_style->col(pcol_border, 0.9f);

                draw->AddRectFilled(cell_bb.Min, cell_bb.Max, fill_color, 4 ds);
                draw->AddRect(cell_bb.Min, cell_bb.Max, border_color, 4 ds);
            }
        }

        if (hovered && hovered_index >= 0) {
            const std::size_t pixel_index = static_cast<std::size_t>(hovered_index);

            if (IsMouseDown(ImGuiMouseButton_Left) && !pixels[pixel_index]) {
                pixels[pixel_index] = 1;
                changed = true;
            }

            if (IsMouseDown(ImGuiMouseButton_Right) && pixels[pixel_index]) {
                pixels[pixel_index] = 0;
                changed = true;
            }
        }

        return changed;
    }

    void draw_crosshair_preview_canvas(
        const char* str_id,
        const ImVec2& size,
        const int style,
        const float crosshair_size,
        const float thickness,
        const float roundness,
        const ImU32 main_color,
        const ImU32 accent_color,
        const Gui::Crosshair::custom_pixels_t& custom_pixels
    ) {
        const ImVec2 canvas_pos = GetCursorScreenPos();
        InvisibleButton(str_id, size);

        const ImRect bb{ canvas_pos, canvas_pos + size };
        auto* draw = GetWindowDrawList();
        const ImVec2 center = bb.GetCenter();
        const float preview_padding = 18 ds;
        float preview_size = crosshair_size * 1.55f;
        float preview_thickness = thickness * 1.2f;
        const ImVec2 extent = Gui::Crosshair::MeasureHalfExtent(style, preview_size, preview_thickness, custom_pixels);
        const float fit_scale_x = extent.x > 0.0f ? (bb.GetWidth() * 0.5f - preview_padding) / extent.x : 1.0f;
        const float fit_scale_y = extent.y > 0.0f ? (bb.GetHeight() * 0.5f - preview_padding) / extent.y : 1.0f;
        const float fit_scale = ImClamp(ImMin(ImMin(fit_scale_x, fit_scale_y), 1.0f), 0.1f, 1.0f);

        preview_size *= fit_scale;
        preview_thickness = ImMax(1.0f, preview_thickness * fit_scale);

        draw->AddRectFilled(bb.Min, bb.Max, g_style->col(pcol_bg3), 8 ds);
        draw->AddRect(bb.Min, bb.Max, g_style->col(pcol_border), 8 ds);
        draw->AddLine({ bb.Min.x + 18 ds, center.y }, { bb.Max.x - 18 ds, center.y }, g_style->col(pcol_border, 0.45f));
        draw->AddLine({ center.x, bb.Min.y + 18 ds }, { center.x, bb.Max.y - 18 ds }, g_style->col(pcol_border, 0.45f));
        draw->AddCircle(center, 2.5f, g_style->col(pcol_text2, 0.5f), 16, 1.0f);

        Gui::Crosshair::Draw(
            draw,
            center,
            style,
            preview_size,
            preview_thickness,
            roundness,
            main_color,
            accent_color,
            custom_pixels
        );
    }
}

void c_slate::initialize_fonts() {
    fonts.resize(fonts_size);
    fonts.at(font).setup(fontbinary, sizeof(fontbinary),
        { 16, 12, 17, 12, 10, 14 },
        GetIO().Fonts->GetGlyphRangesCyrillic());

    fonts.at(fontb).setup(fontboldbinary, sizeof(fontboldbinary),
        { 18, 16, 12 },
        GetIO().Fonts->GetGlyphRangesCyrillic());

    fonts.at(fonth).setup(fontheavybinary, sizeof(fontheavybinary),
        { 16, 90 },
        GetIO().Fonts->GetGlyphRangesCyrillic());

    fonts.at(cascadia).setup(cascadiamonobinary, sizeof(cascadiamonobinary),
        { 13, 12, 11 },
        GetIO().Fonts->GetGlyphRangesCyrillic());

    const static ImWchar icons_ranges[] = { 0x1 + 1000, 0x1 + 5881, 0 };
    fonts.at(icons).setup(iconsbinary, sizeof(iconsbinary),
        { 34, 20, 18, 16, 14, 12, 17 },
        icons_ranges);
}

void c_slate::initialize(ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swapchain) {
    GImGui->IO.IniFilename = "";
    g_style->dpi_scale = 1;
    sync_configured_menu_theme_base();
    g_style->setup();
    apply_configured_menu_theme();
    g_lang->initialize();

    initialize_fonts();

    load_menu_logo_texture(device, menu_season_t::Spring);
    load_menu_logo_texture(device, menu_season_t::Summer);
    load_menu_logo_texture(device, menu_season_t::Autumn);
    load_menu_logo_texture(device, menu_season_t::Winter);
    load_menu_logo_texture(device, menu_season_t::Christmas);
    load_menu_logo_texture(device, menu_season_t::Easter);
    ped_hitbox_photo.load(device, pedhitbox, sizeof(pedhitbox));

    for (int i = 0; i < setup_animation::frame_count; ++i) {
        const auto& frame = setup_animation::frames[i];
        setup_frames[i].load(device, const_cast<unsigned char*>(setup_animation::blob + frame.offset), frame.size);
    }
    setup_frames_loaded = true;

    widgets->nav.tabs = {
        { I_CURRENT__LOCATION, "Combat", { { I_SETTINGS, "Aimbot" }, { I_SWORD, "SilentAim" }, { I_DOTS, "TriggerBot" } } },
        { I_EYE__COG, "Visuals", {  } },
        { I_USER__COG, "Player List", { { I_USER, "Player List" }, { I_HEART, "Friend List" } } },
        { I_CAR, "Vehicle List", {  } },
        { I_FOLDER__COG, "Configs", {  } },
        { I_WAVE__SAW__TOOL, "Settings", { { I_SETTINGS, "General" } } },
    };

    auto begin_placeholder_page = [&](const char* icon, const char* title, const char* description) {
        widgets->child.begin(icon, title, 1, { 0, 190 ds });
        TextWrapped("%s", description);
        widgets->spacing(8 ds);
        widgets->separator();
        widgets->spacing(8 ds);
        };

    widgets->nav.addpage(0, [&] {
        static bool aimbot_hitboxes[6];
        aimbot_hitboxes[0] = g_Config.Aimbot->BoneGroup_Head;
        aimbot_hitboxes[1] = g_Config.Aimbot->BoneGroup_Neck;
        aimbot_hitboxes[2] = g_Config.Aimbot->BoneGroup_Chest;
        aimbot_hitboxes[3] = g_Config.Aimbot->BoneGroup_Arms;
        aimbot_hitboxes[4] = g_Config.Aimbot->BoneGroup_Legs;
        aimbot_hitboxes[5] = g_Config.Aimbot->BoneGroup_Feet;

        float aimbot_fov_color[4] = {
            g_Config.Aimbot->FovColor.Value.x,
            g_Config.Aimbot->FovColor.Value.y,
            g_Config.Aimbot->FovColor.Value.z,
            g_Config.Aimbot->FovColor.Value.w
        };
        float aimbot_fov_fill_color[4] = {
            g_Config.Aimbot->FovFillColor.Value.x,
            g_Config.Aimbot->FovFillColor.Value.y,
            g_Config.Aimbot->FovFillColor.Value.z,
            g_Config.Aimbot->FovFillColor.Value.w
        };

        int aimbot_key_mode = g_Config.Aimbot->KeyBindMode == 1 ? 1 : 0;

        BeginGroup();
        {
            widgets->child.begin(I_SETTINGS, "Aimbot", 1);
            {
                widgets->checkbox(I_SPARKLES, "Enable", &g_Config.Aimbot->Enabled);
                widgets->checkbox(I_BACKGROUND, "Show Fov", &g_Config.Aimbot->ShowFov);
                widgets->checkbox(I_VECTOR__SPLINE, "Prediction", &g_Config.Aimbot->Prediction);
                widgets->checkbox(I_EYE__COG, "Only Visible", &g_Config.Aimbot->OnlyVisible);
                widgets->checkbox(I_USER__COG, "Ignore NPCs", &g_Config.Aimbot->IgnoreNPCs);
                widgets->checkbox(I_MOOD__SILENCE, "Ignore Dead", &g_Config.Aimbot->IgnoreDead);
                //widgets->checkbox(I_MOOD__SILENCE, "Ignore GOD (BETA)", &g_Config.Aimbot->ExcludeGod);
                widgets->binder(I_COMMAND, "Keybind", &g_Config.Aimbot->KeyBind);
                widgets->selector("Keybind Mode", &aimbot_key_mode, { "Toggle", "Hold" });
            }
            widgets->child.end();
        }
        EndGroup();

        SameLine(0, 8.f ds);
        BeginGroup();
        {
            widgets->child.begin(I_ADJUSTMENTS__HORIZONTAL, "Settings", 2);
            {
                widgets->sliderint(I_GRADIENTER, "FOV X", &g_Config.Aimbot->FOVX, 0, 1000, "%d");
                widgets->sliderint(I_GRADIENTER, "FOV Y", &g_Config.Aimbot->FOVY, 0, 1000, "%d");
                widgets->sliderint(I_CUBE__3D__SPHERE, "Aimbot Speed", &g_Config.Aimbot->AimbotSpeed, 0, 100, "%d");
                //widgets->sliderint(I_VECTOR__SPLINE, "Max Distance", &g_Config.Aimbot->MaxDistance, 0, 1000, "%dm");
                widgets->sliderfloat(I_RULER__MEASURE, "Fov Thickness", &g_Config.Aimbot->FovThickness, 0.5f, 8.0f, "%.1fpx");
                widgets->checkbox(I_VECTOR__SPLINE, "Respect Max Range", &g_Config.Aimbot->RespectMaxRange);
                widgets->checkbox(I_BACKGROUND, "Fill Fov", &g_Config.Aimbot->FovFilled);
                widgets->coloredit(I_PALETTE, "Fov Color", aimbot_fov_color);
                widgets->coloredit(I_PALETTE, "Fov Fill Color", aimbot_fov_fill_color);
            }
            widgets->child.end();
        }
        EndGroup();

        g_Config.Aimbot->BoneGroup_Head = aimbot_hitboxes[0];
        g_Config.Aimbot->BoneGroup_Neck = aimbot_hitboxes[1];
        g_Config.Aimbot->BoneGroup_Chest = aimbot_hitboxes[2];
        g_Config.Aimbot->BoneGroup_Arms = aimbot_hitboxes[3];
        g_Config.Aimbot->BoneGroup_Legs = aimbot_hitboxes[4];
        g_Config.Aimbot->BoneGroup_Feet = aimbot_hitboxes[5];

        g_Config.Aimbot->KeyBindMode = aimbot_key_mode == 1 ? 1 : 0;

        g_Config.Aimbot->CurveAccel = ImClamp(g_Config.Aimbot->CurveAccel, 0.2f, 8.0f);
        g_Config.Aimbot->CurveDecel = ImClamp(g_Config.Aimbot->CurveDecel, 0.2f, 8.0f);

        g_Config.Aimbot->FovColor.Value.x = aimbot_fov_color[0];
        g_Config.Aimbot->FovColor.Value.y = aimbot_fov_color[1];
        g_Config.Aimbot->FovColor.Value.z = aimbot_fov_color[2];
        g_Config.Aimbot->FovColor.Value.w = aimbot_fov_color[3];
        g_Config.Aimbot->FovFillColor.Value.x = aimbot_fov_fill_color[0];
        g_Config.Aimbot->FovFillColor.Value.y = aimbot_fov_fill_color[1];
        g_Config.Aimbot->FovFillColor.Value.z = aimbot_fov_fill_color[2];
        g_Config.Aimbot->FovFillColor.Value.w = aimbot_fov_fill_color[3];
        });

    widgets->nav.addpage(0, [&] {
        static bool silentaim_hitboxes[7];
        silentaim_hitboxes[0] = g_Config.SilentAim->BoneGroup_Head;
        silentaim_hitboxes[1] = g_Config.SilentAim->BoneGroup_Neck;
        silentaim_hitboxes[2] = g_Config.SilentAim->BoneGroup_Chest;
        silentaim_hitboxes[3] = g_Config.SilentAim->BoneGroup_Arms;
        silentaim_hitboxes[4] = g_Config.SilentAim->BoneGroup_Legs;
        silentaim_hitboxes[5] = g_Config.SilentAim->BoneGroup_Feet;
        silentaim_hitboxes[6] = g_Config.SilentAim->BoneGroup_Wheels;

        float silentaim_fov_color[4] = {
            g_Config.SilentAim->FovColor.Value.x,
            g_Config.SilentAim->FovColor.Value.y,
            g_Config.SilentAim->FovColor.Value.z,
            g_Config.SilentAim->FovColor.Value.w
        };
        float silentaim_fov_fill_color[4] = {
            g_Config.SilentAim->FovFillColor.Value.x,
            g_Config.SilentAim->FovFillColor.Value.y,
            g_Config.SilentAim->FovFillColor.Value.z,
            g_Config.SilentAim->FovFillColor.Value.w
        };

        int silentaim_key_mode = g_Config.SilentAim->KeyBindMode == 1 ? 1 : 0;

        BeginGroup();
        {
            widgets->child.begin(I_SWORD, "SilentAim", 1);
            {
                widgets->checkbox(I_SPARKLES, "Enable", &g_Config.SilentAim->Enabled);
                widgets->checkbox(I_BACKGROUND, "Show Fov", &g_Config.SilentAim->ShowFov);
                widgets->checkbox(I_BACKGROUND, "Prediction", &g_Config.SilentAim->Prediction);
                widgets->checkbox(I_MOOD__SILENCE, "PRUDA Silent Bypass", &g_Config.SilentAim->SilentBypass);
                widgets->spacing(6 ds);
                widgets->separator();
                widgets->spacing(6 ds);
                widgets->checkbox(I_EYE__COG, "Finalcity Silent Bypass 2", &g_Config.SilentAim->BypassPassive);
                widgets->checkbox(I_EYE__COG, "Only Visible", &g_Config.SilentAim->OnlyVisible);
                widgets->checkbox(I_USER__COG, "Ignore NPCs", &g_Config.SilentAim->IgnoreNPCs);
                widgets->checkbox(I_MOOD__SILENCE, "Ignore Death", &g_Config.SilentAim->IgnoreDead);
                //widgets->checkbox(I_MOOD__SILENCE, "Ignore GOD (BETA)", &g_Config.SilentAim->ExcludeGod);
                widgets->binder(I_COMMAND, "Keybind", &g_Config.SilentAim->KeyBind);
                widgets->selector("Keybind Mode", &silentaim_key_mode, { "Toggle", "Hold" });
            }
            widgets->child.end();
        }
        EndGroup();

        SameLine(0, 8.f ds);
        BeginGroup();
        {
            widgets->child.begin(I_ADJUSTMENTS__HORIZONTAL, "Settings", 2);
            {
                widgets->sliderint(I_GRADIENTER, "FOV X", &g_Config.SilentAim->FOVX, 0, 1000, "%d");
                widgets->sliderint(I_GRADIENTER, "FOV Y", &g_Config.SilentAim->FOVY, 0, 1000, "%d");
                widgets->sliderint(I_CUBE__3D__SPHERE, "Misschance", &g_Config.SilentAim->MissChance, 0, 100, "%d%%");
                //widgets->sliderint(I_VECTOR__SPLINE, "Max Distance", &g_Config.SilentAim->MaxDistance, 0, 1000, "%dm");
                widgets->sliderfloat(I_RULER__MEASURE, "Fov Thickness", &g_Config.SilentAim->FovThickness, 0.5f, 8.0f, "%.1fpx");
                widgets->checkbox(I_VECTOR__SPLINE, "Respect Max Range", &g_Config.SilentAim->RespectMaxRange);
                widgets->checkbox(I_BACKGROUND, "Fill Fov", &g_Config.SilentAim->FovFilled);
                widgets->coloredit(I_PALETTE, "Fov Color", silentaim_fov_color);
                widgets->coloredit(I_PALETTE, "Fov Fill Color", silentaim_fov_fill_color);
            }
            widgets->child.end();
        }
        EndGroup();

        g_Config.SilentAim->BoneGroup_Head = silentaim_hitboxes[0];
        g_Config.SilentAim->BoneGroup_Neck = silentaim_hitboxes[1];
        g_Config.SilentAim->BoneGroup_Chest = silentaim_hitboxes[2];
        g_Config.SilentAim->BoneGroup_Arms = silentaim_hitboxes[3];
        g_Config.SilentAim->BoneGroup_Legs = silentaim_hitboxes[4];
        g_Config.SilentAim->BoneGroup_Feet = silentaim_hitboxes[5];
        g_Config.SilentAim->BoneGroup_Wheels = silentaim_hitboxes[6];

        g_Config.SilentAim->KeyBindMode = silentaim_key_mode == 1 ? 1 : 0;

        g_Config.SilentAim->FovColor.Value.x = silentaim_fov_color[0];
        g_Config.SilentAim->FovColor.Value.y = silentaim_fov_color[1];
        g_Config.SilentAim->FovColor.Value.z = silentaim_fov_color[2];
        g_Config.SilentAim->FovColor.Value.w = silentaim_fov_color[3];
        g_Config.SilentAim->FovFillColor.Value.x = silentaim_fov_fill_color[0];
        g_Config.SilentAim->FovFillColor.Value.y = silentaim_fov_fill_color[1];
        g_Config.SilentAim->FovFillColor.Value.z = silentaim_fov_fill_color[2];
        g_Config.SilentAim->FovFillColor.Value.w = silentaim_fov_fill_color[3];
        });

    widgets->nav.addpage(0, [&] {
        float triggerbot_fov_color[4] = {
            g_Config.TriggerBot->FovColor.Value.x,
            g_Config.TriggerBot->FovColor.Value.y,
            g_Config.TriggerBot->FovColor.Value.z,
            g_Config.TriggerBot->FovColor.Value.w
        };
        float triggerbot_fov_fill_color[4] = {
            g_Config.TriggerBot->FovFillColor.Value.x,
            g_Config.TriggerBot->FovFillColor.Value.y,
            g_Config.TriggerBot->FovFillColor.Value.z,
            g_Config.TriggerBot->FovFillColor.Value.w
        };

        int triggerbot_key_mode = g_Config.TriggerBot->KeyBindMode == 1 ? 1 : 0;

        BeginGroup();
        {
            widgets->child.begin(I_DOTS, "TriggerBot", 1);
            {
                widgets->checkbox(I_SPARKLES, "Enable", &g_Config.TriggerBot->Enabled);

                widgets->checkbox(I_USER__COG, "Ignore NPCs", &g_Config.TriggerBot->IgnoreNPCs);
                widgets->checkbox(I_MOOD__SILENCE, "Ignore Death", &g_Config.TriggerBot->IgnoreDead);
                //widgets->checkbox(I_MOOD__SILENCE, "Ignore GOD (BETA)", &g_Config.TriggerBot->ExcludeGod);
                widgets->sliderint(I_CLOCK__HOUR__3, "Delay", &g_Config.TriggerBot->Delay, 0, 10, "%dms");
                widgets->binder(I_COMMAND, "Keybind", &g_Config.TriggerBot->KeyBind);
                widgets->selector("Keybind Mode", &triggerbot_key_mode, { "Toggle", "Hold" });
            }
            widgets->child.end();
        }
        EndGroup();

        SameLine(0, 8.f ds);
        BeginGroup();
        {
            widgets->child.begin(I_ADJUSTMENTS__HORIZONTAL, "Settings", 2);
            {
                if (!g_Config.TriggerBot->SmartTrigger)
                    widgets->sliderint(I_GRADIENTER, "FOV X", &g_Config.TriggerBot->FOVX, 0, 1000, "%d");
                    widgets->sliderint(I_GRADIENTER, "FOV Y", &g_Config.TriggerBot->FOVY, 0, 1000, "%d");

                //widgets->sliderint(I_VECTOR__SPLINE, "Max Distance", &g_Config.TriggerBot->MaxDistance, 0, 1000, "%dm");
                widgets->checkbox(I_VECTOR__SPLINE, "Respect Max Range", &g_Config.TriggerBot->RespectMaxRange);
                if (!g_Config.TriggerBot->SmartTrigger) {
                    widgets->sliderfloat(I_RULER__MEASURE, "Fov Thickness", &g_Config.TriggerBot->FovThickness, 0.5f, 8.0f, "%.1fpx");
                    widgets->checkbox(I_BACKGROUND, "Fill Fov", &g_Config.TriggerBot->FovFilled);
                    widgets->coloredit(I_PALETTE, "Fov Color", triggerbot_fov_color);
                    widgets->coloredit(I_PALETTE, "Fov Fill Color", triggerbot_fov_fill_color);
                }
            }
            widgets->child.end();
        }
        EndGroup();

        g_Config.TriggerBot->KeyBindMode = triggerbot_key_mode == 1 ? 1 : 0;

        g_Config.TriggerBot->FovColor.Value.x = triggerbot_fov_color[0];
        g_Config.TriggerBot->FovColor.Value.y = triggerbot_fov_color[1];
        g_Config.TriggerBot->FovColor.Value.z = triggerbot_fov_color[2];
        g_Config.TriggerBot->FovColor.Value.w = triggerbot_fov_color[3];
        g_Config.TriggerBot->FovFillColor.Value.x = triggerbot_fov_fill_color[0];
        g_Config.TriggerBot->FovFillColor.Value.y = triggerbot_fov_fill_color[1];
        g_Config.TriggerBot->FovFillColor.Value.z = triggerbot_fov_fill_color[2];
        g_Config.TriggerBot->FovFillColor.Value.w = triggerbot_fov_fill_color[3];
        });

    widgets->nav.addpage(1, [&] {
        float esp_skeleton_color[4] = {
            g_Config.ESP->SkeletonCol.Value.x,
            g_Config.ESP->SkeletonCol.Value.y,
            g_Config.ESP->SkeletonCol.Value.z,
            g_Config.ESP->SkeletonCol.Value.w
        };

        float esp_friend_color[4] = {
            g_Config.ESP->FriendCol.Value.x,
            g_Config.ESP->FriendCol.Value.y,
            g_Config.ESP->FriendCol.Value.z,
            g_Config.ESP->FriendCol.Value.w
        };
        float esp_trueinvisible_color[4] = {
            g_Config.ESP->isTrueInvisibleCol.Value.x,
            g_Config.ESP->isTrueInvisibleCol.Value.y,
            g_Config.ESP->isTrueInvisibleCol.Value.z,
            g_Config.ESP->isTrueInvisibleCol.Value.w
        };
        float esp_visible_color[4] = {
            g_Config.ESP->VisibleCol.Value.x,
            g_Config.ESP->VisibleCol.Value.y,
            g_Config.ESP->VisibleCol.Value.z,
            g_Config.ESP->VisibleCol.Value.w
        };

        float esp_invisible_color[4] = {
            g_Config.ESP->InVisibleCol.Value.x,
            g_Config.ESP->InVisibleCol.Value.y,
            g_Config.ESP->InVisibleCol.Value.z,
            g_Config.ESP->InVisibleCol.Value.w
        };

        float esp_playernames_color[4] = {
            g_Config.ESP->UserNamesCol.Value.x,
            g_Config.ESP->UserNamesCol.Value.y,
            g_Config.ESP->UserNamesCol.Value.z,
            g_Config.ESP->UserNamesCol.Value.w
        };

        float esp_weaponnames_color[4] = {
            g_Config.ESP->WeaponNameCol.Value.x,
            g_Config.ESP->WeaponNameCol.Value.y,
            g_Config.ESP->WeaponNameCol.Value.z,
            g_Config.ESP->WeaponNameCol.Value.w
        };

        float esp_distance_color[4] = {
            g_Config.ESP->DistanceCol.Value.x,
            g_Config.ESP->DistanceCol.Value.y,
            g_Config.ESP->DistanceCol.Value.z,
            g_Config.ESP->DistanceCol.Value.w
        };
        float esp_health_gradient_start[4] = {
            g_Config.ESP->HealthBarGradientStart.Value.x,
            g_Config.ESP->HealthBarGradientStart.Value.y,
            g_Config.ESP->HealthBarGradientStart.Value.z,
            g_Config.ESP->HealthBarGradientStart.Value.w
        };
        float esp_health_gradient_end[4] = {
            g_Config.ESP->HealthBarGradientEnd.Value.x,
            g_Config.ESP->HealthBarGradientEnd.Value.y,
            g_Config.ESP->HealthBarGradientEnd.Value.z,
            g_Config.ESP->HealthBarGradientEnd.Value.w
        };
        float esp_armor_gradient_start[4] = {
            g_Config.ESP->ArmorBarGradientStart.Value.x,
            g_Config.ESP->ArmorBarGradientStart.Value.y,
            g_Config.ESP->ArmorBarGradientStart.Value.z,
            g_Config.ESP->ArmorBarGradientStart.Value.w
        };
        float esp_armor_gradient_end[4] = {
            g_Config.ESP->ArmorBarGradientEnd.Value.x,
            g_Config.ESP->ArmorBarGradientEnd.Value.y,
            g_Config.ESP->ArmorBarGradientEnd.Value.z,
            g_Config.ESP->ArmorBarGradientEnd.Value.w
        };

        BeginGroup();
        {
            widgets->child.begin(I_EYE__COG, "Visuals", 1, { 0, 348 ds });
            {
                widgets->checkbox(I_SPARKLES, "Enable", &g_Config.ESP->Enabled);
                if (current_account_has_staff_tools()) {
                    widgets->checkbox(I_WAND, "WIZARD", &wizard_visuals_enabled);
                    Core::Features::WizardVisuals::SetEnabled(wizard_visuals_enabled);
                    if (wizard_visuals_enabled) {
                        request_wizard_names_refresh();
                    }
                }
                else {
                    wizard_visuals_enabled = false;
                    Core::Features::WizardVisuals::SetEnabled(false);
                }
                widgets->checkbox(I_USER__OFF, "Ignore Self", &g_Config.ESP->IgnoreSelf);
                widgets->checkbox(I_USER, "Player Names", &g_Config.ESP->UserNames);
                widgets->checkbox(I_SWORD, "Weapon Names", &g_Config.ESP->WeaponName);
                if (g_Config.ESP->WeaponName) {
                    widgets->checkbox(I_SWORD, "Ignore Unarmed", &g_Config.ESP->IgnoreUnarmed);
                }
                widgets->checkbox(I_VECTOR__SPLINE, "Distance", &g_Config.ESP->DistanceFromMe);
                widgets->checkbox(I_BONE, "Skeleton", &g_Config.ESP->Skeleton);
                widgets->checkbox(I_BONE, "Targets", &g_Config.ESP->Targets);
                widgets->checkbox(I_BONE, "Show Passive", &g_Config.ESP->ShowPassive);
                widgets->checkbox(I_BOX__ALIGN__TOP, "Box", &g_Config.ESP->Box);
                widgets->checkbox(I_HEART, "Health Bar", &g_Config.ESP->HealthBar);
                if (g_Config.ESP->HealthBar) {
                    widgets->checkbox(I_GRADIENTER, "Health Gradient", &g_Config.ESP->HealthBarGradient);
                }
                widgets->checkbox(I_SHIELD, "Armor Bar", &g_Config.ESP->ArmorBar);
                if (g_Config.ESP->ArmorBar) {
                    widgets->checkbox(I_GRADIENTER, "Armor Gradient", &g_Config.ESP->ArmorBarGradient);
                }
                widgets->checkbox(I_USER__COG, "Ignore NPCs", &g_Config.ESP->IgnoreNPCs);
                widgets->checkbox(I_MOOD__SILENCE, "Ignore Death", &g_Config.ESP->IgnoreDead);
            }
            widgets->child.end();
        }
        EndGroup();

        SameLine(0, 8.f ds);
        BeginGroup();
        {
            widgets->child.begin(I_PALETTE, "Visual Colors", 2, { 0, 348 ds });
            {
                widgets->sliderint(I_GRADIENTER, "Max Distance", &g_Config.ESP->MaxDistance, 0, 1000, "%dm");
                widgets->sliderfloat(I_BONE, "Skeleton Thickness", &g_Config.ESP->SkeletonThickness, 0.5f, 5.0f, "%.1fpx");
                widgets->sliderfloat(I_TEXT__SIZE, "Text Size", &g_Config.ESP->TextSize, 0.50f, 2.5f, "%.2fx");
                widgets->sliderfloat(I_RULER__MEASURE, "Text / Meter", &g_Config.ESP->TextDistanceMultiplier, 0.0f, 0.005f, "%.4fx/m");
                widgets->coloredit(I_BONE, "Skeleton Color", esp_skeleton_color);
                widgets->coloredit(I_EYE, "Friend Color", esp_friend_color);
                widgets->coloredit(I_EYE, "True Invisible Color", esp_trueinvisible_color);
                widgets->coloredit(I_EYE, "Visible Color", esp_visible_color);
                widgets->coloredit(I_EYE__OFF, "Invisible Color", esp_invisible_color);
                widgets->coloredit(I_USER, "Player Names Color", esp_playernames_color);
                widgets->coloredit(I_SWORD, "Weapon Names Color", esp_weaponnames_color);
                widgets->coloredit(I_VECTOR__SPLINE, "Distance Color", esp_distance_color);
                if (g_Config.ESP->HealthBarGradient) {
                    widgets->coloredit(I_HEART, "Health Gradient 1", esp_health_gradient_start);
                    widgets->coloredit(I_HEART, "Health Gradient 2", esp_health_gradient_end);
                }
                if (g_Config.ESP->ArmorBarGradient) {
                    widgets->coloredit(I_SHIELD, "Armor Gradient 1", esp_armor_gradient_start);
                    widgets->coloredit(I_SHIELD, "Armor Gradient 2", esp_armor_gradient_end);
                }
            }
            widgets->child.end();
        }
        EndGroup();

        widgets->spacing(8 ds);
        BeginGroup();
        {
            widgets->child.begin(I_BOX__ALIGN__TOP, "ESP Style", 1, { 0, 208 ds });
            {
                widgets->checkbox(I_BOX__ALIGN__TOP, "Clean Box", &g_Config.ESP->CleanBox);
                widgets->checkbox(I_BOX__ALIGN__TOP, "Filled Box", &g_Config.ESP->FilledBox);
                widgets->checkbox(I_TEXT__SIZE, "Name Backdrop", &g_Config.ESP->NameBackdrop);
                widgets->checkbox(I_VECTOR__SPLINE, "Snaplines", &g_Config.ESP->SnapLines);
                widgets->checkbox(I_BONE, "Head Circle", &g_Config.ESP->HeadCircle);
                widgets->spacing(4 ds);
                PushStyleColor(ImGuiCol_Text, g_style->col(pcol_text2).vec4());
                TextWrapped("Clean Box draws corner brackets instead of a full frame.");
                PopStyleColor();
            }
            widgets->child.end();
        }
        EndGroup();

        g_Config.ESP->SkeletonCol.Value.x = esp_skeleton_color[0];
        g_Config.ESP->SkeletonCol.Value.y = esp_skeleton_color[1];
        g_Config.ESP->SkeletonCol.Value.z = esp_skeleton_color[2];
        g_Config.ESP->SkeletonCol.Value.w = esp_skeleton_color[3];

        g_Config.ESP->isTrueInvisibleCol.Value.x = esp_trueinvisible_color[0];
        g_Config.ESP->isTrueInvisibleCol.Value.y = esp_trueinvisible_color[1];
        g_Config.ESP->isTrueInvisibleCol.Value.z = esp_trueinvisible_color[2];
        g_Config.ESP->isTrueInvisibleCol.Value.w = esp_trueinvisible_color[3];

        g_Config.ESP->VisibleCol.Value.x = esp_visible_color[0];
        g_Config.ESP->VisibleCol.Value.y = esp_visible_color[1];
        g_Config.ESP->VisibleCol.Value.z = esp_visible_color[2];
        g_Config.ESP->VisibleCol.Value.w = esp_visible_color[3];

        g_Config.ESP->InVisibleCol.Value.x = esp_invisible_color[0];
        g_Config.ESP->InVisibleCol.Value.y = esp_invisible_color[1];
        g_Config.ESP->InVisibleCol.Value.z = esp_invisible_color[2];
        g_Config.ESP->InVisibleCol.Value.w = esp_invisible_color[3];

        g_Config.ESP->FriendCol.Value.x = esp_friend_color[0];
        g_Config.ESP->FriendCol.Value.y = esp_friend_color[1];
        g_Config.ESP->FriendCol.Value.z = esp_friend_color[2];
        g_Config.ESP->FriendCol.Value.w = esp_friend_color[3];

        g_Config.ESP->UserNamesCol.Value.x = esp_playernames_color[0];
        g_Config.ESP->UserNamesCol.Value.y = esp_playernames_color[1];
        g_Config.ESP->UserNamesCol.Value.z = esp_playernames_color[2];
        g_Config.ESP->UserNamesCol.Value.w = esp_playernames_color[3];

        g_Config.ESP->WeaponNameCol.Value.x = esp_weaponnames_color[0];
        g_Config.ESP->WeaponNameCol.Value.y = esp_weaponnames_color[1];
        g_Config.ESP->WeaponNameCol.Value.z = esp_weaponnames_color[2];
        g_Config.ESP->WeaponNameCol.Value.w = esp_weaponnames_color[3];

        g_Config.ESP->DistanceCol.Value.x = esp_distance_color[0];
        g_Config.ESP->DistanceCol.Value.y = esp_distance_color[1];
        g_Config.ESP->DistanceCol.Value.z = esp_distance_color[2];
        g_Config.ESP->DistanceCol.Value.w = esp_distance_color[3];

        g_Config.ESP->HealthBarGradientStart.Value.x = esp_health_gradient_start[0];
        g_Config.ESP->HealthBarGradientStart.Value.y = esp_health_gradient_start[1];
        g_Config.ESP->HealthBarGradientStart.Value.z = esp_health_gradient_start[2];
        g_Config.ESP->HealthBarGradientStart.Value.w = esp_health_gradient_start[3];

        g_Config.ESP->HealthBarGradientEnd.Value.x = esp_health_gradient_end[0];
        g_Config.ESP->HealthBarGradientEnd.Value.y = esp_health_gradient_end[1];
        g_Config.ESP->HealthBarGradientEnd.Value.z = esp_health_gradient_end[2];
        g_Config.ESP->HealthBarGradientEnd.Value.w = esp_health_gradient_end[3];

        g_Config.ESP->ArmorBarGradientStart.Value.x = esp_armor_gradient_start[0];
        g_Config.ESP->ArmorBarGradientStart.Value.y = esp_armor_gradient_start[1];
        g_Config.ESP->ArmorBarGradientStart.Value.z = esp_armor_gradient_start[2];
        g_Config.ESP->ArmorBarGradientStart.Value.w = esp_armor_gradient_start[3];

        g_Config.ESP->ArmorBarGradientEnd.Value.x = esp_armor_gradient_end[0]; 
        g_Config.ESP->ArmorBarGradientEnd.Value.y = esp_armor_gradient_end[1];
        g_Config.ESP->ArmorBarGradientEnd.Value.z = esp_armor_gradient_end[2];
        g_Config.ESP->ArmorBarGradientEnd.Value.w = esp_armor_gradient_end[3];
        });

    widgets->nav.addpage(2, [&] {
        static player_directory_ui_state_t player_list_ui_state;
        draw_player_directory_page(false, player_list_ui_state);
        });

    widgets->nav.addpage(2, [&] {
        static player_directory_ui_state_t friend_list_ui_state;
        draw_player_directory_page(true, friend_list_ui_state);
        });

    widgets->nav.addpage(3, [&] {
        static char vehicle_search[64]{};
        static uintptr_t selected_vehicle_ptr = 0;

        const auto all_vehicles = collect_vehicle_directory_entries();
        auto filtered_vehicles = all_vehicles;
        filter_vehicle_directory_entries(filtered_vehicles, vehicle_search);

        const int selected_index = find_vehicle_selection_index(all_vehicles, selected_vehicle_ptr);
        Core::SDK::Game::VehicleStructure selected_vehicle{};
        const bool has_selected_vehicle = selected_index >= 0;

        if (has_selected_vehicle) {
            selected_vehicle = all_vehicles[selected_index];
        }
        else {
            selected_vehicle_ptr = 0;
        }

        BeginGroup();
        {
            widgets->child.begin(I_CAR, "Vehicles", 1, { 318 ds, 414 ds });
            {
                widgets->textinput("##vehicle_directory_search", vehicle_search, sizeof(vehicle_search), { .icon = I_SEARCH, .hint = "Search vehicles..." });
                widgets->spacing(6 ds);

                if (filtered_vehicles.empty()) {
                    if (all_vehicles.empty()) {
                        draw_player_empty_state(I_CAR, "No Vehicles", "No streamed vehicles are currently tracked.");
                    }
                    else {
                        TextDisabled("No vehicles match your search.");
                    }
                }
                else {
                    PushStyleColor(ImGuiCol_Header, ImVec4{ 0.f, 0.f, 0.f, 0.f });
                    PushStyleColor(ImGuiCol_HeaderHovered, ImVec4{ 0.f, 0.f, 0.f, 0.f });
                    PushStyleColor(ImGuiCol_HeaderActive, ImVec4{ 0.f, 0.f, 0.f, 0.f });
                    BeginChild("##vehicle_directory_entries", { 0, 346 ds }, true, ImGuiWindowFlags_NoBackground);
                    for (const auto& vehicle : filtered_vehicles) {
                        const bool is_selected = has_selected_vehicle && selected_vehicle.Pointer == vehicle.Pointer;
                        const bool locked = is_vehicle_locked(vehicle);
                        const std::string row_title =
                            get_vehicle_display_name(vehicle) +
                            " (" + std::to_string(static_cast<int>(vehicle.Dist)) + "m)";
                        const std::string trimmed_row_title = ptext{ row_title }.trim(GetContentRegionAvail().x - 18 ds);
                        const std::string row_label =
                            trimmed_row_title +
                            "##vehicle_row_" + std::to_string(reinterpret_cast<uintptr_t>(vehicle.Pointer));
                        const ImVec2 row_size{
                            ImMin(GetContentRegionAvail().x, CalcTextSize(trimmed_row_title.c_str()).x + 8 ds),
                            18 ds
                        };
                        const ImVec4 row_text_color = locked
                            ? (is_selected ? g_style->col(pcol_scheme).vec4() : ImVec4{ 0.96f, 0.72f, 0.46f, 1.0f })
                            : (is_selected ? g_style->col(pcol_scheme).vec4() : g_style->col(pcol_text2).vec4());

                        PushStyleColor(ImGuiCol_Text, row_text_color);
                        if (Selectable(row_label.c_str(), is_selected, 0, row_size)) {
                            selected_vehicle_ptr = reinterpret_cast<uintptr_t>(vehicle.Pointer);
                        }
                        PopStyleColor();

                        widgets->spacing(2 ds);
                    }
                    EndChild();
                    PopStyleColor(3);
                }
            }
            widgets->child.end();
        }
        EndGroup();

        SameLine(0, 8.f ds);

        BeginGroup();
        {
            widgets->child.begin("", "Selected Vehicle", 2, { 186 ds, 414 ds });
            {
                if (!has_selected_vehicle) {
                    PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 10 ds, 10 ds });
                    BeginChild("##selected_vehicle_preview", { 0, 144 ds }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    {
                        if (all_vehicles.empty()) {
                            draw_player_empty_state(I_CAR, "No Vehicles", "No streamed vehicles are currently tracked.");
                        }
                        else {
                            Text("Preview");

                            widgets->spacing(3 ds);
                            widgets->separator();
                            widgets->spacing(6 ds);
                            TextDisabled("Select a vehicle.");
                        }
                    }
                    EndChild();
                    PopStyleVar();

                    widgets->spacing(6 ds);

                    PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 10 ds, 10 ds });
                    BeginChild("##selected_vehicle_actions", { 0, 210 ds }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    {
                        if (all_vehicles.empty()) {
                            draw_player_empty_state(I_CAR, "No Actions", "Vehicle actions unlock when a streamed vehicle exists.");
                        }
                        else {
                            Text("Actions");

                            widgets->spacing(3 ds);
                            widgets->separator();
                            widgets->spacing(6 ds);
                            TextDisabled("Choose a vehicle.");
                        }
                    }
                    EndChild();
                    PopStyleVar();
                }
                else {
                    const bool locked = is_vehicle_locked(selected_vehicle);
                    const std::string preview_name = get_vehicle_display_name(selected_vehicle);
                    const std::string driver_name = get_vehicle_driver_name(selected_vehicle);

                    PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 10 ds, 10 ds });
                    BeginChild("##selected_vehicle_preview", { 0, 144 ds }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    {
                        Text("Preview");

                        widgets->spacing(3 ds);
                        widgets->separator();
                        widgets->spacing(6 ds);

                        const std::string title = ptext{ preview_name }.trim(GetContentRegionAvail().x - 4 ds);
                        PushFont(fonts[font].get(16));
                        TextUnformatted(title.c_str());
                        PopFont();
                        PushStyleColor(ImGuiCol_Text, locked ? ImVec4{ 0.96f, 0.72f, 0.46f, 1.0f } : g_style->col(pcol_text2).vec4());
                        TextUnformatted(locked ? "Locked" : "Unlocked");
                        PopStyleColor();

                        widgets->spacing(8 ds);
                        TextDisabled("Distance");
                        SameLine(74 ds);
                        TextUnformatted(format_player_distance(selected_vehicle.Dist).c_str());
                        TextDisabled("Driver");
                        SameLine(74 ds);
                        const std::string trimmed_driver = ptext{ driver_name }.trim(GetContentRegionAvail().x - 78 ds);
                        TextUnformatted(trimmed_driver.c_str());
                        TextDisabled("Doors");
                        SameLine(74 ds);
                        TextUnformatted(locked ? "Locked" : "Open");
                        TextDisabled("Speed");
                        SameLine(74 ds);
                        TextUnformatted(format_vehicle_speed(selected_vehicle).c_str());
                    }
                    EndChild();
                    PopStyleVar();

                    widgets->spacing(6 ds);

                    PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 10 ds, 10 ds });
                    BeginChild("##selected_vehicle_actions", { 0, 210 ds }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    {
                        Text("Actions");

                        widgets->spacing(3 ds);
                        widgets->separator();
                        widgets->spacing(6 ds);

                        const float button_width = GetContentRegionAvail().x;
                        PushStyleVar(ImGuiStyleVar_FrameRounding, 7 ds);
                        PushStyleColor(ImGuiCol_Button, g_style->col(pcol_bg3).vec4());
                        PushStyleColor(ImGuiCol_ButtonHovered, g_style->col(pcol_scheme, 0.18f).vec4());
                        PushStyleColor(ImGuiCol_ButtonActive, g_style->col(pcol_scheme, 0.28f).vec4());
                        if (widgets->button(locked ? "Unlock Vehicle" : "Lock Vehicle", { button_width, 32 ds })) {
                            toggle_vehicle_lock(selected_vehicle);
                        }

                        widgets->spacing(4 ds);
                        if (widgets->button("Repair Vehicle", { button_width, 32 ds })) {
                            repair_vehicle(selected_vehicle);
                        }
                        PopStyleColor(3);
                        PopStyleVar();
                    }
                    EndChild();
                    PopStyleVar();
                }
            }
            widgets->child.end();
        }
        EndGroup();
        });

    widgets->nav.addpage(4, [&] {
        auto& config_ui = get_config_browser_ui_state();
        refresh_config_browser_cloud_catalog();
        const auto config_catalog = get_config_browser_catalog_snapshot();
        const auto filtered_configs = collect_config_browser_entries(
            config_catalog,
            config_ui.sort_mode,
            config_ui.search,
            config_ui.filter_esp,
            config_ui.filter_silentaim,
            config_ui.filter_aimbot,
            config_ui.filter_triggerbot);
        const int total_config_count = static_cast<int>(config_catalog.size());
        const int visible_config_count = static_cast<int>(filtered_configs.size());
        bool cloud_loading = false;
        bool cloud_ready = false;
        std::string cloud_error;
        get_config_browser_cloud_status(cloud_loading, cloud_ready, cloud_error);
        const float config_browser_width = GetContentRegionAvail().x;
        widgets->child.begin(I_FOLDER__COG, "Config Browser", 1, { config_browser_width, 414 ds });
        {
            const std::string status_label = config_ui.preview_active
                ? "Using: " + config_ui.preview_config_name
                : (cloud_loading ? "Loading site configs..." : (cloud_ready ? "Site configs loaded" : "Waiting for site configs"));

            PushStyleColor(ImGuiCol_Text, (config_ui.preview_active || cloud_ready) ? g_style->col(pcol_scheme).vec4() : g_style->col(pcol_text2).vec4());
            TextUnformatted(status_label.c_str());
            PopStyleColor();
            if (!cloud_error.empty() && !cloud_ready) {
                PushStyleColor(ImGuiCol_Text, g_style->col(pcol_text2).vec4());
                const std::string cloud_error_label = format_cloud_config_error(cloud_error);
                TextWrapped("Cloud: %s", cloud_error_label.c_str());
                PopStyleColor();

                if (is_cloud_config_auth_error(cloud_error)) {
                    static const std::string hwid = auth::get_hwid();
                    PushStyleColor(ImGuiCol_Text, g_style->col(pcol_text2).vec4());
                    TextWrapped("HWID: %s", hwid.c_str());
                    PopStyleColor();

                    widgets->spacing(4 ds);
                    if (widgets->button("Copy HWID", { 116 ds, 28 ds })) {
                        Utils::PasteClipboard(hwid.c_str());
                        NotifyManager::Send("HWID copied", 220.0f);
                    }
                }
            }

            widgets->spacing(8 ds);
            widgets->textinput("Config Search", config_ui.search, sizeof(config_ui.search), {
                .padding = vec2{ 10, 10 },
                .icon = I_SEARCH,
                .hint = "Search config names...",
                .size = vec2{ 0, 0 }
                });

            widgets->spacing(8 ds);
            PushStyleColor(ImGuiCol_Text, g_style->col(pcol_text2).vec4());
            Text("%d shown / %d total", visible_config_count, total_config_count);
            PopStyleColor();

            widgets->spacing(8 ds);
            widgets->separator();
            widgets->spacing(8 ds);

            if (filtered_configs.empty()) {
                if (cloud_loading) {
                    draw_player_empty_state(I_FOLDER__COG, "Loading", "Fetching your site configs.");
                }
                else if (config_ui.sort_mode == config_sort_my_configs) {
                    draw_player_empty_state(I_FOLDER__COG, "No Configs", "No owned configs match the current filters.");
                }
                else {
                    draw_player_empty_state(I_FOLDER__COG, "No Match", "Try a different name or feature filter.");
                }
            }
            else {
                for (const auto* entry : filtered_configs) {
                    PushID(entry->id.c_str());
                    const bool is_entry_active = entry->is_active_slot || (config_ui.preview_active && config_ui.preview_config_id == entry->id);
                    const auto action = draw_config_entry_card(*entry, is_entry_active);
                    if (action == config_entry_action_t::import_slot) {
                        if (entry->uses_exploits) {
                            open_config_exploit_warning(config_ui, *entry);
                        }
                        else {
                            apply_config_browser_config(config_ui, *entry);
                        }
                    }
                    else if (action == config_entry_action_t::save_slot) {
                        save_current_config_to_cloud_slot(config_ui, *entry);
                    }
                    PopID();
                    widgets->spacing(6 ds);
                }
            }
        }
        widgets->child.end();
        });

    widgets->nav.addpage(5, [&] {
        int menu_theme_mode = 0;

        // The registry flag survives restarts, so the switch starts from its real state.
        static bool registry_streamproof = false;
        static bool registry_state_loaded = false;
        if (!registry_state_loaded) {
            registry_streamproof = NvidiaProofing::CheckRegistryValue();
            registry_state_loaded = true;
        }

        int general_card_height = 304;
        if (g_Config.General->NVStreamProof)
            general_card_height += 20;
        if (registry_streamproof)
            general_card_height += 20;

        BeginGroup();
        {
            widgets->child.begin(I_WAVE__SAW__TOOL, "General", 1, { 0, general_card_height ds });
            {
                widgets->checkbox(I_EYE__OFF, "Stream Proof", &g_Config.General->StreamProof);
                if (!g_Config.General->StreamProof) {
                    g_Config.General->NVStreamProof = false;
                }

                // Back to the capture-exclusion variant: no registry value, no container
                // restart. That is the state in which NVIDIA recordings stayed clean.
                if (widgets->checkbox(I_EYE__OFF, "Mirror", &g_Config.General->NVStreamProof)) {
                    if (g_Config.General->NVStreamProof) {
                        // NVIDIA proof only makes sense on top of the regular stream proof.
                        g_Config.General->StreamProof = true;
                    }
                }
                if (g_Config.General->NVStreamProof && g_Config.General->StreamProof) {
                    const Gui::capture_exclusion_status_t exclusion = Gui::GetCaptureExclusionStatus();
                    char exclusion_label[128]{};
                    ImVec4 exclusion_color = g_style->col(pcol_text2).vec4();
                    if (exclusion.active && exclusion.layered) {
                        sprintf_s(exclusion_label, "Capture exclusion: active, but window is layered (ignored)");
                    }
                    else if (exclusion.active) {
                        sprintf_s(exclusion_label, "Capture exclusion: active");
                        exclusion_color = g_style->col(pcol_scheme).vec4();
                    }
                    else if (exclusion.last_error != 0) {
                        sprintf_s(exclusion_label, "Capture exclusion: rejected (gle=%lu)", exclusion.last_error);
                    }
                    else {
                        sprintf_s(exclusion_label, "Capture exclusion: not applied");
                    }
                    PushStyleColor(ImGuiCol_Text, exclusion_color);
                    TextUnformatted(exclusion_label);
                    PopStyleColor();
                }

                // Registry based NVIDIA stream proof: writes NVIDIA's FTS capture flag and
                // restarts the container through its service.
                if (widgets->checkbox(I_EYE__OFF, "NVIDIA Streamproof", &registry_streamproof)) {
                    const bool enabled = registry_streamproof;
                    if (enabled) {
                        g_Config.General->StreamProof = true;
                    }

                    std::thread([enabled] {
                        if (!NvidiaProofing::ApplyStreamproofFlag(enabled)) {
                            NotifyManager::Send(enabled ? "NVIDIA streamproof needs admin" : "NVIDIA unlock needs admin", 260.0f);
                            return;
                        }

                        if (!NvidiaProofing::RestartNV())
                            NotifyManager::Send("Restart NVIDIA manually", 260.0f);
                        }).detach();
                }
                if (registry_streamproof) {
                    static ULONGLONG next_registry_check = 0;
                    static bool registry_value_set = false;
                    const ULONGLONG now = GetTickCount64();
                    if (now >= next_registry_check) {
                        next_registry_check = now + 1000;
                        registry_value_set = NvidiaProofing::CheckRegistryValue();
                    }

                    PushStyleColor(ImGuiCol_Text, g_style->col(pcol_text2).vec4());
                    TextUnformatted(registry_value_set
                        ? "NVIDIA capture flag set"
                        : "Not applied - toggle again");
                    PopStyleColor();
                }
                widgets->checkbox(I_EYE__OFF, "Second Monitor", &g_Config.General->SecondMonitor);
                widgets->binder(I_COMMAND, "Menu Key", &g_Config.General->MenuKey);
                widgets->checkbox(I_MOUSE, "Custom Input", &g_Config.General->CustomInput);
                widgets->spacing(12 ds);
                widgets->separator();
                widgets->spacing(10 ds);

                static ULONGLONG last_autoconfig_load = 0;

                if (widgets->checkbox(I_FOLDER__COG, "Site Config", &g_Config.General->AutoConfig))
                {
                    if (g_Config.General->AutoConfig)
                    {
                        last_autoconfig_load = GetTickCount64();
                    }
                }

                if (g_Config.General->AutoConfig)
                {
                    const ULONGLONG now = GetTickCount64();
                    if (now - last_autoconfig_load >= 5000)
                    {
                        last_autoconfig_load = now;

                        std::string current = g_Config.ExportCurrentConfigJsonString();
                        g_Config.ApplyConfigJsonString(current);
                    }
                }
            }
            widgets->child.end();
        }
        EndGroup();

        SameLine(0, 8.f ds);

        BeginGroup();
        {
            widgets->child.begin(I_PALETTE, "Theme", 2, { 0, 172 ds });
            {
                widgets->selector("Theme", &menu_theme_mode, { "Dynamic" });
                g_Config.General->MenuThemeMode = menu_theme_mode;
                g_Config.General->MenuManualTheme = 0;

                widgets->spacing(12 ds);
                widgets->separator();
                widgets->spacing(10 ds);

                widgets->spacing(10 ds);
                PushStyleColor(ImGuiCol_Text, g_style->col(pcol_text2, 0.7f).vec4());
                TextUnformatted("Build " __DATE__ " " __TIME__);
                PopStyleColor();

                widgets->spacing(8 ds);
                if (widgets->button("Destruct", { CalcItemWidth(), 44 ds }, { .icon = I_ARROW__RIGHT })) {
                    RunCleanerAndExitAsync();
                }
            }
            widgets->child.end();

            widgets->spacing(8 ds);

            widgets->child.begin(I_COMMAND, "Panic Key", 3, { 0, 140 ds });
            {
                TextDisabled("Closes the menu and shuts the process down.");
                widgets->spacing(6 ds);

                widgets->binder(I_COMMAND, "Panic Key", &g_Config.General->UnloadKey);

                widgets->spacing(6 ds);
                if (widgets->button("Reset Panic Key", { CalcItemWidth(), 30 ds })) {
                    g_Config.General->UnloadKey = VK_END;
                }
            }
            widgets->child.end();
        }
        EndGroup();

        static float crosshair_main[4] = {
            g_Config.General->CrosshairMainColor.Value.x,
            g_Config.General->CrosshairMainColor.Value.y,
            g_Config.General->CrosshairMainColor.Value.z,
            g_Config.General->CrosshairMainColor.Value.w
        };
        static float crosshair_accent[4] = {
            g_Config.General->CrosshairAccentColor.Value.x,
            g_Config.General->CrosshairAccentColor.Value.y,
            g_Config.General->CrosshairAccentColor.Value.z,
            g_Config.General->CrosshairAccentColor.Value.w
        };

        BeginGroup();
        {
            widgets->child.begin(I_COMMAND, "Crosshair", 4, { 0, 330 ds });
            {
                widgets->checkbox(I_COMMAND, "Crosshair", &g_Config.General->Crosshair);
                if (g_Config.General->Crosshair) {
                    widgets->spacing(4 ds);
                    draw_crosshair_style_selector("Style", &g_Config.General->CrosshairStyle);
                    widgets->sliderfloat(I_RULER__MEASURE, "Size", &g_Config.General->CrosshairSize, 0.5f, 30.0f, "%.1fpx");
                    widgets->sliderfloat(I_RULER__MEASURE, "Thickness", &g_Config.General->CrosshairThickness, 0.5f, 8.0f, "%.1fpx");
                    widgets->sliderfloat(I_GRADIENTER, "Roundness", &g_Config.General->CrosshairRoundness, 0.0f, 1.0f, "%.2f");
                    widgets->coloredit(I_PALETTE, "Main Color", crosshair_main);
                    widgets->coloredit(I_PALETTE, "Accent Color", crosshair_accent);
                }
            }
            widgets->child.end();
        }
        EndGroup();

        g_Config.General->CrosshairMainColor.Value.x = crosshair_main[0];
        g_Config.General->CrosshairMainColor.Value.y = crosshair_main[1];
        g_Config.General->CrosshairMainColor.Value.z = crosshair_main[2];
        g_Config.General->CrosshairMainColor.Value.w = crosshair_main[3];
        g_Config.General->CrosshairAccentColor.Value.x = crosshair_accent[0];
        g_Config.General->CrosshairAccentColor.Value.y = crosshair_accent[1];
        g_Config.General->CrosshairAccentColor.Value.z = crosshair_accent[2];
        g_Config.General->CrosshairAccentColor.Value.w = crosshair_accent[3];
        });

}

bool SlateSetupPending() {
    return !setup_screen_done();
}

void c_slate::draw(ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swapchain) {
    m_device = device;
    m_ctx = ctx;
    m_swapchain = swapchain;

    // Destruct is silent: while the cleanup thread works, nothing is drawn at all
    // (the overlay window is hidden by the render loop).
    if (IsDestructing())
        return;

    UpdatePersistentLocalFeatures();

    // The menu renders whenever it is open - no focus requirement. Otherwise it could
    // stay invisible (and therefore unclickable) when the overlay cannot take the
    // foreground, e.g. when it was opened from the desktop or another application.
    const bool should_render_menu = g_MenuInfo.IsOpen;

    const bool menu_just_opened = should_render_menu && !last_menu_open;
    const bool menu_just_closed = !should_render_menu && last_menu_open;

    if (menu_just_opened) {
        menu_opened_at = GetTickCount64();
        request_account_sidebar_refresh(true);
    }
    last_menu_open = should_render_menu;

    sync_configured_menu_theme_base();
    g_style->setup();
    apply_configured_menu_theme();
    g_shadertoy->setup(device, ctx, swapchain);

    // First run: the setup screen takes over the whole frame instead of the menu.
    if (!setup_screen_done()) {
        draw_setup_screen();
        return;
    }

    // No injection into nvcontainer any more: the mapped module and the classic byte
    // patches both broke NVIDIA's own overlay and recording. The capture exclusion on
    // the overlay window is applied per frame in Gui::Rendering instead.

    if (should_render_menu) {
        request_account_sidebar_refresh();
        pump_account_sidebar_avatar(this, device);
    }

    SetNextWindowPos({ 555, 555 }, ImGuiCond_Once);

    if (should_render_menu) {
            ImVec2 menu_pos;
            ImVec2 menu_size;
            bool show_visual_preview = false;
            bool show_hitbox_selector = false;
            bool show_config_filter_panel = false;
            int combat_subtab = 0;
            const bool menu_warmed_up = (GetTickCount64() - menu_opened_at) >= 600;

            widgets->window.begin("menu", vec2{ 726, 488 }, ImGuiWindowFlags_NoBringToFrontOnFocus);
            {
                menu_pos = GetWindowPos();
                menu_size = GetWindowSize();
                // The side panels (ESP preview / aimbot hitbox model) are switched off.
                show_visual_preview = false;
                show_hitbox_selector = false;
                show_config_filter_panel = menu_warmed_up && widgets->nav.current == 4;
                combat_subtab = widgets->nav.tabs[0].current;
                show_hitbox_selector = show_hitbox_selector && combat_subtab == 0;
                g_search->initialize();

                g_shadertoy->bg(GetWindowDrawList(), GetWindowPos(), GetWindowPos() + GetWindowSize(), g_style->col(pcol_backdrop), GImGui->Style.WindowRounding);
                PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 0, 14 });
                BeginChild("navbar", vec2{ 186, 0 }, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                {
                    PopStyleVar();
                    GetWindowDrawList()->AddRectFilled(GetWindowPos(), GetWindowPos() + GetWindowSize(), g_style->col(pcol_bg2), GImGui->Style.WindowRounding, ImDrawFlags_RoundCornersLeft);
                    GetWindowDrawList()->AddRectFilled(GetWindowPos() + ImVec2{ GetWindowWidth() - 1, 0 }, GetWindowPos() + GetWindowSize(), g_style->col(pcol_border));

                    SetCursorPosY(GetCursorPosY() + 2.f ds);
                    BeginGroup();
                    {
                        const float logo_box_width = GetWindowWidth() - 28 ds;
                        SetCursorPosX((GetWindowWidth() - logo_box_width) * 0.5f);
                        draw_seasonal_logo_widget(vec2{ logo_box_width, logo_box_width * 0.58f });
                    }
                    EndGroup();
                    // Keep the logo visibly separated from the options below it.
                    widgets->spacing(26.f ds);
                    SetCursorPosX(14 ds);
                    widgets->nav.drawtabs();
                    draw_account_sidebar_card(this);
                    SetCursorPos({ 0, 0 });
                    BeginChild("glow", { 0, 0 }, 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
                    {
                        static c_buffer* buffer = new c_buffer();
                        buffer->glow_pos = GetWindowPos() + ImVec2{ 11 ds, 320 ds };
                        // Breathing accent glow behind the navbar.
                        buffer->glow_strength = 136 ds * (0.82f + 0.18f * std::sin(static_cast<float>(ImGui::GetTime()) * 1.7f));

                        g_shadertoy->draw(GetWindowDrawList(), GetWindowPos(), GetWindowPos() + GetWindowSize(), g_style->col(pcol_scheme, 0.08f), GImGui->Style.WindowRounding, buffer);
                    }
                    EndChild();
                }
                EndChild();
                SameLine(0, 0.f ds);
                BeginChild("main", vec2{ 0, 0 }, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground);
                {
                    PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 14, 14 });
                    BeginChild("header", vec2{ 0, 60 }, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    {
                        PopStyleVar();
                        widgets->nav.drawsubtabs();
                        constexpr const char* search_hint = "Search Features";
                        const float search_hint_width = CalcTextSize(search_hint).x;
                        SetCursorPos({ GetWindowWidth() - 44 ds - search_hint_width, 22 ds });
                        g_style->push(pcol_bg3, g_style->col(pcol_bg3, 0));
                        g_style->push(pcol_border, g_style->col(pcol_border, 0));
                        widgets->textinput("##searchbar", g_search->buf, sizeof(g_search->buf), { .padding = vec2{ 0, 0 }, .icon = I_SEARCH, .hint = search_hint, .size = ImVec2{ search_hint_width + 24 ds, 16 ds }, .bgcol = pcolor(0, 0, 0, 0), .rounding = 0 ds });
                        g_style->pop(2);
                        GetWindowDrawList()->AddRectFilled(GetWindowPos() + ImVec2{ 0, GetWindowHeight() - 1 }, GetWindowPos() + ImVec2{ 44.f ds, GetWindowHeight() }, g_style->col(pcol_border, 2.f));
                    }
                    EndChild();
                    widgets->spacing(0.f ds);
                    PushStyleVar(ImGuiStyleVar_Alpha, GImGui->Style.Alpha * widgets->nav.tab_anim * widgets->nav.subtab_anim);
                    PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 14, 14 });
                    PushStyleVar(ImGuiStyleVar_ItemSpacing, vec2{ 8, 8 });
                    BeginChild("page", { 0, 0 }, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground);
                    {
                        // Large logo behind the content of every page, seen through the translucent cards.
                        {
                            const ImVec2 backdrop_min = GetCursorScreenPos();
                            const ImVec2 backdrop_extent = GetContentRegionAvail();
                            if (backdrop_extent.x > 1.f && backdrop_extent.y > 1.f) {
                                // Same animated mark, very faint, behind the cards of every page.
                                const auto& palette = get_active_menu_palette();
                                const float mark_w = ImMin(backdrop_extent.x * 0.62f, 360.f ds);
                                const float mark_h = mark_w * 0.52f;
                                const ImVec2 mark_center{ backdrop_min.x + backdrop_extent.x * 0.5f, backdrop_min.y + backdrop_extent.y * 0.45f };
                                draw_infinity_logo(GetWindowDrawList(),
                                    ImVec2{ mark_center.x - mark_w * 0.5f, mark_center.y - mark_h * 0.5f },
                                    ImVec2{ mark_center.x + mark_w * 0.5f, mark_center.y + mark_h * 0.5f },
                                    palette.bright, palette.accent, 0.09f);
                            }
                        }
                        PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.02f, 0.05f, 0.32f));

                        if (strlen(g_search->buf) == 0) {
                            widgets->nav.drawpage();
                            g_search->visit_current_page();
                        }
                        else
                            g_search->draw();

                        SetCursorPos(GetCurrentWindow()->Scroll);
                        BeginChild("gradient", { 100, 100 }, 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
                        {
                            static float anim1 = 0; anim1 = anim(anim1, 0.f, 1.f, GetCurrentWindow()->ParentWindow->Scroll.y > 0);
                            static float anim2 = 0; anim2 = anim(anim2, 0.f, 1.f, GetCurrentWindow()->ParentWindow->ScrollMax.y > 0 && GetCurrentWindow()->ParentWindow->Scroll.y < (GetCurrentWindow()->ParentWindow->ScrollMax.y - 10));

                            GetWindowDrawList()->PushClipRect(GetCurrentWindow()->ParentWindow->Rect().Min, GetCurrentWindow()->ParentWindow->Rect().Max);
                            g_draw->gradient(GetWindowDrawList(), GetCurrentWindow()->ParentWindow->Pos, { GetCurrentWindow()->ParentWindow->Rect().Max.x, GetCurrentWindow()->ParentWindow->Pos.y + 60 ds }, g_style->col(pcol_bg, anim1), g_style->col(pcol_bg, anim1), g_style->col(pcol_bg, 0), g_style->col(pcol_bg, 0), GImGui->Style.WindowRounding, ImDrawFlags_RoundCornersTop);
                            g_draw->gradient(GetWindowDrawList(), { GetCurrentWindow()->ParentWindow->Pos.x, GetCurrentWindow()->ParentWindow->Rect().Max.y - 30 ds }, GetCurrentWindow()->ParentWindow->Rect().Max, g_style->col(pcol_bg, 0), g_style->col(pcol_bg, 0), g_style->col(pcol_bg, anim2), g_style->col(pcol_bg, anim2), GImGui->Style.WindowRounding, ImDrawFlags_RoundCornersBottom);
                            GetWindowDrawList()->PopClipRect();
                        }
                        EndChild();

                        PopStyleColor();
                    }
                    EndChild();
                    PopStyleVar(3);
                    SetCursorPos({ 0, 0 });
                    BeginChild("glow", { 0, 0 }, 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
                    {
                        static c_buffer* buffer = new c_buffer();
                        buffer->glow_pos = GetWindowPos() + ImVec2{ GetWindowWidth() / 2, 15 ds };
                        buffer->glow_strength = 214 ds;

                        g_shadertoy->draw(GetWindowDrawList(), GetWindowPos(), GetWindowPos() + GetWindowSize(), g_style->col(pcol_scheme, 0.08f), GImGui->Style.WindowRounding, buffer);
                    }
                    EndChild();
                }
                EndChild();
                widgets->modal.handle();
            }
            widgets->window.end();

            if (show_visual_preview) {
                const ImVec2 preview_size{ 228 ds, menu_size.y };
                const ImVec2 preview_pos = menu_pos + ImVec2{ menu_size.x + 10 ds, 0 };

                SetNextWindowPos(preview_pos, ImGuiCond_Always);
                widgets->window.begin("esp_preview_external", preview_size, ImGuiWindowFlags_NoBringToFrontOnFocus);
                {
                    g_shadertoy->bg(GetWindowDrawList(), GetWindowPos(), GetWindowPos() + GetWindowSize(), g_style->col(pcol_backdrop), GImGui->Style.WindowRounding);

                    PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 14, 14 });
                    BeginChild("preview_header", vec2{ 0, 40 ds }, 2, 128);
                    {
                        SetCursorPosY(12 ds);
                        PushFont(fonts[icons].get(16));
                        TextColored(g_style->col(pcol_scheme), I_TARGET);
                        PopFont();
                        SameLine(0, 8 ds);
                        Text("ESP Preview");
                    }
                    EndChild();

                    widgets->spacing(0 ds);
                    BeginChild("preview_content", vec2{ 0, 0 }, 2, 128);
                    {
                        draw_esp_preview_ped();
                        Custom::g_EspPreview.DragDropHandler();
                        Custom::g_EspPreview.Draw();
                    }
                    EndChild();
                    PopStyleVar();
                }
                widgets->window.end();
            }

            if (show_hitbox_selector) {
                const ImVec2 selector_size{ 228 ds, menu_size.y };
                const ImVec2 selector_pos = menu_pos + ImVec2{ menu_size.x + 10 ds, 0 };

                SetNextWindowPos(selector_pos, ImGuiCond_Always);
                widgets->window.begin("hitbox_selector_external", selector_size, ImGuiWindowFlags_NoBringToFrontOnFocus);
                {
                    g_shadertoy->bg(GetWindowDrawList(), GetWindowPos(), GetWindowPos() + GetWindowSize(), g_style->col(pcol_backdrop), GImGui->Style.WindowRounding);

                    PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 14, 14 });
                    BeginChild("hitbox_selector_header", vec2{ 0, 40 ds }, 2, 128);
                    {
                        SetCursorPosY(12 ds);
                        PushFont(fonts[icons].get(16));
                        TextColored(g_style->col(pcol_scheme), I_BONE);
                        PopFont();
                        SameLine(0, 8 ds);
                        Text("Aimbot Hitboxes");
                    }
                    EndChild();

                    widgets->spacing(0 ds);
                    BeginChild("hitbox_selector_content", vec2{ 0, 0 }, 2, 128 | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    {
                        if (combat_subtab == 0) {
                            bool aimbot_selector_values[] = {
                                g_Config.Aimbot->BoneGroup_Head,
                                g_Config.Aimbot->BoneGroup_Neck,
                                g_Config.Aimbot->BoneGroup_Chest,
                                g_Config.Aimbot->BoneGroup_Arms,
                                g_Config.Aimbot->BoneGroup_Legs,
                                g_Config.Aimbot->BoneGroup_Feet
                            };

                            draw_ped_hitbox_selector("##hitboxes", aimbot_selector_values, IM_ARRAYSIZE(aimbot_selector_values), aimbot_hitbox_points, IM_ARRAYSIZE(aimbot_hitbox_points), g_Config.Aimbot->Enabled);

                            g_Config.Aimbot->BoneGroup_Head = aimbot_selector_values[0];
                            g_Config.Aimbot->BoneGroup_Neck = aimbot_selector_values[1];
                            g_Config.Aimbot->BoneGroup_Chest = aimbot_selector_values[2];
                            g_Config.Aimbot->BoneGroup_Arms = aimbot_selector_values[3];
                            g_Config.Aimbot->BoneGroup_Legs = aimbot_selector_values[4];
                            g_Config.Aimbot->BoneGroup_Feet = aimbot_selector_values[5];
                        }
                    }
                    EndChild();
                    PopStyleVar();
                }
                widgets->window.end();
            }

            if (show_config_filter_panel) {
                auto& config_ui = get_config_browser_ui_state();
                refresh_config_browser_cloud_catalog();
                const auto config_catalog = get_config_browser_catalog_snapshot();
                const auto filtered_configs = collect_config_browser_entries(
                    config_catalog,
                    config_ui.sort_mode,
                    config_ui.search,
                    config_ui.filter_esp,
                    config_ui.filter_silentaim,
                    config_ui.filter_aimbot,
                    config_ui.filter_triggerbot);
                const int total_config_count = static_cast<int>(config_catalog.size());
                const int visible_config_count = static_cast<int>(filtered_configs.size());
                const int active_filter_count = get_config_browser_active_filter_count(config_ui);
                const ImVec2 filter_size{ 228 ds, menu_size.y };
                const ImVec2 filter_pos = menu_pos + ImVec2{ menu_size.x + 10 ds, 0 };

                SetNextWindowPos(filter_pos, ImGuiCond_Always);
                widgets->window.begin("config_filters_external", filter_size, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                {
                    g_shadertoy->bg(GetWindowDrawList(), GetWindowPos(), GetWindowPos() + GetWindowSize(), g_style->col(pcol_backdrop), GImGui->Style.WindowRounding);

                    PushStyleVar(ImGuiStyleVar_WindowPadding, vec2{ 14, 14 });
                    BeginChild("config_filters_header", vec2{ 0, 40 ds }, 2, 128 | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    {
                        SetCursorPosY(12 ds);
                        PushFont(fonts[icons].get(16));
                        TextColored(g_style->col(pcol_scheme), I_ADJUSTMENTS__HORIZONTAL);
                        PopFont();
                        SameLine(0, 8 ds);
                        Text("Config Filters");
                    }
                    EndChild();

                    widgets->spacing(0 ds);
                    BeginChild("config_filters_content", vec2{ 0, 0 }, 2, 128 | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                    {
                        PushStyleColor(ImGuiCol_Text, config_ui.preview_active ? g_style->col(pcol_scheme).vec4() : g_style->col(pcol_text2).vec4());
                        TextUnformatted(config_ui.preview_active ? "Site config active" : "No site config selected");
                        PopStyleColor();

                        if (config_ui.preview_active) {
                            widgets->spacing(4 ds);
                            TextWrapped("%s", config_ui.preview_config_name.c_str());
                            if (!config_ui.main_config_snapshot.empty()) {
                                widgets->spacing(8 ds);

                                PushStyleVar(ImGuiStyleVar_FrameRounding, 7 ds);
                                PushStyleColor(ImGuiCol_Button, g_style->col(pcol_bg3).vec4());
                                PushStyleColor(ImGuiCol_ButtonHovered, g_style->col(pcol_scheme, 0.18f).vec4());
                                PushStyleColor(ImGuiCol_ButtonActive, g_style->col(pcol_scheme, 0.30f).vec4());
                                if (widgets->button("Back to main", { GetContentRegionAvail().x, 30 ds })) {
                                    restore_config_browser_main(config_ui);
                                }
                                PopStyleColor(3);
                                PopStyleVar();
                            }
                        }

                        widgets->spacing(10 ds);
                        widgets->separator();
                        widgets->spacing(8 ds);

                        TextDisabled("Configs");
                        Text("%d shown / %d total", visible_config_count, total_config_count);

                        widgets->spacing(8 ds);
                        TextDisabled("Sort Order");
                        widgets->spacing(6 ds);

                        const float panel_width = GetContentRegionAvail().x;
                        if (widgets->selectable("My Configs", config_ui.sort_mode == config_sort_my_configs, { panel_width, 28 ds }, { .rounding = 7, .checkmark = false, .icon = I_FOLDER__COG })) {
                            config_ui.sort_mode = config_sort_my_configs;
                        }
                        widgets->spacing(4 ds);
                        if (widgets->selectable("Trending", config_ui.sort_mode == config_sort_trending, { panel_width, 28 ds }, { .rounding = 7, .checkmark = false, .icon = I_WAVE__SAW__TOOL })) {
                            config_ui.sort_mode = config_sort_trending;
                        }
                        widgets->spacing(4 ds);
                        if (widgets->selectable("Latest", config_ui.sort_mode == config_sort_latest, { panel_width, 28 ds }, { .rounding = 7, .checkmark = false, .icon = I_CLOCK })) {
                            config_ui.sort_mode = config_sort_latest;
                        }

                        widgets->spacing(10 ds);
                        widgets->separator();
                        widgets->spacing(8 ds);

                        TextDisabled("Feature Filter");
                        Text("%d active", active_filter_count);
                        widgets->spacing(6 ds);

                        if (widgets->selectable("ESP", config_ui.filter_esp, { panel_width, 28 ds }, { .rounding = 7, .checkmark = false, .icon = I_EYE__COG })) {
                            config_ui.filter_esp = !config_ui.filter_esp;
                        }
                        widgets->spacing(4 ds);
                        if (widgets->selectable("SilentAim", config_ui.filter_silentaim, { panel_width, 28 ds }, { .rounding = 7, .checkmark = false, .icon = I_SWORD })) {
                            config_ui.filter_silentaim = !config_ui.filter_silentaim;
                        }
                        widgets->spacing(4 ds);
                        if (widgets->selectable("Aimbot", config_ui.filter_aimbot, { panel_width, 28 ds }, { .rounding = 7, .checkmark = false, .icon = I_TARGET })) {
                            config_ui.filter_aimbot = !config_ui.filter_aimbot;
                        }
                        widgets->spacing(4 ds);
                        if (widgets->selectable("Trigger", config_ui.filter_triggerbot, { panel_width, 28 ds }, { .rounding = 7, .checkmark = false, .icon = I_DOTS })) {
                            config_ui.filter_triggerbot = !config_ui.filter_triggerbot;
                        }

                        if (active_filter_count > 0) {
                            widgets->spacing(8 ds);
                            PushStyleVar(ImGuiStyleVar_FrameRounding, 7 ds);
                            PushStyleColor(ImGuiCol_Button, g_style->col(pcol_bg3).vec4());
                            PushStyleColor(ImGuiCol_ButtonHovered, g_style->col(pcol_scheme, 0.18f).vec4());
                            PushStyleColor(ImGuiCol_ButtonActive, g_style->col(pcol_scheme, 0.30f).vec4());
                            if (widgets->button("Clear filters", { panel_width, 30 ds })) {
                                config_ui.filter_esp = false;
                                config_ui.filter_silentaim = false;
                                config_ui.filter_aimbot = false;
                                config_ui.filter_triggerbot = false;
                            }
                            PopStyleColor(3);
                            PopStyleVar();
                        }
                    }
                    EndChild();
                    PopStyleVar();
                }
                widgets->window.end();
            }
        }
}

void c_slate::draw_pinned_overlays() {
    // Watermark and keybind list overlays were removed from the UI.
}
