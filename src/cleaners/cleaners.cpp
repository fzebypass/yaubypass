#include "cleaners.h"
#include <string>
#include <vector>
#include <atomic>
#include <cstring>
#include <thread>
#include <shlobj.h>
#include <knownfolders.h>
#include <objbase.h>
#include <tlhelp32.h>
#include <winioctl.h>

#pragma comment(lib, "advapi32.lib")

static void EnablePrivilege(const char* privilege)
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return;

    TOKEN_PRIVILEGES tp{};
    LUID luid{};
    if (LookupPrivilegeValueA(nullptr, privilege, &luid)) {
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    }

    CloseHandle(hToken);
}

static std::wstring GetSelfPath()
{
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    std::wstring path(buf);
    for (auto& c : path) c = towlower(c);
    return path;
}

static std::wstring GetSelfName()
{
    std::wstring path = GetSelfPath();
    size_t slash = path.find_last_of(L"\\/");
    std::wstring name = (slash != std::wstring::npos) ? path.substr(slash + 1) : path;
    size_t dot = name.rfind(L'.');
    if (dot != std::wstring::npos) name = name.substr(0, dot);
    return name;
}

void DisablePrefetchForCurrentProcess()
{
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return;

    typedef NTSTATUS(NTAPI* pNtSetInformationProcess)(HANDLE, PROCESS_INFORMATION_CLASS, PVOID, ULONG);
    pNtSetInformationProcess NtSetInformationProcess =
        (pNtSetInformationProcess)GetProcAddress(hNtdll, "NtSetInformationProcess");

    if (NtSetInformationProcess) {
        BOOL disable = TRUE;
        NtSetInformationProcess(GetCurrentProcess(), (PROCESS_INFORMATION_CLASS)0x3A, &disable, sizeof(BOOL));
    }
}

void CleanPrefetchEntries()
{
    wchar_t winDir[MAX_PATH];
    GetWindowsDirectoryW(winDir, MAX_PATH);
    std::wstring prefetchPath = std::wstring(winDir) + L"\\Prefetch";

    std::wstring selfName = GetSelfName();
    for (auto& c : selfName) c = towupper(c);
    std::wstring selfPattern = selfName + L"*.pf";

    std::vector<std::wstring> patterns = {
        selfPattern,
        L"PHANTOM*.pf", L"CYPHER*.pf", L"CYPHERBYPAS*.pf",
        L"PH_*.pf", L"CHEAT*.pf", L"OPENIV*.pf", L"RUNTIMEBROKER*.pf"
    };

    for (const auto& pat : patterns) {
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW((prefetchPath + L"\\" + pat).c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;
        do {
            std::wstring fullPath = prefetchPath + L"\\" + fd.cFileName;
            SetFileAttributesW(fullPath.c_str(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(fullPath.c_str());
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW((prefetchPath + L"\\*.pf").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring fullPath = prefetchPath + L"\\" + fd.cFileName;
        HANDLE hFile = CreateFileW(fullPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) continue;
        BYTE buffer[8192] = {};
        DWORD bytesRead = 0;
        SetFilePointer(hFile, 0x80, nullptr, FILE_BEGIN);
        ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);
        CloseHandle(hFile);
        std::string data((char*)buffer, bytesRead);
        for (auto& c : data) c = (char)tolower((unsigned char)c);

        std::wstring selfPath = GetSelfPath();
        std::wstring selfNameLower = GetSelfName();
        std::string selfPathA(selfPath.begin(), selfPath.end());
        std::string selfNameA(selfNameLower.begin(), selfNameLower.end());

        if (data.find("phantom") != std::string::npos ||
            data.find("cypher") != std::string::npos ||
            data.find("openiv") != std::string::npos ||
            data.find(selfPathA) != std::string::npos ||
            data.find(selfNameA) != std::string::npos) {
            SetFileAttributesW(fullPath.c_str(), FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(fullPath.c_str());
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

void CleanBAMEntries()
{
    EnablePrivilege("SeBackupPrivilege");
    EnablePrivilege("SeRestorePrivilege");
    EnablePrivilege("SeTakeOwnershipPrivilege");

    HKEY hKey;
    LSTATUS status = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Services\\bam\\State\\UserSettings",
        REG_OPTION_BACKUP_RESTORE, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey);

    if (status != ERROR_SUCCESS)
        status = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Services\\bam\\State\\UserSettings",
            0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey);

    if (status != ERROR_SUCCESS) return;

    wchar_t subKey[256];
    DWORD index = 0, subKeyLen = 256;
    while (RegEnumKeyExW(hKey, index++, subKey, &subKeyLen,
        nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
    {
        HKEY hUserKey;
        LSTATUS s = RegOpenKeyExW(hKey, subKey, REG_OPTION_BACKUP_RESTORE,
            KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hUserKey);
        if (s != ERROR_SUCCESS)
            s = RegOpenKeyExW(hKey, subKey, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hUserKey);

        if (s == ERROR_SUCCESS) {
            wchar_t valueName[512];
            DWORD valIndex = 0, valNameLen = 512;
            std::vector<std::wstring> toDelete;

            while (RegEnumValueW(hUserKey, valIndex++, valueName, &valNameLen,
                nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
            {
                std::wstring lower(valueName);
                for (auto& c : lower) c = towlower(c);
                std::wstring selfPath = GetSelfPath();
                std::wstring selfName = GetSelfName();
                if (lower.find(L"phantom") != std::wstring::npos ||
                    lower.find(L"cypher") != std::wstring::npos ||
                    lower.find(L"ph_") != std::wstring::npos ||
                    lower.find(L"openiv") != std::wstring::npos ||
                    lower.find(selfName) != std::wstring::npos ||
                    lower.find(selfPath) != std::wstring::npos)
                    toDelete.push_back(valueName);
                valNameLen = 512;
            }
            for (const auto& name : toDelete)
                RegDeleteValueW(hUserKey, name.c_str());
            RegCloseKey(hUserKey);
        }
        subKeyLen = 256;
    }
    RegCloseKey(hKey);
}

void CleanTempDlls()
{
    wchar_t tempDir[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tempDir);

    static const wchar_t* patterns[] = {
        L"ph_*.dll", L"ph_*.tmp", L"ph_*.tmp.dll", nullptr
    };
    for (int i = 0; patterns[i]; i++) {
        WIN32_FIND_DATAW fd{};
        std::wstring search = std::wstring(tempDir) + patterns[i];
        HANDLE hf = FindFirstFileW(search.c_str(), &fd);
        if (hf == INVALID_HANDLE_VALUE) continue;
        do {
            DeleteFileW((std::wstring(tempDir) + fd.cFileName).c_str());
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }

    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    std::wstring recipePath = std::wstring(buf) + L".recipe";
    SetFileAttributesW(recipePath.c_str(), FILE_ATTRIBUTE_NORMAL);
    DeleteFileW(recipePath.c_str());
}

void CleanEventLogs()
{
    static const wchar_t* logs[] = {
        L"Application", L"System", L"Security", nullptr
    };
    for (int i = 0; logs[i]; i++) {
        HANDLE hLog = OpenEventLogW(nullptr, logs[i]);
        if (hLog) {
            ClearEventLogW(hLog, nullptr);
            CloseEventLog(hLog);
        }
    }
}

void CleanUsnJournal()
{
    HANDLE hVol = CreateFileW(L"\\\\.\\C:", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (hVol == INVALID_HANDLE_VALUE) return;

    USN_JOURNAL_DATA_V0 jd{};
    DWORD bytes = 0;
    if (DeviceIoControl(hVol, FSCTL_QUERY_USN_JOURNAL,
        nullptr, 0, &jd, sizeof(jd), &bytes, nullptr))
    {
        DELETE_USN_JOURNAL_DATA duj{};
        duj.UsnJournalID = jd.UsnJournalID;
        duj.DeleteFlags = USN_DELETE_FLAG_DELETE | USN_DELETE_FLAG_NOTIFY;
        DeviceIoControl(hVol, FSCTL_DELETE_USN_JOURNAL,
            &duj, sizeof(duj), nullptr, 0, &bytes, nullptr);

        CREATE_USN_JOURNAL_DATA cuj{};
        cuj.MaximumSize = 32 * 1024 * 1024;
        cuj.AllocationDelta = 4 * 1024 * 1024;
        DeviceIoControl(hVol, FSCTL_CREATE_USN_JOURNAL,
            &cuj, sizeof(cuj), nullptr, 0, &bytes, nullptr);
    }
    CloseHandle(hVol);
}

void CleanRecentDocs()
{
    wchar_t recentPath[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_RECENT, nullptr,
        SHGFP_TYPE_CURRENT, recentPath))) return;

    static const wchar_t* patterns[] = {
        L"phantom*", L"cheat*", L"ph_*", L"cypher*", nullptr
    };
    for (int i = 0; patterns[i]; i++) {
        WIN32_FIND_DATAW fd{};
        std::wstring search = std::wstring(recentPath) + L"\\" + patterns[i];
        HANDLE hf = FindFirstFileW(search.c_str(), &fd);
        if (hf == INVALID_HANDLE_VALUE) continue;
        do {
            DeleteFileW((std::wstring(recentPath) + L"\\" + fd.cFileName).c_str());
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }
}

void CleanShimCache()
{
    EnablePrivilege("SeBackupPrivilege");

    HKEY hKey;
    LSTATUS s = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\AppCompatCache",
        0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey);
    if (s != ERROR_SUCCESS) return;

    RegDeleteValueW(hKey, L"AppCompatCache");
    RegCloseKey(hKey);
}

void CleanUserAssist()
{
    HKEY hHKU;
    if (RegOpenKeyExW(HKEY_USERS, nullptr, 0, KEY_ENUMERATE_SUB_KEYS, &hHKU) != ERROR_SUCCESS)
        return;

    static const wchar_t* rot13Patterns[] = {
        L"cuncgbz", L"plcure", L"cu_",
        L"bcravie", L"ehagvzroebyxre", nullptr
    };

    wchar_t sid[256];
    DWORD sidLen = 256, idx = 0;
    while (RegEnumKeyExW(hHKU, idx++, sid, &sidLen,
        nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
    {
        std::wstring countPath = std::wstring(sid) +
            L"\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\UserAssist";

        HKEY hUA;
        if (RegOpenKeyExW(hHKU, countPath.c_str(), 0,
            KEY_ENUMERATE_SUB_KEYS, &hUA) == ERROR_SUCCESS)
        {
            wchar_t guidKey[256];
            DWORD guidLen = 256, guidIdx = 0;
            while (RegEnumKeyExW(hUA, guidIdx++, guidKey, &guidLen,
                nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
            {
                std::wstring countSubPath = std::wstring(guidKey) + L"\\Count";
                HKEY hCount;
                if (RegOpenKeyExW(hUA, countSubPath.c_str(), 0,
                    KEY_ALL_ACCESS, &hCount) == ERROR_SUCCESS)
                {
                    wchar_t valName[1024];
                    DWORD valNameLen = 1024, valIdx = 0;
                    std::vector<std::wstring> toDelete;

                    while (RegEnumValueW(hCount, valIdx++, valName, &valNameLen,
                        nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
                    {
                        std::wstring lower(valName);
                        for (auto& c : lower) c = towlower(c);
                        for (int p = 0; rot13Patterns[p]; p++) {
                            if (lower.find(rot13Patterns[p]) != std::wstring::npos) {
                                toDelete.push_back(valName);
                                break;
                            }
                        }
                        valNameLen = 1024;
                    }
                    for (const auto& name : toDelete)
                        RegDeleteValueW(hCount, name.c_str());
                    RegCloseKey(hCount);
                }
                guidLen = 256;
            }
            RegCloseKey(hUA);
        }
        sidLen = 256;
    }
    RegCloseKey(hHKU);
}

void CleanAmcache()
{
    EnablePrivilege("SeBackupPrivilege");
    EnablePrivilege("SeRestorePrivilege");

    wchar_t winDir[MAX_PATH];
    GetWindowsDirectoryW(winDir, MAX_PATH);
    std::wstring hivePath = std::wstring(winDir) + L"\\appcompat\\Programs\\Amcache.hve";

    static const wchar_t* mountPoint = L"AMCACHE_TMP";
    RegUnLoadKeyW(HKEY_LOCAL_MACHINE, mountPoint);

    LSTATUS s = RegLoadKeyW(HKEY_LOCAL_MACHINE, mountPoint, hivePath.c_str());
    if (s != ERROR_SUCCESS) return;

    static const wchar_t* amcachePaths[] = {
        L"AMCACHE_TMP\\Root\\File",
        L"AMCACHE_TMP\\Root\\InventoryApplicationFile",
        nullptr
    };

    static const wchar_t* keywords[] = {
        L"phantom", L"cypher", L"ph_", L"cheat",
        L"openiv", L"runtimebroker", nullptr
    };

    for (int ap = 0; amcachePaths[ap]; ap++)
    {
        HKEY hRoot;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, amcachePaths[ap],
            0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hRoot) != ERROR_SUCCESS) continue;

        wchar_t containerName[256];
        DWORD containerLen = 256, containerIdx = 0;
        std::vector<std::wstring> containersToVisit;

        while (RegEnumKeyExW(hRoot, containerIdx++, containerName, &containerLen,
            nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
        {
            containersToVisit.push_back(containerName);
            containerLen = 256;
        }

        for (const auto& container : containersToVisit)
        {
            HKEY hContainer;
            if (RegOpenKeyExW(hRoot, container.c_str(), 0,
                KEY_ALL_ACCESS, &hContainer) != ERROR_SUCCESS) continue;

            wchar_t entryName[256];
            DWORD entryLen = 256, entryIdx = 0;
            std::vector<std::wstring> toDelete;

            while (RegEnumKeyExW(hContainer, entryIdx++, entryName, &entryLen,
                nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
            {
                HKEY hEntry;
                if (RegOpenKeyExW(hContainer, entryName, 0,
                    KEY_READ, &hEntry) == ERROR_SUCCESS)
                {
                    wchar_t pathVal[1024]{};
                    DWORD pathValSize = sizeof(pathVal);
                    LSTATUS rv = RegQueryValueExW(hEntry, L"FullPath", nullptr,
                        nullptr, (LPBYTE)pathVal, &pathValSize);
                    if (rv != ERROR_SUCCESS) {
                        pathValSize = sizeof(pathVal);
                        rv = RegQueryValueExW(hEntry, nullptr, nullptr,
                            nullptr, (LPBYTE)pathVal, &pathValSize);
                    }
                    RegCloseKey(hEntry);

                    std::wstring lower(pathVal);
                    for (auto& c : lower) c = towlower(c);
                    for (int k = 0; keywords[k]; k++) {
                        if (lower.find(keywords[k]) != std::wstring::npos) {
                            toDelete.push_back(entryName);
                            break;
                        }
                    }
                }
                entryLen = 256;
            }

            for (const auto& name : toDelete)
                RegDeleteKeyW(hContainer, name.c_str());

            RegCloseKey(hContainer);
        }
        RegCloseKey(hRoot);
    }

    RegFlushKey(HKEY_LOCAL_MACHINE);
    RegUnLoadKeyW(HKEY_LOCAL_MACHINE, mountPoint);
}

static void DeleteFilesInFolder(const std::wstring& folder)
{
    WIN32_FIND_DATAW fd{};
    HANDLE find = FindFirstFileW((folder + L"\\*").c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE)
        return;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        DeleteFileW((folder + L"\\" + fd.cFileName).c_str());
    } while (FindNextFileW(find, &fd));

    FindClose(find);
}

// Jump lists keep a trace of every opened file, independent of the Recent folder.
void CleanJumpLists()
{
    PWSTR appData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData)))
        return;

    const std::wstring recent = std::wstring(appData) + L"\\Microsoft\\Windows\\Recent";
    CoTaskMemFree(appData);

    DeleteFilesInFolder(recent + L"\\CustomDestinations");
    DeleteFilesInFolder(recent + L"\\AutomaticDestinations");
}

static DWORD GetServiceProcessIdByName(const wchar_t* serviceName)
{
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm)
        return 0;

    DWORD pid = 0;
    SC_HANDLE service = OpenServiceW(scm, serviceName, SERVICE_QUERY_STATUS);
    if (service)
    {
        SERVICE_STATUS_PROCESS status{};
        DWORD needed = 0;
        if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status),
            sizeof(status), &needed))
        {
            pid = status.dwProcessId;
        }
        CloseServiceHandle(service);
    }

    CloseServiceHandle(scm);
    return pid;
}

static DWORD FindProcessIdByNameW(const wchar_t* processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    DWORD pid = 0;
    if (Process32FirstW(snapshot, &entry))
    {
        do {
            if (_wcsicmp(entry.szExeFile, processName) == 0)
            {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

// Overwrites every occurrence of the pattern with zero bytes.
static int WipePatternInProcess(HANDLE process, const std::vector<uint8_t>& needle)
{
    if (needle.empty())
        return 0;

    int wiped = 0;
    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);

    uintptr_t address = reinterpret_cast<uintptr_t>(systemInfo.lpMinimumApplicationAddress);
    const uintptr_t maxAddress = reinterpret_cast<uintptr_t>(systemInfo.lpMaximumApplicationAddress);
    MEMORY_BASIC_INFORMATION region{};

    while (address < maxAddress
        && VirtualQueryEx(process, reinterpret_cast<LPCVOID>(address), &region, sizeof(region)))
    {
        const uintptr_t regionBase = reinterpret_cast<uintptr_t>(region.BaseAddress);
        const size_t regionSize = region.RegionSize;
        address = regionBase + regionSize;

        if (region.State != MEM_COMMIT || region.Protect == PAGE_NOACCESS || (region.Protect & PAGE_GUARD))
            continue;
        if (regionSize < needle.size() || regionSize > 256ull * 1024ull * 1024ull)
            continue;

        std::vector<uint8_t> buffer(regionSize);
        SIZE_T read = 0;
        if (!ReadProcessMemory(process, reinterpret_cast<LPCVOID>(regionBase), buffer.data(), regionSize, &read))
            continue;
        if (read < needle.size())
            continue;

        const std::vector<uint8_t> zeros(needle.size(), 0);
        for (size_t offset = 0; offset + needle.size() <= read; ++offset)
        {
            if (std::memcmp(buffer.data() + offset, needle.data(), needle.size()) != 0)
                continue;

            SIZE_T written = 0;
            if (WriteProcessMemory(process, reinterpret_cast<LPVOID>(regionBase + offset),
                zeros.data(), zeros.size(), &written))
            {
                ++wiped;
            }

            offset += needle.size() - 1;
        }
    }

    return wiped;
}

// Traces of the executable remain in the processes that cache file paths
// (jump lists in explorer, DNS/SRUM caches). Their strings are wiped here.
void WipeSelfStrings()
{
    wchar_t selfPath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, selfPath, MAX_PATH) == 0)
        return;

    EnablePrivilege("SeDebugPrivilege");

    const std::wstring widePath(selfPath);
    const size_t slash = widePath.find_last_of(L"\\/");
    const std::wstring wideName = slash == std::wstring::npos ? widePath : widePath.substr(slash + 1);

    std::vector<std::vector<uint8_t>> needles;
    auto add_needle = [&needles](const std::wstring& text) {
        if (text.size() < 8)
            return;
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(text.c_str());
        needles.emplace_back(bytes, bytes + text.size() * sizeof(wchar_t));
        };
    add_needle(widePath);
    add_needle(wideName);

    struct target_t { const wchar_t* name; bool service; };
    static const target_t targets[] = {
        { L"explorer.exe", false },
        { L"Dnscache",     true  },
        { L"Dps",          true  },
        { L"Pcasvc",       true  },
    };

    const DWORD selfPid = GetCurrentProcessId();
    for (const auto& target : targets)
    {
        const DWORD pid = target.service
            ? GetServiceProcessIdByName(target.name)
            : FindProcessIdByNameW(target.name);

        if (pid == 0 || pid == selfPid)
            continue;

        HANDLE process = OpenProcess(
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!process)
            continue;

        for (const auto& needle : needles)
            WipePatternInProcess(process, needle);

        CloseHandle(process);
    }
}

// Drops the executable from disk once this process is gone: the name is moved
// into an alternate data stream and the file is marked for deletion on close.
static void ScheduleSelfDelete()
{
    wchar_t selfPath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, selfPath, MAX_PATH) == 0)
        return;

    HANDLE file = CreateFileW(selfPath, DELETE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file == INVALID_HANDLE_VALUE)
    {
        MoveFileExW(selfPath, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        return;
    }

    static const wchar_t kStream[] = L":wizard";
    const DWORD streamBytes = static_cast<DWORD>(wcslen(kStream) * sizeof(wchar_t));
    std::vector<uint8_t> renameBuffer(sizeof(FILE_RENAME_INFO) + streamBytes + sizeof(wchar_t), 0);
    auto* renameInfo = reinterpret_cast<FILE_RENAME_INFO*>(renameBuffer.data());
    renameInfo->ReplaceIfExists = FALSE;
    renameInfo->RootDirectory = nullptr;
    renameInfo->FileNameLength = streamBytes;
    std::memcpy(renameInfo->FileName, kStream, streamBytes);

    SetFileInformationByHandle(file, FileRenameInfo, renameInfo,
        static_cast<DWORD>(renameBuffer.size()));

    FILE_DISPOSITION_INFO disposition{};
    disposition.DeleteFile = TRUE;
    SetFileInformationByHandle(file, FileDispositionInfo, &disposition, sizeof(disposition));

    CloseHandle(file);
}

static std::wstring EscapePowerShellSingleQuotedPath(const std::wstring& value)
{
    std::wstring escaped;
    escaped.reserve(value.size() + 16);
    for (wchar_t c : value) {
        if (c == L'\'')
            escaped += L"''";
        else
            escaped += c;
    }
    return escaped;
}

// ---------- Launcher-Spuren ----------
// Der Cheat liegt nach install.cmd in %LOCALAPPDATA%\YauBypass und wird ueber die
// Aufgabe NVOverlayHost bei jeder Anmeldung gestartet. Destruct raeumt beides weg,
// damit nach dem Neustart nichts nachgeladen wird.

static void DeleteFileIfExists(const std::wstring& path)
{
    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    DeleteFileW(path.c_str());
}

static void DeleteTempPattern(const std::wstring& tempDir, const wchar_t* pattern)
{
    WIN32_FIND_DATAW entry = {};
    HANDLE hFind = FindFirstFileW((tempDir + pattern).c_str(), &entry);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do {
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        DeleteFileIfExists(tempDir + entry.cFileName);
    } while (FindNextFileW(hFind, &entry));
    FindClose(hFind);
}

static std::wstring ReadRegString(HKEY root, const std::wstring& key, const wchar_t* value)
{
    std::wstring result;
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, key.c_str(), 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return result;

    wchar_t buffer[128] = {};
    DWORD size = sizeof(buffer);
    if (RegQueryValueExW(hKey, value, nullptr, nullptr, reinterpret_cast<BYTE*>(buffer), &size) == ERROR_SUCCESS)
        result = buffer;
    RegCloseKey(hKey);
    return result;
}

static void RemoveLauncherTask(const wchar_t* taskName)
{
    wchar_t sysDir[MAX_PATH] = {};
    GetSystemDirectoryW(sysDir, MAX_PATH);

    std::wstring arguments = std::wstring(L"/delete /tn \"") + taskName + L"\" /f";
    std::vector<wchar_t> argumentsBuffer(arguments.begin(), arguments.end());
    argumentsBuffer.push_back(L'\0');

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    if (CreateProcessW((std::wstring(sysDir) + L"\\schtasks.exe").c_str(), argumentsBuffer.data(),
        nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    const std::wstring taskKey =
        std::wstring(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Schedule\\TaskCache\\Tree\\") + taskName;

    // Wird schtasks geblockt, bleiben die XML-Datei und der GUID-Eintrag in
    // TaskCache\Tasks als Waisen zurueck - deshalb direkt mit entfernen.
    const std::wstring taskId = ReadRegString(HKEY_LOCAL_MACHINE, taskKey, L"Id");
    if (!taskId.empty()) {
        RegDeleteTreeW(HKEY_LOCAL_MACHINE,
            (std::wstring(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Schedule\\TaskCache\\Tasks\\") + taskId).c_str());
    }
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, taskKey.c_str());

    wchar_t winDir[MAX_PATH] = {};
    GetWindowsDirectoryW(winDir, MAX_PATH);
    DeleteFileIfExists(std::wstring(winDir) + L"\\System32\\Tasks\\" + taskName);
}

void CleanLauncherTraces()
{
    for (const wchar_t* taskName : { L"NVOverlayHost", L"NvOverlayHost2", L"NvOverlayTask" })
        RemoveLauncherTask(taskName);

    HKEY hRun = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hRun) == ERROR_SUCCESS)
    {
        RegDeleteValueW(hRun, L"NVOverlayHost");
        RegCloseKey(hRun);
    }

    wchar_t localAppData[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH) && localAppData[0]) {
        const std::wstring installDir = std::wstring(localAppData) + L"\\YauBypass";
        DeleteFileIfExists(installDir + L"\\boot.cmd");
        DeleteFileIfExists(installDir + L"\\yaubypass.dat");
        RemoveDirectoryW(installDir.c_str());
    }

    wchar_t tempPath[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempPath);
    const std::wstring tempDir(tempPath);
    DeleteFileIfExists(tempDir + L"y.cmd");
    DeleteFileIfExists(tempDir + L"y.b64");
    DeleteFileIfExists(tempDir + L"yau.cmd");
    DeleteFileIfExists(tempDir + L"c.b64");
    DeleteFileIfExists(tempDir + L"c.cfg");
    RemoveDirectoryW((tempDir + L"wz").c_str());

    // Alles, was der Launcher selbst im Temp anlegt. Die laufende EXE laesst sich
    // hier nicht loeschen - das erledigt ScheduleSelfDelete.
    DeleteTempPattern(tempDir, L"nv_*.exe");
    DeleteTempPattern(tempDir, L"nv_*.tmp");
    DeleteTempPattern(tempDir, L"nv_*.b64");
    DeleteTempPattern(tempDir, L"yb_*.ok");
    DeleteTempPattern(tempDir, L"yb_elev_*.vbs");

    // NVIDIA Streamproof zuruecksetzen: nur der Wert, den der Cheat setzt.
    const std::wstring ftsKey = L"SOFTWARE\\NVIDIA Corporation\\Global\\NvApp\\ShadowPlay\\FTS";
    HKEY hFts = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, ftsKey.c_str(), 0, KEY_SET_VALUE, &hFts) == ERROR_SUCCESS) {
        RegDeleteValueW(hFts, L"{497B8458-4244-4EE6-BFEA-F3D2BA294F21}");
        RegCloseKey(hFts);
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, ftsKey.c_str());  // greift nur, wenn der Zweig jetzt leer ist
    }
}

static std::atomic<bool> g_destructing{ false };

bool IsDestructing()
{
    return g_destructing.load();
}

// Runs the cleanup on a worker thread so the menu keeps rendering instead of
// freezing while registry hives, USN journal and prefetch are processed.
void RunCleanerAndExitAsync()
{
    static std::atomic<bool> started{ false };
    if (started.exchange(true))
        return;

    g_destructing.store(true);

    // Safety net: should a cleanup step ever block, the process still disappears
    // instead of staying around invisibly without a window.
    std::thread([] {
        Sleep(15000);
        ExitProcess(0);
        }).detach();

    std::thread([] { RunCleanerAndExit(); }).detach();
}

void RunCleanerAndExit()
{
    DWORD selfPid = GetCurrentProcessId();
    wchar_t selfExe[MAX_PATH] = {};
    GetModuleFileNameW(NULL, selfExe, MAX_PATH);
    std::wstring exePath(selfExe);
    std::wstring exeNameUpper = exePath.substr(exePath.find_last_of(L"\\/") + 1);
    for (auto& c : exeNameUpper) c = towupper(c);

    const size_t dot = exeNameUpper.rfind(L'.');
    if (dot != std::wstring::npos)
        exeNameUpper = exeNameUpper.substr(0, dot);

    const std::wstring exeDir = exePath.substr(0, exePath.find_last_of(L"\\/"));
    const std::wstring exeDirEscaped = EscapePowerShellSingleQuotedPath(exeDir);
    const wchar_t driveLetter = exePath.empty() ? L'C' : towupper(exePath[0]);

    // The steps are independent, so they run in parallel: destruct now takes about
    // as long as the slowest group instead of the sum of every single step.
    std::thread file_cleanup([] {
        CleanTempDlls();
        CleanRecentDocs();
        CleanJumpLists();
        CleanEventLogs();
    });

    std::thread registry_cleanup([] {
        CleanBAMEntries();
        CleanShimCache();
        CleanUserAssist();
        CleanAmcache();
    });

    CleanUsnJournal();
    WipeSelfStrings();
    CleanLauncherTraces();

    file_cleanup.join();
    registry_cleanup.join();

    wchar_t tempDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempDir);
    const std::wstring psScript = std::wstring(tempDir) + L"cl.ps1";

    const std::wstring pfNames =
        L"'" + exeNameUpper + L"','SIHOST','RUNTIMEBROKER','OPENIV'";

    const std::wstring script =
        L"$pid2 = " + std::to_wstring(selfPid) + L"\n"
        L"try { $p = Get-Process -Id $pid2 -EA Stop; $p.WaitForExit(15000) } catch {}\n"
        L"Start-Sleep -Milliseconds 800\n"
        L"$pf = 'C:\\Windows\\Prefetch'\n"
        L"takeown /f $pf /r /d y 2>&1 | Out-Null\n"
        L"icacls $pf /grant Everyone:F /t /c /q 2>&1 | Out-Null\n"
        + pfNames + L" | ForEach-Object { Get-ChildItem $pf -Filter \"${_}*.pf\" -EA SilentlyContinue | Remove-Item -Force -EA SilentlyContinue }\n"
        L"Get-ChildItem -Path '" + exeDirEscaped + L"' -Filter '*.recipe' -Recurse -EA SilentlyContinue | Remove-Item -Force -EA SilentlyContinue\n"
        L"$drv = '" + std::wstring(1, driveLetter) + L":'\n"
        L"fsutil usn deletejournal /D $drv 2>&1 | Out-Null\n"
        L"fsutil usn createjournal m=0x2000000 a=0x400000 $drv 2>&1 | Out-Null\n"
        L"Start-Sleep -Seconds 2\n"
        + pfNames + L" | ForEach-Object { Get-ChildItem $pf -Filter \"${_}*.pf\" -EA SilentlyContinue | Remove-Item -Force -EA SilentlyContinue }\n"
        L"fsutil usn deletejournal /D $drv 2>&1 | Out-Null\n"
        L"fsutil usn createjournal m=0x2000000 a=0x400000 $drv 2>&1 | Out-Null\n"
        L"Remove-Item -LiteralPath $PSCommandPath -Force -EA SilentlyContinue\n";

    HANDLE hFile = CreateFileW(psScript.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        WORD bom = 0xFEFF;
        DWORD written = 0;
        WriteFile(hFile, &bom, 2, &written, nullptr);
        WriteFile(hFile, script.c_str(), static_cast<DWORD>(script.size() * sizeof(wchar_t)), &written, nullptr);
        CloseHandle(hFile);
    }

    std::wstring args = L"-NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass -File \"" + psScript + L"\"";
    std::vector<wchar_t> argsBuffer(args.begin(), args.end());
    argsBuffer.push_back(L'\0');

    STARTUPINFOW si2 = { sizeof(si2) };
    si2.dwFlags = STARTF_USESHOWWINDOW;
    si2.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi2 = {};
    CreateProcessW(L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
        argsBuffer.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si2, &pi2);
    if (pi2.hProcess) CloseHandle(pi2.hProcess);
    if (pi2.hThread) CloseHandle(pi2.hThread);

    ScheduleSelfDelete();
    ExitProcess(0);
}
