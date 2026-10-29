// Redirector: use the slate_imgui version as the single imgui source of truth
#pragma once
#include <imgui.h>

// Compatibility shims for field renames and removed flags
#define InFlags ItemFlags
#define ImGuiWindowFlags_WithoutScrollClamp (ImGuiWindowFlags)0
#define ImGuiButtonFlags_Repeat (ImGuiButtonFlags)0

namespace ImGui {

    IMGUI_API bool ListSelectable(const char* label, bool* p_selected);
    IMGUI_API bool Keybind(const char* label, int* key, int* mode);
//  IMGUI_API bool BeginContent(const char* str_id, const ImVec2& size_arg = ImVec2(0, 0), bool border = false, ImGuiWindowFlags extra_flags = 0);
    IMGUI_API void EndContent();
}
