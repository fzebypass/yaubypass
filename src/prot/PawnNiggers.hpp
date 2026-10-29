#pragma once

#define WRAPPER_ALIAN(x) x

#include <Windows.h>
#include <string>

#include <Includes/Includes.hpp>
#include <Security/Api/structs/structs.hpp>
#include <Core/SDK/Structs/Structs.hpp>

namespace AntiCrack {

    inline BOOL CALLBACK enum_windows_callback(HWND hWnd, LPARAM lparam);

    inline bool FindHookInAddr(BYTE* func);

    inline void StopService(const char* service_name);

    inline void CheckWindows();
    inline void CheckServices();
    inline void AntiDbg();

    inline OBJECT_ATTRIBUTES InitObjectAttributes(
        PUNICODE_STRING name,
        ULONG attributes,
        HANDLE hRoot,
        PSECURITY_DESCRIPTOR security
    );

    inline HANDLE DumpHandle(HANDLE hProcessId, HANDLE hHandleValue);

    inline void KillHandle(HANDLE hProcessId, HANDLE hHandleValue);

    inline std::string GetProcesssNameByPid(DWORD pid);

    inline bool KillProcess(DWORD pid);

    inline void CheckProcesses();

    inline void DoProtect();
}