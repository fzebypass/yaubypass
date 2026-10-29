#include "SilentAim.hpp"
#include "TargetHighlight.hpp"
#include <Core/Lifecycle.hpp>
#include <cmath>
#include <vector>
#include <thread>
#include <array>
#include <cfloat>
#include <random>
#include <mutex>
#include <iostream>
#include <Includes/CustomWidgets/Notify.hpp>
#include "cam.hpp"

#define SKEL_Pelvis 0x2e28
#define SKEL_L_Thigh 0xe39f
#define SKEL_L_Calf 0xf9bb
#define SKEL_L_Foot 0x3779
#define SKEL_R_Thigh 0xca72
#define SKEL_R_Calf 0x9000
#define SKEL_R_Foot 0xcc4d
#define SKEL_Spine1 0x60f0
#define SKEL_Spine2 0x60f1
#define SKEL_Spine3 0x60f2
#define SKEL_L_UpperArm 0xb1c5
#define SKEL_L_Forearm 0xeeeb
#define SKEL_L_Hand 0x49d9
#define SKEL_R_UpperArm 0x9d4d
#define SKEL_R_Forearm 0x6e5c
#define SKEL_R_Hand 0xdead
#define SKEL_Neck_1 0x9995
#define SKEL_Head 0x796e

bool Core::Features::cSilentAim::HasVisualTarget = false;
D3DXVECTOR2 Core::Features::cSilentAim::VisualTargetScreenPos = { 0.0f, 0.0f };
std::mutex Core::Features::cSilentAim::VisualTargetMutex;
bool Core::Features::cSilentAim::HookedCamera = false;

inline static uintptr_t g_IsPlayerFreeAimingAtEntityFuncAddr = 0;
inline static uintptr_t g_IsPlayerFreeAimingAtEntityShellcodeAddr = 0;
inline static std::vector<uint8_t> g_OriginalIsPlayerFreeAimingAtEntityBytes;
inline static bool g_IsPlayerFreeAimingAtEntityHooked = false;

static void CreateIsPlayerFreeAimingAtEntityHook()
{
    if (g_IsPlayerFreeAimingAtEntityShellcodeAddr)
        return;

    std::vector<uint8_t> shellcode = {
        0xB0, 0x01,
        0xC3
    };

    uintptr_t shellcodeAddr = Mem.CreateCodeCave(shellcode.size());
    if (!shellcodeAddr)
        return;

    if (Mem.WriteBytes(shellcodeAddr, shellcode))
    {
        g_IsPlayerFreeAimingAtEntityShellcodeAddr = shellcodeAddr;
    }
}

static bool InitializeIsPlayerFreeAimingAtEntityHook() {
    if (g_IsPlayerFreeAimingAtEntityHooked) return true;

    g_IsPlayerFreeAimingAtEntityFuncAddr = Mem.FindSignatureStr(xorstr(
        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 8D 64 24 ? 48 89 3C 24 48 83 EC ? 48 8D"
    ));

    if (!g_IsPlayerFreeAimingAtEntityFuncAddr) {
        g_IsPlayerFreeAimingAtEntityFuncAddr = Mem.FindSignatureStr(xorstr("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 E9 ? ? ? ? E8 ? ? ? ? E9 ? ? ? ? 90 E8"));
        if (!g_IsPlayerFreeAimingAtEntityFuncAddr) {
            g_IsPlayerFreeAimingAtEntityFuncAddr = Mem.FindSignatureStr(xorstr("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 8D 64 24 F8 48 89 3C 24 48 83 EC 20 48 8D 64 24"));
            if (!g_IsPlayerFreeAimingAtEntityFuncAddr) {
                g_IsPlayerFreeAimingAtEntityFuncAddr = Mem.FindSignatureStr(xorstr("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 8B EA BF"));
                if (!g_IsPlayerFreeAimingAtEntityFuncAddr) {
                    g_IsPlayerFreeAimingAtEntityFuncAddr = Mem.FindSignatureStr(xorstr("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 8B EA BF"));
                    if (!g_IsPlayerFreeAimingAtEntityFuncAddr) {
                        g_IsPlayerFreeAimingAtEntityFuncAddr = Mem.FindSignatureStr(xorstr("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 8D 64 24 F8 48 89 3C 24 48 83 EC 20 48 8D"));
                        if (!g_IsPlayerFreeAimingAtEntityFuncAddr) {
                            return false;
                        }
                    }
                }
            }
        }
    }

    g_OriginalIsPlayerFreeAimingAtEntityBytes = Mem.ReadBytes(g_IsPlayerFreeAimingAtEntityFuncAddr, 32);

    CreateIsPlayerFreeAimingAtEntityHook();
    if (!g_IsPlayerFreeAimingAtEntityShellcodeAddr) {
        return false;
    }

    if (!Mem.HookJMP(g_IsPlayerFreeAimingAtEntityFuncAddr, g_IsPlayerFreeAimingAtEntityShellcodeAddr)) {
        return false;
    }

    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)g_IsPlayerFreeAimingAtEntityFuncAddr, 16);
    g_IsPlayerFreeAimingAtEntityHooked = true;

    return true;
}

static void UnhookIsPlayerFreeAimingAtEntity() {
    if (!g_IsPlayerFreeAimingAtEntityHooked || !g_IsPlayerFreeAimingAtEntityFuncAddr) return;

    if (Mem.WriteBytes(g_IsPlayerFreeAimingAtEntityFuncAddr, g_OriginalIsPlayerFreeAimingAtEntityBytes)) {
        FlushInstructionCache(GetCurrentProcess(), (LPCVOID)g_IsPlayerFreeAimingAtEntityFuncAddr, g_OriginalIsPlayerFreeAimingAtEntityBytes.size());
        g_IsPlayerFreeAimingAtEntityHooked = false;
    }
}
void Core::Features::cSilentAim::CreateGetFinalRenderedCamRotHook()
{
    if (g_GetFinalRenderedCamRotShellcodeAddr) return;

    uintptr_t shellcodeAddr = Mem.CreateCodeCave(512);
    if (!shellcodeAddr) return;

    uintptr_t coordsAddr = shellcodeAddr + 256;
    uintptr_t counterAddr = shellcodeAddr + 500;

    std::vector<uint8_t> shellcode = {
        0xFF, 0x05, 0x00, 0x00, 0x00, 0x00,
        0x48, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00,
        0x0F, 0x10, 0x00,
        0xC3
    };

    int32_t counterOffset = static_cast<int32_t>(counterAddr - (shellcodeAddr + 6));
    memcpy(shellcode.data() + 2, &counterOffset, sizeof(counterOffset));

    int32_t coordsOffset = static_cast<int32_t>(coordsAddr - (shellcodeAddr + 13));
    memcpy(shellcode.data() + 9, &coordsOffset, sizeof(coordsOffset));

    if (Mem.WriteBytes(shellcodeAddr, shellcode))
    {
        Mem.Write<float>(coordsAddr + 0, 0.0f);
        Mem.Write<float>(coordsAddr + 8, 0.0f);
        Mem.Write<float>(coordsAddr + 16, 0.0f);
        Mem.Write<int32_t>(counterAddr, 0);
        g_GetFinalRenderedCamRotShellcodeAddr = shellcodeAddr;
    }
}

void Core::Features::cSilentAim::CreateGetGameplayCamRotHook()
{
    if (g_GetGameplayCamRotShellcodeAddr) return;

    uintptr_t shellcodeAddr = Mem.CreateCodeCave(512);
    if (!shellcodeAddr) return;

    uintptr_t coordsAddr = shellcodeAddr + 256;
    uintptr_t counterAddr = shellcodeAddr + 500;

    std::vector<uint8_t> shellcode = {
        0xFF, 0x05, 0x00, 0x00, 0x00, 0x00,
        0x48, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00,
        0x0F, 0x10, 0x00,
        0xC3
    };

    int32_t counterOffset = static_cast<int32_t>(counterAddr - (shellcodeAddr + 6));
    memcpy(shellcode.data() + 2, &counterOffset, sizeof(counterOffset));

    int32_t coordsOffset = static_cast<int32_t>(coordsAddr - (shellcodeAddr + 13));
    memcpy(shellcode.data() + 9, &coordsOffset, sizeof(coordsOffset));

    if (Mem.WriteBytes(shellcodeAddr, shellcode))
    {
        Mem.Write<float>(coordsAddr + 0, 0.0f);
        Mem.Write<float>(coordsAddr + 8, 0.0f);
        Mem.Write<float>(coordsAddr + 16, 0.0f);
        Mem.Write<int32_t>(counterAddr, 0);
        g_GetGameplayCamRotShellcodeAddr = shellcodeAddr;
    }
}

bool Core::Features::cSilentAim::InitalizeGetFinalRenderedCamRotHook()
{
    if (g_GetFinalRenderedCamRotHooked) return true;

    g_GetFinalRenderedCamRotFuncAddr = Mem.FindSignatureStr(xorstr("48 89 5C 24 ? 57 48 83 EC ? 8B DA 48 8B F9 E8 ? ? ? ? 48 8D 4C 24 ? 44 8B C3"));
    if (!g_GetFinalRenderedCamRotFuncAddr) return false;

    g_OriginalGetFinalRenderedCamRotBytes = Mem.ReadBytes(g_GetFinalRenderedCamRotFuncAddr, 16);

    CreateGetFinalRenderedCamRotHook();
    if (!g_GetFinalRenderedCamRotShellcodeAddr) return false;

    if (!Mem.HookJMP(g_GetFinalRenderedCamRotFuncAddr, g_GetFinalRenderedCamRotShellcodeAddr)) return false;

    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)g_GetFinalRenderedCamRotFuncAddr, 16);
    g_GetFinalRenderedCamRotHooked = true;
    return true;
}

bool Core::Features::cSilentAim::InitalizeGetGameplayCamRotHook()
{
    if (g_GetGameplayCamRotHooked) return true;

    g_GetGameplayCamRotFuncAddr = Mem.FindSignatureStr(xorstr("48 89 5C 24 08 57 48 83 EC 30 8B DA 48 8B F9 E8 ?? ?? ?? ?? 48 8D 4C 24 20 48"));
    if (!g_GetGameplayCamRotFuncAddr) return false;

    g_OriginalGetGameplayCamRotBytes = Mem.ReadBytes(g_GetGameplayCamRotFuncAddr, 16);

    CreateGetGameplayCamRotHook();
    if (!g_GetGameplayCamRotShellcodeAddr) return false;

    if (!Mem.HookJMP(g_GetGameplayCamRotFuncAddr, g_GetGameplayCamRotShellcodeAddr)) return false;

    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)g_GetGameplayCamRotFuncAddr, 16);
    g_GetGameplayCamRotHooked = true;
    return true;
}

bool Core::Features::cSilentAim::setfinalcamrot(float x, float y, float z)
{
    if (!g_GetFinalRenderedCamRotShellcodeAddr) return false;
    uintptr_t addr = g_GetFinalRenderedCamRotShellcodeAddr + 256;
    Mem.Write<float>(addr + 0, x);
    Mem.Write<float>(addr + 8, y);
    Mem.Write<float>(addr + 16, z);
    return true;
}

bool Core::Features::cSilentAim::setgameplaycamrot(float x, float y, float z)
{
    if (!g_GetGameplayCamRotShellcodeAddr) return false;
    uintptr_t addr = g_GetGameplayCamRotShellcodeAddr + 256;
    Mem.Write<float>(addr + 0, x);
    Mem.Write<float>(addr + 8, y);
    Mem.Write<float>(addr + 16, z);
    return true;
}

void Core::Features::cSilentAim::UnhookGetFinalRenderedCamRot()
{
    if (g_GetFinalRenderedCamRotHooked && g_GetFinalRenderedCamRotFuncAddr && !g_OriginalGetFinalRenderedCamRotBytes.empty())
    {
        Mem.WriteBytes(g_GetFinalRenderedCamRotFuncAddr, g_OriginalGetFinalRenderedCamRotBytes);
        FlushInstructionCache(GetCurrentProcess(), (LPCVOID)g_GetFinalRenderedCamRotFuncAddr, 16);
        g_GetFinalRenderedCamRotHooked = false;
    }
}

void Core::Features::cSilentAim::UnhookGetGameplayCamRot()
{
    if (g_GetGameplayCamRotHooked && g_GetGameplayCamRotFuncAddr && !g_OriginalGetGameplayCamRotBytes.empty())
    {
        Mem.WriteBytes(g_GetGameplayCamRotFuncAddr, g_OriginalGetGameplayCamRotBytes);
        FlushInstructionCache(GetCurrentProcess(), (LPCVOID)g_GetGameplayCamRotFuncAddr, 16);
        g_GetGameplayCamRotHooked = false;
    }
}

void Core::Features::cSilentAim::UnhookAllCamRot()
{
    UnhookGetFinalRenderedCamRot();
    UnhookGetGameplayCamRot();
}

void Core::Features::cSilentAim::ResetProcessState()
{
    if (Mem.IsProcessAlive())
    {
        try
        {
            UnhookAllCamRot();
            UnhookIsPlayerFreeAimingAtEntity();

            if (!g_GetFinalRenderedCamRotHooked && g_GetFinalRenderedCamRotShellcodeAddr)
                Mem.FreeCave(g_GetFinalRenderedCamRotShellcodeAddr);

            if (!g_GetGameplayCamRotHooked && g_GetGameplayCamRotShellcodeAddr)
                Mem.FreeCave(g_GetGameplayCamRotShellcodeAddr);

            if (!g_IsPlayerFreeAimingAtEntityHooked && g_IsPlayerFreeAimingAtEntityShellcodeAddr)
                Mem.FreeCave(g_IsPlayerFreeAimingAtEntityShellcodeAddr);
        }
        catch (...)
        {
        }
    }

    HookedCamera = false;

    g_GetFinalRenderedCamRotFuncAddr = 0;
    g_GetFinalRenderedCamRotShellcodeAddr = 0;
    g_OriginalGetFinalRenderedCamRotBytes.clear();
    g_GetFinalRenderedCamRotHooked = false;

    g_GetGameplayCamRotFuncAddr = 0;
    g_GetGameplayCamRotShellcodeAddr = 0;
    g_OriginalGetGameplayCamRotBytes.clear();
    g_GetGameplayCamRotHooked = false;

    g_IsPlayerFreeAimingAtEntityFuncAddr = 0;
    g_IsPlayerFreeAimingAtEntityShellcodeAddr = 0;
    g_OriginalIsPlayerFreeAimingAtEntityBytes.clear();
    g_IsPlayerFreeAimingAtEntityHooked = false;

    ClearVisualTarget();
}

D3DXVECTOR3 Core::Features::cSilentAim::CalcRotation(D3DXVECTOR3 src, D3DXVECTOR3 dst)
{
    D3DXVECTOR3 d = dst - src;
    float distXY = sqrtf(d.x * d.x + d.y * d.y);

    D3DXVECTOR3 rot;
    rot.x = atan2f(d.z, distXY) * 57.295779513f;
    rot.z = -atan2f(d.x, d.y) * 57.295779513f;
    rot.y = 0.f;

    if (rot.x > 89.f)  rot.x = 89.f;
    if (rot.x < -89.f) rot.x = -89.f;

    // Yaw normaliseren
    if (rot.z > 180.f)  rot.z -= 360.f;
    if (rot.z < -180.f) rot.z += 360.f;

    return rot;
}

void Core::Features::cSilentAim::SpoofCamera(bool enable)
{
    static std::mutex cameraMutex;
    std::lock_guard<std::mutex> lock(cameraMutex);

    if (enable)
    {
        if (!HookedCamera)
        {
            try
            {
                InitalizeGetFinalRenderedCamRotHook();
                InitalizeGetGameplayCamRotHook();
                HookedCamera = true;
            }
            catch (...)
            {
                HookedCamera = false;
            }
        }
    }
    else
    {
        if (HookedCamera)
        {
            try
            {
                UnhookGetFinalRenderedCamRot();
                UnhookGetGameplayCamRot();
                HookedCamera = false;
            }
            catch (...)
            {
                HookedCamera = false;
            }
        }
    }
}

void Core::Features::cSilentAim::SetVisualTarget(const D3DXVECTOR2& screenPos)
{
    std::lock_guard<std::mutex> lock(VisualTargetMutex);
    VisualTargetScreenPos = screenPos;
    HasVisualTarget = true;
}

void Core::Features::cSilentAim::ClearVisualTarget()
{
    std::lock_guard<std::mutex> lock(VisualTargetMutex);
    VisualTargetScreenPos = D3DXVECTOR2(0, 0);
    HasVisualTarget = false;
}

bool Core::Features::cSilentAim::GetVisualTarget(D3DXVECTOR2& screenPos)
{
    std::lock_guard<std::mutex> lock(VisualTargetMutex);
    if (!HasVisualTarget) return false;
    screenPos = VisualTargetScreenPos;
    return true;
}

bool IsSilentAimActive()
{
    auto* cfg = g_Config.SilentAim;
    HWND ActiveWindow = GetForegroundWindow();
    if (g_MenuInfo.IsOpen || ActiveWindow != g_Variables.g_hGameWindow)
    {
        cfg->KeyBindToggled = false;
        cfg->KeyBindLast = false;
        return false;
    }

    if (!cfg->Enabled || cfg->KeyBind == 0)
    {
        cfg->KeyBindToggled = false;
        cfg->KeyBindLast = false;
        return false;
    }

    if (cfg->KeyBindMode == 1)
    {
        const bool down = (GetAsyncKeyState(cfg->KeyBind) & 0x8000) != 0;
        cfg->KeyBindToggled = down;
        cfg->KeyBindLast = down;
        return down;
    }

    bool down = (GetAsyncKeyState(cfg->KeyBind) & 0x8000) != 0;
    if (down && !cfg->KeyBindLast)
    {
        cfg->KeyBindToggled = !cfg->KeyBindToggled;
        if (cfg->KeyBindToggled)
            NotifyManager::Send("SilentAim Enabled", 200.0f);
        else
            NotifyManager::Send("SilentAim Disabled", 200.0f);
    }
    cfg->KeyBindLast = down;
    return cfg->KeyBindToggled;
}

std::vector<uint16_t> GetSelectedBoneGroups(CPed* ped)
{
    std::vector<uint16_t> bones;
    auto* cfg = g_Config.SilentAim;

    if (ped && ped->HasFlag(ePedConfigFlag::InVehicle))
    {
        if (cfg->BoneGroup_Head) bones.push_back(SKEL_Head);
        if (cfg->BoneGroup_Neck) bones.push_back(SKEL_Neck_1);
        if (cfg->BoneGroup_Chest)
        {
            bones.push_back(SKEL_Spine3);
            bones.push_back(SKEL_Spine2);
        }
    }
    else
    {
        if (cfg->BoneGroup_Head) bones.push_back(SKEL_Head);
        if (cfg->BoneGroup_Neck) bones.push_back(SKEL_Neck_1);
        if (cfg->BoneGroup_Chest)
        {
            bones.push_back(SKEL_Spine3);
            bones.push_back(SKEL_Spine2);
            bones.push_back(SKEL_Spine1);
        }
        if (cfg->BoneGroup_Arms)
        {
            bones.insert(bones.end(), { SKEL_L_UpperArm, SKEL_L_Forearm, SKEL_L_Hand, SKEL_R_UpperArm, SKEL_R_Forearm, SKEL_R_Hand });
        }
        if (cfg->BoneGroup_Legs)
        {
            bones.insert(bones.end(), { SKEL_L_Thigh, SKEL_L_Calf, SKEL_R_Thigh, SKEL_R_Calf });
        }
        if (cfg->BoneGroup_Feet)
        {
            bones.insert(bones.end(), { SKEL_L_Foot, SKEL_R_Foot });
        }
    }

    if (bones.empty())
        bones.push_back(SKEL_Head);

    return bones;
}

void Core::Features::cSilentAim::RestoreSilent()
{
    if (!g_Offsets.m_HandleBullet || !Mem.ModBase)
        return;

    Mem.WriteProcessMemoryImpl(g_Offsets.m_HandleBullet, ReWriteTable.data(), ReWriteTable.size());
    Mem.WriteProcessMemoryImpl(Mem.ModBase + 0x34E, AngleReWriteTable.data(), AngleReWriteTable.size());
    aim_target = {};
}

void Core::Features::cSilentAim::ApplySilent(D3DXVECTOR3 EndPos, D3DXVECTOR3 localPos)
{

    uint64_t HandleBullet = g_Offsets.m_HandleBullet;
    uint64_t AllocPtr = Mem.ModBase + 0x34E;

    if (!HandleBullet || !Mem.ModBase)
        return;

    auto Rel = [](uint64_t src, uint64_t dst, int size = 5) -> uint32_t {
        return static_cast<uint32_t>(dst - (src + size));
        };

    union { float f; uint32_t i; } x{ EndPos.x }, y{ EndPos.y }, z{ EndPos.z };

    std::array<uint8_t, 5> jmp = { 0xE9, 0,0,0,0 };
    uint32_t off = Rel(HandleBullet, AllocPtr);
    memcpy(&jmp[1], &off, 4);

    Mem.WriteProcessMemoryImpl(HandleBullet, jmp.data(), jmp.size());

    uint32_t back = Rel(AllocPtr, HandleBullet, 28);
    std::array<uint8_t, 33> code = {
        0x41,0xC7,0x01, BYTE(x.i), BYTE(x.i >> 8), BYTE(x.i >> 16), BYTE(x.i >> 24),
        0x41,0xC7,0x41,0x04, BYTE(y.i), BYTE(y.i >> 8), BYTE(y.i >> 16), BYTE(y.i >> 24),
        0x41,0xC7,0x41,0x08, BYTE(z.i), BYTE(z.i >> 8), BYTE(z.i >> 16), BYTE(z.i >> 24),
        0xF3,0x41,0x0F,0x10,0x19,
        0xE9,0,0,0,0
    };
    memcpy(&code[29], &back, 4);

    Mem.WriteProcessMemoryImpl(AllocPtr, code.data(), code.size());
}

void Core::Features::cSilentAim::Start(uint64_t worker_epoch)
{
    bool silentApplied = false;
    constexpr auto idle_sleep = std::chrono::milliseconds(24);
    constexpr auto active_sleep = std::chrono::milliseconds(10);
    constexpr auto rewrite_interval = std::chrono::milliseconds(12);
    auto last_write_time = std::chrono::steady_clock::time_point{};
    D3DXVECTOR3 last_applied_pos{};

    while (Core::ShouldGameThreadRun(worker_epoch))
    {
        if (!IsSilentAimActive())
        {
            ClearVisualTarget();
            Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::SilentAim);
            if (silentApplied)
            {
                SpoofCamera(false);
                RestoreSilent();

                silentApplied = false;
            }
            last_write_time = {};
            last_applied_pos = {};
            std::this_thread::sleep_for(idle_sleep);
            continue;
        }

        CPed* local = Core::SDK::Pointers::pLocalPlayer;
        if (!local)
        {
            ClearVisualTarget();
            Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::SilentAim);
            if (silentApplied) { 
                RestoreSilent();
                silentApplied = false; 
            }
            last_write_time = {};
            last_applied_pos = {};
            std::this_thread::sleep_for(idle_sleep);
            continue;
        }

        float bestDist = FLT_MAX;
        D3DXVECTOR3 bestPos{};
        D3DXVECTOR3 bestPosRandomInt{};
        D3DXVECTOR2 bestScreen{};
        CPed* targetPed = nullptr;
        bool found = false;

        uintptr_t wpM = (g_Offsets.CurrentBuild >= 2802) ? 0x10B8ULL : 0x10D8ULL;
        uintptr_t weaponManager = Mem.Read<uintptr_t>((uintptr_t)local + wpM);
        uintptr_t wepI = 0;
        if (weaponManager)
        {
            uintptr_t unk = Mem.Read<uintptr_t>(weaponManager + 0x78);
            uintptr_t pWeapon = Mem.Read<uintptr_t>(unk + 0x340);
            if (pWeapon) wepI = Mem.Read<uintptr_t>(pWeapon + 0x20);
        }

        D3DXVECTOR3 localPos = Mem.Read<D3DXVECTOR3>((uintptr_t)local + 0x90);
        CPed* ped = Core::SDK::Game::GetClosestPed(
            g_Config.SilentAim->MaxDistance,
            g_Config.SilentAim->IgnoreNPCs,
            g_Config.SilentAim->IgnoreDead,
            g_Config.SilentAim->OnlyVisible,
            true,
            g_Config.SilentAim->ExcludeGod
        );

        if (g_Config.SilentAim->BoneGroup_Wheels && ped && ped->InVehicle())
        {
            D3DXVECTOR3 wheelPos = Core::SDK::Game::bestWheel(g_Config.SilentAim->MaxDistance, g_Config.SilentAim->IgnoreUnoccupied);
            D3DXVECTOR3 wheelPosPred;
            if (wheelPos.x != 0.0f || wheelPos.y != 0.0f || wheelPos.z != 0.0f)
            {
                D3DXVECTOR3 diff = wheelPos - localPos;
                float distance = D3DXVec3Length(&diff);
                if (g_Config.SilentAim->Prediction) {
                    wheelPosPred = Core::SDK::Game::AplPredict((uintptr_t)ped, distance, wheelPos, wepI);
                }
                else wheelPosPred = wheelPos;

                D3DXVECTOR2 screen = Core::SDK::Game::WorldToScreen(wheelPos);
                if (Core::SDK::Game::IsOnScreen(screen))
                {
                    float dx = screen.x - g_Variables.g_vGameWindowCenter.x;
                    float dy = screen.y - g_Variables.g_vGameWindowCenter.y;
                    const float dist = Core::FovUtils::GetNormalizedDistanceSq(
                        dx,
                        dy,
                        static_cast<float>(g_Config.SilentAim->FOVX),
                        static_cast<float>(g_Config.SilentAim->FOVY));
                    if (dist < bestDist && dist <= 1.0f)
                    {
                        bestDist = dist;
                        bestPos = wheelPosPred;
                        bestScreen = screen;
                        found = true;
                        targetPed = ped;
                    }
                }
            }
        }

        if (ped)
        {
            uintptr_t frag = Mem.Read<uintptr_t>((uintptr_t)ped + g_Offsets.m_FragInst);
            if (frag)
            {
                uintptr_t v9 = Mem.Read<uintptr_t>(frag + 0x68);
                if (v9)
                {
                    Core::SDK::Game::cSkeleton_t skel{};
                    skel.m_pSkeleton = Mem.Read<uintptr_t>(v9 + 0x178);
                    if (skel.m_pSkeleton)
                    {
                        skel.crSkeletonData.Ptr = Mem.Read<uintptr_t>(skel.m_pSkeleton);
                        skel.crSkeletonData.m_Used = Mem.Read<unsigned int>(skel.crSkeletonData.Ptr + 0x1A);
                        skel.crSkeletonData.m_NumBones = Mem.Read<unsigned int>(skel.crSkeletonData.Ptr + 0x5E);
                        skel.crSkeletonData.m_BoneIdTable_Slots = Mem.Read<unsigned short>(skel.crSkeletonData.Ptr + 0x18);
                        skel.crSkeletonData.m_BoneIdTable = Mem.Read<uintptr_t>(skel.crSkeletonData.Ptr + 0x10);
                        skel.Arg1 = Mem.Read<D3DXMATRIX>(Mem.Read<uintptr_t>(skel.m_pSkeleton + 0x8));
                        skel.Arg2 = Mem.Read<uintptr_t>(skel.m_pSkeleton + 0x18);

                        auto bones = GetSelectedBoneGroups(ped);
                        for (auto bone : bones)
                        {
                            D3DXVECTOR3 bonePosPred;
                            D3DXVECTOR3 bonePos = Core::SDK::Game::GetBonePosComplex(ped, bone, skel);
                            D3DXVECTOR3 diff = bonePos - localPos;
                            float distance = D3DXVec3Length(&diff);
                            if (g_Config.SilentAim->Prediction) {
                                bonePosPred = Core::SDK::Game::AplPredict((uintptr_t)ped, distance, bonePos, wepI);
                            }
                            else bonePosPred = bonePos;
                            D3DXVECTOR2 screen = Core::SDK::Game::WorldToScreen(bonePos);

                            if (!Core::SDK::Game::IsOnScreen(screen)) continue;

                            float dx = screen.x - g_Variables.g_vGameWindowCenter.x;
                            float dy = screen.y - g_Variables.g_vGameWindowCenter.y;
                            const float dist = Core::FovUtils::GetNormalizedDistanceSq(
                                dx,
                                dy,
                                static_cast<float>(g_Config.SilentAim->FOVX),
                                static_cast<float>(g_Config.SilentAim->FOVY));

                            if (dist < bestDist && dist <= 1.0f)
                            {
                                bestDist = dist;
                                bestPos = bonePosPred;
                                bestScreen = screen;
                                found = true;
                                targetPed = ped;
                            }
                        }
                    }
                }
            }
        }

        if (!found)
        {
            ClearVisualTarget();
            Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::SilentAim);
            if (silentApplied) { RestoreSilent();
            if (g_Config.SilentAim->SilentBypass) {
                SpoofCamera(false);
                UnhookIsPlayerFreeAimingAtEntity();
            }
            silentApplied = false;
            }
            last_write_time = {};
            last_applied_pos = {};
            std::this_thread::sleep_for(idle_sleep);
            continue;
        }

        bool shouldApply = true;
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> distRand(0, 99);
        static std::uniform_real_distribution<double> dist(0.01, 0.05);

        if (g_Config.SilentAim->MissChance > 0 && distRand(gen) < g_Config.SilentAim->MissChance)
            shouldApply = false;

        if (shouldApply)
        {
                if (targetPed && g_Config.SilentAim->BypassPassive)
                {
                    targetPed->SetConfigFlag(ePedConfigFlag::DisablePlayerLockon, false);
                    targetPed->SetConfigFlag(ePedConfigFlag::AllowPlayerLockOnIfFriendly, false);
                    targetPed->SetConfigFlag(ePedConfigFlag::TreatAsFriendlyForTargetingAndDamage, false);
                }

                SetVisualTarget(bestScreen);
                Core::Features::TargetHighlight::Mark(Core::Features::TargetHighlight::Source::SilentAim, targetPed);
                const auto now = std::chrono::steady_clock::now();
                const bool target_changed =
                    std::fabs(bestPos.x - last_applied_pos.x) > 0.01f ||
                    std::fabs(bestPos.y - last_applied_pos.y) > 0.01f ||
                    std::fabs(bestPos.z - last_applied_pos.z) > 0.01f;

                if (g_Config.SilentAim->SilentBypass) {
                    SpoofCamera(true);
                }

                uintptr_t CamFollowPedCamera = Mem.Read<uintptr_t>(Core::SDK::Pointers::pCamGamePlayDirector + 0x2C0);
                D3DXVECTOR3 CamPos = Mem.Read<D3DXVECTOR3>(CamFollowPedCamera + 0x60);

                D3DXVECTOR3 rot = CalcRotation(CamPos, bestPos);

                if (g_Config.SilentAim->SilentBypass) {
                    setfinalcamrot(rot.x, 0.f, rot.z);
                    setgameplaycamrot(rot.x, 0.f, rot.z);
                }
             
                bestPosRandomInt.x = bestPos.x + dist(gen);
                bestPosRandomInt.y = bestPos.y + dist(gen);
                bestPosRandomInt.z = bestPos.z;

                ApplySilent(bestPosRandomInt, localPos);
                last_applied_pos = bestPos;
                last_write_time = now;
            

            silentApplied = true;
        }
        else if (silentApplied)
        {

            ClearVisualTarget();
            RestoreSilent();

            Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::SilentAim);
            silentApplied = false;
            last_write_time = {};
            last_applied_pos = {};
        }
        else
        {
            ClearVisualTarget();
            Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::SilentAim);
        }

        std::this_thread::sleep_for(active_sleep);
    }

    ClearVisualTarget();
    Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::SilentAim);
    if (silentApplied && g_Offsets.m_HandleBullet && Mem.ModBase)
    {
        RestoreSilent();
    }
    SpoofCamera(false);
    UnhookIsPlayerFreeAimingAtEntity();
}
