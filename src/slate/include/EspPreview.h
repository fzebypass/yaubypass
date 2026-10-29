#pragma once

#include <vector>

struct EElement;

struct EArea {
	std::string name;
	ImVec2 pos;
	ImVec2 size;
	ImVec2 offset;
	ImRect rect;
	std::vector< EElement* > items;
	void Draw( );

	void AddText( const std::string_view& label, const std::string_view& text, float* col, bool* enable );
	void AddBar( const std::string_view& label, float* col, bool* enable, int* type, bool* rounded );
};

inline struct {
	ImRect bb;
	bool visible = true;
	float* col;
} Box;

enum class EElementType { Bar = 0, Text = 1, Other = 2 };
struct EElement {
	std::string_view label;
	EElementType type;
	ImVec2 size;
	ImVec2 pos;
	bool* visible = nullptr;
	bool dragging = false;

	void Drag( const ImVec2& pos, EArea* current_area );
	virtual void Draw( ImVec2 drawpos, ImVec2& area_size, EArea* current_area ) = 0;
};

struct EText : public EElement {
	std::string text;
	float* col;

	void Draw( ImVec2 drawpos, ImVec2& area_size, EArea* current_area ) override;
};

struct EBar : public EElement {
	float* col;
	int* bartype;
	bool* rounded;
	float fill;

	void Draw( ImVec2 drawpos, ImVec2& area_size, EArea* current_area ) override {
		if ( current_area->name == "Top" || current_area->name == "Bottom" ) {
			size = ImVec2{ Box.bb.GetWidth( ), 6 };
		} else {
			size = ImVec2{ 6, Box.bb.GetHeight( ) };
		}

		float rounding = *rounded ? 999 : 0;

		if ( *bartype == 0 ) {
			ImGui::GetWindowDrawList( )->AddRectFilled( ImGui::GetWindowPos( ) + drawpos, ImGui::GetWindowPos( ) + drawpos + size, ImColor{ 0, 0, 0, int( 255 * GImGui->Style.Alpha ) }, rounding );
		
			if ( current_area->name == "Top" || current_area->name == "Bottom" ) {
				ImGui::GetWindowDrawList( )->AddRectFilled( ImGui::GetWindowPos( ) + drawpos + ImVec2{ 1, 1 }, ImGui::GetWindowPos( ) + drawpos + ImVec2{ 1 + ( size.x - 2 ) * fill, size.y - 1 }, ImColor{ col[0], col[1], col[2], GImGui->Style.Alpha }, rounding );
			} else {
				ImGui::GetWindowDrawList( )->AddRectFilled( ImGui::GetWindowPos( ) + drawpos + ImVec2{ 1, 1 + ( size.y - 2 ) * ( 1.f - fill ) }, ImGui::GetWindowPos( ) + drawpos + size - ImVec2{ 1, 1 }, ImColor{ col[0], col[1], col[2], GImGui->Style.Alpha }, rounding );
			}
		} else {
			for ( int i = 0; i < 4; ++i ) {
				if ( current_area->name == "Top" || current_area->name == "Bottom" ) {
					ImGui::GetWindowDrawList( )->PushClipRect( ImGui::GetWindowPos( ) + drawpos + ImVec2{ size.x / 4 * i, 0 }, ImGui::GetWindowPos( ) + drawpos + ImVec2{ size.x / 4 * ( i + 1 ) - ( i == 3 ? 0 : 1 ), size.y } );
				} else {
					ImGui::GetWindowDrawList( )->PushClipRect( ImGui::GetWindowPos( ) + drawpos + ImVec2{ 0, size.y / 4 * i }, ImGui::GetWindowPos( ) + drawpos + ImVec2{ size.x, size.y / 4 * ( i + 1 ) - ( i == 3 ? 0 : 1 ) } );
				}
				ImGui::GetWindowDrawList( )->AddRectFilled( ImGui::GetWindowPos( ) + drawpos, ImGui::GetWindowPos( ) + drawpos + size, ImColor{ 0, 0, 0, int( 255 * GImGui->Style.Alpha ) }, rounding );
				
				if ( current_area->name == "Top" || current_area->name == "Bottom" ) {
					ImGui::GetWindowDrawList( )->AddRectFilled( ImGui::GetWindowPos( ) + drawpos + ImVec2{ 1, 1 }, ImGui::GetWindowPos( ) + drawpos + ImVec2{ 1 + ( size.x - 2 ) * fill, size.y - 1 }, ImColor{ col[0], col[1], col[2], GImGui->Style.Alpha }, rounding );
				} else {
					ImGui::GetWindowDrawList( )->AddRectFilled( ImGui::GetWindowPos( ) + drawpos + ImVec2{ 1, 1 + ( size.y - 2 ) * ( 1.f - fill ) }, ImGui::GetWindowPos( ) + drawpos + size - ImVec2{ 1, 1 }, ImColor{ col[0], col[1], col[2], GImGui->Style.Alpha }, rounding );
				}
				
				ImGui::GetWindowDrawList( )->PopClipRect( );
			}
		}
	}
};

class EspPreview {
	std::vector< EArea > areas;
	bool open = false;
	float anim = 0.f;
public:
	void initialize( );
	void draw( );
	void begincategory( const char* label );
	void endcategory( );
	bool& is_open( ) {
		return open;
	}

	std::vector< EArea >& get_areas( ) {
		return areas;
	}

	struct {
		bool* enabled;
		int* type;
		float* col;
	} box;
	struct {
		bool* enabled;
		float* col;
	} name;
	struct {
		bool* enabled;
		float* col;
	} fraction;
	struct {
		bool* enabled;
		float* col;
	} level;
	struct {
		bool* enabled;
		float* col;
	} staticid;
	struct {
		bool* enabled;
		float* col;
	} distance;
	struct {
		bool* enabled;
		float* col;
	} weapon;
	struct {
		bool* enabled;
		float* thickness;
		float* col;
	} skeleton;
	struct {
		bool* enabled;
		bool* rounding;
		int* type;
	} healthbar;
	struct {
		bool* enabled;
		bool* rounding;
		int* type;
	} armorbar;
};

inline auto g_esppreview = std::make_unique< EspPreview >( );
