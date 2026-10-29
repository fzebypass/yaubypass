#include <iostream>
#include <string>
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <algorithm>
#include <Psapi.h>
#include <locale>
#include <map>
#include <codecvt>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")
#pragma warning(disable: 4996)


    std::wstring zkr() {
        wchar_t qpg[MAX_PATH] = { 0 };
        GetModuleFileNameW(nullptr, qpg, MAX_PATH);
        PathStripPathW(qpg);
        return std::wstring(qpg);
    }

    std::wstring vxn() {
        wchar_t ljy[MAX_PATH] = { 0 };
        GetModuleFileNameW(nullptr, ljy, MAX_PATH);
        return std::wstring(ljy);
    }

    std::vector<LPVOID> tmf(DWORD wqp, const std::string& rdk) {
        std::vector<LPVOID> bgy;
        HANDLE nxc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, wqp);
        if (nxc == NULL) return bgy;

        SYSTEM_INFO kzl;
        GetSystemInfo(&kzl);
        LPVOID ufv = kzl.lpMinimumApplicationAddress;
        LPVOID pdz = kzl.lpMaximumApplicationAddress;
        std::string qjx = rdk;
        std::transform(qjx.begin(), qjx.end(), qjx.begin(), ::tolower);

        MEMORY_BASIC_INFORMATION iyo;
        while (ufv < pdz && VirtualQueryEx(nxc, ufv, &iyo, sizeof(iyo))) {
            if (iyo.State == MEM_COMMIT && (iyo.Protect & PAGE_READWRITE) && !(iyo.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
                std::vector<char> wkt(iyo.RegionSize);
                SIZE_T xbq;
                if (ReadProcessMemory(nxc, iyo.BaseAddress, wkt.data(), iyo.RegionSize, &xbq)) {
                    std::string msw(wkt.begin(), wkt.begin() + xbq);
                    std::transform(msw.begin(), msw.end(), msw.begin(), ::tolower);
                    size_t vhu = 0;
                    while ((vhu = msw.find(qjx, vhu)) != std::string::npos) {
                        LPVOID qsl = static_cast<char*>(iyo.BaseAddress) + vhu;
                        bgy.push_back(qsl);
                        vhu += qjx.length();
                    }
                }
            }
            ufv = static_cast<char*>(iyo.BaseAddress) + iyo.RegionSize;
        }

        CloseHandle(nxc);
        return bgy;
    }

    std::vector<LPVOID> oey(DWORD ybt, const std::wstring& fcq) {
        std::vector<LPVOID> zku;
        HANDLE gvm = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, ybt);
        if (gvm == NULL) return zku;

        SYSTEM_INFO rwx;
        GetSystemInfo(&rwx);
        LPVOID jmn = rwx.lpMinimumApplicationAddress;
        LPVOID pei = rwx.lpMaximumApplicationAddress;
        std::wstring khl = fcq;
        std::transform(khl.begin(), khl.end(), khl.begin(), ::towlower);

        MEMORY_BASIC_INFORMATION tjn;
        while (jmn < pei && VirtualQueryEx(gvm, jmn, &tjn, sizeof(tjn))) {
            if (tjn.State == MEM_COMMIT && (tjn.Protect & PAGE_READWRITE) && !(tjn.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
                std::vector<wchar_t> uiq(tjn.RegionSize / sizeof(wchar_t));
                SIZE_T dza;
                if (ReadProcessMemory(gvm, tjn.BaseAddress, uiq.data(), tjn.RegionSize, &dza)) {
                    std::wstring yko(uiq.begin(), uiq.begin() + (dza / sizeof(wchar_t)));
                    std::transform(yko.begin(), yko.end(), yko.begin(), ::towlower);
                    size_t lwp = 0;
                    while ((lwp = yko.find(khl, lwp)) != std::wstring::npos) {
                        LPVOID bvt = static_cast<char*>(tjn.BaseAddress) + (lwp * sizeof(wchar_t));
                        zku.push_back(bvt);
                        lwp += khl.length();
                    }
                }
            }
            jmn = static_cast<char*>(tjn.BaseAddress) + tjn.RegionSize;
        }

        CloseHandle(gvm);
        return zku;
    }

    std::wstring ujf(const std::string& lki) {
        if (lki.empty()) return L"";
        int wzo = MultiByteToWideChar(CP_UTF8, 0, &lki[0], (int)lki.size(), nullptr, 0);
        std::wstring pxs(wzo, 0);
        MultiByteToWideChar(CP_UTF8, 0, &lki[0], (int)lki.size(), &pxs[0], wzo);
        return pxs;
    }

    std::string qit(const std::wstring& vaz) {
        if (vaz.empty()) return "";
        int jhe = WideCharToMultiByte(CP_ACP, 0, vaz.c_str(), (int)vaz.size(), nullptr, 0, nullptr, nullptr);
        std::string mdr(jhe, 0);
        WideCharToMultiByte(CP_ACP, 0, vaz.c_str(), (int)vaz.size(), &mdr[0], jhe, nullptr, nullptr);
        return mdr;
    }

    bool oac(DWORD xfm, const std::string& zqy, HANDLE jtr) {
        bool yvx = false;
        std::vector<LPVOID> ngl = tmf(xfm, zqy);
        for (LPVOID kcj : ngl) {
            std::vector<char> xfn(zqy.size(), 0);
            if (WriteProcessMemory(jtr, kcj, xfn.data(), xfn.size(), nullptr)) {
                yvx = true;
            }
        }

        std::wstring rnq = ujf(zqy);
        std::vector<LPVOID> phw = oey(xfm, rnq);
        for (LPVOID gqs : phw) {
            std::vector<wchar_t> jpl(rnq.size(), 0);
            if (WriteProcessMemory(jtr, gqs, jpl.data(), jpl.size() * sizeof(wchar_t), nullptr)) {
                yvx = true;
            }
        }

        return yvx;
    }

    bool vpo(DWORD irc, const std::wstring& wjf, HANDLE pbg) {
        bool qvx = false;
        std::vector<LPVOID> cga = oey(irc, wjf);
        for (LPVOID nlx : cga) {
            std::vector<wchar_t> mst(wjf.size(), 0);
            if (WriteProcessMemory(pbg, nlx, mst.data(), mst.size() * sizeof(wchar_t), nullptr)) {
                qvx = true;
            }
        }
        return qvx;
    }

    BOOL kzh() {
        HANDLE wqd;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &wqd))
            return FALSE;

        TOKEN_PRIVILEGES hqo;
        hqo.PrivilegeCount = 1;
        hqo.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &hqo.Privileges[0].Luid)) {
            CloseHandle(wqd);
            return FALSE;
        }

        BOOL ydt = AdjustTokenPrivileges(wqd, FALSE, &hqo, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr);
        CloseHandle(wqd);
        return ydt && (GetLastError() != ERROR_NOT_ALL_ASSIGNED);
    }

    DWORD sgj(const std::wstring& qpg) {
        SC_HANDLE uxw = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!uxw) return 0;

        SC_HANDLE bai = OpenServiceW(uxw, qpg.c_str(), SERVICE_QUERY_STATUS);
        if (!bai) {
            CloseServiceHandle(uxw);
            return 0;
        }

        DWORD rqy = 0;
        SERVICE_STATUS_PROCESS fkm;
        DWORD vat;
        if (QueryServiceStatusEx(bai, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&fkm), sizeof(fkm), &vat)) {
            rqy = fkm.dwProcessId;
        }

        CloseServiceHandle(bai);
        CloseServiceHandle(uxw);
        return rqy;
    }

    DWORD xqk(const std::wstring& kjo) {
        HANDLE lmg = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (lmg == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32W hxj;
        hxj.dwSize = sizeof(PROCESSENTRY32W);

        DWORD rfa = 0;
        if (Process32FirstW(lmg, &hxj)) {
            do {
                if (_wcsicmp(hxj.szExeFile, kjo.c_str()) == 0) {
                    rfa = hxj.th32ProcessID;
                    break;
                }
            } while (Process32NextW(lmg, &hxj));
        }

        CloseHandle(lmg);
        return rfa;
    }

    void yzg(DWORD wln, const std::vector<std::string>& bkg, const std::vector<std::wstring>& kqo) {
        HANDLE fud = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, wln);
        if (!fud) return;

        for (const auto& str : bkg) {
            oac(wln, str, fud);
        }

        for (const auto& wstr : kqo) {
            vpo(wln, wstr, fud);
        }

        CloseHandle(fud);
    }

    int strings() {
        if (!kzh()) {
            return 1;
        }

        std::wstring vpc = zkr();
        std::wstring tzq = vxn();

        std::vector<std::wstring> uiw = { L"DiagTrack", L"AppInfo", L"dps", L"Lsass", L"csrss", L"SearchIndexer", L"sihost", L"smartscreen", };
        std::vector<std::wstring> kdf = { L"explorer.exe", L"svchost.exe", L"Lsass.exe", L"csrss.exe", L"SearchIndexer.exe", L"sihost.exe", L"smartscreen.exe", };

        std::vector<std::string> pcu = {
            qit(vpc),
            qit(tzq),
            "novaproject.lol",
            "novaproject.lol/api"
        };

        std::vector<std::wstring> tzh = {
            vpc,
            tzq,
            L"novaproject.lol",
            L"novaproject.lol/api"
        };

        for (const auto& mwk : uiw) {
            DWORD qrf = sgj(mwk);
            if (qrf) {
                yzg(qrf, pcu, tzh);
            }
        }

        for (const auto& fhw : kdf) {
            DWORD qfc = xqk(fhw);
            if (qfc) {
                yzg(qfc, pcu, tzh);
            }
        }

        return 0;
    }