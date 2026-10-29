#pragma once
// ---------------------------------------------------------------------------
//  PRUDA licence client (key based)
//  Talks to the PHP API in server/ (login.php / admin.php).
//  The base URL can be overridden without a rebuild: put a file named
//  "api.txt" next to the exe that contains the folder URL, e.g.
//      https://fivem.ovh/goonen/license/
// ---------------------------------------------------------------------------

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <cstdlib>
#include <cctype>
#pragma comment(lib, "winhttp.lib")

namespace license {

	inline const wchar_t* DEFAULT_API_BASE = L"https://fivem.ovh/goonen/license/";

	struct Url {
		std::wstring host;
		std::wstring path = L"/";
		INTERNET_PORT port = 443;
		bool https = true;
	};

	// --------------------------------------------------------------- string
	inline std::wstring widen(const std::string& s) {
		if (s.empty()) return L"";
		int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
		std::wstring out((size_t)len, 0);
		MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], len);
		return out;
	}

	inline std::string narrow(const std::wstring& s) {
		if (s.empty()) return "";
		int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
		std::string out((size_t)len, 0);
		WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len, nullptr, nullptr);
		return out;
	}

	inline std::string trim(std::string s) {
		size_t a = s.find_first_not_of(" \t\r\n");
		if (a == std::string::npos) return "";
		size_t b = s.find_last_not_of(" \t\r\n");
		return s.substr(a, b - a + 1);
	}

	inline std::wstring trim_w(std::wstring s) {
		size_t a = s.find_first_not_of(L" \t\r\n\xFEFF");
		if (a == std::wstring::npos) return L"";
		size_t b = s.find_last_not_of(L" \t\r\n");
		return s.substr(a, b - a + 1);
	}

	inline std::string upper(std::string s) {
		for (char& c : s) c = (char)toupper((unsigned char)c);
		return s;
	}

	// Keys are typed by humans: drop spaces and dashes, compare uppercase.
	inline std::string normalize_key(const std::string& raw) {
		std::string out;
		for (unsigned char c : raw) {
			if (isalnum(c)) out.push_back((char)toupper(c));
		}
		return out;
	}

	inline std::string url_encode(const std::string& s) {
		static const char* hex = "0123456789ABCDEF";
		std::string out;
		for (unsigned char c : s) {
			if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out.push_back((char)c);
			else { out.push_back('%'); out.push_back(hex[c >> 4]); out.push_back(hex[c & 15]); }
		}
		return out;
	}

	inline std::vector<std::string> split(const std::string& s, char sep) {
		std::vector<std::string> out;
		std::string cur;
		for (char c : s) {
			if (c == sep) { out.push_back(cur); cur.clear(); }
			else cur.push_back(c);
		}
		out.push_back(cur);
		return out;
	}

	// ------------------------------------------------------------------ url
	inline std::wstring exe_dir() {
		wchar_t path[MAX_PATH]{};
		if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return L"";
		std::wstring p(path);
		size_t pos = p.find_last_of(L"\\/");
		return pos == std::wstring::npos ? L"" : p.substr(0, pos + 1);
	}

	inline std::wstring api_base() {
		const std::wstring file = exe_dir() + L"api.txt";
		HANDLE h = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h != INVALID_HANDLE_VALUE) {
			char buf[512]{};
			DWORD read = 0;
			BOOL ok = ReadFile(h, buf, sizeof(buf) - 1, &read, nullptr);
			CloseHandle(h);
			if (ok && read > 0) {
				std::wstring url = trim_w(widen(std::string(buf, read)));
				if (!url.empty()) {
					if (url.back() != L'/') url.push_back(L'/');
					return url;
				}
			}
		}
		return DEFAULT_API_BASE;
	}

	inline std::wstring slice_authority(const std::wstring& u, size_t slash) {
		return slash == std::wstring::npos ? u : u.substr(0, slash);
	}

	inline Url parse_url(const std::wstring& input) {
		Url r;
		std::wstring u = input;
		if (u.rfind(L"https://", 0) == 0) { r.https = true; r.port = 443; u = u.substr(8); }
		else if (u.rfind(L"http://", 0) == 0) { r.https = false; r.port = 80; u = u.substr(7); }

		size_t slash = u.find(L'/');
		std::wstring authority = slice_authority(u, slash);
		r.path = slash == std::wstring::npos ? L"/" : u.substr(slash);

		// authority may carry an explicit port (host:8080)
		size_t colon = authority.find(L':');
		if (colon != std::wstring::npos) {
			r.host = authority.substr(0, colon);
			const int port = _wtoi(authority.substr(colon + 1).c_str());
			if (port > 0 && port <= 65535) r.port = (INTERNET_PORT)port;
		}
		else {
			r.host = authority;
		}
		return r;
	}

	// ----------------------------------------------------------------- http
	inline DWORD& last_error() {
		static DWORD value = 0;
		return value;
	}

	inline bool http_post(const Url& url, const std::string& body, std::string& out) {
		out.clear();
		last_error() = 0;

		// DEFAULT_PROXY uses the normal system proxy settings and bypasses
		// localhost automatically - AUTOMATIC_PROXY fails on machines without
		// WPAD/PAC and made every request look like "no connection".
		HINTERNET session = WinHttpOpen(L"PRUDA/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session) { last_error() = GetLastError(); return false; }

		bool ok = false;
		HINTERNET connect = WinHttpConnect(session, url.host.c_str(), url.port, 0);
		if (!connect) last_error() = GetLastError();

		if (connect) {
			HINTERNET request = WinHttpOpenRequest(connect, L"POST", url.path.c_str(), nullptr,
				WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, url.https ? WINHTTP_FLAG_SECURE : 0);
			if (!request) last_error() = GetLastError();

			if (request) {
				const wchar_t* headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
				if (WinHttpSendRequest(request, headers, (DWORD)-1L, (LPVOID)body.data(),
					(DWORD)body.size(), (DWORD)body.size(), 0) && WinHttpReceiveResponse(request, nullptr)) {

					DWORD status = 0, len = sizeof(status);
					WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
						WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);

					char buf[1024];
					DWORD read = 0;
					while (WinHttpReadData(request, buf, sizeof(buf), &read) && read > 0)
						out.append(buf, read);

					ok = (status == 200) && !out.empty();
					if (!ok) last_error() = status;
				}
				else {
					last_error() = GetLastError();
				}
				WinHttpCloseHandle(request);
			}
			WinHttpCloseHandle(connect);
		}
		WinHttpCloseHandle(session);
		return ok;
	}

	// ------------------------------------------------------------- response
	struct Response {
		bool ok = false;
		std::string code = "network";
		std::string message;
		std::string expires = "-";
		int days_left = 0;
		std::string key;                 // one key, or "K1,K2,K3" for bulk
		std::vector<std::string> keys;
	};

	inline std::string error_text(const std::string& code) {
		if (code == "wrong_key")         return "Falscher Key";
		if (code == "expired")           return "Key abgelaufen";
		if (code == "banned")            return "Key gesperrt";
		if (code == "bad_key")           return "Key-Format ungueltig";
		if (code == "auth")              return "Admin-Token falsch";
		if (code == "db")                return "Server-Datenbank Fehler";
		if (code == "bad_request")       return "Ungueltige Anfrage";
		if (code == "network")           return "Keine Verbindung zum Server";
		return "Unbekannter Fehler: " + code;
	}

	inline Response parse_login(const std::string& raw) {
		Response r;
		auto parts = split(trim(raw), '|');
		if (parts.size() >= 2 && parts[0] == "OK") {
			r.ok = true;
			r.code = "OK";
			r.expires = parts[1];
			r.days_left = parts.size() > 2 ? atoi(parts[2].c_str()) : 0;
			r.message = r.expires == "LIFETIME" ? "Key aktiv - Lifetime" : "Key aktiv";
			return r;
		}
		r.code = parts.size() > 1 ? trim(parts[1]) : "bad_request";
		r.message = error_text(r.code);
		return r;
	}

	// ---------------------------------------------------------------- calls
	inline Response login(const std::string& key) {
		const std::string body = "key=" + url_encode(normalize_key(key));

		std::string raw;
		if (!http_post(parse_url(api_base() + L"login.php"), body, raw)) {
			Response r;
			r.code = "network";
			r.message = error_text("network");
			return r;
		}
		return parse_login(raw);
	}

	inline Response admin_create(const std::string& token, int days, int count = 1) {
		if (count < 1) count = 1;
		if (count > 100) count = 100;

		std::string body = "token=" + url_encode(trim(token));
		body += "&days=" + std::to_string(days);
		body += "&count=" + std::to_string(count);

		std::string raw;
		if (!http_post(parse_url(api_base() + L"admin.php"), body, raw)) {
			Response r;
			r.code = "network";
			r.message = error_text("network");
			return r;
		}

		Response r;
		auto parts = split(trim(raw), '|');
		if (parts.size() >= 3 && parts[0] == "OK") {
			r.ok = true;
			r.code = "OK";
			r.key = parts[1];
			r.expires = parts[2];
			for (const auto& k : split(r.key, ',')) {
				const std::string clean = trim(k);
				if (!clean.empty()) r.keys.push_back(clean);
			}
			r.message = r.keys.size() > 1
				? std::to_string(r.keys.size()) + " Keys erstellt"
				: "Key erstellt";
			return r;
		}
		r.code = parts.size() > 1 ? trim(parts[1]) : "bad_request";
		r.message = error_text(r.code);
		return r;
	}

	// ---------------------------------------------------------------- cheat
	inline std::wstring cheat_path() {
		const std::wstring dir = exe_dir();
		const wchar_t* names[] = { L"Wizard External.exe", L"WizardExternal.exe", L"wizard.exe" };
		for (const wchar_t* name : names) {
			const std::wstring candidate = dir + name;
			if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES)
				return candidate;
		}
		return L"";
	}

	inline bool start_cheat(std::wstring& error) {
		const std::wstring path = cheat_path();
		if (path.empty()) {
			error = L"Wizard External.exe fehlt neben dem Loader";
			return false;
		}
		const std::wstring dir = exe_dir();
		HINSTANCE result = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, dir.c_str(), SW_SHOWNORMAL);
		if ((INT_PTR)result <= 32) {
			error = L"Cheat konnte nicht gestartet werden";
			return false;
		}
		return true;
	}

} // namespace license
