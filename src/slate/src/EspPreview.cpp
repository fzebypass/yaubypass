#include <slate.h>
#include <algorithm>

using namespace ImGui;

static EElement* last_swapped_with = nullptr;

void EText::Draw( ImVec2 drawpos, ImVec2& area_size, EArea* current_area ) {
	size = ImGui::CalcTextSize( text.c_str( ) );

	const ImVec2 text_pos = ImGui::GetWindowPos( ) + drawpos;
	const ImU32 shadow_col = ImColor( 0, 0, 0, int( 140 * GImGui->Style.Alpha ) );
	const ImU32 text_col = col
		? ImGui::GetColorU32( ImVec4{ col[0], col[1], col[2], col[3] * GImGui->Style.Alpha } )
		: ImGui::GetColorU32( ImGuiCol_Text );

	ImGui::GetWindowDrawList( )->AddText( text_pos + ImVec2{ 1, 1 }, shadow_col, text.c_str( ) );
	ImGui::GetWindowDrawList( )->AddText( text_pos, text_col, text.c_str( ) );
}

void EElement::Drag( const ImVec2& pos, EArea* current_area ) {
	this->pos.x = ImLerp( this->pos.x, pos.x - GetWindowPos( ).x, GetIO( ).DeltaTime * 28 );
	this->pos.y = ImLerp( this->pos.y, pos.y - GetWindowPos( ).y, GetIO( ).DeltaTime * 28 );
	

	if ( IsMouseHoveringRect( pos, pos + size ) && IsMouseClicked( 0 ) ) {
		dragging = true;
	} if ( !IsMouseDown( 0 ) ) {
		dragging = false;
	}

	if ( dragging ) {
		for ( auto& area : g_esppreview->get_areas( ) ) {
			for (size_t i = 0; i < area.items.size(); ++i) {
				auto& item = area.items[i];

				if (item == this || item->type != type || !*item->visible) continue;

				ImVec2 itemMin = item->pos + GetWindowPos();
				ImVec2 itemMax = itemMin + item->size;

				if (IsMouseHoveringRect(itemMin, itemMax)) {
					if (last_swapped_with != item) {
						last_swapped_with = item;

						auto& items = area.items;
						auto it_self = std::find(items.begin(), items.end(), this);
						auto it_hover = std::find(items.begin(), items.end(), item);

						if (it_self != items.end() && it_hover != items.end()) {
							auto self_ptr = *it_self;
							auto hover_ptr = *it_hover;

							if ( it_self < it_hover ) {
								items.erase(it_hover);
								it_self = std::find(items.begin(), items.end(), this);

								items.insert(it_self, hover_ptr);
							} else {
								items.erase(it_self);
								it_hover = std::find(items.begin(), items.end(), item);

								items.insert(it_hover, self_ptr);
							}
						}
					}
				} else if ( last_swapped_with == item ) {
					last_swapped_with = nullptr;
				}
			}
		
			if ( area.name == current_area->name ) continue;
			bool area_hovered = IsMouseHoveringRect( area.rect.Min, area.rect.Max );
			if ( !area_hovered ) continue;

			current_area->items.erase( std::remove( current_area->items.begin( ), current_area->items.end( ), this ), current_area->items.end( ) );
			area.items.push_back( this );
		}
	}

	bool dragging = false;
	for ( auto& area : g_esppreview->get_areas( ) ) {
		for ( auto& item : area.items ) {
			if ( item->dragging ) dragging = true;
		}
	}

	if ( !dragging ) last_swapped_with = nullptr;
}

void EArea::AddText( const std::string_view& label, const std::string_view& text, float* col, bool* enable ) {
	EText* item = new EText;
	item->label = label;
	item->type = EElementType::Text;
	item->text = text;
	item->size = CalcTextSize( text.data( ) );
	item->col = col;
	item->visible = enable;

	items.push_back( item );
}

void EArea::AddBar( const std::string_view& label, float* col, bool* enable, int* type, bool* rounded ) {
	EBar* item = new EBar;
	item->label = label;
	item->type = EElementType::Bar;
	item->bartype = type;
	item->rounded = rounded;
	item->col = col;
	item->fill = 0.5f;
	item->visible = enable;
	items.push_back( item );
}

void EArea::Draw( ) {
	rect = ImRect{ GetWindowPos( ) + pos, GetWindowPos( ) + pos + size };

	std::sort(items.begin(), items.end(), [](EElement* a, EElement* b) {
		return static_cast<int>(a->type) < static_cast<int>(b->type);
	});

	static ImVec2 items_offset;
	items_offset = ImVec2{ 0, 0 };
	for ( auto& item : items ) {
		if ( !*item->visible ) continue;

		item->Drag( rect.Min + rect.GetSize( ) * offset - item->size * offset + items_offset, this );
		if ( item->type == EElementType::Text ) {
		item->Draw( ( item->dragging ? GetIO( ).MousePos - GetWindowPos( ) : item->pos ), size, this );
		}
			
		else
			item->Draw( ( item->dragging ? GetIO( ).MousePos - GetWindowPos( ) : item->pos ), size, this );

		if ( item->dragging ) {
			GetWindowDrawList( )->AddRect( GetWindowPos( ) + item->pos, GetWindowPos( ) + item->pos + item->size, GetColorU32( ImGuiCol_Border ), 3 );
		}

		if ( item->type == EElementType::Text ) {
			if ( name == "Top" ) {
				items_offset.y -= item->size.y + 4 ds;
			} else if ( name == "Bottom" ) {
				items_offset.y += item->size.y + 4 ds;
			} else {
				items_offset.y += item->size.y + 4 ds;
			}
		} else {
			if ( name == "Top" ) {
				items_offset.y -= item->size.y + 4 ds;
			} else if ( name == "Bottom" ) {
				items_offset.y += item->size.y + 4 ds;
			} else if ( name == "Right" ) {
				items_offset.x += 10 ds;
			} else if ( name == "Left" ) {
				items_offset.x -= 10 ds;
			}
		}

		if ( name == "Top" ) {
			size.y = -items_offset.y;
			pos.y = Box.bb.Min.y - size.y - 4 ds;
		} else if ( name == "Bottom" ) {
			size.y = items_offset.y;
			pos.y = Box.bb.Max.y + 4 ds;
		}
	}
}

float text_col[4] { 1.f, 1.f, 1.f, 1.f };
float bar_col[4] { 1.f, 1.f, 1.f, 1.f };
float box_color[4]{ 45 / 255.f, 193 / 255.f, 87 / 255.f, 1.f };

void EspPreview::initialize( ) {
	static bool init = false;

	float offset1 = 0, offset2 = 0;
	if ( init ) {
		offset1 = ImMax( areas[0].size.y - 65.f ds, 0.f );
		offset2 = ImMax( areas[2].size.y - 65.f ds, 0.f );
	}
	ImVec2 boxsize{ 100 ds, 268 ds - offset1 - offset2 };

	Box.bb = ImRect{ GetWindowSize( ) / 2 - boxsize / 2 + ImVec2{ 0, offset1 / 2 - offset2 / 2 }, GetWindowSize( ) / 2 + boxsize / 2 + ImVec2{ 0, offset1 / 2 - offset2 / 2 } };
	Box.col = box_color;

	if ( !init ) {
		areas.push_back( EArea {
			"Top",
			Box.bb.Min - ImVec2{ 0, 64 ds },
			{ Box.bb.GetWidth( ), 60 ds },
			{ 0.5f, 1.f }
		} );

		areas.push_back( EArea {
			"Right",
			{ Box.bb.Max.x + 5 ds, Box.bb.Min.y },
			{ 60 ds, Box.bb.GetHeight( ) },
			{ 0.f, 0.f }
		} );

		areas.push_back( EArea {
			"Bottom",
			{ Box.bb.Min.x, Box.bb.Max.y + 4 ds },
			{ Box.bb.GetWidth( ), 60 ds },
			{ 0.5f, 0.f }
		} );

		areas.push_back( EArea {
			"Left",
			{ Box.bb.Min.x - 65 ds, Box.bb.Min.y },
			{ 60 ds, Box.bb.GetHeight( ) },
			{ 1.f, 0.f }
		} );

		areas[0].AddText( "Name", "Nickname", name.col, name.enabled );
		areas[3].AddText( "Fraction", "FIB", fraction.col, fraction.enabled );
		areas[3].AddText( "Level", "13 lvl", level.col, level.enabled );
		areas[3].AddText( "Static ID", "1337", staticid.col, staticid.enabled );
		areas[3].AddText( "Distance", "679m", distance.col, distance.enabled );
		areas[3].AddText( "Weapon", "AK-47", weapon.col, weapon.enabled );
		areas[3].AddBar( "Health", bar_col, healthbar.enabled, healthbar.type, healthbar.rounding );
		areas[2].AddBar( "Armor", bar_col, armorbar.enabled, armorbar.type, armorbar.rounding );

		init = true;
	}

	areas[0].pos.x = Box.bb.Min.x;
	areas[0].size.x = Box.bb.GetWidth( );

	areas[1].pos = ImVec2{ Box.bb.Max.x + 5 ds, Box.bb.Min.y };
	areas[1].size = ImVec2{ 60 ds, Box.bb.GetHeight( ) };

	areas[2].pos.x = Box.bb.Min.x;
	areas[2].size.x = Box.bb.GetWidth( );

	areas[3].pos = ImVec2{ Box.bb.Min.x - 65 ds, Box.bb.Min.y };
	areas[3].size = { 60 ds, Box.bb.GetHeight( ) };
}

void EspPreview::draw( ) {
	initialize( );
	GetCurrentWindow( )->Flags |= ImGuiWindowFlags_NoMove;

	ImVec2 agent_size{ 150, 424 };

	if ( *box.enabled ) {
		if ( *box.type == 0 ) {
			GetWindowDrawList( )->AddRect( GetWindowPos( ) + Box.bb.Min, GetWindowPos( ) + Box.bb.Max, ImColor{ box.col[0], box.col[1], box.col[2], GImGui->Style.Alpha }, 3, 0, 2 );
		} else {
			GetWindowDrawList( )->PushClipRect( GetWindowPos( ), GetWindowPos( ) + Box.bb.Min + vec2{ 24, 24 } );
			GetWindowDrawList( )->AddRect( GetWindowPos( ) + Box.bb.Min, GetWindowPos( ) + Box.bb.Max, ImColor{ box.col[0], box.col[1], box.col[2], GImGui->Style.Alpha }, 3, 0, 2 );
			GetWindowDrawList( )->PopClipRect( );

			GetWindowDrawList( )->PushClipRect( GetWindowPos( ) + ImVec2{ Box.bb.Max.x, 0 } - vec2{ 24, 0 }, GetWindowPos( ) + ImVec2{ GetWindowWidth( ), Box.bb.Min.y + 24 } );
			GetWindowDrawList( )->AddRect( GetWindowPos( ) + Box.bb.Min, GetWindowPos( ) + Box.bb.Max, ImColor{ box.col[0], box.col[1], box.col[2], GImGui->Style.Alpha }, 3, 0, 2 );
			GetWindowDrawList( )->PopClipRect( );

			GetWindowDrawList( )->PushClipRect( GetWindowPos( ) + ImVec2{ 0, Box.bb.Max.y - 24 ds }, GetWindowPos( ) + ImVec2{ Box.bb.Min.x + 24 ds, GetWindowHeight( ) } );
			GetWindowDrawList( )->AddRect( GetWindowPos( ) + Box.bb.Min, GetWindowPos( ) + Box.bb.Max, ImColor{ box.col[0], box.col[1], box.col[2], GImGui->Style.Alpha }, 3, 0, 2 );
			GetWindowDrawList( )->PopClipRect( );

			GetWindowDrawList( )->PushClipRect( GetWindowPos( ) + Box.bb.Max - vec2{ 24, 24 }, GetWindowPos( ) + GetWindowSize( ) );
			GetWindowDrawList( )->AddRect( GetWindowPos( ) + Box.bb.Min, GetWindowPos( ) + Box.bb.Max, ImColor{ box.col[0], box.col[1], box.col[2], GImGui->Style.Alpha }, 3, 0, 2 );
			GetWindowDrawList( )->PopClipRect( );
		}
	}
	
	if ( *skeleton.enabled ) {
		ImVec2 p = GetWindowPos( ) + Box.bb.Min + ImVec2{ 3, 0 };
		float w = Box.bb.GetWidth( );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2, 34 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2, 87 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2, 230 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 + 27, 265 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 + 33, 339 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 + 37, 421 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 + 51, 432 } );
		GetWindowDrawList( )->PathStroke( ImColor{ skeleton.col[0], skeleton.col[1], skeleton.col[2], skeleton.col[3] * GImGui->Style.Alpha } );
		
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2, 230 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 - 27, 265 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 - 33, 339 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 - 37, 421 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 - 51, 432 } );
		GetWindowDrawList( )->PathStroke( ImColor{ skeleton.col[0], skeleton.col[1], skeleton.col[2], skeleton.col[3] * GImGui->Style.Alpha } );

		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2, 87 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 + 48, 103 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 + 61, 180 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 + 62, 232 } );
		GetWindowDrawList( )->PathStroke( ImColor{ skeleton.col[0], skeleton.col[1], skeleton.col[2], skeleton.col[3] * GImGui->Style.Alpha } );

		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2, 87 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 - 48, 103 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 - 61, 180 } );
		GetWindowDrawList( )->PathLineTo( p + ImVec2{ w / 2 - 62, 232 } );
		GetWindowDrawList( )->PathStroke( ImColor{ skeleton.col[0], skeleton.col[1], skeleton.col[2], skeleton.col[3] * GImGui->Style.Alpha } );
	}

	for ( auto& area : areas ) {
		area.Draw( );
	}
}
