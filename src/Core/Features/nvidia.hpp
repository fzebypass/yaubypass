#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <array>
#include <shellapi.h>
#include "NVIDIA/NVIDIA.hpp"

namespace NvidiaProofing {

    // Runs a helper that needs administrator rights. Windows shows a single UAC
    // prompt; the call returns the helper's exit code (false when the user cancels).
    inline bool RunElevatedProcess(const std::wstring& exePath, const std::wstring& parameters, DWORD timeoutMs = 25000) {
        SHELLEXECUTEINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        info.lpVerb = L"runas";
        info.lpFile = exePath.c_str();
        info.lpParameters = parameters.empty() ? nullptr : parameters.c_str();
        info.nShow = SW_HIDE;

        if (!ShellExecuteExW(&info))
            return false;

        if (!info.hProcess)
            return true;

        WaitForSingleObject(info.hProcess, timeoutMs);
        DWORD exitCode = 1;
        GetExitCodeProcess(info.hProcess, &exitCode);
        CloseHandle(info.hProcess);
        return exitCode == 0;
    }

    inline bool IsProcessRunning(const std::wstring& processName) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
            return false;

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        bool found = false;

        if (Process32FirstW(snap, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                    found = true;
                    break;
                }
            } while (Process32NextW(snap, &entry));
        }

        CloseHandle(snap);
        return found;
    }

    inline bool KillProcessByName(const wchar_t* processName) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32W entry = { 0 };
        entry.dwSize = sizeof(entry);

        if (Process32FirstW(snap, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, processName) == 0) {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                    if (hProcess) {
                        TerminateProcess(hProcess, 0);
                        CloseHandle(hProcess);
                    }
                }
            } while (Process32NextW(snap, &entry));
        }

        CloseHandle(snap);
        return true;
    }

    inline std::wstring FindProcessPathByName(const std::wstring& processName) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return L"";

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        std::wstring result;

        if (Process32FirstW(snap, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                    HANDLE hProcess = OpenProcess(
                        PROCESS_QUERY_LIMITED_INFORMATION,
                        FALSE,
                        entry.th32ProcessID
                    );

                    if (hProcess) {
                        wchar_t path[MAX_PATH];
                        DWORD size = MAX_PATH;

                        if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
                            result = path;
                            CloseHandle(hProcess);
                            break;
                        }

                        CloseHandle(hProcess);
                    }
                }
            } while (Process32NextW(snap, &entry));
        }

        CloseHandle(snap);
        return result;
    }

    inline bool StartProcess(const std::wstring& exePath) {
        if (exePath.empty()) return false;

        STARTUPINFOW si{};
        si.cb = sizeof(si);

        PROCESS_INFORMATION pi{};

        std::wstring cmd = L"\"" + exePath + L"\"";

        BOOL result = CreateProcessW(
            exePath.c_str(),
            cmd.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi
        );

        if (result) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        return result == TRUE;
    }

    inline bool StopNV(const std::wstring& processName = L"nvcontainer.exe") {
        KillProcessByName(processName.c_str());

        for (int i = 0; i < 20; ++i) {
            if (!IsProcessRunning(processName))
                return true;
            Sleep(100);
        }

        // A kill without administrator rights silently fails. Ask Windows once and
        // finish the job with an elevated taskkill instead of spawning a duplicate
        // container next to the running one (that used to break NVIDIA's overlay).
        RunElevatedProcess(L"C:\\Windows\\System32\\taskkill.exe", L"/f /im " + processName);

        for (int i = 0; i < 30; ++i) {
            if (!IsProcessRunning(processName))
                return true;
            Sleep(100);
        }

        return false;
    }

    // nvcontainer.exe is owned by the "NVIDIA Display Container LS" service. Killing
    // the process and starting the exe by hand spawns extra containers, and with two
    // or more containers alive the capture pipeline breaks - NVIDIA recordings come
    // out green or black. The only correct way is to restart the service.
    inline bool RestartNV(const std::wstring& processName = L"nvcontainer.exe") {
        (void)processName;

        Core::g_ShouldReapplyNvidiaPatches.store(true, std::memory_order_release);

        const std::wstring command =
            L"/c taskkill /f /im nvcontainer.exe >nul 2>&1 & "
            L"timeout /t 1 /nobreak >nul & "
            L"sc start \"NVIDIA Display Container LS\" >nul 2>&1 & exit /b 0";

        // Runs elevated; without admin rights the kill and the service start both fail
        // silently and NVIDIA keeps whatever state it is in (no duplicate spawn).
        return RunElevatedProcess(L"C:\\Windows\\System32\\cmd.exe", command, 30000);
    }
    inline std::wstring toLower(const std::wstring& str) {
        std::wstring result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::towlower);
        return result;
    }

    inline std::string bytesToHexString(const uint8_t* bytes, size_t size) {
        std::ostringstream oss;
        for (size_t i = 0; i < size; ++i)
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i] << " ";
        return oss.str();
    }

    inline std::vector<DWORD> getProcessesByName(const std::wstring& processName) {
        std::vector<DWORD> processIDs;
        HANDLE hProcessSnap;
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hProcessSnap == INVALID_HANDLE_VALUE) return processIDs;
        if (!Process32FirstW(hProcessSnap, &pe32)) { CloseHandle(hProcessSnap); return processIDs; }
        std::wstring targetName = toLower(processName);
        do {
            std::wstring exeName(pe32.szExeFile);
            if (toLower(exeName) == targetName)
                processIDs.push_back(pe32.th32ProcessID);
        } while (Process32NextW(hProcessSnap, &pe32));
        CloseHandle(hProcessSnap);
        return processIDs;
    }

    inline bool isModuleLoaded(DWORD processID, const std::wstring& moduleName) {
        HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processID);
        if (hModuleSnap == INVALID_HANDLE_VALUE) return false;
        MODULEENTRY32W me32;
        me32.dwSize = sizeof(MODULEENTRY32W);
        if (!Module32FirstW(hModuleSnap, &me32)) { CloseHandle(hModuleSnap); return false; }
        std::wstring targetModuleName = toLower(moduleName);
        do {
            std::wstring modName(me32.szModule);
            std::wstring exePath(me32.szExePath);
            if (toLower(modName) == targetModuleName || toLower(exePath) == targetModuleName) {
                CloseHandle(hModuleSnap);
                return true;
            }
        } while (Module32NextW(hModuleSnap, &me32));
        CloseHandle(hModuleSnap);
        return false;
    }

    inline uintptr_t getRemoteModuleBaseAddress(HANDLE hProcess, const wchar_t* moduleName) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetProcessId(hProcess));
        if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
        MODULEENTRY32W me; me.dwSize = sizeof(MODULEENTRY32W);
        uintptr_t moduleBase = 0;
        if (Module32FirstW(hSnapshot, &me)) do {
            if (_wcsicmp(me.szModule, moduleName) == 0) { moduleBase = (uintptr_t)me.modBaseAddr; break; }
        } while (Module32NextW(hSnapshot, &me));
        CloseHandle(hSnapshot);
        return moduleBase;
    }

    inline uintptr_t getExportedFunctionAddress(HANDLE hProcess, uintptr_t moduleBase, const wchar_t* moduleName, const char* functionName) {
        HMODULE hLocalModule = LoadLibraryW(moduleName);
        if (!hLocalModule) return 0;
        FARPROC localProcAddress = GetProcAddress(hLocalModule, functionName);
        if (!localProcAddress) { FreeLibrary(hLocalModule); return 0; }
        uintptr_t offset = (uintptr_t)localProcAddress - (uintptr_t)hLocalModule;
        FreeLibrary(hLocalModule);
        return moduleBase + offset;
    }

    inline uintptr_t allocateMemoryNearAddress(HANDLE process, uintptr_t desiredAddress, SIZE_T size,
        DWORD protection = PAGE_EXECUTE_READWRITE, SIZE_T range = 0x20000000 - 0x2000) {
        const SIZE_T step = 0x1000;
        uintptr_t baseAddress = desiredAddress - range;
        uintptr_t endAddress = desiredAddress + range;
        for (uintptr_t address = baseAddress; address < endAddress; address += step) {
            void* allocatedMemory = VirtualAllocEx(process, reinterpret_cast<void*>(address),
                size, MEM_RESERVE | MEM_COMMIT, protection);
            if (allocatedMemory != NULL) return reinterpret_cast<uintptr_t>(allocatedMemory);
        }
        return 0;
    }

    inline bool assembleJumpNearInstruction(uint8_t* buffer, uintptr_t sourceAddress, uintptr_t targetAddress) {
        intptr_t jumpOffset = targetAddress - (sourceAddress + 5);
        if (std::abs(jumpOffset) > 0x7FFFFFFF) return false;
        buffer[0] = 0xE9;
        *reinterpret_cast<int32_t*>(buffer + 1) = static_cast<int32_t>(jumpOffset);
        return true;
    }

    inline bool writeMemory(HANDLE hProcess, uintptr_t address, const void* buffer, SIZE_T size) {
        SIZE_T written;
        return WriteProcessMemory(hProcess, reinterpret_cast<void*>(address), buffer, size, &written) && written == size;
    }

    inline bool writeMemoryWithProtection(HANDLE hProcess, uintptr_t address, const void* buffer, SIZE_T size) {
        DWORD oldProtect;
        if (!VirtualProtectEx(hProcess, reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
        bool success = writeMemory(hProcess, address, buffer, size);
        VirtualProtectEx(hProcess, reinterpret_cast<void*>(address), size, oldProtect, &oldProtect);
        return success;
    }

    inline bool writeMemoryWithProtectionDynamic(HANDLE hProcess, uintptr_t address, const std::vector<uint8_t>& buffer) {
        DWORD oldProtect;
        if (!VirtualProtectEx(hProcess, reinterpret_cast<void*>(address), buffer.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
        bool success = writeMemory(hProcess, address, buffer.data(), buffer.size());
        VirtualProtectEx(hProcess, reinterpret_cast<void*>(address), buffer.size(), oldProtect, &oldProtect);
        return success;
    }

    inline int patchGetWindowDisplayAffinity(HANDLE hProcess) {
        uintptr_t remoteUser32Base = getRemoteModuleBaseAddress(hProcess, L"USER32.dll");
        if (!remoteUser32Base) return 1;
        uintptr_t remoteTargetAddress = getExportedFunctionAddress(hProcess, remoteUser32Base, L"USER32.dll", "GetWindowDisplayAffinity");
        if (!remoteTargetAddress) return 1;
        uintptr_t allocatedMemory = allocateMemoryNearAddress(hProcess, remoteTargetAddress, 0x1000);
        if (!allocatedMemory) return 1;
        if (!writeMemoryWithProtectionDynamic(hProcess, allocatedMemory, { 0x48, 0x31, 0xC0, 0xC3 })) return 1;
        std::array<uint8_t, 5> jmpInstructionBytes;
        if (!assembleJumpNearInstruction(jmpInstructionBytes.data(), remoteTargetAddress, allocatedMemory)) return 1;
        std::array<uint8_t, 6> buffer;
        std::copy(jmpInstructionBytes.begin(), jmpInstructionBytes.end(), buffer.begin());
        buffer[5] = 0x90;
        if (!writeMemoryWithProtection(hProcess, remoteTargetAddress, buffer.data(), buffer.size())) return 1;
        return 0;
    }

    inline int patchKernel32Module32FirstW(HANDLE hProcess) {
        uintptr_t moduleBaseAddress = getRemoteModuleBaseAddress(hProcess, L"KERNEL32.DLL");
        if (!moduleBaseAddress) return 1;
        uintptr_t functionAddress = getExportedFunctionAddress(hProcess, moduleBaseAddress, L"KERNEL32.DLL", "Module32FirstW");
        if (!functionAddress) return 1;
        uintptr_t allocatedMemory = allocateMemoryNearAddress(hProcess, functionAddress, 0x1000);
        if (!allocatedMemory) return 1;
        if (!writeMemoryWithProtectionDynamic(hProcess, allocatedMemory, { 0x48, 0x31, 0xC0, 0xC3 })) return 1;
        std::array<uint8_t, 5> jmpInstructionBytes;
        if (!assembleJumpNearInstruction(jmpInstructionBytes.data(), functionAddress, allocatedMemory)) return 1;
        std::array<uint8_t, 7> buffer;
        std::copy(jmpInstructionBytes.begin(), jmpInstructionBytes.end(), buffer.begin());
        buffer[5] = 0x90;
        buffer[6] = 0x90;
        if (!writeMemoryWithProtection(hProcess, functionAddress, buffer.data(), buffer.size())) return 1;
        return 0;
    }

    inline bool CheckRegistryValue() {
        HKEY hKey;
        const wchar_t* regPath = L"SOFTWARE\\NVIDIA Corporation\\Global\\NvApp\\ShadowPlay\\FTS";
        const wchar_t* valueName = L"{497B8458-4244-4EE6-BFEA-F3D2BA294F21}";
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD value = 0, size = sizeof(DWORD);
            if (RegQueryValueExW(hKey, valueName, NULL, NULL, (BYTE*)&value, &size) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                return (value == 0x24);
            }
            RegCloseKey(hKey);
        }
        return false;
    }

    inline bool SetRegistryValue() {
        HKEY hKey;
        const wchar_t* regPath = L"SOFTWARE\\NVIDIA Corporation\\Global\\NvApp\\ShadowPlay\\FTS";
        const wchar_t* valueName = L"{497B8458-4244-4EE6-BFEA-F3D2BA294F21}";
        DWORD value = 0x24;
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, NULL,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            LONG result = RegSetValueExW(hKey, valueName, 0, REG_DWORD, (BYTE*)&value, sizeof(DWORD));
            RegCloseKey(hKey);
            return (result == ERROR_SUCCESS);
        }
        return false;
    }

    // Removes only the stream proof value instead of wiping the whole
    // ShadowPlay key tree (paths, hotkeys and settings stay intact).
    inline bool ClearStreamproofValue() {
        HKEY hKey;
        const wchar_t* regPath = L"SOFTWARE\\NVIDIA Corporation\\Global\\NvApp\\ShadowPlay\\FTS";
        const wchar_t* valueName = L"{497B8458-4244-4EE6-BFEA-F3D2BA294F21}";
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
            return false;
        const LONG result = RegDeleteValueW(hKey, valueName);
        RegCloseKey(hKey);
        return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
    }

    // Writes (or removes) NVIDIA's FTS capture flag. When the process is not
    // elevated the value is applied through one elevated reg.exe call, so the
    // toggle works without the user starting the cheat as administrator.
    inline bool ApplyStreamproofFlag(bool enable) {
        const std::wstring keyPath = L"HKLM\\SOFTWARE\\NVIDIA Corporation\\Global\\NvApp\\ShadowPlay\\FTS";
        const std::wstring valueName = L"{497B8458-4244-4EE6-BFEA-F3D2BA294F21}";

        if (enable) {
            if (!SetRegistryValue()) {
                const std::wstring params = L"add \"" + keyPath + L"\" /v \"" + valueName + L"\" /t REG_DWORD /d 36 /f";
                if (!RunElevatedProcess(L"C:\\Windows\\System32\\reg.exe", params))
                    return false;
            }
            return CheckRegistryValue();
        }

        if (!ClearStreamproofValue()) {
            const std::wstring params = L"delete \"" + keyPath + L"\" /v \"" + valueName + L"\" /f";
            if (!RunElevatedProcess(L"C:\\Windows\\System32\\reg.exe", params))
                return false;
        }
        return !CheckRegistryValue();
    }

    inline bool ResetRegisteryValue() {
        const wchar_t* regPath = L"SOFTWARE\\NVIDIA Corporation\\Global\\NvApp\\ShadowPlay";

        LONG result = RegDeleteTreeW(HKEY_LOCAL_MACHINE, regPath);

        return (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND);
    }
    inline int ApplyPatches() {
        std::vector<DWORD> processIDs = getProcessesByName(L"nvcontainer.exe");
        std::vector<DWORD> filteredProcessIDs;
        for (DWORD processID : processIDs)
            if (isModuleLoaded(processID, L"nvd3dumx.dll"))
                filteredProcessIDs.push_back(processID);
        if (filteredProcessIDs.size() != 1) return 1;
        DWORD nvcontainerProcessID = filteredProcessIDs[0];
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, nvcontainerProcessID);
        if (!hProcess) return 1;
        int errorCode = patchGetWindowDisplayAffinity(hProcess);
        if (errorCode) { CloseHandle(hProcess); return errorCode; }
        errorCode = patchKernel32Module32FirstW(hProcess);
        if (errorCode) { CloseHandle(hProcess); return errorCode; }
        CloseHandle(hProcess);
        return 0;
    }

    inline std::string Execute(bool enabled) {
        if (enabled) {
            if (!CheckRegistryValue()) {
                if (!SetRegistryValue())
                    return "";
                else RestartNV();

            }
        }
        else {
            if (CheckRegistryValue()) {
                ResetRegisteryValue();
                RestartNV();
            }
        }

        return "";
    }
}