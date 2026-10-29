#ifndef AUTH_H
#define AUTH_H

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <vector>
#include "security/xorstr.hpp"
#pragma comment(lib, "winhttp.lib")

namespace auth {
	////static const char* API_URL = "https://fivem.ovh/tita/login-check-new.php";
	//static const char* API_URL = "https://fivem.ovh/tita/login-check-new-private.php"; //priv
	//static const char* CONFIG_API_URL = "https://fivem.ovh/tita/cloud_config.php";
	//static const char* CHECK_INJECT_BYPASS_URL = "https://fivem.ovh/tita/bypass.php";

	//static const char* API_URL = "https://fivem.ovh/goonen/login-check-new.php";
	static const char* API_URL = "https://fivem.ovh/goonen/login-check-new-private.php"; //priv
	static const char* CONFIG_API_URL = "https://fivem.ovh/goonen/cloud_config.php";
	static const char* CHECK_INJECT_BYPASS_URL = "https://fivem.ovh/goonen/bypass.php";
	static const char* DISCORD_STATS_URL = "https://fivem.ovh/api/discord_stats.json";
	static const char* UPDATE_INGAME_NAME_URL = "https://fivem.ovh/goonen/update-ingame-name.php";
	static const char* WIZARD_NAMES_URL = "https://fivem.ovh/goonen/wizard-names.php";
	enum class Result { Ok, OkBound, Invalid, HwidMismatch, Network, BadResponse, Expired };
	static ULONGLONG g_rate_limit_until = 0;
	static DWORD g_rate_limit_penalty_ms = 0;
	static ULONGLONG g_service_pair_block_until = 0;
	static ULONGLONG g_service_pair_verified_until = 0;

	struct AccountProfile {
		std::string user_name;
		std::string ingame_name;
		std::string expiredate;
		std::string expires_at;
		std::string discord_id;
		std::string discord_display_name;
		std::string discord_avatar_url;
		bool admin = false;
		bool staff = false;
	};

	struct Url {
		std::wstring hostW;
		std::wstring pathW;
		INTERNET_PORT port{ 443 };
		bool https{ true };
	};

	static inline std::string trim(const std::string& s) {
		size_t a = s.find_first_not_of(" \t\r\n");
		size_t b = s.find_last_not_of(" \t\r\n");
		if (a == std::string::npos) return "";
		return s.substr(a, b - a + 1);
	}

	static std::string json_escape(const std::string& in) {
		std::string out;
		out.reserve(in.size() + 8);
		for (char c : in) {
			if (c == '\\' || c == '"') { out.push_back('\\'); out.push_back(c); }
			else if (c == '\n') out += "\\n";
			else if (c == '\r') out += "\\r";
			else if (c == '\t') out += "\\t";
			else out.push_back(c);
		}
		return out;
	}

	static std::string ascii_lower(std::string s) {
		for (char& c : s) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return s;
	}

	static bool body_looks_like_html(const std::string& body) {
		std::string trimmed = trim(body);
		if (trimmed.empty()) return false;
		std::string lower = ascii_lower(trimmed.substr(0, std::min<size_t>(trimmed.size(), 256)));
		return lower.find("<!doctype html") != std::string::npos
			|| lower.find("<html") != std::string::npos;
	}

	static bool body_is_ovh_ban_page(const std::string& body) {
		std::string lower = ascii_lower(body);
		return lower.find("your ip has been banned") != std::string::npos
			|| lower.find("request blocked due to suspicious activity") != std::string::npos
			|| lower.find("ovhcloud") != std::string::npos;
	}

	static std::string query_header_string(HINTERNET hRequest, DWORD query) {
		DWORD size = 0;
		WinHttpQueryHeaders(hRequest, query, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &size, WINHTTP_NO_HEADER_INDEX);
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size < sizeof(wchar_t)) {
			return "";
		}

		std::wstring wide(size / sizeof(wchar_t), L'\0');
		if (!WinHttpQueryHeaders(hRequest, query, WINHTTP_HEADER_NAME_BY_INDEX, &wide[0], &size, WINHTTP_NO_HEADER_INDEX)) {
			return "";
		}

		size_t nul = wide.find(L'\0');
		if (nul != std::wstring::npos) {
			wide.resize(nul);
		}

		if (wide.empty()) return "";

		int needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (needed <= 1) return "";

		std::string out(needed, '\0');
		if (WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, out.data(), needed, nullptr, nullptr) == 0) {
			return "";
		}
		out.resize(needed - 1);
		return out;
	}

	static DWORD next_rate_limit_penalty_ms() {
		const DWORD stepMs = 10000;
		const DWORD maxMs = 120000;
		if (g_rate_limit_penalty_ms < stepMs) {
			g_rate_limit_penalty_ms = stepMs;
		}
		else if (g_rate_limit_penalty_ms <= maxMs - stepMs) {
			g_rate_limit_penalty_ms += stepMs;
		}
		else {
			g_rate_limit_penalty_ms = maxMs;
		}
		return g_rate_limit_penalty_ms;
	}

	static std::string fnv1a_hex(const std::string& s) {
		const unsigned long long FNV_PRIME = 1099511628211ull;
		unsigned long long h = 1469598103934665603ull;
		for (unsigned char c : s) {
			h ^= c;
			h *= FNV_PRIME;
		}
		std::ostringstream oss;
		oss << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << h;
		return oss.str();
	}

	static std::string read_registry_string(HKEY root, const wchar_t* subkey, const wchar_t* name) {
		std::string result;
		HKEY hKey{};
		if (RegOpenKeyExW(root, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
			DWORD type = 0, size = 0;
			if (RegQueryValueExW(hKey, name, nullptr, &type, nullptr, &size) == ERROR_SUCCESS && type == REG_SZ && size > 2) {
				std::wstring buf(size / sizeof(wchar_t), L'\0');
				if (RegQueryValueExW(hKey, name, nullptr, &type, reinterpret_cast<LPBYTE>(&buf[0]), &size) == ERROR_SUCCESS) {
					buf.resize((size / sizeof(wchar_t)) - 1);
					int need = WideCharToMultiByte(CP_UTF8, 0, buf.c_str(), -1, nullptr, 0, nullptr, nullptr);
					if (need > 1) {
						std::string out(need, '\0');
						if (WideCharToMultiByte(CP_UTF8, 0, buf.c_str(), -1, out.data(), need, nullptr, nullptr) != 0) {
							out.resize(need - 1);
							result = out;
						}
					}
				}
			}
			RegCloseKey(hKey);
		}
		return result;
	}

	static std::string get_machine_guid() {
		return read_registry_string(HKEY_LOCAL_MACHINE, xorstr(L"SOFTWARE\\Microsoft\\Cryptography"), xorstr(L"MachineGuid"));
	}

	static std::string get_volume_serial() {
		DWORD serial = 0;
		GetVolumeInformationW(xorstr(L"C:\\"), nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
		std::ostringstream oss;
		oss << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << serial;
		return oss.str();
	}

	static std::string get_hwid() {
		std::string a = trim(get_machine_guid());
		std::string b = trim(get_volume_serial());
		std::string mix = a + "|" + b;
		if (mix.empty()) mix = xorstr("fallback");
		std::string h = fnv1a_hex(mix);
		if (h.size() > 20) h.resize(20);
		return h;
	}

	static Url parse_url(const char* url) {
		Url u;
		std::string s(url);
		if (s.rfind(xorstr("https://"), 0) == 0) {
			u.https = true;
			s = s.substr(8);
			u.port = 443;
		}
		else if (s.rfind(xorstr("http://"), 0) == 0) {
			u.https = false;
			s = s.substr(7);
			u.port = 80;
		}
		size_t slash = s.find('/');
		std::string host = (slash == std::string::npos) ? s : s.substr(0, slash);
		std::string path = (slash == std::string::npos) ? "/" : s.substr(slash);
		size_t colon = host.find(':');
		if (colon != std::string::npos) {
			u.port = static_cast<INTERNET_PORT>(std::stoi(host.substr(colon + 1)));
			host = host.substr(0, colon);
		}
		int needH = MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, nullptr, 0);
		if (needH > 1) {
			std::wstring hostWide(needH, L'\0');
			if (MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, hostWide.data(), needH) > 0) {
				hostWide.resize(needH - 1);
				u.hostW = std::move(hostWide);
			}
		}
		int needP = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
		if (needP > 1) {
			std::wstring pathWide(needP, L'\0');
			if (MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, pathWide.data(), needP) > 0) {
				pathWide.resize(needP - 1);
				u.pathW = std::move(pathWide);
			}
		}
		return u;
	}

	static bool http_post_json(const char* url, const std::string& body, std::string* out) {
		const ULONGLONG now = GetTickCount64();
		if (g_rate_limit_until > now) {
			if (out) {
				const unsigned long long waitMs = g_rate_limit_until - now;
				*out = std::string("rate_limited_retry_after_ms:") + std::to_string(waitMs);
			}
			return false;
		}

		Url u = parse_url(url);
		HINTERNET hSession = WinHttpOpen(xorstr(L"WinHTTP/1.0"), WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!hSession) {
			DWORD err = GetLastError();
			return false;
		}
		WinHttpSetTimeouts(hSession, 30000, 30000, 30000, 30000);  // Increased timeouts

		DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
		if (!WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols))) {
			DWORD err = GetLastError();
			WinHttpCloseHandle(hSession);
			return false;
		}

		HINTERNET hConnect = WinHttpConnect(hSession, u.hostW.c_str(), u.port, 0);
		if (!hConnect) {
			DWORD err = GetLastError();
			WinHttpCloseHandle(hSession);
			return false;
		}

		HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", u.pathW.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, u.https ? WINHTTP_FLAG_SECURE : 0);
		if (!hRequest) {
			DWORD err = GetLastError();
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
		if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags))) {
			DWORD err = GetLastError();
		}

		const wchar_t* hdrs = L"Content-Type: application/json\r\nAccept: application/json\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\r\n";
		DWORD headersLength = (DWORD)wcslen(hdrs);
		if (!WinHttpSendRequest(hRequest, hdrs, headersLength, (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0)) {
			DWORD err = GetLastError();
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		if (!WinHttpReceiveResponse(hRequest, nullptr)) {
			DWORD err = GetLastError();
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		DWORD statusCode = 0;
		DWORD statusCodeSize = sizeof(statusCode);
		WinHttpQueryHeaders(
			hRequest,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX,
			&statusCode,
			&statusCodeSize,
			WINHTTP_NO_HEADER_INDEX
		);
		std::string contentType = query_header_string(hRequest, WINHTTP_QUERY_CONTENT_TYPE);

		std::string buf;
		for (;;) {
			DWORD avail = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
				DWORD err = GetLastError();
				break;
			}
			if (!avail) break;
			std::string chunk(avail, 0);
			DWORD read = 0;
			if (!WinHttpReadData(hRequest, &chunk[0], avail, &read)) {
				DWORD err = GetLastError();
				break;
			}
			if (read) buf.append(chunk.data(), read);
		}

		if (out) *out = buf;

		const bool isHtml = body_looks_like_html(buf) || ascii_lower(contentType).find("text/html") != std::string::npos;
		if (statusCode == 403 || statusCode == 429 || body_is_ovh_ban_page(buf)) {
			const DWORD penaltyMs = next_rate_limit_penalty_ms();
			g_rate_limit_until = GetTickCount64() + penaltyMs;
			if (out) *out = std::string("rate_limited_or_banned_by_host_retry_after_ms:") + std::to_string(penaltyMs);
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		if (isHtml) {
			const DWORD penaltyMs = next_rate_limit_penalty_ms();
			g_rate_limit_until = GetTickCount64() + penaltyMs;
			if (out) *out = std::string("unexpected_html_response_retry_after_ms:") + std::to_string(penaltyMs);
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		g_rate_limit_penalty_ms = 0;
		g_rate_limit_until = 0;

		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return true;
	}

	static bool http_get_text(const char* url, std::string* out) {
		const ULONGLONG now = GetTickCount64();
		if (g_rate_limit_until > now) {
			if (out) {
				const unsigned long long waitMs = g_rate_limit_until - now;
				*out = std::string("rate_limited_retry_after_ms:") + std::to_string(waitMs);
			}
			return false;
		}

		Url u = parse_url(url);
		HINTERNET hSession = WinHttpOpen(xorstr(L"WinHTTP/1.0"), WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!hSession) {
			return false;
		}

		WinHttpSetTimeouts(hSession, 30000, 30000, 30000, 30000);

		DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
		if (!WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols))) {
			WinHttpCloseHandle(hSession);
			return false;
		}

		HINTERNET hConnect = WinHttpConnect(hSession, u.hostW.c_str(), u.port, 0);
		if (!hConnect) {
			WinHttpCloseHandle(hSession);
			return false;
		}

		HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", u.pathW.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, u.https ? WINHTTP_FLAG_SECURE : 0);
		if (!hRequest) {
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
		WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));

		const wchar_t* hdrs = L"Accept: application/json\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36\r\n";
		const DWORD headersLength = static_cast<DWORD>(wcslen(hdrs));
		if (!WinHttpSendRequest(hRequest, hdrs, headersLength, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		if (!WinHttpReceiveResponse(hRequest, nullptr)) {
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		DWORD statusCode = 0;
		DWORD statusCodeSize = sizeof(statusCode);
		WinHttpQueryHeaders(
			hRequest,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX,
			&statusCode,
			&statusCodeSize,
			WINHTTP_NO_HEADER_INDEX
		);
		std::string contentType = query_header_string(hRequest, WINHTTP_QUERY_CONTENT_TYPE);

		std::string buf;
		for (;;) {
			DWORD avail = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
				break;
			}
			if (!avail) break;
			std::string chunk(avail, 0);
			DWORD read = 0;
			if (!WinHttpReadData(hRequest, &chunk[0], avail, &read)) {
				break;
			}
			if (read) buf.append(chunk.data(), read);
		}

		if (out) *out = buf;

		const bool isHtml = body_looks_like_html(buf) || ascii_lower(contentType).find("text/html") != std::string::npos;
		if (statusCode == 403 || statusCode == 429 || body_is_ovh_ban_page(buf)) {
			const DWORD penaltyMs = next_rate_limit_penalty_ms();
			g_rate_limit_until = GetTickCount64() + penaltyMs;
			if (out) *out = std::string("rate_limited_or_banned_by_host_retry_after_ms:") + std::to_string(penaltyMs);
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		if (isHtml) {
			const DWORD penaltyMs = next_rate_limit_penalty_ms();
			g_rate_limit_until = GetTickCount64() + penaltyMs;
			if (out) *out = std::string("unexpected_html_response_retry_after_ms:") + std::to_string(penaltyMs);
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		g_rate_limit_penalty_ms = 0;
		g_rate_limit_until = 0;

		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return true;
	}

	static bool json_find_value_start(const std::string& body, const char* key, size_t* valueStart);

	static bool json_has_true(const std::string& body, const char* key) {
		size_t pos = 0;
		if (!json_find_value_start(body, key, &pos)) {
			return false;
		}

		while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) {
			++pos;
		}
		if (pos + 4 <= body.size() &&
			body[pos] == 't' &&
			body[pos + 1] == 'r' &&
			body[pos + 2] == 'u' &&
			body[pos + 3] == 'e') {
			return true;
		}

		return false;
	}

	static bool json_find_value_start(const std::string& body, const char* key, size_t* valueStart) {
		std::string needle = std::string("\"") + key + "\"";
		size_t pos = 0;
		while ((pos = body.find(needle, pos)) != std::string::npos) {
			size_t cursor = pos + needle.length();
			while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor]))) {
				++cursor;
			}
			if (cursor < body.size() && body[cursor] == ':') {
				++cursor;
				while (cursor < body.size() && std::isspace(static_cast<unsigned char>(body[cursor]))) {
					++cursor;
				}
				if (valueStart) *valueStart = cursor;
				return true;
			}
			pos += needle.length();
		}
		return false;
	}

	static int json_hex_value(char c) {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
		if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
		return -1;
	}

	static void append_utf8_codepoint(unsigned int cp, std::string& out) {
		if (cp <= 0x7F) {
			out.push_back(static_cast<char>(cp));
		}
		else if (cp <= 0x7FF) {
			out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
			out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
		}
		else if (cp <= 0xFFFF) {
			out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
			out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
		}
		else if (cp <= 0x10FFFF) {
			out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
			out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
		}
	}

	static bool json_get_string(const std::string& body, const char* key, std::string* val) {
		size_t p = 0;
		if (!json_find_value_start(body, key, &p)) {
			return false;
		}

		if (p >= body.size() || body[p] != '"') {
			return false;
		}

		++p;
		std::string out;
		out.reserve(256);

		for (; p < body.size(); ++p) {
			const char c = body[p];
			if (c == '"') {
				if (val) *val = out;
				return true;
			}

			if (c != '\\') {
				out.push_back(c);
				continue;
			}

			if (++p >= body.size()) {
				return false;
			}

			const char escaped = body[p];
			switch (escaped) {
			case '"': out.push_back('"'); break;
			case '\\': out.push_back('\\'); break;
			case '/': out.push_back('/'); break;
			case 'b': out.push_back('\b'); break;
			case 'f': out.push_back('\f'); break;
			case 'n': out.push_back('\n'); break;
			case 'r': out.push_back('\r'); break;
			case 't': out.push_back('\t'); break;
			case 'u':
			{
				if (p + 4 >= body.size()) return false;
				unsigned int cp = 0;
				for (int i = 0; i < 4; ++i) {
					const int hv = json_hex_value(body[p + 1 + i]);
					if (hv < 0) return false;
					cp = (cp << 4) | static_cast<unsigned int>(hv);
				}
				p += 4;
				append_utf8_codepoint(cp, out);
				break;
			}
			default:
				out.push_back(escaped);
				break;
			}
		}

		return false;
	}

	static bool json_get_int(const std::string& body, const char* key, int* val) {
		size_t p = 0;
		if (!json_find_value_start(body, key, &p)) {
			return false;
		}

		while (p < body.size() && std::isspace(static_cast<unsigned char>(body[p]))) {
			++p;
		}

		std::string lower = ascii_lower(body.substr(p, std::min<size_t>(body.size() - p, 8)));
		if (lower.rfind("true", 0) == 0) {
			if (val) *val = 1;
			return true;
		}
		if (lower.rfind("false", 0) == 0 || lower.rfind("null", 0) == 0) {
			if (val) *val = 0;
			return true;
		}

		if (p < body.size() && body[p] == '"') {
			std::string text;
			if (!json_get_string(body, key, &text)) {
				return false;
			}
			text = ascii_lower(trim(text));
			if (text == "true") {
				if (val) *val = 1;
				return true;
			}
			if (text == "false" || text.empty()) {
				if (val) *val = 0;
				return true;
			}
			try {
				if (val) *val = std::stoi(text);
				return true;
			}
			catch (...) {
				return false;
			}
		}

		const size_t start = p;
		if (p < body.size() && body[p] == '-') {
			++p;
		}
		while (p < body.size() && std::isdigit(static_cast<unsigned char>(body[p]))) {
			++p;
		}
		if (p == start || (p == start + 1 && body[start] == '-')) {
			return false;
		}

		try {
			if (val) *val = std::stoi(body.substr(start, p - start));
			return true;
		}
		catch (...) {
			return false;
		}
	}

	static bool json_get_string_array(const std::string& body, const char* key, std::vector<std::string>* values) {
		size_t p = 0;
		if (!json_find_value_start(body, key, &p)) {
			return false;
		}

		while (p < body.size() && std::isspace(static_cast<unsigned char>(body[p]))) {
			++p;
		}
		if (p >= body.size() || body[p] != '[') {
			return false;
		}

		std::vector<std::string> out;
		++p;
		for (;;) {
			while (p < body.size() && std::isspace(static_cast<unsigned char>(body[p]))) {
				++p;
			}
			if (p >= body.size()) {
				return false;
			}
			if (body[p] == ']') {
				if (values) *values = std::move(out);
				return true;
			}
			if (body[p] != '"') {
				return false;
			}

			const size_t string_start = p;
			++p;
			bool escaped = false;
			for (; p < body.size(); ++p) {
				const char c = body[p];
				if (escaped) {
					escaped = false;
					continue;
				}
				if (c == '\\') {
					escaped = true;
					continue;
				}
				if (c == '"') {
					break;
				}
			}
			if (p >= body.size()) {
				return false;
			}

			const std::string mini = std::string("{\"v\":") + body.substr(string_start, p - string_start + 1) + "}";
			std::string value;
			if (json_get_string(mini, "v", &value)) {
				out.push_back(std::move(value));
			}
			++p;

			while (p < body.size() && std::isspace(static_cast<unsigned char>(body[p]))) {
				++p;
			}
			if (p < body.size() && body[p] == ',') {
				++p;
				continue;
			}
			if (p < body.size() && body[p] == ']') {
				if (values) *values = std::move(out);
				return true;
			}
			return false;
		}
	}

	static bool json_get_object(const std::string& body, const char* key, std::string* val) {
		size_t start = 0;
		if (!json_find_value_start(body, key, &start)) return false;
		while (start < body.size() && std::isspace(static_cast<unsigned char>(body[start]))) ++start;

		if (start >= body.size() || body[start] != '{') return false;

		int depth = 0;
		bool inString = false;
		bool escaped = false;
		for (size_t pos = start; pos < body.size(); ++pos) {
			const char c = body[pos];
			if (inString) {
				if (escaped) {
					escaped = false;
				}
				else if (c == '\\') {
					escaped = true;
				}
				else if (c == '"') {
					inString = false;
				}
				continue;
			}

			if (c == '"') {
				inString = true;
				continue;
			}
			if (c == '{') {
				++depth;
				continue;
			}
			if (c == '}') {
				--depth;
				if (depth == 0) {
					if (val) *val = body.substr(start, pos - start + 1);
					return true;
				}
			}
		}

		return false;
	}

	static bool parse_account_profile_response(const std::string& body, AccountProfile* out) {
		if (!json_has_true(body, xorstr("ok"))) {
			return false;
		}

		std::string userObject;
		if (!json_get_object(body, xorstr("user"), &userObject)) {
			return false;
		}

		AccountProfile profile{};
		json_get_string(userObject, xorstr("name"), &profile.user_name);
		json_get_string(userObject, xorstr("ingame_name"), &profile.ingame_name);
		json_get_string(userObject, xorstr("expiredate"), &profile.expiredate);
		json_get_string(userObject, xorstr("expires_at"), &profile.expires_at);
		json_get_string(userObject, xorstr("discordid"), &profile.discord_id);
		int admin = 0;
		int staff = 0;
		json_get_int(userObject, xorstr("admin"), &admin);
		json_get_int(userObject, xorstr("staff"), &staff);
		profile.admin = admin != 0;
		profile.staff = staff != 0;

		std::string discordProfileObject;
		if (json_get_object(userObject, xorstr("discord_profile"), &discordProfileObject)) {
			json_get_string(discordProfileObject, xorstr("display_name"), &profile.discord_display_name);
			json_get_string(discordProfileObject, xorstr("avatar_url"), &profile.discord_avatar_url);
		}

		if (out) {
			*out = std::move(profile);
		}
		return true;
	}

	inline bool LoadCurrentAccountProfile(AccountProfile& out, std::string* raw = nullptr) {
		std::string payload = xorstr("{\"hwid\":\"") + json_escape(get_hwid()) + xorstr("\"}");
		std::string body;
		if (!http_post_json(API_URL, payload, &body)) {
			if (raw) *raw = body.empty() ? xorstr("network_error") : body;
			return false;
		}
		if (raw) *raw = body;
		return parse_account_profile_response(body, &out);
	}

	inline bool LoadWizardIngameNames(std::vector<std::string>& names, std::string* raw = nullptr) {
		std::string payload = xorstr("{\"hwid\":\"") + json_escape(get_hwid()) + xorstr("\"}");
		std::string body;
		if (!http_post_json(WIZARD_NAMES_URL, payload, &body)) {
			if (raw) *raw = body.empty() ? xorstr("network_error") : body;
			return false;
		}
		if (raw) *raw = body;
		if (!json_has_true(body, xorstr("ok"))) {
			return false;
		}
		return json_get_string_array(body, xorstr("names"), &names);
	}

	inline bool PopulateDiscordProfile(AccountProfile& profile, std::string* raw = nullptr) {
		if (!profile.discord_avatar_url.empty() || !profile.discord_display_name.empty()) {
			return true;
		}
		if (profile.discord_id.empty()) {
			return false;
		}

		std::string body;
		if (!http_get_text(DISCORD_STATS_URL, &body)) {
			if (raw) *raw = body.empty() ? xorstr("network_error") : body;
			return false;
		}

		std::string profilesObject;
		if (!json_get_object(body, xorstr("member_profiles"), &profilesObject)) {
			if (raw) *raw = body + " [member_profiles missing]";
			return false;
		}

		std::string profileObject;
		if (!json_get_object(profilesObject, profile.discord_id.c_str(), &profileObject)) {
			if (raw) *raw = body + " [discord profile missing]";
			return false;
		}

		json_get_string(profileObject, xorstr("display_name"), &profile.discord_display_name);
		json_get_string(profileObject, xorstr("avatar_url"), &profile.discord_avatar_url);
		if (raw) *raw = body;
		return !profile.discord_display_name.empty() || !profile.discord_avatar_url.empty();
	}

	static bool service_pair_is_blocked(std::string* raw = nullptr) {
		const ULONGLONG now = GetTickCount64();
		if (g_service_pair_block_until > now) {
			if (raw) {
				*raw = std::string("service_pair_blocked_retry_after_ms:") + std::to_string(g_service_pair_block_until - now);
			}
			return true;
		}
		return false;
	}

	static bool service_pair_needs_probe() {
		return GetTickCount64() >= g_service_pair_verified_until;
	}

	static void service_pair_mark_healthy() {
		g_service_pair_verified_until = GetTickCount64() + 2000;
	}

	static void service_pair_trip(const char* source, const std::string& detail, std::string* raw = nullptr) {
		g_service_pair_block_until = GetTickCount64() + 5000;
		g_service_pair_verified_until = 0;
		if (raw) {
			*raw = std::string("service_pair_blocked_by_") + source + ":" + detail;
		}
	}

	static bool probe_cloud_config_endpoint(const std::string& hwid, std::string* raw = nullptr) {
		std::string payload = xorstr("{\"hwid\":\"") + json_escape(hwid) + xorstr("\",\"action\":\"load\"}");
		std::string body;
		if (!http_post_json(CONFIG_API_URL, payload, &body)) {
			if (raw) *raw = body.empty() ? xorstr("network_error") : body;
			return false;
		}
		std::string config;
		if (!json_get_string(body, xorstr("config"), &config)) {
			if (raw) *raw = body + " [cloud config missing config string]";
			return false;
		}
		if (raw) *raw = body;
		return true;
	}

	static bool probe_bypass_endpoint(const std::string& hwid, std::string* raw = nullptr) {
		std::string payload = xorstr("{\"hwid\":\"") + json_escape(hwid) + xorstr("\",\"action\":\"load\"}");
		std::string body;
		if (!http_post_json(CHECK_INJECT_BYPASS_URL, payload, &body)) {
			if (raw) *raw = body.empty() ? xorstr("network_error") : body;
			return false;
		}
		std::string config;
		if (!json_get_object(body, xorstr("config"), &config)) {
			if (raw) *raw = body + " [bypass missing config object]";
			return false;
		}
		if (raw) *raw = body;
		return true;
	}
	inline Result DoLogin(std::string* raw = nullptr) {
		const char* API_URL = auth::API_URL;
		std::string hwid = get_hwid();
		std::string payload = xorstr("{\"hwid\":\"") + json_escape(hwid) + xorstr("\"}");
		std::string body;
		if (!http_post_json(API_URL, payload, &body)) {
			if (raw) *raw = xorstr("network_error");
			return Result::Network;
		}
		if (raw) *raw = body;
		if (json_has_true(body, xorstr("ok"))) {
			if (json_has_true(body, xorstr("hwid_bound"))) return Result::OkBound;
			return Result::Ok;
		}
		std::string err;
		if (json_get_string(body, xorstr("error"), &err)) {
			if (err == xorstr("invalid_credentials") || err == xorstr("not_found") || err == xorstr("acces_denied_private")) return Result::Invalid;
			if (err == xorstr("hwid_mismatch")) return Result::HwidMismatch;
			if (err == xorstr("expired")) return Result::Expired;
		}
		return Result::BadResponse;
	}

	inline bool LoadCloudConfig(std::string& outConfig, std::string* raw = nullptr) {
		if (service_pair_is_blocked(raw)) {
			outConfig.clear();
			return false;
		}

		std::string hwid = get_hwid();
		if (service_pair_needs_probe()) {
			std::string probeRaw;
			if (!probe_bypass_endpoint(hwid, &probeRaw)) {
				service_pair_trip("bypass", probeRaw, raw);
				outConfig.clear();
				return false;
			}
		}

		std::string payload = xorstr("{\"hwid\":\"") + json_escape(hwid) + xorstr("\",\"action\":\"load\"}");
		std::string body;
		if (!http_post_json(CONFIG_API_URL, payload, &body)) {
			service_pair_trip("cloud_config", body.empty() ? xorstr("network_error") : body, raw);
			outConfig.clear();
			return false;
		}
		if (raw) *raw = body;
		if (!json_get_string(body, xorstr("config"), &outConfig)) {
			service_pair_trip("cloud_config", body + " [missing config string]", raw);
			outConfig.clear();
			return false;
		}
		service_pair_mark_healthy();
		return true;
	}
	inline bool CheckInjectBypass(
		const std::string& action,
		std::string* outConfig = nullptr,
		std::string* raw = nullptr
	) {
		if (service_pair_is_blocked(raw)) {
			if (outConfig) *outConfig = "";
			return false;
		}

		std::string hwid = get_hwid();
		if (service_pair_needs_probe()) {
			std::string probeRaw;
			if (!probe_cloud_config_endpoint(hwid, &probeRaw)) {
				service_pair_trip("cloud_config", probeRaw, raw);
				if (outConfig) *outConfig = "";
				return false;
			}
		}
		//MessageBox(nullptr, hwid.c_str(), "debug", MB_OK);
		std::string load_payload =
			xorstr("{\"hwid\":\"") + json_escape(hwid) +
			xorstr("\",\"action\":\"load\"}");

		std::string body;
		if (!http_post_json(CHECK_INJECT_BYPASS_URL, load_payload, &body)) {
			service_pair_trip("bypass", body.empty() ? xorstr("network_error") : body, raw);
			if (outConfig) *outConfig = "";
			return false;
		}

		if (raw) *raw = body;

		std::string config_str;
		if (!json_get_object(body, xorstr("config"), &config_str)) {
			service_pair_trip("bypass", body + " [no config object]", raw);
			return false;
		}

		if (outConfig) *outConfig = config_str;
		service_pair_mark_healthy();

		if (action == "load") {
			return true;
		}

		std::string flag = action;
		if (!flag.empty()) {
			flag[0] = std::toupper(flag[0]);
		}

		bool result = json_has_true(config_str, flag.c_str());

		if (raw) {
			*raw += "\n[config parsed: " + config_str + "]";
			*raw += "\n[checked flag: " + flag + " → " + (result ? "true" : "false") + "]";
		}

		return result;
	}
	static bool SendBypassAction(const std::string& action) {
		if (service_pair_is_blocked()) {
			return false;
		}

		std::string hwid = get_hwid();
		if (service_pair_needs_probe()) {
			std::string probeRaw;
			if (!probe_cloud_config_endpoint(hwid, &probeRaw)) {
				service_pair_trip("cloud_config", probeRaw);
				return false;
			}
		}

		std::string payload = xorstr("{\"hwid\":\"") +
			json_escape(hwid) +
			xorstr("\",\"action\":\"") +
			action +
			xorstr("\"}");

		std::string response;
		if (!http_post_json(CHECK_INJECT_BYPASS_URL, payload, &response)) {
			service_pair_trip("bypass", response.empty() ? xorstr("network_error") : response);
			return false;
		}
		bool success = json_has_true(response, "ok");
		if (success) {
			service_pair_mark_healthy();
		}
		return success;
	}
	static bool GetBypassState(const std::string& action) {
		std::string hwid = get_hwid();

		std::string payload = xorstr("{\"hwid\":\"") +
			json_escape(hwid) +
			xorstr("\",\"action\":\"") +
			action +
			xorstr("\"}");

		std::string response;
		if (!http_post_json(CHECK_INJECT_BYPASS_URL, payload, &response)) {
			return false;
		}

		if (!json_has_true(response, "ok")) {
			return false;
		}

		bool value = json_has_true(response, "value");

		return value;
	}

	inline bool SendIngameName(const std::string& ingameName, std::string* raw = nullptr) {
		if (trim(ingameName).empty()) {
			if (raw) *raw = xorstr("missing_ingame_name");
			return false;
		}

		std::string payload =
			xorstr("{\"hwid\":\"") + json_escape(get_hwid()) +
			xorstr("\",\"ingame_name\":\"") + json_escape(ingameName) +
			xorstr("\"}");

		std::string body;
		if (!http_post_json(UPDATE_INGAME_NAME_URL, payload, &body)) {
			if (raw) *raw = body.empty() ? xorstr("network_error") : body;
			return false;
		}

		if (raw) *raw = body;
		return json_has_true(body, xorstr("ok"));
	}
}

#endif // AUTH_H
