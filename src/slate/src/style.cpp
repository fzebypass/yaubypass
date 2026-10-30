#include <slate.h>

void c_style::setup( ) {
	static bool init = false;
	if ( !init )
	{
		themes.resize( 2 );

		for ( auto& t : themes ) {
			t.colors.resize( pcol_count );
		}

		colors.resize( pcol_count );

		themes[0].colors[pcol_scheme] = pcolor{ 173, 104, 219, 1.0 };
		themes[0].colors[pcol_text] = pcolor{ 222, 222, 222 };
		themes[0].colors[pcol_text2] = pcolor{ 77, 77, 77 };
		themes[0].colors[pcol_text3] = pcolor{ 77, 77, 77, 0.6f };
		themes[0].colors[pcol_bg] = pcolor{ 6, 6, 6 };
		themes[0].colors[pcol_bg2] = pcolor{ 11, 11, 11, 0.8 };
		themes[0].colors[pcol_bg3] = pcolor{ 18, 18, 18 };
		themes[0].colors[pcol_bg4] = pcolor{ 22, 22, 22 };
		themes[0].colors[pcol_bg5] = pcolor{ 28, 28, 28 };
		themes[0].colors[pcol_bg6] = pcolor{ 188, 188, 188, 0.05f };
		themes[0].colors[pcol_bg7] = pcolor{ 9, 11, 12 };
		themes[0].colors[pcol_border] = pcolor{ 182, 192, 242, 0.05f };
		themes[0].colors[pcol_separator] = pcolor{ 255, 255, 255, 0.04f };
		themes[0].colors[pcol_checkboxdot] = pcolor{ 38, 38, 40 };
		themes[0].colors[pcol_textonscheme] = pcolor{ 0, 0, 0 };
		themes[0].colors[pcol_listbox] = pcolor{ 16, 16, 16, 0.5f };
		themes[0].colors[pcol_tab] = pcolor{ 21, 21, 21 };
		themes[0].colors[pcol_backdrop] = pcolor{ 255, 255, 255, 0.05f };

        // Exact values of the reference design: translucent near-black window, a cooler
        // panel tone, cards at 39% opacity and text that only lights up when active.
        themes[0].colors[pcol_bg] = pcolor{ 13, 12, 18, 0.90f };
        themes[0].colors[pcol_bg2] = pcolor{ 27, 25, 31, 0.82f };
        themes[0].colors[pcol_bg3] = pcolor{ 46, 41, 55, 1.00f };
        themes[0].colors[pcol_bg4] = pcolor{ 33, 30, 38, 1.00f };
		themes[0].colors[pcol_bg5] = pcolor{ 39, 39, 50, 1.00f };
		themes[0].colors[pcol_bg6] = pcolor{ 33, 30, 38, 1.00f };
        themes[0].colors[pcol_bg7] = pcolor{ 4, 8, 19, 1.00f };
        themes[0].colors[pcol_border] = pcolor{ 46, 41, 55, 1.00f };
        themes[0].colors[pcol_separator] = pcolor{ 255, 255, 255, 0.05f };
        themes[0].colors[pcol_checkboxdot] = pcolor{ 39, 39, 50, 1.00f };
        themes[0].colors[pcol_listbox] = pcolor{ 33, 30, 38, 1.00f };
        themes[0].colors[pcol_scheme] = pcolor{ 226, 186, 96, 1.00f };
        themes[0].colors[pcol_text] = pcolor{ 68, 67, 79, 1.00f };
        themes[0].colors[pcol_text2] = pcolor{ 98, 97, 109, 1.00f };
        themes[0].colors[pcol_text3] = pcolor{ 68, 67, 79, 0.65f };
		themes[0].colors[pcol_tab] = pcolor{ 4, 8, 19, 1.00f };
		themes[0].colors[pcol_backdrop] = pcolor{ 13, 12, 18, 0.90f };

		themes[1].colors[pcol_bg] = pcolor{ 255, 255, 255, 1.00f };
        themes[1].colors[pcol_bg2] = pcolor{ 239, 239, 239, 1.00f };
        themes[1].colors[pcol_bg3] = pcolor{ 252, 252, 252, 1.00f };
        themes[1].colors[pcol_bg4] = pcolor{ 212, 212, 212, 1.00f };
        themes[1].colors[pcol_bg5] = pcolor{ 213, 213, 213, 1.00f };
        themes[1].colors[pcol_bg6] = pcolor{ 239, 239, 239, 1.00f };
        themes[1].colors[pcol_border] = pcolor{ 0, 0, 0, 0.12f };
        themes[1].colors[pcol_scheme] = pcolor{ 27, 27, 27, 1.00f };
        themes[1].colors[pcol_text] = pcolor{ 15, 15, 15, 1.00f };
        themes[1].colors[pcol_text2] = pcolor{ 156, 156, 156, 1.00f };
        themes[1].colors[pcol_text3] = pcolor{ 156, 156, 156, 0.60f };
		themes[1].colors[pcol_tab] = pcolor{ 255, 255, 255 };
		themes[1].colors[pcol_backdrop] = pcolor{ 0, 0, 0, 0.2f };

		init = true;
	}

	for ( int i = 0; i < colors.size( ); ++i ) {
		colors[i].r = ImLerp( colors[i].r, themes[theme].colors[i].r, ImGui::GetIO( ).DeltaTime * 17 );
		colors[i].g = ImLerp( colors[i].g, themes[theme].colors[i].g, ImGui::GetIO( ).DeltaTime * 17 );
		colors[i].b = ImLerp( colors[i].b, themes[theme].colors[i].b, ImGui::GetIO( ).DeltaTime * 17 );
		colors[i].a = ImLerp( colors[i].a, themes[theme].colors[i].a, ImGui::GetIO( ).DeltaTime * 17 );
	}

	GImGui->Style.Colors[ImGuiCol_Text] = colors[pcol_text];
	GImGui->Style.Colors[ImGuiCol_TextDisabled] = colors[pcol_text2];
	GImGui->Style.Colors[ImGuiCol_WindowBg] = colors[pcol_bg];
	GImGui->Style.Colors[ImGuiCol_ChildBg] = colors[pcol_bg2];
	GImGui->Style.Colors[ImGuiCol_Border] = colors[pcol_border];
	GImGui->Style.Colors[ImGuiCol_TextSelectedBg] = colors[pcol_scheme].alpha( 0.3f );
	GImGui->Style.Colors[ImGuiCol_WindowShadow] = pcolor{ 0.f, 0.f, 0.f, 0.0f };

	auto& style = GImGui->Style;    // Geometry follows the reference design: a 6px card radius, 2px frames and
    // a uniform 20px rhythm so cards line up on every page.
    style.WindowRounding = 6 ds;
    style.WindowPadding = ImVec2{ 0, 0 };
    style.WindowBorderSize = 1;

    style.FrameRounding = 2 ds;
    style.FramePadding = vec2{ 12, 10 };
    style.FrameBorderSize = 0;

    style.PopupRounding = 6 ds;
    style.PopupBorderSize = 0;

    style.ChildRounding = 6 ds;
    style.ChildBorderSize = 1;

    style.ItemSpacing = vec2{ 14, 14 };
    style.ItemInnerSpacing = vec2{ 10, 4 };

	style.ScrollbarRounding = 4;
    style.ScrollbarSize = 4;
    style.WindowMinSize = ImVec2{ 1, 1 };
	style.AntiAliasedLines = true;
	style.AntiAliasedLinesUseTex = true;
	style.AntiAliasedFill = true;
	style.CurveTessellationTol = 1.10f;
	style.CircleTessellationMaxError = 0.10f;
}



void c_style::push( pcol_ col, pcolor newcol )
{
	pushcache.push_back( { col, g_style->col( col ) } );
	colors[col] = newcol;
}
void c_style::pop( int k )
{
	for ( int i = 0; i < k; ++i ) {
		auto& col = pushcache[pushcache.size( ) - 1];
		colors[col.col] = col.oldcol;
		pushcache.pop_back( );
	}
}
