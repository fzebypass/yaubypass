#include "anti_vm.hpp"
#include <windows.h>
#include <vector>
#include <intrin.h>

using u64 = unsigned long long;
using u32 = unsigned int;
using u8 = unsigned char;

#define FLG_BIOS 0x1
#define FLG_CPU  0x2
#define FLG_TIME 0x4

unsigned int check_anti_vm() {
    u32 res = 0;

    // 0x52534D42 is 'RSMB'
    DWORD sz = GetSystemFirmwareTable(0x52534D42, 0, 0, 0);
    if (sz) {
        std::vector<u8> buf(sz);
        GetSystemFirmwareTable(0x52534D42, 0, buf.data(), sz);
        u8* p = buf.data() + 8;
        u8* end = buf.data() + sz;

        while (p < end) {
            if (*p == 127) break;
            if (*p == 0 && *(p + 1) > 0x12) {
                u64 chars = *(u64*)(p + 8);
                if ((chars >> 3) & 1) res |= FLG_BIOS;

                // count set bits - real bios usually have more then 10
                int n = 0;
                for (int i = 0; i < 64; ++i) n += ((chars >> i) & 1);
                if (n < 10) res |= FLG_BIOS;
                break;
            }
            p += *(p + 1);
            while (p < end - 1 && (*p || *(p + 1))) p++;
            p += 2;
        }
    }

    // timing n cpuid
    int cpu[4];

    // rdtscp check
    __cpuid(cpu, 0x80000001);
    if (!((cpu[3] >> 27) & 1)) res |= FLG_CPU;

    u64 t1 = __rdtsc();
    __cpuid(cpu, 0);
    u64 t2 = __rdtsc();
    // VMs often have very high CPUID timing (10000+); physical PCs usually < 5000 even with power-saving
    if ((t2 - t1) > 8000) res |= FLG_TIME;

    return res;
}
