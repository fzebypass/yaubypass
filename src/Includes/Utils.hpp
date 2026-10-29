#pragma once

#include <Includes/Includes.hpp>
#include <shlobj_core.h>
#include <fstream>
#include <sddl.h>
#include <random>
#include <regex>

namespace Utils {
	inline std::string ValidateTextForRendering(const std::string& text, ImFont* font = nullptr) {
		if (text.empty()) return text;

		std::string result;
		result.reserve(text.length());

		for (unsigned char c : text) {

			if (c >= 32 || c == '\t' || c == '\n' || c == '\r') {
				result += c;
			}
		}

		return result;
	}
	inline ImVec2 CalcTextSize( ImFont * font, int size, const char * label ) {
		return font->CalcTextSizeA( size, FLT_MAX, 0, label );
	}

	inline void OpenConsole( ) {
		AllocConsole( );
		FILE * fDummy;
		freopen_s( &fDummy, xorstr("CONIN$"), xorstr("r"), stdin );
		freopen_s( &fDummy, xorstr("CONOUT$"), xorstr("w"), stderr );
		freopen_s( &fDummy, xorstr("CONOUT$"), xorstr("w"), stdout );
	}

	inline std::string StringToLowerCase( std::string Input )
	{
		std::transform( Input.begin( ), Input.end( ), Input.begin( ), [ ] ( unsigned char c ) { return std::tolower( c ); } );
		return Input;
	}

	inline std::string StringToFirstUpperCase( std::string Input )
	{
		if ( !Input.empty( ) )
		{
			Input[ 0 ] = std::toupper( Input[ 0 ] );
			std::transform( Input.begin( ) + 1, Input.end( ), Input.begin( ) + 1, [ ] ( unsigned char c ) { return std::tolower( c ); } );
		}
		return Input;
	}

	inline static std::string EncodeB64( const std::string & in ) {

		std::string out;

		int val = 0, valb = -6;
		for ( u_char c : in ) {
			val = ( val << 8 ) + c;
			valb += 8;
			while ( valb >= 0 ) {
				out.push_back( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[ ( val >> valb ) & 0x3F ] );
				valb -= 6;
			}
		}
		if ( valb > -6 ) out.push_back( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[ ( ( val << 8 ) >> ( valb + 8 ) ) & 0x3F ] );
		while ( out.size( ) % 4 ) out.push_back( '=' );
		return out;
	}

	inline static std::string DecodeB64( const std::string & in ) {

		std::string out;

		std::vector<int> T( 256, -1 );
		for ( int i = 0; i < 64; i++ ) {
			T[ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[ i ] ] = i;
		}

		int val = 0, valb = -8;
		for ( u_char c : in ) {
			if ( T[ c ] == -1 ) break;
			val = ( val << 6 ) + T[ c ];
			valb += 6;
			if ( valb >= 0 ) {
				out.push_back( char( ( val >> valb ) & 0xFF ) );
				valb -= 8;
			}
		}
		return out;
	}

	inline std::string Hex2Str( const std::string & in ) {
		std::string output;

		size_t cnt = in.length( ) / 2;

		for ( size_t i = 0; cnt > i; ++i ) {
			uint32_t s = 0;
			std::stringstream ss;
			ss << std::hex << in.substr( i * 2, 2 );
			ss >> s;

			output.push_back( static_cast< unsigned char >( s ) );
		}

		return output;
	}

	inline bool KeyPressedWithDelay(int key, int delay_ms) {
		static std::chrono::time_point<std::chrono::steady_clock> lastTime;
		auto now = std::chrono::steady_clock::now();

		if ((GetAsyncKeyState(key) & 0x8000)) {
			if (now - lastTime >= std::chrono::milliseconds(delay_ms)) {
				lastTime = now;
				return true;
			}
		}
		return false;
	}

	inline std::string Str2Hex( const std::string & input )
	{
		static const char hex_digits[ ] = "0123456789ABCDEF";

		std::string output;
		output.reserve( input.length( ) * 2 );
		for ( unsigned char c : input )
		{
			output.push_back( hex_digits[ c >> 4 ] );
			output.push_back( hex_digits[ c & 15 ] );
		}
		return output;
	}

	inline bool GetMonitorRectFromWindow(HWND window, RECT& rect) {
		if (!window || !IsWindow(window)) {
			return false;
		}

		const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
		if (!monitor) {
			return false;
		}

		MONITORINFO monitor_info{ sizeof(monitor_info) };
		if (!GetMonitorInfo(monitor, &monitor_info)) {
			return false;
		}

		rect = monitor_info.rcMonitor;
		return true;
	}

	inline bool GetClientRectOnScreen(HWND window, RECT& rect) {
		if (!window || !IsWindow(window)) {
			return false;
		}

		RECT client_rect{};
		if (!GetClientRect(window, &client_rect)) {
			return false;
		}

		POINT top_left{ client_rect.left, client_rect.top };
		POINT bottom_right{ client_rect.right, client_rect.bottom };
		if (!ClientToScreen(window, &top_left) || !ClientToScreen(window, &bottom_right)) {
			return false;
		}

		rect = { top_left.x, top_left.y, bottom_right.x, bottom_right.y };
		return rect.right > rect.left && rect.bottom > rect.top;
	}

	inline bool RectWithinTolerance(const RECT& left, const RECT& right, LONG tolerance = 2) {
		const auto within_tolerance = [tolerance](LONG a, LONG b) {
			const LONG diff = a > b ? a - b : b - a;
			return diff <= tolerance;
		};

		return within_tolerance(left.left, right.left) &&
			within_tolerance(left.top, right.top) &&
			within_tolerance(left.right, right.right) &&
			within_tolerance(left.bottom, right.bottom);
	}

	inline bool IsFullscreenLikeWindow(HWND window) {
		RECT window_rect{};
		RECT monitor_rect{};
		return window &&
			IsWindow(window) &&
			GetWindowRect(window, &window_rect) &&
			GetMonitorRectFromWindow(window, monitor_rect) &&
			RectWithinTolerance(window_rect, monitor_rect);
	}

	inline bool GetWindowBounds(HWND window, RECT& rect) {
		if (!window || !IsWindow(window) || IsIconic(window)) {
			return false;
		}

		RECT client_rect{};
		RECT window_rect{};
		RECT monitor_rect{};
		const bool has_client_rect = GetClientRectOnScreen(window, client_rect);
		const bool has_monitor_rect = GetMonitorRectFromWindow(window, monitor_rect);

		if (has_monitor_rect && IsFullscreenLikeWindow(window)) {
			rect = monitor_rect;
			return true;
		}

		if (has_client_rect) {
			rect = client_rect;
			return true;
		}

		if (GetWindowRect(window, &window_rect)) {
			rect = window_rect;
			return true;
		}

		if (has_monitor_rect) {
			rect = monitor_rect;
			return true;
		}

		return false;
	}

	inline std::pair<ImVec2, ImVec2> GetWindowPosAndSize(HWND window) {
		RECT rect{};
		if (!GetWindowBounds(window, rect)) {
			return std::make_pair(ImVec2{ 0, 0 }, ImVec2{ 0, 0 });
		}

		const ImVec2 position{ static_cast<float>(rect.left), static_cast<float>(rect.top) };
		const ImVec2 size{
			static_cast<float>(rect.right - rect.left),
			static_cast<float>(rect.bottom - rect.top)
		};

		return std::make_pair(position, size);
	}

	inline  std::string GetClipboard( ) {
		OpenClipboard( NULL );
		HANDLE hData = GetClipboardData( CF_TEXT );
		char * pszText = static_cast< char * >( GlobalLock( hData ) );
		std::string text( pszText );
		GlobalUnlock( hData );
		CloseClipboard( );
		return text;
	}

	inline void PasteClipboard( const char * seila ) {
		const size_t len = strlen( seila ) + 1;
		HGLOBAL hMem = GlobalAlloc( GMEM_MOVEABLE, len );
		memcpy( GlobalLock( hMem ), seila, len );
		GlobalUnlock( hMem );
		OpenClipboard( 0 );
		EmptyClipboard( );
		SetClipboardData( CF_TEXT, hMem );
		CloseClipboard( );
	}

	inline int GenRandomInt( int min, int max ) {
		std::random_device rd;
		std::mt19937 gen( rd( ) );
		std::uniform_int_distribution<> distrib( min, max );
		return distrib( gen );
	}

	inline void RenderBlur( HWND hwnd, bool remove )
	{
		struct ACCENTPOLICY {
			int na;
			int nf;
			int nc;
			int nA;
		};
		struct WINCOMPATTRDATA {
			int na;
			PVOID pd;
			ULONG ul;
		};

		const HINSTANCE hm = LoadLibrary( xorstr( "user32.dll" ) );
		if ( hm )
		{
			typedef BOOL( WINAPI * pSetWindowCompositionAttribute )( HWND, WINCOMPATTRDATA * );

			const pSetWindowCompositionAttribute SetWindowCompositionAttribute = ( pSetWindowCompositionAttribute ) GetProcAddress( hm, xorstr( "SetWindowCompositionAttribute" ) );
			if ( SetWindowCompositionAttribute )
			{
				ACCENTPOLICY policy;
				if ( remove ) {
					policy = { 0 };
				}
				else {
					policy = { 3, 0, 0, 0 };
				}
				WINCOMPATTRDATA data = { 19, &policy,sizeof( ACCENTPOLICY ) };

				SetWindowCompositionAttribute( hwnd, &data );
			}
			FreeLibrary( hm );
		}
	}

	inline std::string get_hwid() {
		HANDLE hToken;
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_READ | TOKEN_QUERY, &hToken)) {
			DWORD dwSize = 0;
			GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
			if (dwSize > 0) {
				PTOKEN_USER pTokenUser = (PTOKEN_USER)malloc(dwSize);
				if (GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize)) {
					LPSTR sidString = NULL;  // Cambiado de LPWSTR a LPSTR para compatibilidad con ANSI
					if (ConvertSidToStringSidA(pTokenUser->User.Sid, &sidString)) {  // ConvertSidToStringSidA para ANSI
						std::string fullSid(sidString);
						size_t startPos = fullSid.find("S-1-5-21-");
						if (startPos != std::string::npos) {
							std::string sidPart = fullSid.substr(startPos + 9);
							size_t firstDash = sidPart.find('-');
							size_t secondDash = sidPart.find('-', firstDash + 1);
							if (firstDash != std::string::npos && secondDash != std::string::npos) {
								std::string hwid = sidPart.substr(firstDash + 1, secondDash - firstDash - 1);
								LocalFree(sidString);
								free(pTokenUser);
								CloseHandle(hToken);
								return hwid;
							}
						}
						LocalFree(sidString);
					}
				}
				free(pTokenUser);
			}
			CloseHandle(hToken);
		}
		return "12345";  // Valor por defecto si no se obtiene el HWID
	}
}
