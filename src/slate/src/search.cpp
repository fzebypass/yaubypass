#include <slate.h>
#include <algorithm>

using namespace ImGui;

namespace {
	int get_total_nav_page_count() {
		int total_pages = 0;
		for ( const auto& tab : widgets->nav.tabs ) {
			total_pages += static_cast< int >( tab.pages.size( ) );
		}

		return total_pages;
	}

	int get_current_nav_page_index() {
		const int current_tab = widgets->nav.current;
		if ( current_tab < 0 || current_tab >= static_cast< int >( widgets->nav.tabs.size( ) ) ) {
			return -1;
		}

		int page_index = 0;
		for ( int tab_index = 0; tab_index < current_tab; ++tab_index ) {
			page_index += static_cast< int >( widgets->nav.tabs[tab_index].pages.size( ) );
		}

		const auto& active_tab = widgets->nav.tabs[current_tab];
		if ( active_tab.current < 0 || active_tab.current >= static_cast< int >( active_tab.pages.size( ) ) ) {
			return -1;
		}

		return page_index + active_tab.current;
	}
}

void c_search::initialize( ) {
	const int nav_page_count = get_total_nav_page_count( );
	if ( nav_page_count != total_pages ) {
		visited_pages.resize( nav_page_count, false );
		total_pages = nav_page_count;
	}

	if ( initialized )
		return;

	// Do not pre-execute every page here.
	// Running all page lambdas on the first menu open can touch cloud/network
	// code and game-dependent pointers before the user actually visits those tabs.
	// Search items still register naturally as widgets are drawn on visited pages.
	initialized = true;
}

void c_search::add_item( c_searchitem item ) {
	if ( std::find_if( items.begin( ), items.end( ), [item]( const c_searchitem& i ) { return i.label == item.label; } ) != items.end( ) )
		return;

	if ( GImGui->CurrentItemFlags & ImGuiItemFlags_NoNav )
		return;

	item.tab = widgets->nav.tabs[widgets->nav.current];

	items.push_back( item );
}

void c_search::visit_current_page( ) {
	initialize( );

	const int current_page_index = get_current_nav_page_index( );
	if ( current_page_index < 0 || current_page_index >= static_cast< int >( visited_pages.size( ) ) ) {
		return;
	}

	visited_pages[current_page_index] = true;
}

int c_search::visited_page_count( ) const {
	return static_cast< int >( std::count( visited_pages.begin( ), visited_pages.end( ), true ) );
}

std::string to_lower( const char* str ) {
	std::string result;

	for ( int i = 0; str[i]; ++i ) {
		result += std::tolower( str[i] );
	}

	return result;
}

bool compare( const char* str, const char* substr ) {
	return strstr( to_lower( str ).c_str( ), to_lower( substr ).c_str( ) );
}

void c_search::draw( ) {
	std::vector< c_searchitem* > res;
	for ( c_searchitem& item : items ) {
		if ( compare( item.label.data( ), buf ) ) {
			res.push_back( &item );
		}
	}

	PushItemWidth( GetWindowWidth( ) - GImGui->Style.WindowPadding.x * 2 );
	for ( auto item : res ) {
		if ( item->tab.subtabs.empty( ) ) {
			Text( item->tab.label.translate( ).data( ) );
		} else {
			TextDisabled( "%s, ", item->tab.label.translate( ).data( ) );
			SameLine( 0, 0 );
			Text( item->tab.subtabs[item->tab.current].label.translate( ).data( ) );
		}

		item->code( );

		widgets->separator( );
	}
	PopItemWidth( );
}
