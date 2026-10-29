#pragma once

#include <iostream>
#include <fstream>
#include <Windows.h>
#include <vector>
#include <cstring>
#include <thread>
#include <memory>
#include "tchar.h"
#include <TlHelp32.h>
#include <winternl.h>
#include <Psapi.h>
#include <intrin.h>
#include <strsafe.h>
#include <Shlwapi.h>
#include "oxorany_include.h"

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "wbemuuid.lib")

#define STATUS_SUCCESS   ((NTSTATUS)0x00000000L) 

typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(
    HANDLE ProcessHandle,
    PROCESSINFOCLASS ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
    );

typedef NTSTATUS(NTAPI* pZwQuerySystemInformation)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    );
typedef DWORD(WINAPI* PFZWQUERYINFORMATIONPROCESS) (
    HANDLE		ProcessHandle,
    DWORD		ProcessInformationClass,	// Origianl : _PROCESS_INFORMATION_CLASS
    PVOID		ProcessInformation,
    ULONG		ProcessInformationLength,
    PULONG		ReturnLength
    );

__forceinline std::uint32_t find_process(const char* name) {
    DWORD processes[1024], needed, numProcesses;

    if (!EnumProcesses(processes, sizeof(processes), &needed)) {
        return 0;
    }

    numProcesses = needed / sizeof(DWORD);

    for (unsigned int i = 0; i < numProcesses; i++) {
        if (processes[i] != 0) {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processes[i]);
            if (hProcess) {
                char szProcessName[MAX_PATH] = { 0 };
                HMODULE hMod;
                DWORD cbNeeded;

                if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
                    GetModuleBaseNameA(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(char));

                    if (!_stricmp(name, szProcessName)) {
                        CloseHandle(hProcess);
                        return processes[i];
                    }
                }

                CloseHandle(hProcess);
            }
        }
    }

    return 0;
}

__forceinline void protection_exit(const char* detection_type = "Unknown")
{
    Sleep(100);
    TerminateProcess(GetCurrentProcess(), 0);
    ExitProcess(0);
}

__forceinline void protection_exit(const std::string& detection_type)
{
    Sleep(100);
    TerminateProcess(GetCurrentProcess(), 0);
    ExitProcess(0);
}

__forceinline void check_breakpoint()
{
    CONTEXT ct = { 0 };
    ct.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    HANDLE hThread = GetCurrentThread();
    if (GetThreadContext(hThread, &ct))
    {
        if (ct.Dr0 || ct.Dr1 || ct.Dr2 || ct.Dr3)
            protection_exit("");
    }
}

__forceinline void check_peb()
{
    PROCESS_BASIC_INFORMATION pbi = { 0 };
    ULONG returnLength = 0;
    HMODULE hNtdll = GetModuleHandleA(WRAPPER_ALIAN("ntdll.dll"));
    if (!hNtdll) return;
    pNtQueryInformationProcess NtQueryInfoProcess = (pNtQueryInformationProcess)GetProcAddress(hNtdll, WRAPPER_ALIAN("NtQueryInformationProcess"));
    if (NtQueryInfoProcess && NT_SUCCESS(NtQueryInfoProcess(GetCurrentProcess(), ProcessBasicInformation, &pbi, sizeof(pbi), &returnLength)))
    {
        BYTE* peb = (BYTE*)pbi.PebBaseAddress;
        if (peb && peb[2] == 1)
            protection_exit("");
    }
}

__forceinline void check_global_peb()
{
    PROCESS_BASIC_INFORMATION pbi = { 0 };
    ULONG returnLength = 0;
    HMODULE hNtdll = GetModuleHandleA(WRAPPER_ALIAN("ntdll.dll"));
    if (!hNtdll) return;
    pNtQueryInformationProcess NtQueryInfoProcess = (pNtQueryInformationProcess)GetProcAddress(hNtdll, WRAPPER_ALIAN("NtQueryInformationProcess"));
    if (NtQueryInfoProcess && NT_SUCCESS(NtQueryInfoProcess(GetCurrentProcess(), ProcessBasicInformation, &pbi, sizeof(pbi), &returnLength)))
    {
        if (pbi.PebBaseAddress)
        {
            ULONG NtGlobalFlags = *(ULONG*)((BYTE*)pbi.PebBaseAddress + 0x68);
            if ((NtGlobalFlags & 0x70) != 0)
                protection_exit("");
        }
    }
}

__forceinline void check_debug_port()
{
    typedef DWORD(WINAPI* PFZWQUERYINFORMATIONPROCESS)(HANDLE, DWORD, PVOID, ULONG, PULONG);
    DWORD ProcessDebugPort;
    PFZWQUERYINFORMATIONPROCESS pfZwQueryInformationProcess;
    HMODULE h_ntdll = GetModuleHandle("ntdll.dll");
    if (!h_ntdll) return;
    pfZwQueryInformationProcess = (PFZWQUERYINFORMATIONPROCESS)GetProcAddress(h_ntdll, "ZwQueryInformationProcess");
    if (pfZwQueryInformationProcess)
    {
        pfZwQueryInformationProcess(GetCurrentProcess(), 0x7, &ProcessDebugPort, 4, 0);
        if (ProcessDebugPort == -1)
            protection_exit("");
    }
}
__forceinline void check_remote_debugger()
{
    BOOL result;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &result);
    if (result)
        protection_exit("");
}

__forceinline void check_debug_present()
{
    if (IsDebuggerPresent())
        protection_exit("");
}

__forceinline void check_debug_flags()
{
    typedef NTSTATUS(WINAPI* pNtQueryInformationProcess)(IN HANDLE, IN UINT, OUT PVOID, IN ULONG, OUT PULONG);
    const int ProcessDebugFlags = 0x1f;
    pNtQueryInformationProcess NtQueryInfoProcess = NULL;
    NTSTATUS Status;
    DWORD NoDebugInherit = 0;
    HMODULE hNtDll = GetModuleHandleA(WRAPPER_ALIAN("ntdll.dll"));
    if (hNtDll)
    {
        NtQueryInfoProcess = (pNtQueryInformationProcess)GetProcAddress(hNtDll, WRAPPER_ALIAN("NtQueryInformationProcess"));
        if (NtQueryInfoProcess)
        {
            Status = NtQueryInfoProcess(GetCurrentProcess(), ProcessDebugFlags, &NoDebugInherit, sizeof(DWORD), NULL);
            if (Status == 0x00000000 && NoDebugInherit == FALSE)
                protection_exit("");
        }
    }
}

__forceinline void check_kernel_debug()
{
    typedef struct _SYSTEM_KERNEL_DEBUGGER_INFORMATION {
        BOOLEAN DebuggerEnabled;
        BOOLEAN DebuggerNotPresent;
    } SYSTEM_KERNEL_DEBUGGER_INFORMATION, * PSYSTEM_KERNEL_DEBUGGER_INFORMATION;
    SYSTEM_KERNEL_DEBUGGER_INFORMATION Info;
    SYSTEM_INFORMATION_CLASS sic = (SYSTEM_INFORMATION_CLASS)35;
    pZwQuerySystemInformation ZwQuerySystemInformation = NULL;
    HMODULE hNtDll = GetModuleHandleA(WRAPPER_ALIAN("ntdll.dll"));
    if (hNtDll)
    {
        ZwQuerySystemInformation = (pZwQuerySystemInformation)GetProcAddress(hNtDll, WRAPPER_ALIAN("ZwQuerySystemInformation"));
        if (ZwQuerySystemInformation)
        {
            if (STATUS_SUCCESS == ZwQuerySystemInformation(sic, &Info, sizeof(Info), NULL))
            {
                if (Info.DebuggerEnabled && !Info.DebuggerNotPresent)
                    protection_exit("");
            }
        }
    }
}

__forceinline void check_driver_detect()
{
    const TCHAR* devices[] = { "\\\\.\\Dumper", "\\\\.\\KsDumper" };
    WORD iLength = sizeof(devices) / sizeof(devices[0]);
    for (int i = 0; i < iLength; i++)
    {
        HANDLE hFile = CreateFile(devices[i], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hFile);
            protection_exit("");
        }
    }
}

__forceinline void check_detect_exe()
{
    const char* exe_names[] = {
        // Debuggers and Disassemblers
        "x64dbg.exe", "x32dbg.exe", "x64_dbg.exe", "x32_dbg.exe", "x64dbg64.exe", "x32dbg32.exe",
        "x64dbg_x64.exe", "x32dbg_x86.exe", "x64dbg_x86.exe", "x32dbg_x64.exe",
        "x64dbg", "x32dbg", "x64_dbg", "x32_dbg",
        "ida.exe", "ida64.exe", "ida32.exe", "idaq.exe", "idaq64.exe", "idaq32.exe",
        "ida_x64.exe", "ida_x86.exe", "ida64_x64.exe", "ida32_x86.exe",
        "OLLYDBG.exe", "ollydbg.exe", "OLLYDBG_x86.exe", "ollydbg_x86.exe",
        "windbg.exe", "windbg64.exe", "windbg32.exe", "windbg_x64.exe", "windbg_x86.exe",
        "kd.exe", "kd64.exe", "kd32.exe", "cdb.exe", "cdb64.exe", "cdb32.exe",
        "ntsd.exe", "ntsd64.exe", "ntsd32.exe",
        "immunity_debugger.exe", "immunitydebugger.exe", "immunity_debugger_x86.exe",
        "radare2.exe", "r2.exe", "radare2_x64.exe", "radare2_x86.exe", "r2_x64.exe", "r2_x86.exe",
        "ghidra.exe", "ghidraRun.exe", "ghidra_x64.exe", "ghidra_x86.exe",
        "binaryninja.exe", "binaryninja_x64.exe", "binaryninja_x86.exe",
        "hopper.exe", "hopper_x64.exe", "hopper_x86.exe",
        "snowman.exe", "snowman_x64.exe", "snowman_x86.exe",
        "relyze.exe", "relyze_x64.exe", "relyze_x86.exe",

        // Memory Dumpers and Analysis
        "KsDumper.exe", "KsDumperClient.exe", "KsDumper_x64.exe", "KsDumper_x86.exe",
        "KsDumperClient_x64.exe", "KsDumperClient_x86.exe",
        "scylla.exe", "scylla_x64.exe", "scylla_x86.exe", "scylla_x64_v0.9.8.exe",
        "scylla_x86_v0.9.8.exe", "scylla_x64_v0.9.5a.exe", "scylla_x86_v0.9.5a.exe",
        "scylla_x64_v0.9.5.exe", "scylla_x86_v0.9.5.exe",

        // Process Monitors and System Tools
        "ProcessHacker.exe", "processhacker.exe", "ProcessHacker_x64.exe", "ProcessHacker_x86.exe",
        "SystemInformer.exe", "systeminformer.exe", "SystemInformer_x64.exe", "SystemInformer_x86.exe",
        "procexp.exe", "procexp64.exe", "procexp32.exe", "procexp_x64.exe", "procexp_x86.exe",
        "procmon.exe", "procmon64.exe", "procmon32.exe", "procmon_x64.exe", "procmon_x86.exe",
        "vmmap.exe", "vmmap64.exe", "vmmap32.exe", "vmmap_x64.exe", "vmmap_x86.exe",
        "rammap.exe", "rammap_x64.exe", "rammap_x86.exe",
        "handle.exe", "handle64.exe", "handle32.exe", "handle_x64.exe", "handle_x86.exe",

        // Remote Access Tools
        "AnyDesk.exe", "anydesk.exe", "AnyDesk_x64.exe", "AnyDesk_x86.exe",
        "parsecd.exe", "parsecd_x64.exe", "parsecd_x86.exe",
        "TeamViewer.exe", "teamviewer.exe", "TeamViewer_x64.exe", "TeamViewer_x86.exe",

        // Hex Editors
        "HxD.exe", "HxD64.exe", "HxD32.exe", "HxD_x64.exe", "HxD_x86.exe",
        "HxD64_x64.exe", "HxD32_x86.exe",

        // PE Analyzers
        "pestudio.exe", "pestudio_x64.exe", "pestudio_x86.exe",
        "peid.exe", "peid_x86.exe",
        "cffexplorer.exe", "cffexplorer_x64.exe", "cffexplorer_x86.exe",
        "pe-bear.exe", "pebear.exe", "pe-bear_x64.exe", "pebear_x64.exe",
        "exeinfope.exe", "exeinfope_x86.exe",
        "die.exe", "die_x64.exe", "die_x86.exe",
        "ResourceHacker.exe", "reshacker.exe", "ResourceHacker_x86.exe",

        // Network Analysis
        "Wireshark.exe", "wireshark_x64.exe", "wireshark_x86.exe",
        "tshark.exe", "tshark_x64.exe", "tshark_x86.exe",
        "Fiddler.exe", "Fiddler Everywhere.exe", "Fiddler_x64.exe", "Fiddler_x86.exe",
        "HTTPDebuggerUI.exe", "HTTPDebuggerSvc.exe", "HTTPDebuggerUI_x64.exe", "HTTPDebuggerSvc_x64.exe",
        "HTTP Debugger Windows Service (32 bit).exe",
        "burpsuite.exe", "burpsuite_x64.exe", "burpsuite_x86.exe",
        "zaproxy.exe", "zaproxy_x64.exe", "zaproxy_x86.exe",
        "charles.exe", "charles_x64.exe", "charles_x86.exe",
        "mitmproxy.exe", "mitmproxy_x64.exe", "mitmproxy_x86.exe",
        "proxyman.exe", "proxyman_x64.exe", "proxyman_x86.exe",
        "tcpview.exe", "tcpview64.exe", "tcpview32.exe", "tcpview_x64.exe", "tcpview_x86.exe",
        "currports.exe", "currports_x64.exe", "currports_x86.exe",
        "nmap.exe", "nmap_x64.exe", "nmap_x86.exe",
        "zenmap.exe", "zenmap_x64.exe", "zenmap_x86.exe",
        "ettercap.exe", "ettercap_x64.exe", "ettercap_x86.exe",
        "networkminer.exe", "networkminer_x64.exe", "networkminer_x86.exe",

        // API Monitors
        "apimonitor.exe", "apimonitor-x64.exe", "apimonitor-x86.exe",
        "apimonitor_x64.exe", "apimonitor_x86.exe",
        "winAPIoverride.exe", "winAPIoverride_x64.exe", "winAPIoverride_x86.exe",

        // Injection and Hooking Tools
        "Xenos64.exe", "Xenos32.exe", "Xenos_x64.exe", "Xenos_x86.exe",
        "detours.exe", "detours_x64.exe", "detours_x86.exe",

        // Cheat Engines and Game Hacking Tools
        "Cheat Engine.exe", "cheatengine.exe", "Cheat Engine_x64.exe", "Cheat Engine_x86.exe",
        "cheatengine_x64.exe", "cheatengine_x86.exe",

        // Other Analysis Tools
        "FolderChangesView.exe", "FolderChangesView_x64.exe", "FolderChangesView_x86.exe",
        "upx.exe", "upx_x64.exe", "upx_x86.exe",
        "de4dot.exe", "de4dot_x64.exe", "de4dot_x86.exe",
        "dnspy.exe", "dnspy_x64.exe", "dnspy_x86.exe",
        "reflexil.exe", "reflexil_x64.exe", "reflexil_x86.exe"
    };
    int count = sizeof(exe_names) / sizeof(exe_names[0]);
    for (int i = 0; i < count; i++)
    {
        if (find_process(exe_names[i]))
            protection_exit("");
    }
}

// Detect debuggers by window class names (harder to bypass than process names)
__forceinline void check_detect_window_class()
{
    // x64dbg/x32dbg window classes
    const char* class_names[] = {
        "Qt5QWindowIcon",           // x64dbg uses Qt framework
        "Qt5QWindowToolSaveBits",   // x64dbg Qt window class
        "Qt5QWindowOwnDCIcon",      // x64dbg Qt window class
        "OLLYDBG",                  // OllyDbg window class
        "WinDbgFrameClass",         // WinDbg window class
        "IDAWIN",                    // IDA Pro window class
    };

    int count = sizeof(class_names) / sizeof(class_names[0]);
    for (int i = 0; i < count; i++)
    {
        HWND window = FindWindowA(class_names[i], NULL);
        if (window)
            protection_exit("");
    }
}

__forceinline void check_detect_title()
{
    const char* title_names[] = {
        // IDA Pro
        "IDA: Quick start", "IDA Pro", "IDA", "IDA Free", "IDA Freeware",

        // x64dbg / x32dbg
        "x32DBG", "x64DBG", "x32_dbg", "x64_dbg", "x32dbg", "x64dbg",
        "x64dbg -", "x32dbg -", "x64dbg.exe", "x32dbg.exe",

        // Cheat Engine
        "Cheat Engine", "Cheat Engine 7.4", "Cheat Engine 7.3", "Cheat Engine 7.2",
        "Cheat Engine 7.1", "Cheat Engine 7.0", "CheatEngine",

        // Memory Viewers
        "Memory Viewer", "Memory View", "Memory Editor",

        // Debuggers
        "OllyDbg", "Olly Debugger", "Olly", "WinDbg", "WinDbg Preview",
        "Immunity Debugger", "ImmunityDebugger",

        // Dumpers
        "KsDumper", "Scylla x86 v0.9.8", "Scylla x64 v0.9.8",
        "Scylla x86 v0.9.5a", "Scylla x64 v0.9.5a",
        "Scylla x86 v0.9.5", "Scylla x64 v0.9.5",

        // Fiddler
        "Fiddler Everywhere", "Fiddler Classic", "Fiddler Jam", "FiddlerCap", "FiddlerCore",

        // Hex Editors
        "HxD", "HxD Hex Editor",

        // Disassemblers
        "Snowman", "Radare2", "r2", "Ghidra", "Binary Ninja", "Hopper Disassembler",
        "Relyze",

        // PE Analyzers
        "pestudio", "PEiD", "CFF Explorer", "PE-bear", "010 Editor", "Hex Workshop",
        "Detect It Easy v3.01",

        // Process Monitors
        "ProcessHacker", "Process Hacker", "Process Monitor", "ProcMon",
        "VMMap", "RamMap", "API Monitor",

        // Network Tools
        "Wireshark", "TShark", "tcpdump", "Ettercap", "NetworkMiner", "Kismet",
        "Aircrack-ng", "Nmap", "Zenmap", "Angry IP Scanner", "Advanced IP Scanner",
        "SoftPerfect Network Scanner",

        // Proxy Tools
        "Burp Suite", "Burp Suite Professional", "Burp Suite Community",
        "OWASP ZAP", "Zed Attack Proxy", "Charles Proxy", "mitmproxy", "Proxyman",
        "HTTP Toolkit", "Postman Interceptor",

        // System Tools
        "NetWorx", "NetLimiter", "GlassWire", "Little Snitch", "TCPView", "CurrPorts",
        "Netstat", "Resource Monitor",

        // Injection Tools
        "Process Injection", "DLL Injection", "WinAPIOverride", "Detours"
    };
    int count = sizeof(title_names) / sizeof(title_names[0]);
    for (int i = 0; i < count; i++)
    {
        HWND window = FindWindowA(0, title_names[i]);
        if (window)
            protection_exit("");
    }
}

// Mask: only run when cover process is active; otherwise launch fallback and exit
__forceinline bool is_process_running(const char* processName)
{
    return find_process(processName) != 0;
}

__forceinline bool is_window_open(const char* windowTitle)
{
    return FindWindowA(NULL, windowTitle) != NULL;
}

__forceinline bool is_any_window_open(const char* titles[], int count)
{
    for (int i = 0; i < count; i++)
    {
        if (FindWindowA(NULL, titles[i]) != NULL)
            return true;
    }
    return false;
}

inline VOID WINAPI tls_callback(PVOID hModule, DWORD dwReason, PVOID pContext)
{
    (void)hModule;
    (void)pContext;
    if (dwReason == DLL_PROCESS_ATTACH) {
        // Early anti-debug: check PEB->BeingDebugged before any code runs
        BOOL isDebugged = FALSE;
        __try {
            PPEB pPeb = (PPEB)__readgsqword(0x60);
            if (pPeb && pPeb->BeingDebugged) {
                isDebugged = TRUE;
            }
            // Check NtGlobalFlag (offset 0xBC in 64-bit PEB)
            DWORD ntGlobalFlag = *(DWORD*)((BYTE*)pPeb + 0xBC);
            if (ntGlobalFlag & 0x70) { // FLG_HEAP_ENABLE_TAIL_CHECK | FLG_HEAP_ENABLE_FREE_CHECK | FLG_HEAP_VALIDATE_PARAMETERS
                isDebugged = TRUE;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (isDebugged) {
            TerminateProcess(GetCurrentProcess(), 0);
        }
    }
}





class c_protection
{
public:
    void run_protect();

    void module_hook();
    void page_breakpoint();
    void write_watch();
    void process_job();
    void kernel_debugger();
    void protected_handle();
    void memory_breakpoint();
    void trap_flag();
    void interrupt3();
    void software_breakpoints();
    void hardware_breakpoints();
    void unhandle_debug();
    void invalid_handle();
    void final_debug_check();

};

inline std::unique_ptr<c_protection> protect = std::make_unique<c_protection>();

__forceinline void run_protection()
{
    check_breakpoint();
    check_peb();
    check_global_peb();
    check_debug_port();
    check_remote_debugger();
    check_debug_present();
    check_debug_flags();
    check_kernel_debug();
    check_driver_detect();
    check_detect_title();
    check_detect_window_class();
    check_detect_exe();
    protect->module_hook();
    protect->page_breakpoint();
    protect->write_watch();
    protect->process_job();
    protect->kernel_debugger();
    protect->protected_handle();
    protect->memory_breakpoint();
    protect->trap_flag();
    protect->interrupt3();
    protect->software_breakpoints();
    protect->hardware_breakpoints();
    protect->unhandle_debug();
    protect->invalid_handle();

    static int fast_check_counter = 0;

    while (true)
    {
        fast_check_counter++;

        if (fast_check_counter >= 300)
        {
            fast_check_counter = 0;
            check_breakpoint();
            check_peb();
            check_global_peb();
            check_debug_port();
            check_remote_debugger();
            check_debug_present();
            check_debug_flags();
            check_kernel_debug();
            check_driver_detect();

            protect->module_hook();
            protect->page_breakpoint();
            protect->write_watch();
            protect->process_job();
            protect->kernel_debugger();
            protect->protected_handle();
            protect->memory_breakpoint();
            protect->trap_flag();
            protect->interrupt3();
            protect->software_breakpoints();
            protect->hardware_breakpoints();
            protect->unhandle_debug();
            protect->invalid_handle();
        }

        SleepEx(100, true);
    }
}

// Helper function to log and exit on detection (must be after protect declaration)
// Uses TerminateProcess for forceful unload - cannot be bypassed by hooks
__forceinline void protection_exit_detection(const char* detection_type)
{
    // protect->logger(std::string(detection_type));
    Sleep(100); 
    TerminateProcess(GetCurrentProcess(), 0);
    ExitProcess(0); // Fallback
}

// Overload for std::string
__forceinline void protection_exit_detection(const std::string& detection_type)
{
    // protect->logger(detection_type);
    Sleep(100); 
    TerminateProcess(GetCurrentProcess(), 0);
    ExitProcess(0); // Fallback
}

// Removed unused exe_names and title_names arrays - detection uses arrays in main.cpp instead

