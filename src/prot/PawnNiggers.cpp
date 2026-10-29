#include "pawnniggers.hpp"

#include <thread>
#include <vector>
#include <algorithm>
#include <chrono>

#define close_app TerminateProcess(GetCurrentProcess(),0); ExitProcess(0);
#define force_crash abort();

namespace AntiCrack {

    BOOL CALLBACK enum_windows_callback(HWND hWnd, LPARAM lparam) {

        const std::vector<std::string> bad_titles =
        {
            WRAPPER_ALIAN("httpdebugger"),
            WRAPPER_ALIAN("x64dbg"),
            WRAPPER_ALIAN("debugger"),
            WRAPPER_ALIAN("wireshark"),
            WRAPPER_ALIAN("ghidra"),
            WRAPPER_ALIAN("ollydbg"),
        };

        int length = GetWindowTextLength(hWnd);
        if (length == 0) return TRUE;

        std::vector<char> buffer(length + 1);
        GetWindowText(hWnd, buffer.data(), length + 1);

        std::string title(buffer.data());
        std::transform(title.begin(), title.end(), title.begin(), ::tolower);

        if (IsWindowVisible(hWnd)) {
            for (const auto& s : bad_titles) {
                if (title.find(s) != std::string::npos) {
                    Sleep(1000);
                    close_app;
                    return TRUE;
                }
            }
        }

        return TRUE;
    }

    bool FindHookInAddr(BYTE* func) {
        if (func[0] == 0xE9) return true;
        if (func[0] == 0xFF && func[1] == 0x25) return true;
        if (func[0] == 0x90 && func[1] == 0x90 && func[2] == 0xE9) return true;
        return false;
    }

    void StopService(const char* service_name) {

        SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (!scm) return;

        SC_HANDLE service = OpenService(scm, service_name, SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (service) {
            SERVICE_STATUS status;
            ControlService(service, SERVICE_CONTROL_STOP, &status);
            CloseServiceHandle(service);
        }

        CloseServiceHandle(scm);
    }

    void CheckWindows() {
        while (true) {
            EnumWindows(enum_windows_callback, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void CheckServices() {
        while (true) {
            StopService(WRAPPER_ALIAN("HTTPDebuggerPro"));
            StopService(WRAPPER_ALIAN("KsDumper"));
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
        }
    }

    void AntiDbg() {
        while (true) {
            if (IsDebuggerPresent()) {
                Sleep(1000);
                close_app;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(600));
        }
    }

    OBJECT_ATTRIBUTES InitObjectAttributes(PUNICODE_STRING name, ULONG attributes, HANDLE hRoot, PSECURITY_DESCRIPTOR security)
    {
        OBJECT_ATTRIBUTES obj{};
        obj.Length = sizeof(OBJECT_ATTRIBUTES);
        obj.ObjectName = name;
        obj.Attributes = attributes;
        obj.RootDirectory = hRoot;
        obj.SecurityDescriptor = security;
        return obj;
    }

    HANDLE DumpHandle(HANDLE pid, HANDLE handle)
    {
        auto NtOpenProcess = (_NtOpenProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtOpenProcess");

        HANDLE process;
        CLIENT_ID cid{};
        cid.UniqueProcess = pid;

        OBJECT_ATTRIBUTES obj = InitObjectAttributes(NULL, 0, NULL, NULL);

        if (!NT_SUCCESS(NtOpenProcess(&process, PROCESS_ALL_ACCESS, &obj, &cid)))
            return nullptr;

        HANDLE localHandle = nullptr;

        if (DuplicateHandle(process, handle, GetCurrentProcess(), &localHandle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            CloseHandle(process);
            return localHandle;
        }

        CloseHandle(process);
        return nullptr;
    }

    void KillHandle(HANDLE pid, HANDLE handle)
    {
        auto NtOpenProcess = (_NtOpenProcess)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtOpenProcess");

        HANDLE process;
        CLIENT_ID cid{};
        cid.UniqueProcess = pid;

        OBJECT_ATTRIBUTES obj = InitObjectAttributes(NULL, 0, NULL, NULL);

        if (NT_SUCCESS(NtOpenProcess(&process, PROCESS_ALL_ACCESS, &obj, &cid))) {
            HANDLE tmp;
            DuplicateHandle(process, handle, GetCurrentProcess(), &tmp, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
            CloseHandle(process);
        }
    }

    std::string GetProcesssNameByPid(DWORD pid) {

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) return "";

        char buffer[MAX_PATH]{};
        if (GetModuleFileNameExA(hProcess, NULL, buffer, MAX_PATH)) {
            CloseHandle(hProcess);
            return buffer;
        }

        CloseHandle(hProcess);
        return "";
    }

    bool KillProcess(DWORD pid) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (!h) return false;

        bool result = TerminateProcess(h, 1);
        CloseHandle(h);
        return result;
    }

    void CheckProcesses() {
        while (true) {
            // (ingekort – jouw originele logica kan hier blijven)
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    void DoProtect() {
        std::thread(CheckWindows).detach();
        std::thread(CheckServices).detach();
        std::thread(CheckProcesses).detach();
        std::thread(AntiDbg).detach();
    }
}