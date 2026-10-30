// Compatibility layer between the reference ImGui build and code written against
// the 1.90+/1.91 API. Kept in a separate translation unit so the reference widget
// and layout code stays untouched.
#include "imgui.h"
#include "imgui_internal.h"
#include "reference_fonts.hpp"

// The reference widgets use these shared font slots; the overlay fills them once the
// atlas is built (see Overlay.cpp).
namespace font
{
    ImFont* inter_bold = nullptr;
    ImFont* inter_child = nullptr;
    ImFont* manrope_widgets = nullptr;
    ImFont* icon_font = nullptr;
}

namespace RefFonts
{
void Set(ImFont* bold, ImFont* child, ImFont* widgets, ImFont* icons)
{
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    ImFont* fallback = bold ? bold : (atlas && atlas->Fonts.Size > 0 ? atlas->Fonts[0] : nullptr);
    font::inter_bold = fallback;
    font::inter_child = child ? child : fallback;
    font::manrope_widgets = widgets ? widgets : fallback;
    font::icon_font = icons ? icons : fallback;
}
ImFont* Bold() { return font::inter_bold; }
ImFont* Child() { return font::inter_child; }
ImFont* Widgets() { return font::manrope_widgets; }
ImFont* Icons() { return font::icon_font; }
}

namespace ImGui
{

static bool ChildFlagsToBorder(ImGuiChildFlags child_flags)
{
    return (child_flags & (ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_FrameStyle)) != 0;
}

static ImGuiWindowFlags ChildFlagsToWindowFlags(ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags)
{
    if (child_flags & (ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY))
        window_flags |= ImGuiWindowFlags_AlwaysAutoResize;
    if (child_flags & ImGuiChildFlags_AlwaysUseWindowPadding)
        window_flags |= ImGuiWindowFlags_ChildWindow;
    return window_flags;
}

bool BeginChild(const char* str_id, const ImVec2& size, ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* parent_window = g.CurrentWindow;
    const ImGuiID id = parent_window->GetID(str_id);
    return BeginChildEx(str_id, id, size, ChildFlagsToBorder(child_flags), ChildFlagsToWindowFlags(child_flags, window_flags));
}

bool BeginChild(ImGuiID id, const ImVec2& size, ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags)
{
    return BeginChildEx(NULL, id, size, ChildFlagsToBorder(child_flags), ChildFlagsToWindowFlags(child_flags, window_flags));
}

} // namespace ImGui

// Ellipse primitives (1.90+) drawn with the path API of this build.
void ImDrawList::AddEllipse(const ImVec2& center, const ImVec2& radius, ImU32 col, float rot, int num_segments, float thickness)
{
    if ((col & IM_COL32_A_MASK) == 0)
        return;

    if (num_segments <= 0)
        num_segments = ImMax(12, (int)(ImMax(radius.x, radius.y) * 1.5f));

    const float a_max = IM_PI * 2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    const float cs_rot = ImCos(rot), sn_rot = ImSin(rot);

    for (int i = 0; i < num_segments; i++)
    {
        const float a = a_max * (float)i / (float)(num_segments - 1);
        const float x = radius.x * ImCos(a);
        const float y = radius.y * ImSin(a);
        PathLineTo(ImVec2(center.x + x * cs_rot - y * sn_rot, center.y + x * sn_rot + y * cs_rot));
    }
    PathStroke(col, true, thickness);
}

void ImDrawList::AddEllipseFilled(const ImVec2& center, const ImVec2& radius, ImU32 col, float rot, int num_segments)
{
    if ((col & IM_COL32_A_MASK) == 0)
        return;

    if (num_segments <= 0)
        num_segments = ImMax(12, (int)(ImMax(radius.x, radius.y) * 1.5f));

    const float a_max = IM_PI * 2.0f * ((float)num_segments - 1.0f) / (float)num_segments;
    const float cs_rot = ImCos(rot), sn_rot = ImSin(rot);

    for (int i = 0; i < num_segments; i++)
    {
        const float a = a_max * (float)i / (float)(num_segments - 1);
        const float x = radius.x * ImCos(a);
        const float y = radius.y * ImSin(a);
        PathLineTo(ImVec2(center.x + x * cs_rot - y * sn_rot, center.y + x * sn_rot + y * cs_rot));
    }
    PathFillConvex(col);
}
