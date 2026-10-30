#pragma once

// The reference widgets keep their fonts in a few shared globals. `font` cannot be
// used as a namespace name in translation units that also see the UI framework's
// `font` variable, so the slots are filled through this tiny accessor instead.
struct ImFont;

namespace RefFonts
{
    void Set(ImFont* bold, ImFont* child, ImFont* widgets, ImFont* icons);
    ImFont* Bold();
    ImFont* Child();
    ImFont* Widgets();
    ImFont* Icons();
}
