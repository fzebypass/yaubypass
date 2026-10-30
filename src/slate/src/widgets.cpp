#define NOMINMAX
#include <slate.h>
#include <imgui_freetype.h>
#include <thread>
#pragma comment(lib, "winmm.lib")

using namespace ImGui;

vec2::operator ImVec2( ) const {
	return ImVec2{ x, y } ds;
}

extern c_image logo;
void c_font::setup( unsigned char* data, size_t data_size, std::vector< float > sizes, const ImWchar* ranges ) {
	auto* atlas = ImGui::GetIO( ).Fonts;
	atlas->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType( );
	atlas->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_LightHinting;
	atlas->TexGlyphPadding = ImMax( atlas->TexGlyphPadding, 2 );

	auto config = ImFontConfig( );
	config.FontDataOwnedByAtlas = false;
	config.FontBuilderFlags = atlas->FontBuilderFlags;
	config.OversampleH = 3;
	config.OversampleV = 2;
	config.PixelSnapH = true;
	config.RasterizerDensity = 1.25f;
	config.RasterizerMultiply = 1.05f;

	for ( auto& sz : sizes ) {
		fonts.push_back( { sz, ImGui::GetIO( ).Fonts->AddFontFromMemoryTTF( data, data_size, sz * g_style->dpi_scale, &config, ranges ) } );
	}
}

bool c_widgets::button( const ptext& label, ImVec2 size, c_buttonstyle style ) 
{
	struct s {
		float hover;
		float held;
	}; auto& obj = anim_obj( label.str.data( ), 0, s{ } );

	auto* window = GetCurrentWindow( );
	bool pressed = InvisibleButton( label.str.data( ), CalcItemSize( size, GImGui->Style.FramePadding.x * 2 + CalcTextSize( label.translate( ).data( ), 0, 1 ).x + ( ( style.iconsize + 8 ) ds ) * bool( style.icon ), GetFrameHeight( ) ) );
	bool hovered = IsItemHovered( ), held = IsItemActive( );
	ImRect& bb = GImGui->LastItemData.Rect;

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.held = anim( obj.held, 0.f, 1.f, held );

	pcolor iconcolor, labelcolor;
	switch ( style.style ) {
	case button_default:
	{
		iconcolor = g_style->col( pcol_bg );
		labelcolor = g_style->col( pcol_bg );

		window->DrawList->AddRectFilled( bb.Min, bb.Max, col_anim( g_style->col( pcol_scheme ), g_style->col( pcol_scheme, 0.7f ), obj.hover ), style.rounding );
	}
	break;
	}

	float spacing = style.iconsize + 8;
	ImVec2 label_pos = bb.GetCenter( ) - CalcTextSize( label.translate( ).data( ), 0, 1 ) / 2;
	if ( style.icon )
	{
		label_pos.x -= ( spacing / 2 ) ds;
		g_draw->text( icons, style.iconsize, { label_pos.x + 8 ds + CalcTextSize( label.translate( ).data( ), 0, 1 ).x, bb.GetCenter( ).y - ( style.iconsize ds ) / 2 }, iconcolor, style.icon );
	}

	window->DrawList->AddText( label_pos, labelcolor, label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );

	return pressed;
}
bool c_widgets::checkbox( const char* icon, const ptext& label, bool* v, c_checkboxstyle style ) 
{
    g_search->add_item( { label.str, [=] { checkbox( icon, label, v, style ); } } );
    
    struct s {
        float anim;
        float hover;
        float enabled;
        ImVec2 optsize;
    }; auto& obj = anim_obj( label.str.data( ), 0, s{ } );
    
    auto* window = GetCurrentWindow( );
    
    float square_sz = 16 ds;
    
    bool pressed = InvisibleButton( label.str.data( ), { CalcItemWidth( ), square_sz } );
    bool hovered = IsItemHovered( ), held = IsItemActive( );
    ImRect total_bb = GImGui->LastItemData.Rect;
    ImRect bb{ { total_bb.Max.x - 24 ds, total_bb.Max.y - 16 ds }, { total_bb.Max.x - 0 ds, total_bb.Max.y } };
    
    obj.hover = anim( obj.hover, 0.f, 1.f, hovered && !*v );
    obj.enabled = anim( obj.enabled, 0.f, 1.f, *v );
    obj.anim = anim( obj.anim, 0.f, 1.f, hovered || *v );
    
    if ( pressed ) {
        *v = !*v;
    }
    
    auto bgcol = col_anim( col_anim( g_style->col( pcol_bg3 ), g_style->col( pcol_bg4 ), obj.hover ), g_style->col( pcol_scheme ), obj.enabled );
    auto col = col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_text ), obj.enabled );
    
    window->DrawList->AddRectFilled( bb.Min, bb.Max, col_anim( col_anim( g_style->col( pcol_bg3 ), g_style->col( pcol_bg4 ), obj.hover ), g_style->col( pcol_scheme ), obj.enabled ), 8 ds );
    window->DrawList->AddRect( bb.Min, bb.Max, col_anim( g_style->col( pcol_border ), g_style->col( pcol_border, 0.f ), obj.enabled ), 8 ds );
    g_draw->text( icons, 16, total_bb.Min, g_style->col( pcol_scheme ), icon );
    window->DrawList->AddText( total_bb.Min + vec2{ 24, 0 }, col, label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );
    window->DrawList->AddRectFilled( bb.Min + ImVec2{ 3 ds + ( bb.GetWidth( ) - bb.GetHeight( ) ) * obj.enabled, 3 ds }, bb.Min + ImVec2{ 3 ds + ( bb.GetWidth( ) - bb.GetHeight( ) ) * obj.enabled, 3 ds } + vec2{ 10, 10 } + vec2{ 2, 0 } * obj.hover, col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_bg3 ), obj.enabled ), 5, 240 );
    
    auto pos = window->DC.CursorPos;
    auto posprev = window->DC.CursorPosPrevLine;
    
    window->DC.CursorPos = ImVec2{ bb.Min.x - obj.optsize.x - 8 ds, bb.GetCenter( ).y - obj.optsize.y / 2 };
    char temp[128];
    ImFormatString( temp, sizeof( temp ), "%s opt", label.str.data( ) );
    PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 1, 1 } );
    BeginChild( temp, { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground );
    {
        if ( style.key ) {
            ImFormatString( temp, sizeof( temp ), "##%s key", label.str.data( ) );
            widgets->binder( 0, temp, style.key );
            SameLine( 0, 8 ds );
        }
        if ( style.col.size( ) > 0 ) {
            for ( int i = 0; i < style.col.size( ); ++i ) {
                ImFormatString( temp, sizeof( temp ), "##%s col %d", label.str.data( ), i );
                SetCursorPosY( GetWindowHeight( ) / 2 - 6 ds );
                widgets->coloredit( 0, temp, style.col[i], vec2{ 12, 12 } );
                SameLine( 0, 8 ds );
            }
        }
        if ( style.options ) {
            ImFormatString( temp, sizeof( temp ), "##%s settings", label.str.data( ) );
            SetCursorPosY( GetWindowHeight( ) / 2 - 8 ds );
            widgets->optionsbtn( temp, style.options );
            SameLine( 0, 8 ds );
        }
        if ( style.tooltipicon ) {
            ImFormatString( temp, sizeof( temp ), "##%s tt", label.str.data( ) );
            widgets->tooltipbtn( temp, style.tooltiptext, style.tooltipicon );
            SameLine( 0, 8 ds );
        }
        
        obj.optsize = GetWindowSize( );
    }
    EndChild( );
    PopStyleVar( );
    
    window->DC.CursorPos = pos;
    window->DC.CursorPosPrevLine = posprev;
    
    return pressed;
}
bool c_widgets::multicombo( const ptext& label, bool* v, std::vector< std::string_view > items )
{
	return multicombo( "", label, v, items );
}
template <typename T>
bool c_widgets::slider(const char* icon, const ptext& label, T* v, T min, T max, const char* format, c_sliderstyle style)
{
	struct s {
		float anim;
		float hover;
		float held;
		float val_anim;
		bool washeld = false;
	};
	auto& obj = anim_obj(label.str.data(), 1120, s{});

	auto* window = GetCurrentWindow();
	auto id = window->GetID(label.str.data());
	ImRect total_bb{ window->DC.CursorPos, window->DC.CursorPos + ImVec2{ CalcItemWidth(), 30 ds } };
	ImRect bb{ { total_bb.Min.x, total_bb.Max.y - 6.f ds }, { total_bb.Max.x, total_bb.Max.y } };
	ItemSize(total_bb);
	ItemAdd(total_bb, id);

	bool result = false;

	auto data = GImGui->LastItemData;

	bool hovered, held;
	ButtonBehavior(total_bb, id, &hovered, &held);

	obj.hover = anim(obj.hover, 0.f, 1.f, hovered);
	obj.held = anim(obj.held, 0.f, 1.f, held);
	obj.anim = anim(obj.anim, 0.f, 1.f, hovered || held);
	obj.val_anim = ImLerp(
		obj.val_anim,
		(ImClamp(*v, min, max) - min * 1.f) / (max - min) * bb.GetWidth(),
		GetIO().DeltaTime * 17
	);

	if (held) {
		*v = ImClamp(T(min + (GetIO().MousePos.x - bb.Min.x) / bb.GetWidth() * (max - min)), min, max);
		if (!obj.washeld)
			obj.washeld = true;
	}
	else {
		if (obj.washeld) {
			obj.washeld = false;
			result = true;
		}
	}

	window->DrawList->AddRectFilled(bb.Min, bb.Max,
		col_anim(col_anim(g_style->col(pcol_bg3), g_style->col(pcol_bg4), obj.hover),
			g_style->col(pcol_bg4), obj.held), 3 ds);
	window->DrawList->AddRect(bb.Min, bb.Max, g_style->col(pcol_border), 3 ds);
	window->DrawList->AddRectFilled(bb.Min, { bb.Min.x + obj.val_anim, bb.Max.y },
		col_anim(col_anim(g_style->col(pcol_scheme), g_style->col(pcol_scheme, 0.6f), obj.hover),
			g_style->col(pcol_scheme, 0.6f), obj.held), 3 ds);

	g_draw->gradient(window->DrawList, bb.Min, { bb.Min.x + obj.val_anim, bb.Max.y },
		pcolor{ 0, 0, 0, 0.30f },
		pcolor{ 0, 0, 0, 0.00f },
		pcolor{ 0, 0, 0, 0.00f },
		pcolor{ 0, 0, 0, 0.30f }, 3 ds);

	window->DrawList->AddCircleFilled(
		{ bb.Min.x + obj.val_anim, bb.GetCenter().y },
		8 ds + obj.anim - 2 * obj.held,
		g_style->col(pcol_bg5)
	);

	g_draw->text(icons, 16, total_bb.Min, g_style->col(pcol_scheme), icon);
	window->DrawList->AddText(
		total_bb.Min + vec2{ 24, 0 },
		g_style->col(pcol_text),
		label.translate().data(),
		FindRenderedTextEnd(label.translate().data())
	);

	char buf[32];
	ImFormatString(buf, sizeof(buf), format, *v);

	if constexpr (std::is_floating_point_v<T>) {
		for (char* p = buf; *p; ++p) {
			if (*p == '.')
				*p = ',';
		}
	}

	auto pos = window->DC.CursorPos;
	window->DC.CursorPos = ImVec2{ total_bb.Max.x - CalcTextSize(buf).x, total_bb.Min.y };

	char temp[128];
	ImFormatString(temp, sizeof(temp), "##%s", label.str.data());

	BeginChild(temp, CalcTextSize(buf), 0, ImGuiWindowFlags_NoBackground);
	PushStyleColor(ImGuiCol_FrameBg, GetColorU32(ImGuiCol_FrameBg, 0));
	PushStyleColor(ImGuiCol_Text, g_style->col(pcol_text2).vec4());
	PushStyleVar(ImGuiStyleVar_FramePadding, { 0, 0 });

	if (InputTextEx(temp, "", buf, sizeof(buf), CalcTextSize(buf), ImGuiInputTextFlags_NoHorizontalScroll)) {
		if constexpr (std::is_floating_point_v<T>) {
			for (char* p = buf; *p; ++p) {
				if (*p == ',')
					*p = '.';
			}
		}

		DataTypeApplyFromText(
			buf,
			std::is_same_v<T, int> ? ImGuiDataType_S32 : ImGuiDataType_Float,
			v,
			format
		);

		*v = ImClamp(*v, min, max);
	}

	PopStyleVar();
	PopStyleColor(2);
	EndChild();
	window->DC.CursorPos = pos;

	GImGui->LastItemData = data;

	return result;
}

bool c_widgets::sliderint( const char* icon, const ptext& label, int* v, int min, int max, const char* format ) 
{
	bool result = slider( icon, label, v, min, max, format );
	keybinds.popup( label.str, v, ht_sliderint, min, max );
	g_search->add_item( { label.str, [=] { sliderint( icon, label, v, min, max, format ); } } );

	return result;
}
bool c_widgets::sliderfloat( const char* icon, const ptext& label, float* v, float min, float max, const char* format ) 
{
	bool result = slider( icon, label, v, min, max, format );
	keybinds.popup( label.str, v, ht_sliderfloat, min, max );
	g_search->add_item( { label.str, [=] { sliderfloat( icon, label, v, min, max, format ); } } );

	return result;
}

bool c_widgets::coloredit( const char* icon, const ptext& label, float* col, ImVec2 size )
{
	struct s {
		float anim;
		float hover;
		float open_anim;
		bool open;
	}; auto& obj = anim_obj( label.str.data( ), 0, s{ } );

	if ( size == ImVec2{ 0, 0 } ) size = vec2{ 16, 16 };

	auto window = GetCurrentWindow( );
	auto id = window->GetID( label.str.data( ) );
	ImRect total_bb{ window->DC.CursorPos, window->DC.CursorPos + ImVec2{ CalcTextSize( label.str.data( ), 0, 1 ).x > 0 ? CalcItemWidth( ) : size.x, size.y } };
	ImRect bb{ total_bb.Max - size, total_bb.Max };
	ItemSize( total_bb );
	ItemAdd( total_bb, id );

	bool hovered, held;
	bool pressed = ButtonBehavior( bb, id, &hovered, &held );
	bool value_changed = false;

	if ( pressed ) {
		obj.open = !obj.open;
	}

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.open_anim = anim( obj.open_anim, 0.f, 1.f, obj.open );
	obj.anim = anim( obj.anim, 0.f, 1.f, hovered || obj.open );

	if ( CalcTextSize( label.str.data( ), 0, 1 ).x > 0 ) {
		g_draw->text( icons, 16, total_bb.Min, g_style->col( pcol_scheme ), icon );
		window->DrawList->AddText( { total_bb.Min.x + 24 ds, total_bb.GetCenter( ).y - GImGui->FontSize / 2 }, GetColorU32( ImGuiCol_Text ), label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );
	}

	window->DrawList->AddRectFilled( bb.Min, bb.Max, ImColor{ col[0], col[1], col[2], GImGui->Style.Alpha * 0.2f }, 2 ds );
	window->DrawList->AddRect( bb.Min, bb.Max, ImColor{ col[0], col[1], col[2], GImGui->Style.Alpha }, 2 ds );

	if ( popup.begin( label.str, obj.open_anim, bb.Min, vec2{ 14, 14 } ) ) {
		if ( ( !IsWindowHovered( ImGuiHoveredFlags_AnyWindow ) || ( GImGui->HoveredWindow && !strstr( GImGui->HoveredWindow->Name, "popup" ) ) || FindWindowDisplayIndex( GImGui->HoveredWindow ) < FindWindowDisplayIndex( GetCurrentWindow( ) ) ) && IsMouseClicked( 0 ) ) {
			obj.open = false;
		}

		PushStyleVar( ImGuiStyleVar_ItemSpacing, vec2{ 6, 6 } );
		colorpicker.draw( label.str, col );
		PopStyleVar( );

		popup.end( );
	}

	return value_changed;
}
bool c_widgets::optionsbtn( const std::string_view& str_id, std::function< void( ) > options, ImVec2 windowpos, vec2 padding )
{
	struct s {
		float anim;
		float hover;
		float open_anim;
		bool open = false;
		bool closed = false;
	}; auto& obj = anim_obj( str_id.data( ), 12210, s{ } );

	auto window = GetCurrentWindow( );
	auto id = window->GetID( str_id.data( ) );
	ImRect bb = ImRect{ window->DC.CursorPos, window->DC.CursorPos + vec2{ 16, 16 } };

	ItemSize( bb );
	ItemAdd( bb, id );

	bool hovered, held;
	bool pressed = ButtonBehavior( bb, id, &hovered, &held );

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.open_anim = anim( obj.open_anim, 0.f, 1.f, obj.open );
	obj.anim = anim( obj.anim, 0.f, 1.f, hovered || obj.open );

	if ( pressed && !obj.closed ) {
		obj.open = true;
	}

	if ( !IsMouseDown( 0 ) ) {
		obj.closed = false;
	}

	auto col = col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_text ), obj.open_anim );
	g_draw->rotatestart( );
	g_draw->text( icons, 16, bb.Min, pcolor{ col }, I_SETTINGS );
	g_draw->rotateend( IM_PI / 2 - IM_PI * obj.open_anim, g_draw->rotationcenter( ) );

	if ( windowpos == ImVec2{ 0, 0 } ) {
		windowpos = ImVec2{ bb.Min.x, bb.Max.y + 8 ds };
	}

	if ( popup.begin( str_id, obj.open_anim, windowpos, padding ) ) {
		if ( ( !IsWindowHovered( ImGuiHoveredFlags_AnyWindow ) || ( GImGui->HoveredWindow && !strstr( GImGui->HoveredWindow->Name, "popup" ) ) ) && IsMouseClicked( 0 ) ) {
			obj.open = false;
			obj.closed = true;
		}

		PushItemWidth( 180 ds );
		PushItemFlag( ImGuiItemFlags_NoNav, true );
		options( );
		PopItemFlag( );
		PopItemWidth( );
		popup.end( );
	}

	return pressed;
}
bool c_widgets::iconbutton( const char* str_id, const char* icon )
{
	struct s {
		float anim = 0;
		float hover = 0;
		float held = 0;
	}; auto& obj = anim_obj( str_id, 0, s{ } );

	auto window = GetCurrentWindow( );
	auto id = window->GetID( str_id );
	ImRect bb{ window->DC.CursorPos, window->DC.CursorPos + vec2{ 16, 16 } };
	ItemSize( bb );
	ItemAdd( bb, id );

	bool hovered, held;
	bool pressed = ButtonBehavior( bb, id, &hovered, &held );

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.held = anim( obj.held, 0.f, 1.f, held );
	obj.anim = anim( obj.anim, 0.f, 1.f, hovered || held );

	auto col = col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_text3, 0.6f ), obj.held );

	g_draw->text( icons, 16, bb.Min, col, icon );

	return pressed;
}
bool c_widgets::tooltipbtn( const std::string_view& str_id, const ptext& tooltip, const char* icon )
{
	struct s {
		float anim;
		float hover;
	}; auto& obj = anim_obj( str_id.data( ), 0, s{ } );

	auto window = GetCurrentWindow( );
	auto id = window->GetID( str_id.data( ) );
	ImRect bb = ImRect{ window->DC.CursorPos, window->DC.CursorPos + vec2{ 16, 16 } };

	ItemSize( bb );
	ItemAdd( bb, id );

	bool hovered, held;
	bool pressed = ButtonBehavior( bb, id, &hovered, &held );

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );

	auto col = col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover );
	g_draw->text( icons, 16, bb.Min, pcolor{ col }, icon );

	if ( obj.hover > 0.05f ) {
		GetForegroundDrawList( )->AddRectFilled( { bb.Min.x, bb.Max.y + 4 ds }, ImVec2{ bb.Min.x, bb.Max.y } + vec2{ 16, 20 } + CalcTextSize( tooltip.translate( ).data( ) ), g_style->col( pcol_bg3, obj.hover ), 3 ds );
		GetForegroundDrawList( )->AddRect( { bb.Min.x, bb.Max.y + 4 ds }, ImVec2{ bb.Min.x, bb.Max.y } + vec2{ 16, 20 } + CalcTextSize( tooltip.translate( ).data( ) ), g_style->col( pcol_border, obj.hover ), 3 ds );
		GetForegroundDrawList( )->AddText( ImVec2{ bb.Min.x, bb.Max.y } + vec2{ 8, 12 }, g_style->col( pcol_text, obj.hover ), tooltip.translate( ).data( ) );
	}

	return pressed;
}

bool c_widgets::comboex( const char* icon, const ptext& label, const std::string_view& preview, bool should_close, c_combostyle style ) 
{
    struct s {
        float anim;
        float hover;
        float held;
        float open_anim;
        bool open;
        float w;
    }; auto& obj = anim_obj( label.str.data( ), 123123443, s{ } );
    
    bool result = false;
    
    obj.w = ImLerp( obj.w, g_draw->textsize( font, 16, preview.data( ) ).x, GetIO( ).DeltaTime * 17 );
    
    ImVec2 padding{ 8 ds, 7 ds };
    
    auto* window = GetCurrentWindow( );
    auto id = window->GetID( label.str.data( ) );
    ImRect total_bb{ window->DC.CursorPos, window->DC.CursorPos + ImVec2{ CalcItemWidth( ), 54 ds } };
    ImRect bb{ { total_bb.Min.x, total_bb.Max.y - 30 ds }, { total_bb.Max.x, total_bb.Max.y } };
    
    ItemSize( total_bb );
    ItemAdd( total_bb, id );
    
    bool hovered, held;
    bool pressed = ButtonBehavior( bb, id, &hovered, &held );

    if ( pressed ) {
        obj.open = !obj.open;
    }
    
    obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
    obj.held = anim( obj.held, 0.f, 1.f, held );
    obj.anim = anim( obj.anim, 0.f, 1.f, obj.open || hovered );
    obj.open_anim = anim( obj.open_anim, 0.f, 1.f, obj.open );
    
    const bool has_icon = icon && icon[0] != '\0';

    window->DrawList->AddRectFilled( bb.Min, bb.Max, col_anim( col_anim( g_style->col( pcol_bg3 ), g_style->col( pcol_bg4 ), obj.hover ), g_style->col( pcol_bg4 ), obj.open_anim ), 3 ds );
    window->DrawList->AddRect( bb.Min, bb.Max, g_style->col( pcol_border ), 3 ds );
    if ( has_icon ) {
        g_draw->text( icons, 16, total_bb.Min, g_style->col( pcol_scheme ), icon );
    }
    window->DrawList->AddText( total_bb.Min + vec2{ has_icon ? 24.f : 0.f, 0 }, g_style->col( pcol_text ), label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );
    window->DrawList->AddText( bb.Min + padding, g_style->col( pcol_text ), preview.data( ) );
    if ( style.rotate ) g_draw->rotatestart( );
    GetWindowDrawList( )->AddText( fonts[icons].get( 12 ), fonts[icons].get( 12 )->FontSize, { bb.Max.x - 12 ds - padding.x, bb.GetCenter( ).y - fonts[icons].get( 12 )->FontSize / 2 }, g_style->col( pcol_scheme ), style.icon );
    if ( style.rotate ) g_draw->rotateend( IM_PI / 2 - IM_PI * obj.open_anim, g_draw->rotationcenter( ) );
    
    
    if ( obj.open_anim > 0.05f ) {
        char temp[64];
        ImFormatString( temp, sizeof( temp ), "%s popup", label.str.data( ) );
        PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 0, 4 } );
        PushStyleVar( ImGuiStyleVar_WindowRounding, style.rounding );
        PushStyleVar( ImGuiStyleVar_WindowBorderSize, 1 );
        PushStyleVar( ImGuiStyleVar_ItemSpacing, vec2{ 0, 0 } );
        PushStyleVar( ImGuiStyleVar_Alpha, GImGui->Style.Alpha * obj.open_anim );
        PushStyleColor( ImGuiCol_WindowBg, g_style->col( pcol_bg3 ).vec4( ) );
        Begin( temp, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );
        SetWindowSize( { ImMax( GetCurrentWindow( )->ContentSize.x, bb.GetWidth( ) ), ImMin( GetCurrentWindow( )->ContentSize.y + GImGui->Style.WindowPadding.y * 2 + 2 ds, 200.f ds ) * obj.open_anim } );
        SetWindowPos( { bb.Max.x - GetWindowWidth( ), bb.GetCenter( ).y - GetWindowHeight( ) / 2 } );
        BringWindowToDisplayFront( GetCurrentWindow( ) );
        BringWindowToFocusFront( GetCurrentWindow( ) );
        
        if ( GImGui->HoveredWindow && !strstr( GImGui->HoveredWindow->Name, "popup" ) && IsMouseClicked( 0 ) && !hovered || should_close ) {
            obj.open = false;
        }   
        
        return true;
    }
    
    return false;
}

bool c_widgets::combo( const char* icon, const ptext& label, int* v, std::vector< std::string_view > items ) 
{
	g_search->add_item( { label.str, [=] { combo( icon, label, v, items ); } } );

	struct s {
		bool should_close;
	}; auto& obj = anim_obj( label.str.data( ), 0, s{ } );
	float maxw = 0;
	for ( int i = 0; i < items.size( ); ++i ) {
		maxw = ImMax( CalcTextSize( items[i].data( ), 0, 1 ).x + 48 ds, maxw );
	}

	if ( comboex( icon, label, items[*v], obj.should_close ) ) {
		if ( obj.should_close ) obj.should_close = false;

		for ( int i = 0; i < items.size( ); ++i ) {
			if ( selectable( items[i], *v == i, { ImMax( maxw, GetWindowWidth( ) - GImGui->Style.WindowPadding.x * 2 ), 28 ds } ) ) {
				*v = i;
				obj.should_close = true;
			}
		}

		End( );
		PopStyleColor( );
		PopStyleVar( 5 );
	}

	return obj.should_close;
}
bool c_widgets::multicombo( const char* icon, const ptext& label, bool* v, std::vector< std::string_view > items ) 
{
	g_search->add_item( { label.str, [=] { multicombo( icon, label, v, items ); } } );

	auto& style = GetStyle( );

    std::string buf;

    buf.clear( );
    for ( size_t i = 0; i < items.size( ); ++i ) {
        if ( v[i] ) {
			buf += ptext{ items[i] }.translate( );
            buf += ", ";
        }
    }

    if ( !buf.empty( ) ) {
        buf.resize( buf.size( ) - 2 );
    }

	float maxw = 0;
	for ( int i = 0; i < items.size( ); ++i ) {
		maxw = ImMax( CalcTextSize( items[i].data( ), 0, 1 ).x + 48 ds, maxw );
	}

    if ( CalcTextSize( buf.c_str( ) ).x > 100 ds ) {
        for ( int i = 0; i < buf.size( ) - 1; ++i ) {
            if ( CalcTextSize( buf.substr( 0, i + 1 ).c_str( ) ).x > 100 ds ) {
                buf.resize( i );
                if ( buf[buf.size( ) - 1] == ',' ) {
                    buf.resize( buf.size( ) - 1 );
                }
                buf.append( ".." );
            }
        }
    }

	if ( buf == "" ) { 
		buf = "None";
	}

	bool result = false;
	if ( comboex( icon, label, buf.c_str( ) ) ) {
		for ( int i = 0; i < items.size( ); ++i ) {
			if ( selectable( items[i], v[i], { ImMax( maxw, GetWindowWidth( ) ), 28 ds } ) ) {
				v[i] = !v[i];
				result = true;
			}
		}

		End( );
		PopStyleColor( );
		PopStyleVar( 5 );
	}

	return result;
}
bool c_widgets::binder( const char* icon, const ptext& label, int* key )
{
	struct s {
		float anim;
		float hover;
		float active_anim;
		float w;
		bool active = false;
	}; auto& obj = anim_obj( label.str.data( ), 0, s{ } );
	std::string_view buf = obj.active ? "..." : keys[*key];
	obj.w = ImLerp( obj.w, g_draw->textsize( font, 14, buf.data( ) ).x, GetIO( ).DeltaTime * 17 );

	auto window = GetCurrentWindow( );
	auto id = window->GetID( label.str.data( ) );
	ImRect total_bb{ window->DC.CursorPos, window->DC.CursorPos + ( CalcTextSize( label.str.data( ), 0, 1 ).x > 0 ? ImVec2{ CalcItemWidth( ), 34 ds } : ImVec2{ 42 ds + obj.w, 34 ds } ) };
	ImRect bb{ total_bb.Max - ImVec2{ ( 42 ds + int( obj.w ) ), 34 ds }, total_bb.Max };
	ItemSize( total_bb );
	ItemAdd( total_bb, id );

	bool hovered, held;
	bool pressed = ButtonBehavior( bb, id, &hovered, &held );
	bool value_changed = false;

	if ( pressed ) {
		obj.active = true;
		*key = 7;
	}

	if ( obj.active ) {
		for ( auto i = 0; i < 5; i++ ) {
			if ( GetIO( ).MouseDown[i] ) {
				switch ( i ) {
				case 0:
					*key = VK_LBUTTON;
					break;
				case 1:
					*key = VK_RBUTTON;
					break;
				case 2:
					*key = VK_MBUTTON;
					break;
				case 3:
					*key = VK_XBUTTON1;
					break;
				case 4:
					*key = VK_XBUTTON2;
				}
				value_changed = true;
				obj.active = false;
			}
		}

		if ( !value_changed ) {
			for ( auto i = VK_BACK; i <= VK_RMENU; i++ ) {
				if ( GetAsyncKeyState( i ) & 0x8000 ) {
					*key = i == VK_ESCAPE ? 0 : i;
					value_changed = true;
					obj.active = false;
				}
			}
		}
	}

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.active_anim = anim( obj.active_anim, 0.f, 1.f, obj.active );
	obj.anim = anim( obj.anim, 0.f, 1.f, hovered || obj.active );

	auto bgcol = col_anim( g_style->col( pcol_bg3 ), g_style->col( pcol_bg4 ), obj.anim );
	window->DrawList->AddRectFilled( bb.Min, bb.Max, bgcol, 2 ds );
	g_draw->text( icons, 16, bb.Min + vec2{ 9, 9 }, g_style->col( pcol_scheme ), I_KEYBOARD );
	g_draw->text( font, 14, bb.Min + vec2{ 33, 10 }, g_style->col( pcol_text ), buf.data( ) );

	if ( CalcTextSize( label.str.data( ), 0, 1 ).x > 0 ) {
		if ( icon ) g_draw->text( icons, 16, { total_bb.Min.x, total_bb.GetCenter( ).y - GImGui->FontSize / 2 }, g_style->col( pcol_scheme ), icon );
		window->DrawList->AddText( { total_bb.Min.x + 24 ds, total_bb.GetCenter( ).y - GImGui->FontSize / 2 }, g_style->col( pcol_text ), label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );
	}

	return value_changed;
}
bool c_widgets::selector( const ptext& label, int* v, const std::vector< std::string_view >& items )
{
	struct s {
		ImVec2 size;
	}; auto& obj = anim_obj( label.str.data( ), 0, s{ } );

	bool result = false;

	GetWindowDrawList( )->AddText( GetCurrentWindow( )->DC.CursorPos + ImVec2{ 0, obj.size.y / 2 - GImGui->FontSize / 2 }, g_style->col( pcol_text ), label.translate( ).data( ) );
	Dummy( CalcTextSize( label.translate( ).data( ) ) );
	SameLine( CalcItemWidth( ) - obj.size.x + GImGui->Style.WindowPadding.x );
	PushStyleColor( ImGuiCol_ChildBg, g_style->col( pcol_bg3 ).vec4( ) );
	PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 2, 2 } );
	PushStyleVar( ImGuiStyleVar_ChildRounding, 3 ds );
	BeginChild( label.str.data( ), { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border );
	{
		obj.size = GetWindowSize( );

		for ( int i = 0; i < items.size( ); ++i ) {
			if ( selectable( items[i], *v == i, { 0, CalcTextSize( ptext{ items[i] }.translate( ).data( ) ).y + 4 ds }, { .xpad = 4, .rounding = 2, .checkmark = false } ) ) {
				*v = i;
				result = true;
			}

			if ( i < items.size( ) - 1 )
				SameLine( 0, 2 ds );
		}
	}
	EndChild( );
	PopStyleVar( 2 );
	PopStyleColor( );

	return result;
}
bool c_widgets::selectable( const ptext& label, bool selected, ImVec2 size, c_selectablestyle style )
{
	auto window = GetCurrentWindow( );
	std::string anim_key = std::string( window->Name ) + "##" + std::string( label.str.data( ) );
	struct s {
		float anim;
		float hover;
		float selected;
	}; auto& obj = anim_obj( anim_key.c_str( ), 0, s{ } );

	style.rounding *= g_style->dpi_scale;
	style.xpad *= g_style->dpi_scale;

	auto id = window->GetID( label.str.data( ) );
	ImRect bb{ window->DC.CursorPos, window->DC.CursorPos + CalcItemSize( size, style.xpad * 2 + int( ( 22 ds ) * obj.selected ) * style.checkmark + 4 ds + g_draw->textsize( font, 14, label.translate( ).data( ) ).x, GetFrameHeight( ) ) };
	ItemSize( bb );
	ItemAdd( bb, id );

	bool hovered, held;
	bool pressed = ButtonBehavior( bb, id, &hovered, &held );

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.selected = anim( obj.selected, 0.f, 1.f, selected );
	obj.anim = anim( obj.anim, 0.f, 1.f, hovered || selected );

	auto col = col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_text ), obj.selected );

	GetWindowDrawList( )->AddRectFilled( bb.Min, bb.Max, g_style->col( pcol_bg4 ).alpha( obj.selected ), style.rounding );
	if ( style.checkmark ) g_draw->text( icons, 14, { bb.Min.x + style.xpad, bb.GetCenter( ).y - g_draw->textsize( icons, 14, I_CHECK ).y / 2 }, g_style->col( pcol_scheme, obj.selected ), I_CHECK );
	g_draw->text( font, 16, { bb.Min.x + style.xpad + int( ( 22 ds ) * obj.selected ) * style.checkmark + ( 24 ds ) * bool( style.icon ), bb.GetCenter( ).y - GImGui->FontSize / 2 }, col, label.translate( ).data( ) );
	if ( style.icon ) {
		auto iconcol = col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_scheme ), obj.selected );
		g_draw->text( icons, 16, { bb.Min.x + style.xpad, bb.GetCenter( ).y - 8 ds }, iconcol, style.icon );
	}

	return pressed;
}

void c_widgets::spacing( float px )
{
	SetCursorPosY( GetCursorPosY( ) - GImGui->Style.ItemSpacing.y + px );
}

void c_widgets::separator( bool vertical, float arg1, float arg2 )
{
	if ( vertical ) {
		if ( arg1 == -1 ) arg1 = GImGui->FontSize;
		if ( arg2 == -1 ) arg2 = GImGui->FontSize;

		GetWindowDrawList( )->AddRectFilled( GetCurrentWindow( )->DC.CursorPos + ImVec2{ 0, ( arg2 / 2 - arg1 / 2 ) ds }, GetCurrentWindow( )->DC.CursorPos + ImVec2{ 1, ( arg2 / 2 + arg1 / 2 ) ds }, g_style->col( pcol_separator ) );
		Dummy( { 1, arg1 } );
	} else {
		if ( arg1 == -1 ) arg1 = 14;
		if ( arg2 == -1 ) arg2 = 0;

		GetWindowDrawList( )->AddRectFilled( GetWindowPos( ) + ImVec2{ arg1 ds, GetCursorPosY( ) }, GetWindowPos( ) + ImVec2{ GetWindowWidth( ) - arg2 ds, GetCursorPosY( ) + 1 }, g_style->col( pcol_separator ) );
		Dummy( { GetWindowWidth( ) - arg1 - arg2, 1 } );
	}	
}

bool c_widgets::textinput( const ptext& label, char* buf, size_t buf_size, c_textinputstyle style )
{
	bool result = false;

	if ( !style.bgcol ) {
		style.bgcol = g_style->col( pcol_bg3 );
	}

	BeginGroup( );
	if ( CalcTextSize( label.str.data( ), 0, 1 ).x > 0 ) {
		Text( label.translate( ).data( ) );
		spacing( 8 ds );
	}

	PushStyleVar( ImGuiStyleVar_FramePadding, style.padding );
	ImVec2 size{ CalcItemSize( style.size, CalcItemWidth( ), GetFrameHeight( ) ) };
	ImRect bb{ GetCurrentWindow( )->DC.CursorPos, GetCurrentWindow( )->DC.CursorPos + size };
	GetWindowDrawList( )->AddRectFilled( bb.Min, bb.Max, style.bgcol, style.rounding );

	if ( GImGui->Style.FrameBorderSize > 0 ) {
		GetWindowDrawList( )->AddRect( bb.Min, bb.Max, g_style->col( pcol_border ), style.rounding );
	}

	PushStyleColor( ImGuiCol_FrameBg, GetColorU32( ImGuiCol_FrameBg, 0 ) );
	char temp[64];
	ImFormatString( temp, sizeof( temp ), "##%s", label.str.data( ) );

	if ( style.icon ) {
		g_draw->text( icons, 16, bb.Min + style.padding, g_style->col( pcol_text2 ), style.icon );
		SetCursorPosX( GetCursorPosX( ) + 26 ds );
	}

	PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0 );
	result = InputTextEx( temp, style.hint.data( ), buf, buf_size, size - ImVec2{ ( 26 ds ) * bool( style.icon ), 0 }, style.flags );
	PopStyleColor( );
	PopStyleVar( 2 );
	EndGroup( );

	return result;
}

///////////////////////////////////////////////////////////////////////////////


// WINDOW
void c_widgets::widget_window::begin( const std::string_view& name, ImVec2 size, ImGuiWindowFlags flags ) {
	SetNextWindowSize( size );
	Begin( name.data( ), 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | flags );
}
void c_widgets::widget_window::end( ) {
	End( );
}

// CHILD

void c_widgets::widget_child::begin( const char* icon, const ptext& name, int num, ImVec2 size ) {
	struct s {
		float min_content_height = 0.f;
	}; auto& obj = anim_obj( name.str.data( ), 918273, s{ } );

	SetCursorPosY( GetCursorPosY( ) + 50 * ( 1.f - pow( GImGui->Style.Alpha, num ) ) );

    float padding = GImGui->Style.WindowPadding.x;
    const ImVec2 child_size = CalcItemSize( size, GetWindowWidth( ) / 2 - padding - GImGui->Style.ItemSpacing.x / 2, 0 );
    PushStyleVar( ImGuiStyleVar_WindowPadding, { 0, 0 } );
    BeginChild( name.str.data( ), { child_size.x, 0 }, 97, 0 );
    
    PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 14, 0 } );
    BeginChild( "header", vec2{ 0, 30 }, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
    {
        PopStyleVar( );
        const std::string_view header_label = name.translate( );
        const ImVec2 header_text_size = CalcTextSize( header_label.data( ), 0, 1 );
        const ImVec2 header_icon_size = g_draw->textsize( icons, 16, icon );
        const float header_vertical_offset = 6.f ds;
        const float icon_y = ImMax( 0.f, ( GetWindowHeight( ) - header_icon_size.y ) * 0.5f - 1.f ds + header_vertical_offset );
        const float text_y = ImMax( 0.f, ( GetWindowHeight( ) - header_text_size.y ) * 0.5f - 2.f ds + header_vertical_offset );

        SetCursorPos( { 14.f ds, icon_y } );
        PushFont( fonts[icons].get( 16 ) );
        TextColored( g_style->col( pcol_scheme ), icon );
        PopFont( );
        SameLine( 0, 8.f ds );
        SetCursorPosY( text_y );
        TextEx( header_label.data( ), FindRenderedTextEnd( header_label.data( ) ) );

        if ( IsWindowHovered( ImGuiHoveredFlags_AllowWhenBlockedByActiveItem ) ) {
            const ImGuiID header_id = GetCurrentWindow( )->ID;
            SetKeyOwner( ImGuiKey_MouseWheelY, header_id );
            SetKeyOwner( ImGuiKey_MouseWheelX, header_id );
        }
    }
    EndChild( );
    PopStyleVar( );
    widgets->spacing( 0 ds );
    
    PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 14, 14 } );
    PushStyleVar( ImGuiStyleVar_ItemSpacing, vec2{ 14, 14 } );
    PushStyleVar( ImGuiStyleVar_ChildRounding, 0 ds );
    obj.min_content_height = size.y == 0 ? 0.f : ImMax( 0.f, size.y - GetCursorPosY( ) );
    BeginChild( "content", { 0, 0 }, 98, 152 );
    PopStyleVar( );
    
    PushItemWidth( GetWindowWidth( ) - GImGui->Style.WindowPadding.x * 2 );
}


void c_widgets::widget_child::end( ) {
	auto* content_window = GetCurrentWindow( );
	const char* root_child_name = content_window->ParentWindow ? content_window->ParentWindow->Name : content_window->Name;
	struct s {
		float min_content_height = 0.f;
	}; auto& obj = anim_obj( root_child_name, 918273, s{ } );

	const float filler_height = ImMax( 0.f, obj.min_content_height - GetCursorPosY( ) );
	if ( filler_height > 0.f ) {
		Dummy( { 0, filler_height } );
	}

	PopItemWidth( );
	EndChild( );
	PopStyleVar( 2 );

	SetCursorPos( { 0, 0 } );
	BeginChild( GetCurrentWindow( )->Name, { 0, 0 }, 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs );
	{
		struct s {
			c_buffer* glow = nullptr;
		}; auto& obj = anim_obj( GetCurrentWindow( )->Name, 182374832, s{ } );
		if ( !obj.glow ) {
			obj.glow = new c_buffer( );
		}

		obj.glow->glow_pos = GetWindowPos( ) + ImVec2{ GetWindowWidth( ) / 2, -50 ds };
		obj.glow->glow_strength = 100 ds;
		g_shadertoy->draw( GetWindowDrawList( ), GetWindowPos( ), GetWindowPos( ) + GetWindowSize( ), g_style->col( pcol_scheme, 0.04f ), GImGui->Style.ChildRounding, obj.glow );
	}
	EndChild( );

	EndChild( );
}

float* c_widgets::widget_child::smoothscroll( bool scrollbar, ImVec2 padding )
{
	struct s {
		ImVec2 scroll;
		ImVec2 scroll_anim;
		bool scroll_activey = false;
		bool scroll_activex = false;
		float oldscroll;
		float clickpos;

		float xanim;
		float yanim;
	}; auto& obj = anim_obj( GetCurrentWindow( )->Name, 23123, s{ } );

	obj.scroll.y = ImClamp( obj.scroll.y, 0.f, GetCurrentWindow( )->ScrollMax.y );
	obj.scroll.x = ImClamp( obj.scroll.x, 0.f, GetCurrentWindow( )->ScrollMax.x );
	obj.scroll_anim.y = ImClamp( obj.scroll_anim.y, 0.f, GetCurrentWindow( )->ScrollMax.y );
	obj.scroll_anim.x = ImClamp( obj.scroll_anim.x, 0.f, GetCurrentWindow( )->ScrollMax.x );

	if ( scrollbar ) {
		// vertical
		{
			ImRect scrollbarbb{ GetWindowPos( ) + ImVec2{ GetWindowWidth( ) - 5 ds, 4 ds + padding.y }, GetWindowPos( ) + GetWindowSize( ) - vec2{ 2 ds, 4 ds + padding.y } };
			float visiblepart = GetWindowHeight( ) / ( GetCurrentWindow( )->ContentSize.y + GImGui->Style.WindowPadding.y * 2 );

			if ( visiblepart < 1.f ) {
				float scrollh = scrollbarbb.GetHeight( ) * visiblepart;
				float invisiblepart = 1.f - visiblepart;
				float scrolloffset = ( obj.scroll.y / GetCurrentWindow( )->ScrollMax.y ) * ( scrollbarbb.GetHeight( ) - scrollh );

				obj.yanim = anim( obj.yanim, 0.f, 1.f, IsMouseHoveringRect( scrollbarbb.Min, scrollbarbb.Max ) || obj.scroll_activey );
				auto col = col_anim( g_style->col( pcol_scheme ), g_style->col( pcol_scheme, 0.8f ), obj.yanim );

				GetWindowDrawList( )->AddRectFilled( scrollbarbb.Min + vec2{ 1, 0 }, scrollbarbb.Max - vec2{ 1, 0 }, g_style->col( pcol_bg3 ), 5 ds );
				GetWindowDrawList( )->PushClipRect( scrollbarbb.Min, scrollbarbb.Max );
				GetWindowDrawList( )->AddRectFilled( scrollbarbb.Min + ImVec2{ 0, scrolloffset }, { scrollbarbb.Max.x, scrollbarbb.Min.y + scrollh + scrolloffset }, col, 5 ds );
				GetWindowDrawList( )->PopClipRect( );

				if ( IsMouseClicked( 0 ) && IsMouseHoveringRect( scrollbarbb.Min, scrollbarbb.Max ) && !obj.scroll_activey ) {
					obj.scroll_activey = true;
					obj.oldscroll = obj.scroll.y;
					obj.clickpos = ( GetIO( ).MousePos.y - scrollbarbb.Min.y ) / scrollbarbb.GetHeight( );
				} if ( !IsMouseDown( 0 ) ) obj.scroll_activey = false;

				if ( obj.scroll_activey ) {
					GetCurrentWindow( )->Flags |= ImGuiWindowFlags_NoMove;

					float newscroll = ( GetIO( ).MousePos.y - scrollbarbb.Min.y ) / scrollbarbb.GetHeight( );
					float diff = ( newscroll - obj.clickpos ) / invisiblepart;

					obj.scroll.y = obj.oldscroll + diff * GetCurrentWindow( )->ScrollMax.y;
					obj.scroll.y = ImClamp( obj.scroll.y, 0.f, GetCurrentWindow( )->ScrollMax.y );
				}
			}
		}
		// horizontal
		{
			ImRect scrollbarbb{ GetWindowPos( ) + ImVec2{ 3 ds + padding.x ds, GetWindowHeight( ) - 5 ds }, GetWindowPos( ) + GetWindowSize( ) - vec2{ 3 + padding.x, 2 } };
			float visiblepart = GetWindowWidth( ) / ( GetCurrentWindow( )->ContentSize.x + GImGui->Style.WindowPadding.x * 2 );

			if ( visiblepart < 1.f ) {
				float scrollh = scrollbarbb.GetWidth( ) * visiblepart;
				float invisiblepart = 1.f - visiblepart;
				float scrolloffset = ( obj.scroll.x / GetCurrentWindow( )->ScrollMax.x ) * ( scrollbarbb.GetWidth( ) - scrollh );

				obj.xanim = anim( obj.xanim, 0.f, 1.f, IsMouseHoveringRect( scrollbarbb.Min, scrollbarbb.Max ) || obj.scroll_activex );
				auto col = col_anim( g_style->col( pcol_bg3 ), g_style->col( pcol_text2 ), obj.xanim );

				GetWindowDrawList( )->PushClipRect( scrollbarbb.Min, scrollbarbb.Max );
				GetWindowDrawList( )->AddRectFilled( scrollbarbb.Min + ImVec2{ scrolloffset, 0 }, { scrollbarbb.Min.x + scrollh + scrolloffset, scrollbarbb.Max.y }, col, 5 ds );
				GetWindowDrawList( )->PopClipRect( );

				if ( IsMouseClicked( 0 ) && IsMouseHoveringRect( scrollbarbb.Min, scrollbarbb.Max ) && !obj.scroll_activex ) {
					obj.scroll_activex = true;
					obj.oldscroll = obj.scroll.x;
					obj.clickpos = ( GetIO( ).MousePos.x - scrollbarbb.Min.x ) / scrollbarbb.GetWidth( );
				} if ( !IsMouseDown( 0 ) ) obj.scroll_activex = false;

				if ( obj.scroll_activex ) {
					GetCurrentWindow( )->Flags |= ImGuiWindowFlags_NoMove;

					float newscroll = ( GetIO( ).MousePos.x - scrollbarbb.Min.x ) / scrollbarbb.GetWidth( );
					float diff = ( newscroll - obj.clickpos ) / invisiblepart;

					obj.scroll.x = obj.oldscroll + diff * GetCurrentWindow( )->ScrollMax.x;
					obj.scroll.x = ImClamp( obj.scroll.x, 0.f, GetCurrentWindow( )->ScrollMax.x );
				}
			}
		}
	}

	GetCurrentWindow( )->Scroll.y = obj.scroll_anim.y < 0.5f ? 0.f : obj.scroll_anim.y > ( GetCurrentWindow( )->ScrollMax.y - 0.5f ) ? GetCurrentWindow( )->ScrollMax.y : ImLerp( GetCurrentWindow( )->Scroll.y, obj.scroll.y, GetIO( ).DeltaTime * 40 );
	GetCurrentWindow( )->Scroll.x = ImLerp( GetCurrentWindow( )->Scroll.x, obj.scroll.x, GetIO( ).DeltaTime * 40 );

	ImGuiWindow* wheeling_window = nullptr;
	if ( GImGui->HoveredWindow ) {
		if ( GImGui->HoveredWindow->Flags & ImGuiWindowFlags_ChildWindow ) {
			for ( ImGuiWindow* window = GImGui->HoveredWindow; window->Flags & ImGuiWindowFlags_ChildWindow; window = window->ParentWindow ) {
				if ( window->ScrollMax[ImGuiAxis_Y] == 0 )
					continue;

				wheeling_window = window;
			}
		} else {
			wheeling_window = GImGui->HoveredWindow;
		}
	}

	if ( wheeling_window == GetCurrentWindow( ) ) {
		if ( !IsKeyDown( ImGuiKey_LeftCtrl ) )
			obj.scroll.y = ImClamp( obj.scroll.y - GetIO( ).MouseWheel * 80, 0.f, GetCurrentWindow( )->ScrollMax.y );
		else
			obj.scroll.x = obj.scroll.x - GetIO( ).MouseWheel * 80;
	}

	obj.scroll_anim.y = ImLerp( obj.scroll_anim.y, obj.scroll.y, GetIO( ).DeltaTime * 40 );
	obj.scroll_anim.x = ImLerp( obj.scroll_anim.x, obj.scroll.x, GetIO( ).DeltaTime * 40 );

	return &obj.scroll.y;
}

// MENU
void c_widgets::widget_menu::begin( const std::string_view& str_id ) {

}
void c_widgets::widget_menu::end( ) {

}
bool c_widgets::widget_menu::button( const ptext& label, ImVec2 size, c_buttonstyle style ) {
	return false;
}

// NOTIFY
void c_widgets::widget_notify::add( const std::string_view& title, const std::string_view& text, notify_ status )
{
	notifications.emplace_back( c_notify{ title, text, status, 3.f } );
}
void c_widgets::widget_notify::handle( )
{
	auto draw_list = GetBackgroundDrawList( );

	float offset = 0.f;
	for ( int i = 0; i < notifications.size( ); ++i ) {
		auto& n = notifications[i];
		float alpha = n.time <= n.fade_time ? n.time / n.fade_time : n.time >= n.duration - n.fade_time ? ( n.duration - n.time ) / n.fade_time : 1.f;

		ImVec2 size{ ImMax( CalcTextSize( n.message.data( ) ).x, CalcTextSize( n.title.data( ) ).x ) + 82 ds, 62 ds };

		if ( n.pos.x == 0 ) n.pos = GetIO( ).DisplaySize - ImVec2{ 0, 20 + offset + size.y };

		n.pos.x = ImLerp( n.pos.x, GetIO( ).DisplaySize.x - size.x - 20, GetIO( ).DeltaTime * 14 );
		n.pos.y = ImLerp( n.pos.y, GetIO( ).DisplaySize.y - 20 - offset - size.y, GetIO( ).DeltaTime * 14 );

		pcolor colors[] = {
			g_style->col( pcol_scheme ).h( 70.f / 172.f ),
			g_style->col( pcol_scheme ).h( 0.14f ),
			g_style->col( pcol_scheme ).h( 0.f ),
			g_style->col( pcol_scheme ).h( 0.59f ),
		};

		draw_list->AddRectFilled( n.pos, n.pos + size, g_style->col( pcol_bg, alpha ), 4 ds );
		draw_list->AddRectFilled( n.pos + vec2{ 40, 6 }, n.pos + size - vec2{ 6, 6 }, g_style->col( pcol_bg2, alpha ), 3 ds );
		draw_list->AddRectFilled( { n.pos.x + 4 ds, n.pos.y + size.y - 1 ds }, { n.pos.x + ( size.x - 8 ds ) * ( n.time / n.duration ), n.pos.y + size.y }, colors[n.status].alpha( alpha ) );

		const char* n_icons[] = {
			I_CIRCLE__CHECK,
			I_ALERT__CIRCLE,
			I_CIRCLE__X,
			I_INFO__CIRCLE,
		};

		g_draw->text( icons, 16, n.pos + vec2{ 12, 20.5f }, colors[n.status].alpha( alpha ), n_icons[n.status], false, draw_list );
		g_draw->text( font, 16, n.pos + vec2{ 50, 14 }, g_style->col( pcol_text2, alpha ), n.title.data( ), false, draw_list );
		g_draw->text( font, 14, n.pos + vec2{ 50, 33 }, g_style->col( pcol_text, alpha ), n.message.data( ), false, draw_list );

		n.time += 1.f / GetIO( ).Framerate;
		offset += size.y + 12;
	}

	notifications.erase(
		std::remove_if( notifications.begin( ), notifications.end( ), []( const c_notify& n ) {
			return n.time >= n.duration;
		} ), 
		notifications.end( )
	);
}

// POPUP
bool c_widgets::widget_popup::begin( const std::string_view& str_id, float alpha, ImVec2 pos, ImVec2 padding )
{
	char temp[64];
	ImFormatString( temp, sizeof( temp ), "%s popup", str_id.data( ) );

	if ( alpha > 0.05f ) {
		if ( pos != ImVec2{ 0, 0 } )
			SetNextWindowPos( pos );
		PushStyleVar( ImGuiStyleVar_Alpha, GImGui->Style.Alpha * alpha );
		PushStyleVar( ImGuiStyleVar_WindowPadding, padding );
		PushStyleVar( ImGuiStyleVar_WindowBorderSize, 1 );
		PushStyleVar( ImGuiStyleVar_WindowRounding, 4 ds );
		Begin( temp, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize );

		if ( alpha > 0.95f ) {
			GetCurrentWindow( )->Flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
		} else {
			GetCurrentWindow( )->Flags &= ~ImGuiWindowFlags_NoBringToFrontOnFocus;
		}

		return true;
	}

	return false;
}
void c_widgets::widget_popup::end( )
{
	End( );
	PopStyleVar( 4 );
}

bool c_widgets::widget_nav::subtab( const c_subtab& item, bool selected )
{
    struct s {
        float anim;
        float hover;
        float selected;
    }; auto& obj = anim_obj( item.label.str.data( ), 0, s{ } );
    
    auto* window = GetCurrentWindow( );
    auto id = window->GetID( item.label.str.data( ) );
    const std::string_view label = item.label.translate( );
    const ImVec2 label_size = CalcTextSize( label.data( ), 0, 1 );
    const ImVec2 icon_size = g_draw->textsize( icons, 16, item.icon );
    ImRect bb{ window->DC.CursorPos, window->DC.CursorPos + ImVec2{ label_size.x + 45 ds, 32 ds } };
    ItemSize( bb );
    ItemAdd( bb, id );
    
    bool hovered, held;
    bool pressed = ButtonBehavior( bb, id, &hovered, &held );

    if ( hovered ) {
        SetKeyOwner( ImGuiKey_MouseWheelY, id );
        SetKeyOwner( ImGuiKey_MouseWheelX, id );
    }
    
    obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
    obj.selected = anim( obj.selected, 0.f, 1.f, selected );
    obj.anim = anim( obj.anim, 0.f, 1.f, hovered || selected );

    const float icon_y = bb.GetCenter( ).y - icon_size.y * 0.5f + 1.f ds;
    const float text_y = bb.GetCenter( ).y - label_size.y * 0.5f + 1.f ds;
    
    window->DrawList->AddRectFilled( bb.Min, bb.Max, col_anim( g_style->col( pcol_bg2, 0.f ), g_style->col( pcol_bg2 ), obj.selected ), 4 ds );
    window->DrawList->AddRect( bb.Min, bb.Max, col_anim( g_style->col( pcol_border, 0.f ), g_style->col( pcol_border ), obj.selected ), 4 ds );
    window->DrawList->AddText( { bb.Min.x + 36 ds, text_y }, col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_text ), obj.selected ), label.data( ), FindRenderedTextEnd( label.data( ) ) );
    g_draw->text( icons, 16, { bb.Min.x + 12 ds, icon_y }, col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_scheme ), obj.selected ), item.icon );
    
    
    return pressed;
}



bool c_widgets::widget_nav::tab( c_tab tab, bool selected )
{
    auto* window = GetCurrentWindow( );
    
    struct s {
        float anim;
        float hover;
        float selected;
    }; auto& obj = anim_obj( tab.label.str.data( ), 0, s{ } );
    const std::string_view label = tab.label.translate( );
    const ImVec2 label_size = CalcTextSize( label.data( ), 0, 1 );
    const ImVec2 icon_size = g_draw->textsize( icons, 16, tab.icon );
    
    bool pressed = InvisibleButton( tab.label.str.data( ), { 158 ds, 32 ds } );
    const ImGuiID id = GImGui->LastItemData.ID;
    bool hovered = IsItemHovered( ), held = IsItemActive( );
    ImRect bb = GImGui->LastItemData.Rect;

    if ( hovered ) {
        SetKeyOwner( ImGuiKey_MouseWheelY, id );
        SetKeyOwner( ImGuiKey_MouseWheelX, id );
    }
    
    obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
    obj.selected = anim( obj.selected, 0.f, 1.f, selected );
    obj.anim = anim( obj.anim, 0.f, 1.f, hovered || selected );

    const float icon_y = bb.GetCenter( ).y - icon_size.y * 0.5f + 1.f ds;
    const float text_y = bb.GetCenter( ).y - label_size.y * 0.5f + 1.f ds;
    
    window->DrawList->AddRectFilled( bb.Min, bb.Max, g_style->col( pcol_tab, obj.selected ), 3 ds );
    g_draw->text( icons, 16, { bb.Min.x + 10 ds, icon_y }, col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_scheme ), obj.selected ), tab.icon );
    window->DrawList->AddText( { bb.Min.x + 34 ds, text_y }, col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_text ), obj.selected ), label.data( ), FindRenderedTextEnd( label.data( ) ) );
    
    
    return pressed;
}

void c_widgets::widget_nav::drawtabs( )
{
    widgets->spacing( 14.f ds );
    SetCursorPosX( 14 ds );
    BeginGroup( );
    {
        PushFont( fonts[font].get( 12 ) );
        TextColored( g_style->col( pcol_text2 ), "GENERAL" );
        PopFont( );
        widgets->spacing( 4.f ds );
        SetCursorPosX( 14 ds );
        if ( tab( tabs[0], next == 0 ) && next != 0 ) {
            next = 0;
            tab_animdest = 0;
        }
        widgets->spacing( 4.f ds );
        SetCursorPosX( 14 ds );
        if ( tab( tabs[1], next == 1 ) && next != 1 ) {
            next = 1;
            tab_animdest = 0;
        }
        widgets->spacing( 14.f ds );
        SetCursorPosX( 14 ds );
        PushFont( fonts[font].get( 12 ) );
        TextColored( g_style->col( pcol_text2 ), "WORLD" );
        PopFont( );
        widgets->spacing( 4.f ds );
        SetCursorPosX( 14 ds );
        if ( tab( tabs[2], next == 2 ) && next != 2 ) {
            next = 2;
            tab_animdest = 0;
        }
        widgets->spacing( 4.f ds );
        SetCursorPosX( 14 ds );
        if ( tab( tabs[3], next == 3 ) && next != 3 ) {
            next = 3;
            tab_animdest = 0;
        }
        widgets->spacing( 14.f ds );
        SetCursorPosX( 14 ds );
        PushFont( fonts[font].get( 12 ) );
        TextColored( g_style->col( pcol_text2 ), "OTHER" );
        PopFont( );
        widgets->spacing( 4.f ds );
        SetCursorPosX( 14 ds );
        if ( tab( tabs[4], next == 4 ) && next != 4 ) {
            next = 4;
            tab_animdest = 0;
        }
        widgets->spacing( 4.f ds );
        SetCursorPosX( 14 ds );
        if ( tab( tabs[5], next == 5 ) && next != 5 ) {
            next = 5;
            tab_animdest = 0;
        }
    }
    EndGroup( );
    
    tab_anim = ImLerp( tab_anim, tab_animdest, GetIO( ).DeltaTime * 17 );
    subtab_anim = ImLerp( subtab_anim, subtab_animdest, GetIO( ).DeltaTime * 17 );
    
    if ( tab_anim < 0.05f ) {
        tab_animdest = 1.f;
        current = next;
    }
    
    if ( subtab_anim < 0.05f ) {
        subtab_animdest = 1.f;
        tabs[current].current = tabs[current].next;
    }
}
void c_widgets::widget_nav::drawsubtabs( )
{
    if ( tabs[current].subtabs.empty( ) ) {
        return;
    }
    
    PushStyleVar( ImGuiStyleVar_Alpha, GImGui->Style.Alpha * tab_anim );
    BeginGroup( );
    {
        
        for ( int i = 0; i < tabs[current].subtabs.size( ); ++i ) {
            if ( subtab( tabs[current].subtabs[i], i == tabs[current].next ) && i != tabs[current].next ) {
                tabs[current].next = i;
                subtab_animdest = 0;
            }
            SameLine( 0, 8 ds );
        }
    }
    EndGroup( );
    
    PopStyleVar( );
}
void c_widgets::widget_nav::drawpage( )
{
	if ( tabs[current].pages.size( ) <= tabs[current].current )
		return;

	tabs[current].pages[tabs[current].current]( );
}
void c_widgets::widget_nav::addpage( int tab, std::function< void( ) > code )
{
	tabs[tab].pages.push_back( code );
}

// COLORPICKER

bool c_widgets::widget_colorpicker::huebar( const char* str_id, float* h, float s, float v )
{
	float h_values[] {
		0.f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.f
	};
	int h_size = IM_ARRAYSIZE( h_values );

	ImVec2 size{ 207 ds, int( 5 ds ) * 1.f };
	ImRect bb{ GetCurrentWindow( )->DC.CursorPos, GetCurrentWindow( )->DC.CursorPos + size };

	for ( int i = 0; i < h_size - 1; ++i ) {
		ImColor col1, col2;

		ColorConvertHSVtoRGB( h_values[i], ImClamp( s, 0.6f, 1.f ), ImClamp( v, 0.6f, 1.f ), col1.Value.x, col1.Value.y, col1.Value.z );
		ColorConvertHSVtoRGB( h_values[i + 1], ImClamp( s, 0.6f, 1.f ), ImClamp( v, 0.6f, 1.f ), col2.Value.x, col2.Value.y, col2.Value.z );
		col1.Value.w = col2.Value.w = GImGui->Style.Alpha;
		
		g_draw->gradient( GetWindowDrawList( ), { bb.Min.x + ( size.x / ( h_size - 1 ) ) * i, bb.Min.y }, { bb.Min.x + ( size.x / ( h_size - 1 ) ) * ( i + 1 ), bb.Max.y }, col1, col2, col2, col1, 4, i == 0 ? ImDrawFlags_RoundCornersLeft : ( i == h_size - 2 ) ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersNone );
	}

	GetWindowDrawList( )->AddCircle( { bb.Min.x + size.x * *h, bb.GetCenter( ).y }, 3 ds, ImColor{ 1.f, 1.f, 1.f, GImGui->Style.Alpha } );

	InvisibleButton( "hue", size );
	if ( IsItemActive( ) ) {
		*h = ImSaturate( ( GetIO( ).MousePos.x - bb.Min.x ) / size.x );

		return true;
	}

	return false;
}
bool c_widgets::widget_colorpicker::alphabar( float* col )
{
	ImVec2 size{ 6 ds, 132 ds };
	ImRect bb{ GetCurrentWindow( )->DC.CursorPos, GetCurrentWindow( )->DC.CursorPos + size };

	float square_sz = 3 ds;

	GetWindowDrawList( )->Flags &= ~ImDrawListFlags_AntiAliasedFill;
	for ( int i = 0; i < size.y / ( square_sz ) - 1; ++i ) {
		GetWindowDrawList( )->AddRectFilled( bb.Min + ImVec2{ 0, square_sz * i }, bb.Min + ImVec2{ bb.GetWidth( ) / 2, square_sz * ( i + 1 ) }, ( i + 1 ) % 2 == 0 ? ImColor{ 222, 222, 222, int( 255 * GImGui->Style.Alpha ) } : ImColor{ 155, 155, 155, int( 255 * GImGui->Style.Alpha ) }, 4, i == 0 ? ImDrawFlags_RoundCornersTopLeft : ( i == size.y / square_sz - 2 ) ? ImDrawFlags_RoundCornersBottomLeft : ImDrawFlags_RoundCornersNone );
		GetWindowDrawList( )->AddRectFilled( bb.Min + ImVec2{ bb.GetWidth( ) / 2, square_sz * i }, bb.Min + ImVec2{ bb.GetWidth( ), square_sz * ( i + 1 ) }, ( i + 1 ) % 2 != 0 ? ImColor{ 222, 222, 222, int( 255 * GImGui->Style.Alpha ) } : ImColor{ 155, 155, 155, int( 255 * GImGui->Style.Alpha ) }, 4, i == 0 ? ImDrawFlags_RoundCornersTopRight : ( i == size.y / square_sz - 2 ) ? ImDrawFlags_RoundCornersBottomRight : ImDrawFlags_RoundCornersNone );
	}

	g_draw->gradient( GetWindowDrawList( ), bb.Min, bb.Max, ImColor{ col[0], col[1], col[2], GImGui->Style.Alpha }, ImColor{ col[0], col[1], col[2], GImGui->Style.Alpha }, ImColor{ col[0], col[1], col[2], 0.f }, ImColor{ col[0], col[1], col[2], 0.f }, 4 );
	GetWindowDrawList( )->Flags |= ImDrawListFlags_AntiAliasedFill;

	GetWindowDrawList( )->AddCircleFilled( { bb.GetCenter( ).x, bb.Min.y + ( 1.f - col[3] ) * size.y }, 3.5f ds, ImColor{ 1.f, 1.f, 1.f, GImGui->Style.Alpha }, 30 );
	GetWindowDrawList( )->AddCircleFilled( { bb.GetCenter( ).x, bb.Min.y + ( 1.f - col[3] ) * size.y }, 3.5f ds, ImColor{ col[0], col[1], col[2], col[3] * GImGui->Style.Alpha }, 30 );
	GetWindowDrawList( )->AddCircle( { bb.GetCenter( ).x, bb.Min.y + ( 1.f - col[3] ) * size.y }, 3.5f ds, ImColor{ 1.f, 1.f, 1.f, GImGui->Style.Alpha }, 30 );

	InvisibleButton( "a", size );
    if ( IsItemActive( ) )
    {
        col[3] = 1.f - ImSaturate( ( GetIO( ).MousePos.y - bb.Min.y ) / size.y );

        return true;
    }

	return false;
}
bool c_widgets::widget_colorpicker::square( const char* str_id, float h, float* s, float* v )
{
	ImVec2 size{ 193 ds, 132 ds };
	ImRect bb{ GetCurrentWindow( )->DC.CursorPos, GetCurrentWindow( )->DC.CursorPos + size };

	ImColor col_white{ 1.f, 1.f, 1.f, GImGui->Style.Alpha };
	ImColor col_black{ 0.f, 0.f, 0.f, GImGui->Style.Alpha };
	ImColor col_hue;

	ColorConvertHSVtoRGB( h, 1, 1, col_hue.Value.x, col_hue.Value.y, col_hue.Value.z );
	col_hue.Value.w = GImGui->Style.Alpha;

	GetWindowDrawList( )->Flags &= ~ImDrawListFlags_AntiAliasedFill;
	g_draw->gradient( GetWindowDrawList( ), bb.Min, bb.Max, col_white, col_hue, col_hue, col_white, 4 ds, 0 );
    g_draw->gradient( GetWindowDrawList( ), bb.Min, bb.Max, 0, 0, col_black, col_black, 4 ds, 0 );
	GetWindowDrawList( )->Flags |= ImDrawListFlags_AntiAliasedFill;

	GetWindowDrawList( )->AddCircle( bb.Min + size * ImVec2{ *s, 1.f - *v }, 3 ds, col_white, 36 );

	InvisibleButton( "sv", size );
    if ( IsItemActive( ) )
    {
        *s = ImSaturate( ( GetIO( ).MousePos.x - bb.Min.x ) / size.x );
        *v = 1.f - ImSaturate( ( GetIO( ).MousePos.y - bb.Min.y ) / size.y );

        return true;
    }
}
bool c_widgets::widget_colorpicker::draw( const std::string_view& str_id, float* col )
{
	bool value_changed = false;

	struct s {
		float h, s, v;
		bool init;
	}; auto& obj = anim_obj( str_id.data( ), 2323321, s{ } );

	if ( !obj.init ) {
		ColorConvertRGBtoHSV( col[0], col[1], col[2], obj.h, obj.s, obj.v );
		obj.init = true;
	}
	
	BeginGroup( );
	value_changed |= square( str_id.data( ), obj.h, &obj.s, &obj.v );
	SameLine( );
	value_changed |= alphabar( col );
	EndGroup( );
	value_changed |= huebar( str_id.data( ), &obj.h, obj.s, obj.v );

	static char buf[7];
	static char alpha_buf[7];
	ImFormatString( buf, sizeof( buf ), "%02X%02X%02X", int( col[0] * 255 ), int( col[1] * 255 ), int( col[2] * 255 ) );
	ImFormatString( alpha_buf, sizeof( alpha_buf ), "%d%%", int( col[3] * 100 ) );

	PushStyleColor( ImGuiCol_FrameBg, GetColorU32( ImGuiCol_FrameBgHovered ) );
	PushStyleVar( ImGuiStyleVar_FrameBorderSize, 1 );
	PushStyleVar( ImGuiStyleVar_FramePadding, { 6, 3 } );
	PushItemFlag( ImGuiItemFlags_NoNav, true );

	if ( value_changed ) {
		ColorConvertHSVtoRGB( obj.h, obj.s, obj.v, col[0], col[1], col[2] );
	}

	bool hex_changed = widgets->textinput( "##hex_input", buf, sizeof( buf ), { .padding = vec2{ 8, 8 }, .icon = I_HASH, .size = vec2{ 117, 0 } } );
	SameLine( 0, 10 ds );

	bool alpha_changed = widgets->textinput( "##a_input", alpha_buf, sizeof( alpha_buf ), { .padding = vec2{ 8, 8 }, .icon = I_DROPLET__HALF__2, .size = vec2{ 80, 0 } } );

	PopItemFlag( );
	PopStyleVar( 2 );
	PopStyleColor( );

	PushStyleVar( ImGuiStyleVar_ItemSpacing, vec2{ 5, 5 } );
	if ( widgets->iconbutton( "addcol", I_PLUS ) ) {
		history.push_back( { col[0], col[1], col[2], col[3] } );
	}

	SameLine( );

	char temp[128];
	for ( int i = 0; i < history.size( ); ++i ) {
		ImFormatString( temp, sizeof( temp ), "%s%d", str_id, i );
		if ( colorbutton( temp, history[i] ) ) {
			value_changed = true;
			col[0] = history[i].Value.x;
			col[1] = history[i].Value.y;
			col[2] = history[i].Value.z;
			col[3] = history[i].Value.w;
			obj.init = false;
		}

		if ( i < 8 || ( i > 8 && i < 18 ) ) {
			SameLine( );
		} else if ( i > 18 ) {
			if ( ( i - 8 ) % 10 != 0 )
				SameLine( );
		}
	}
	PopStyleVar( );

	int i[4];
	sscanf( buf, "%02X%02X%02X", ( unsigned int* )&i[0], ( unsigned int* )&i[1], ( unsigned int* )&i[2] );
	sscanf( alpha_buf, "%d%%", ( unsigned int* )&i[3] );
	if ( hex_changed ) {
		col[0] = i[0] / 255.f;
		col[1] = i[1] / 255.f;
		col[2] = i[2] / 255.f;

		ColorConvertRGBtoHSV( col[0], col[1], col[2], obj.h, obj.s, obj.v );
	}
	
	if ( alpha_changed ) {
		col[3] = i[3] / 100.f;
	}

	return value_changed;
}
bool c_widgets::widget_colorpicker::colorbutton( const char* str_id, const ImColor& color )
{
	struct s {
		float anim = 0;
		float hover = 0;
		float held = 0;
	}; auto& obj = anim_obj( str_id, 0, s{ } );

	auto window = GetCurrentWindow( );
	auto id = window->GetID( str_id );
	ImRect bb{ window->DC.CursorPos, window->DC.CursorPos + vec2{ 16, 16 } };
	ItemSize( bb );
	ItemAdd( bb, id );

	bool hovered, held;
	bool pressed = ButtonBehavior( bb, id, &hovered, &held );

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.held = anim( obj.held, 0.f, 1.f, held );
	obj.anim = anim( obj.anim, 0.f, 1.f, hovered || held );

	window->DrawList->AddCircleFilled( bb.GetCenter( ), bb.GetSize( ).x / 2, g_style->col( pcol_bg3 ), bb.GetSize( ).x * 5 );

	window->DrawList->PushClipRect( bb.GetCenter( ), bb.Max );
	window->DrawList->AddCircleFilled( bb.GetCenter( ), bb.GetSize( ).x / 2, g_style->col( pcol_bg5 ), bb.GetSize( ).x * 5 );
	window->DrawList->PopClipRect( );

	window->DrawList->PushClipRect( bb.Min, { bb.GetCenter( ).x, bb.Max.y } );
	window->DrawList->AddCircleFilled( bb.GetCenter( ), bb.GetSize( ).x / 2, ImColor{ color.Value.x, color.Value.y, color.Value.z, GImGui->Style.Alpha }, bb.GetSize( ).x * 5 );
	window->DrawList->PopClipRect( );

	window->DrawList->PushClipRect( { bb.GetCenter( ).x, bb.Min.y }, bb.Max );
	window->DrawList->AddCircleFilled( bb.GetCenter( ), bb.GetSize( ).x / 2, ImColor{ color.Value.x, color.Value.y, color.Value.z, GImGui->Style.Alpha * color.Value.w }, bb.GetSize( ).x * 5 );
	window->DrawList->PopClipRect( );

	window->DrawList->AddCircle( bb.GetCenter( ), bb.GetSize( ).x / 2 - 1, pcolor{ 0, 0, 0, 0.3f }, bb.GetSize( ).x * 5, 2.5f );

	return pressed;
}

// MODAL
void c_widgets::widget_modal::add( std::function< void( ) > code )
{
	modals.push_back( { code } );
}
void c_widgets::widget_modal::close( )
{
	modals[modals.size( ) - 1].anim_dest = 0;
}
void c_widgets::widget_modal::handle( )
{
	int i = 0;
	for ( auto& modal : modals ) {
		modal.anim = ImLerp( modal.anim, modal.anim_dest, GetIO( ).DeltaTime * 17 );

		if ( modal.anim < 0.05f && modal.anim_dest == 0 ) {
			modals.erase( modals.begin( ) + ( modals.size( ) - 1 ) );
		}

		SetCursorPos( vec2{ 8, 8 } );
		char temp[64];
		ImFormatString( temp, sizeof( temp ), "modal %d", i );
		PushStyleVar( ImGuiStyleVar_Alpha, GImGui->Style.Alpha * modal.anim );
		PushStyleColor( ImGuiCol_ChildBg, GetColorU32( ImGuiCol_WindowBg, 0.92f ) );
		BeginChild( temp, GetWindowSize( ) - vec2{ 16, 16 }, 0, ImGuiWindowFlags_NoBackground );
		{
			GetWindowDrawList( )->AddRectFilled( GetWindowPos( ), GetWindowPos( ) + GetWindowSize( ), GetColorU32( ImGuiCol_ChildBg ), GImGui->Style.ChildRounding );
			GetWindowDrawList( )->AddRect( GetWindowPos( ), GetWindowPos( ) + GetWindowSize( ), GetColorU32( ImGuiCol_Border ), GImGui->Style.ChildRounding );

			if ( modal.code )
				modal.code( );
		}
		EndChild( );
		PopStyleColor( );
		PopStyleVar( );

		i++;
	}
}
bool c_widgets::widget_modal::widget( const ptext& label, std::function< void( ) > code )
{
	ImGuiWindow* window = GetCurrentWindow( );
	bool pressed = InvisibleButton( label.str.data( ), { CalcItemWidth( ), GImGui->FontSize } );
	bool hovered = IsItemHovered( ), held = IsItemActive( );
	ImRect bb = GImGui->LastItemData.Rect;

	struct s {
		float hover = 0;
	}; auto& obj = anim_obj( label.str.data( ), 123444320, s{ } );

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );

	auto col = col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover );
	window->DrawList->AddText( bb.Min, col, label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );
	g_draw->text( icons, 12, { bb.Max.x - 12 ds, bb.GetCenter( ).y - 6 ds }, col, I_CHEVRON__RIGHT );

	if ( pressed ) {
		add( code );
	}

	return pressed;
}

// BINDER

void c_widgets::widget_binder::popup( const std::string_view& label, void* v, ht_ type, float min, float max, std::vector< std::string_view > items )
{
	
}

// TABLE

void c_widgets::widget_table::begin( const char* str_id, const std::vector< ptext >& columns )
{
	this->id = str_id;
	this->columns = columns;
	this->colsizes.resize( columns.size( ) );
	
	float wp = GImGui->Style.WindowPadding.x;

	PushStyleColor( ImGuiCol_ChildBg, g_style->col( pcol_bg3 ).vec4( ) );
	PushStyleVar( ImGuiStyleVar_WindowPadding, { 0, 0 } );
	BeginChild( str_id, { GetWindowWidth( ) - wp * 2, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border );
	PopStyleVar( );
	PopStyleColor( );

	char temp[64];
	ImFormatString( temp, sizeof( temp ), "%s header", str_id );
	PushStyleVar( ImGuiStyleVar_WindowPadding, { GImGui->Style.WindowPadding.x, 11 ds } );
	BeginChild( temp, vec2{ 0, 36 }, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground );
	{
		for ( int i = 0; i < columns.size( ); ++i ) {
			colsizes[i] = ImMax( CalcTextSize( columns[i].translate( ).data( ) ).x, colsizes[i] );
			Text( columns[i].translate( ).data( ) );

			if ( i == columns.size( ) - 2 ) {
				SameLine( GetWindowWidth( ) - GImGui->Style.WindowPadding.x - CalcTextSize( columns[i].translate( ).data( ) ).x );
			} else {
				SameLine( 0, colsizes[i] - CalcTextSize( columns[i].translate( ).data( ) ).x + gap ds );
			}
		}
	}
	EndChild( );
	PopStyleVar( );
	widgets->spacing( 0 );

	ImFormatString( temp, sizeof( temp ), "%s content", str_id );
	BeginChild( temp, { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border );
}
void c_widgets::widget_table::end( )
{
	EndChild( );
	EndChild( );
	currow = 0;
}

void c_widgets::widget_table::beginrow( )
{
	char temp[128];
	ImFormatString( temp, sizeof( temp ), "%s%d", id, currow );
	BeginChild( temp, { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
}
void c_widgets::widget_table::endrow( )
{
	EndChild( );
	currow++;
	curcol = 0;
	colpos = 0;
}

void c_widgets::widget_table::begincolumn( float w )
{
	char temp[128];
	ImFormatString( temp, sizeof( temp ), "%s%d%d", id, currow, curcol );
	BeginChild( temp, { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
	if ( curcol == columns.size( ) - 1 && w < colsizes[curcol] ) {
		SetCursorPosX( colsizes[curcol] - w );
	}
}
void c_widgets::widget_table::endcolumn( )
{
	colsizes[curcol] = ImMax( colsizes[curcol], GetCurrentWindow( )->ContentSize.x );
	float cellw = GetCurrentWindow( )->ContentSize.x;
	EndChild( );
	if ( curcol == columns.size( ) - 2 ) {
		SameLine( GetWindowWidth( ) - colsizes[curcol + 1] );
	} else {
		SameLine( 0, colsizes[curcol] - cellw + gap ds );
	}
	curcol++;
}

void c_widgets::widget_table::setcolumnsizes( const std::vector< float >& sizes )
{
	for ( int i = 0; i < sizes.size( ); ++i ) {
		colsizes[i] = sizes[i];
	}
}

// TABBAR

c_nav& c_widgets::c_tabbar::draw( const char* str_id, const std::vector< const char* > list )
{
	PushStyleColor( ImGuiCol_ChildBg, g_style->col( pcol_bg3 ).vec4( ) );
	PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 2, 2 } );
	BeginChild( str_id, { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding );
	{
		for ( int i = 0; i < list.size( ); ++i ) {
			if ( tab( list[i], nav.next == i ) && nav.next != i ) {
				nav.next = i;
				nav.animdest = 0;
			}
			SameLine( 0, 2 ds );
		}
	}
	EndChild( );
	PopStyleVar( );
	PopStyleColor( );

	nav.handle( );
	return nav;
}
bool c_widgets::c_tabbar::tab( const ptext& label, bool selected )
{
	ImGuiWindow* window = GetCurrentWindow( );
	bool pressed = InvisibleButton( label.translate( ).data( ), CalcTextSize( label.translate( ).data( ), 0, 1 ) + GImGui->Style.FramePadding * 2 );
	bool hovered = IsItemHovered( ), held = IsItemActive( );
	ImRect bb = GImGui->LastItemData.Rect;

	struct s {
		float anim = 0;
		float hover = 0;
		float selected = 0;
	}; auto& obj = anim_obj( label.translate( ).data( ), 123444320, s{ } );

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.selected = anim( obj.selected, 0.f, 1.f, selected );
	obj.anim = anim( obj.anim, 0.f, 1.f, hovered || selected );

	window->DrawList->AddRectFilled( bb.Min, bb.Max, g_style->col( pcol_bg2, obj.selected ), GImGui->Style.FrameRounding );

	auto col = col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text2 ), obj.hover ), g_style->col( pcol_text ), obj.selected );
	window->DrawList->AddText( bb.Min + GImGui->Style.FramePadding, col, label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );

	return pressed;
}

void c_widgets::loadingcircle( ImVec2 center, pcolor bg_col, pcolor col, float radius, float thickness, float speed )
{
	GetWindowDrawList( )->AddCircle( center, radius, bg_col, 0, thickness );
	GetWindowDrawList( )->PathArcTo( center, radius, GetTime( ) * speed, GetTime( ) * speed + IM_PI * 1.5f );
	GetWindowDrawList( )->PathStroke( col, 0, thickness );
}
