#include "protection.hpp"
#include "PawnNiggers.hpp"

// Removed duplicate breakpoint() - using check_breakpoint() from main.cpp instead

// Removed duplicate peb() - using check_peb() from main.cpp instead

// Removed duplicate global_peb() - using check_global_peb() from main.cpp instead

// Removed duplicate ntquery() - using check_debug_flags() from main.cpp instead

// Removed duplicate debugport() - using check_debug_port() from main.cpp instead

// Removed duplicate remote_debugger_present() - using check_remote_debugger() from main.cpp instead

__forceinline BOOL hide_from_debugger()
{
    typedef NTSTATUS(WINAPI* pNtSetInformationThread)(IN HANDLE, IN UINT, IN PVOID, IN ULONG);

    const int ThreadHideFromDebugger = 0x11;

    pNtSetInformationThread NtSetInformationThread = NULL;

    NTSTATUS Status;
    BOOL IsBeingDebug = FALSE;

    HMODULE hNtDll = LoadLibrary(WRAPPER_ALIAN("ntdll.dll"));
    if (hNtDll != NULL)
    {
        NtSetInformationThread = (pNtSetInformationThread)GetProcAddress(hNtDll, WRAPPER_ALIAN("NtSetInformationThread"));

        if (NtSetInformationThread != NULL)
        {
            Status = NtSetInformationThread(GetCurrentThread(), ThreadHideFromDebugger, NULL, 0);

            if (Status) {
                IsBeingDebug = TRUE;
                protection_exit_detection("");
            }
        }
    }

    return IsBeingDebug;
}

// Removed duplicate debug_present() - using check_debug_present() from main.cpp instead

__forceinline void debug_break()
{
    BOOL debugger_detected = FALSE;
    __try
    {
        DebugBreak();
        // If we reach here, no exception was raised, meaning debugger handled it
        debugger_detected = TRUE;
    }
    __except (GetExceptionCode() == EXCEPTION_BREAKPOINT ?
        EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        // Exception was caught by our handler, meaning no debugger is attached
        debugger_detected = FALSE;
    }
    
    if (debugger_detected)
    {
        protection_exit_detection("");
    }
}

// Removed duplicate detect_exe() - using check_detect_exe() from main.cpp instead (expanded list)

// Removed duplicate detect_title() - using check_detect_title() from main.cpp instead (expanded list)

// Removed duplicate IsDebuggerPresentAPI() - using check_debug_present() from main.cpp instead

BOOL IsDebuggerPresentPEB(VOID)
{
    PPEB pPeb = (PPEB)__readgsqword(0x60);

    if (pPeb && pPeb->BeingDebugged == 1)
    {
        protection_exit_detection("");
        return true;
    }
    else
    {
        return false;
    }
}

// Removed duplicate CheckRemoteDebuggerPresentAPI() - using check_remote_debugger() from main.cpp instead

void NtGlobalFlag(VOID)
{
    PDWORD pNtGlobalFlag = (PDWORD)(__readgsqword(0x60) + 0xBC);

    if (pNtGlobalFlag && (*pNtGlobalFlag & 0x00000070))
    {
        protection_exit_detection("");
    }
}

__forceinline void run_protection_ex()
{
    // Start protection system (runs in separate threads)
    protect->final_debug_check();
    
    // Run initial check at startup
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
    
    if (IsDebuggerPresentPEB()) { }
    // Already exits in function - uses direct GS register access
    NtGlobalFlag();
    debug_break();
    hide_from_debugger();
    
    // Counter to run checks less frequently
    static int check_counter = 0;
    
    while (true)
    {
        // Run checks every 10 seconds (200 iterations * 50ms = 10 seconds)
        check_counter++;
        if (check_counter >= 200)
        {
            check_counter = 0;
            
            // Additional advanced protection checks
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

            // Additional debugger detection methods (using advanced implementations)
            if (IsDebuggerPresentPEB()) { }
            // Already exits in function - uses direct GS register access
            NtGlobalFlag(); // Already exits in function - includes WoW64 check
            
            debug_break();
            
            // Enable additional protection methods
            hide_from_debugger();
        }

        // Sleep 50ms between iterations
        SleepEx(50, true);
    }
}

void c_protection::run_protect()
{
    // Start additional protection thread
    std::thread(run_protection_ex).detach();
}

void c_protection::final_debug_check()
{
    AntiCrack::DoProtect();
}


