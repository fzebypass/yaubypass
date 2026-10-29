#pragma once

class c_search {
	std::vector< c_searchitem > items;
	std::vector< bool > visited_pages;
	int total_pages = 0;
public:
	char buf[32]{ };
	bool initialized = false;

	void initialize( );
	void add_item( c_searchitem item );
	void draw( );
	void visit_current_page( );
	int visited_page_count( ) const;
	int total_page_count( ) const {
		return total_pages;
	}
	bool has_indexed_all_pages( ) const {
		return total_pages > 0 && visited_page_count( ) >= total_pages;
	}
	int size( ) const {
		return static_cast< int >( items.size( ) );
	}
};

inline auto g_search = std::make_unique< c_search >( );
