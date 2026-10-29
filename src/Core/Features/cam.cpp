#include "cam.hpp"
#include "../SDK/Memory.hpp"
#include "../Offsets.hpp"
#include <Windows.h>
#include <cmath>

namespace Core::Features {

    // Better shellcode (preserves XMM0-XMM5 as well)
    static const uint8_t ShellcodeTemplate[] =
    {
        0x9C,                                           // pushfq
        0x50, 0x51, 0x52, 0x53, 0x55, 0x56, 0x57,       // push rax, rcx, rdx, rbx, rbp, rsi, rdi
        0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53, // push r8-r11
        0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, // push r12-r15

        // Save XMM0 - XMM5 (critical for camera functions returning vectors in XMM)
        0x48, 0x83, 0xEC, 0x38,                         // sub rsp, 0x38
        0x0F, 0x29, 0x04, 0x24,                         // movaps [rsp], xmm0
        0x0F, 0x29, 0x4C, 0x24, 0x10,                   // movaps [rsp+0x10], xmm1
        0x0F, 0x29, 0x54, 0x24, 0x20,                   // movaps [rsp+0x20], xmm2

        // === Your custom camera logic here ===
        0x48, 0xB8, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // mov rax, &g_CameraData
        0xF3, 0x0F, 0x10, 0x00,                         // movss xmm0, [rax]     ← you can expand this

        // Restore XMM0-XMM2
        0x0F, 0x28, 0x04, 0x24,
        0x0F, 0x28, 0x4C, 0x24, 0x10,
        0x0F, 0x28, 0x54, 0x24, 0x20,
        0x48, 0x83, 0xC4, 0x38,                         // add rsp, 0x38

        // Pop registers
        0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C,
        0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58,
        0x5F, 0x5E, 0x5D, 0x5B, 0x5A, 0x59, 0x58,       // pop rdi..rax (reverse order)
        0x9D,                                           // popfq

        // Stolen original bytes (will be filled at runtime)
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,

        // jmp back to original function
        0xE9, 0x00, 0x00, 0x00, 0x00
    };

    void cCameraBypass::HookCamera(const D3DXVECTOR3& targetPos, const D3DXVECTOR3& playerPos)
    {
        std::lock_guard<std::mutex> lock(mtx);

        if (targetPos.x != 0.0f || targetPos.y != 0.0f || targetPos.z != 0.0f)
        {
            D3DXVECTOR3 dir = targetPos - playerPos;
            float len = D3DXVec3Length(&dir);
            if (len > 0.001f)
            {
                dir /= len;
                g_CameraData.pitch = asinf(-dir.z);
                g_CameraData.yaw = atan2f(dir.y, dir.x);
                g_CameraData.roll = 0.0f;
            }
        }

        g_CameraData.x = playerPos.x;
        g_CameraData.y = playerPos.y;
        g_CameraData.z = playerPos.z;

        if (m_Hooked)
            return;

        uintptr_t addr = g_Offsets.m_GetFinalRenderedCamRot;
        if (!addr) return;

        m_HookAddr = addr;

        constexpr size_t instSize = 6;
        constexpr size_t kJumpSize = 5;

        m_OriginalBytes = Mem.ReadBytes(addr, instSize);
        if (m_OriginalBytes.size() != instSize) return;

        m_CaveAddr = Mem.CreateCodeCaveNearby(addr, 0x400);
        if (!m_CaveAddr) return;

        std::vector<uint8_t> shellcode(ShellcodeTemplate, ShellcodeTemplate + sizeof(ShellcodeTemplate));

        *reinterpret_cast<uintptr_t*>(&shellcode[26]) =
            reinterpret_cast<uintptr_t>(&g_CameraData);

        size_t originalOffset = shellcode.size() - (instSize + kJumpSize);
        size_t jumpOpcodeOffset = shellcode.size() - kJumpSize;
        size_t jumpDispOffset = shellcode.size() - 4;

        std::memcpy(&shellcode[originalOffset], m_OriginalBytes.data(), instSize);

        uintptr_t returnAddr = addr + instSize;
        uintptr_t jmpInstr = m_CaveAddr + jumpOpcodeOffset;
        uintptr_t jmpNext = jmpInstr + kJumpSize;

        shellcode[jumpOpcodeOffset] = 0xE9;
        *reinterpret_cast<int32_t*>(&shellcode[jumpDispOffset]) =
            static_cast<int32_t>(returnAddr - jmpNext);

        DWORD oldProt = 0;
        VirtualProtect((LPVOID)m_CaveAddr, shellcode.size(), PAGE_EXECUTE_READWRITE, &oldProt);
        Mem.WriteBytes(m_CaveAddr, shellcode);
        VirtualProtect((LPVOID)m_CaveAddr, shellcode.size(), oldProt, &oldProt);

        VirtualProtect((LPVOID)addr, instSize, PAGE_EXECUTE_READWRITE, &oldProt);
        Mem.HookJMP(addr, m_CaveAddr);

        for (size_t i = 5; i < instSize; ++i)
            Mem.Write<uint8_t>(addr + i, 0x90);

        VirtualProtect((LPVOID)addr, instSize, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), (LPCVOID)addr, instSize);

        m_Hooked = true;
    }

    void cCameraBypass::UnhookCamera()
    {
        std::lock_guard<std::mutex> lock(mtx);

        if (!m_Hooked || m_HookAddr == 0 || m_OriginalBytes.empty())
            return;

        // Maak de originele functie writable en zet bytes terug
        DWORD oldProtect = 0;
        if (VirtualProtect((LPVOID)m_HookAddr, m_OriginalBytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            Mem.WriteBytes(m_HookAddr, m_OriginalBytes);

            VirtualProtect((LPVOID)m_HookAddr, m_OriginalBytes.size(), oldProtect, &oldProtect);

            // Belangrijk: flush de CPU cache
            FlushInstructionCache(GetCurrentProcess(), (LPCVOID)m_HookAddr, m_OriginalBytes.size());
        }

        // Reset variabelen
        m_Hooked = false;
        m_HookAddr = 0;
        m_CaveAddr = 0;
        m_OriginalBytes.clear();

        // Camera data resetten → echte game waarden komen terug
        g_CameraData = Camera{};
    }

    void cCameraBypass::ToggleCamera(bool enable, const D3DXVECTOR3& targetPos, const D3DXVECTOR3& playerPos)
    {
        if (enable)
            HookCamera(targetPos, playerPos);
        else
            UnhookCamera();
    }

} // namespace Core::Features