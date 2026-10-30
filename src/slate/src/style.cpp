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

        themes[0].colors[pcol_bg] = pcolor{ 5, 5, 5, 1.00f };
        themes[0].colors[pcol_bg2] = pcolor{ 8, 8, 8, 1.00f };
        themes[0].colors[pcol_bg3] = pcolor{ 15, 15, 15, 1.00f };
        themes[0].colors[pcol_bg4] = pcolor{ 22, 22, 22, 1.00f };
		themes[0].colors[pcol_bg5] = pcolor{ 232, 232, 232, 1.00f };
		themes[0].colors[pcol_bg6] = pcolor{ 15, 15, 15, 1.00f };
        themes[0].colors[pcol_border] = pcolor{ 255, 255, 255, 0.03f };
        themes[0].colors[pcol_scheme] = pcolor{ 199, 199, 199, 1.00f };
        themes[0].colors[pcol_text] = pcolor{ 231, 231, 231, 1.00f };
        themes[0].colors[pcol_text2] = pcolor{ 76, 76, 76, 1.00f };
        themes[0].colors[pcol_text3] = pcolor{ 76, 76, 76, 0.60f };
		themes[0].colors[pcol_tab] = pcolor{ 21, 21, 21 };
		themes[0].colors[pcol_backdrop] = pcolor{ 255, 255, 255, 0.1f };

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

	auto& style = GImGui->Style;

	style.WindowRounding = 6 ds;
    style.WindowPadding = ImVec2{ 0, 0 };
    style.WindowBorderSize = 1;

    style.FrameRounding = 2 ds;
    style.FramePadding = vec2{ 12, 10 };
    style.FrameBorderSize = 0;

    style.PopupRounding = 2 ds;
    style.PopupBorderSize = 0;

    style.ChildRounding = 4 ds;
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
