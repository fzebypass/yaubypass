#pragma once
#include <windows.h>
#include <TlHelp32.h>
#include <string>
#include <thread>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <security/xorstr.hpp>

#define XWSTR(x) xorstr_(x).crypt_get()

namespace Core
{
    inline std::atomic<bool> g_ShouldReapplyNvidiaPatches{ false };

    class cNvidiaBypass
    {
    private:

        inline static auto REG_PATH =
            xorstr_(L"SOFTWARE\\NVIDIA Corporation\\Global\\NvApp\\ShadowPlay\\FTS");

        inline static auto VALUE_NAME =
            xorstr_(L"{497B8458-4244-4EE6-BFEA-F3D2BA294F21}");

        static constexpr DWORD VALUE_DATA = 0x00000024;
        static constexpr const wchar_t* NV_PROCESS = L"nvcontainer.exe";

        std::atomic<bool> m_running{ false };
        std::thread       m_watchThread;

        static inline cNvidiaBypass* s_instance = nullptr;

        static void StaticCleanup()
        {
            if (s_instance)
                s_instance->EmergencyCleanup();
        }

        bool OpenKey(HKEY& hKey, REGSAM access) const
        {
            LSTATUS st = RegOpenKeyExW(
                HKEY_LOCAL_MACHINE, REG_PATH.crypt_get(), 0, access, &hKey);

            if (st == ERROR_SUCCESS)
                return true;

            if (access & KEY_SET_VALUE)
            {
                DWORD disp = 0;
                st = RegCreateKeyExW(
                    HKEY_LOCAL_MACHINE,
                    REG_PATH.crypt_get(),
                    0, nullptr,
                    REG_OPTION_NON_VOLATILE,
                    access,
                    nullptr,
                    &hKey,
                    &disp);

                return st == ERROR_SUCCESS;
            }
            return false;
        }

        bool WriteValue() const
        {
            HKEY hKey = nullptr;
            if (!OpenKey(hKey, KEY_SET_VALUE))
                return false;

            DWORD data = VALUE_DATA;

            LSTATUS st = RegSetValueExW(
                hKey,
                VALUE_NAME.crypt_get(),
                0,
                REG_DWORD,
                reinterpret_cast<const BYTE*>(&data),
                sizeof(data));

            RegCloseKey(hKey);
            return st == ERROR_SUCCESS;
        }

        bool DeleteValue() const
        {
            HKEY hKey = nullptr;
            if (!OpenKey(hKey, KEY_SET_VALUE))
                return false;

            RegDeleteValueW(hKey, VALUE_NAME.crypt_get());
            RegCloseKey(hKey);
            return true;
        }

        bool ValueIsCorrect() const
        {
            HKEY hKey = nullptr;
            if (!OpenKey(hKey, KEY_QUERY_VALUE))
                return false;

            DWORD data = 0, size = sizeof(data), type = 0;

            LSTATUS st = RegQueryValueExW(
                hKey,
                VALUE_NAME.crypt_get(),
                nullptr,
                &type,
                reinterpret_cast<BYTE*>(&data),
                &size);

            RegCloseKey(hKey);

            return (st == ERROR_SUCCESS) &&
                (type == REG_DWORD) &&
                (data == VALUE_DATA);
        }

        bool KillProcess(const wchar_t* name) const
        {
            bool killed = false;

            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap == INVALID_HANDLE_VALUE)
                return false;

            PROCESSENTRY32W pe{ sizeof(pe) };

            for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
            {
                if (_wcsicmp(pe.szExeFile, name) == 0)
                {
                    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (h)
                    {
                        TerminateProcess(h, 0);
                        CloseHandle(h);
                        killed = true;
                    }
                }
            }

            CloseHandle(snap);
            return killed;
        }

        void RestartNvContainer() const
        {
            KillProcess(NV_PROCESS);
            g_ShouldReapplyNvidiaPatches.store(true, std::memory_order_release);
            Sleep(2000);
        }

        void WatchLoop()
        {
            while (m_running)
            {
                if (!ValueIsCorrect())
                    WriteValue();

                for (int i = 0; i < 50 && m_running; ++i)
                    Sleep(100);
            }
        }

    public:

        void EmergencyCleanup() const
        {
            DeleteValue();
            KillProcess(NV_PROCESS);
        }

        void Apply()
        {
            if (m_running)
                return;

            if (!WriteValue())
                return;

            RestartNvContainer();

            m_running = true;
            m_watchThread = std::thread(&cNvidiaBypass::WatchLoop, this);
            m_watchThread.detach();

            s_instance = this;

            std::atexit(StaticCleanup);

            std::set_terminate([]() {
                StaticCleanup();
                std::abort();
                });

            SetUnhandledExceptionFilter([](EXCEPTION_POINTERS*) -> LONG {
                StaticCleanup();
                return EXCEPTION_CONTINUE_SEARCH;
                });

            AddVectoredExceptionHandler(0, [](EXCEPTION_POINTERS* ep) -> LONG {
                DWORD code = ep->ExceptionRecord->ExceptionCode;

                if (code == EXCEPTION_ACCESS_VIOLATION ||
                    code == EXCEPTION_ILLEGAL_INSTRUCTION ||
                    code == EXCEPTION_STACK_OVERFLOW ||
                    code == EXCEPTION_IN_PAGE_ERROR ||
                    code == EXCEPTION_INT_DIVIDE_BY_ZERO)
                {
                    StaticCleanup();
                }
                return EXCEPTION_CONTINUE_SEARCH;
                });

            std::signal(SIGTERM, [](int) { StaticCleanup(); });
            std::signal(SIGABRT, [](int) { StaticCleanup(); });
            std::signal(SIGINT, [](int) { StaticCleanup(); });

            SetConsoleCtrlHandler([](DWORD) -> BOOL {
                StaticCleanup();
                return FALSE;
                }, TRUE);
        }

        void Restore()
        {
            if (!m_running)
            {
                DeleteValue();
                return;
            }

            m_running = false;

            if (m_watchThread.joinable())
                m_watchThread.join();

            DeleteValue();
            RestartNvContainer();
        }
    };

    inline cNvidiaBypass g_NvidiaBypass;
}
