#pragma once

#define CURL_STATIC_LIB
#define IMGUI_DEFINE_MATH_OPERATORS

#define M_PI 3.14159265358979323846
#include <slate_imgui/imgui.h>
#include <slate_imgui/imgui_internal.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include <Includes/ImGui/FontAwesome/FontAwesome.hpp>
#include <Includes/ImGui/FontAwesome/FontAwesomeBrands.hpp>
#include <Includes/ImGui/FontAwesome/RawAwesome6.hpp>

#include <Security/xorstr.hpp>
#include <Security/Api/json.hpp>

#include <Security/Api/curl/curl.h>
#include <Utils/DirectXCompat.hpp>
#include <Globals.hpp>
#include <security/xorstr.hpp>

#include "Core/Config.hpp"

#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <string>
#include <vector>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

#include <winternl.h>

#pragma comment( lib, "ws2_32.lib" )
#pragma comment( lib, "Normaliz.lib" )
#pragma comment( lib, "Crypt32.lib" )
#pragma comment( lib, "Wldap32.lib" )
#pragma comment( lib, "libcurl.lib" )

#pragma comment( lib, "d3d11.lib" )

#pragma warning( disable : 4996 )

#include <Windows.h>
#include <winternl.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

using NtWriteVirtualMemory_t = NTSTATUS(NTAPI*)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    ULONG NumberOfBytesToWrite,
    PULONG NumberOfBytesWritten
    );

using NtReadVirtualMemory_t = NTSTATUS(NTAPI*)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    ULONG NumberOfBytesToRead,
    PULONG NumberOfBytesRead
    );

inline NtWriteVirtualMemory_t pNtWriteVirtualMemory = nullptr;
inline NtReadVirtualMemory_t  pNtReadVirtualMemory = nullptr;

inline void InitNativeAPIs() {
    HMODULE ntdll = GetModuleHandleW(xorstr(L"ntdll.dll"));
    if (ntdll) {
        pNtWriteVirtualMemory = reinterpret_cast<NtWriteVirtualMemory_t>(
            GetProcAddress(ntdll, xorstr("NtWriteVirtualMemory")));
        pNtReadVirtualMemory = reinterpret_cast<NtReadVirtualMemory_t>(
            GetProcAddress(ntdll, xorstr("NtReadVirtualMemory")));
    }
}
