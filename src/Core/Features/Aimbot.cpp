#include "Aimbot.hpp"
#include "CMetaData.h"
#include "TargetHighlight.hpp"
#include <Core/Lifecycle.hpp>

#include <algorithm>
#include <random>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#define SKEL_Neck_1 0x9995
#define SKEL_Head   0x796e
#define SKEL_Spine3 0x60f2

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> dist(-0.01f, 0.01f);

static CPed* g_StickyTarget = nullptr;
static CPed* g_CurveTarget = nullptr;
static D3DXVECTOR3 g_CurveStartAngles{};
static D3DXVECTOR3 g_CurveGoalAngles{};
static float g_CurveProgress = 0.0f;
static bool g_CurveActive = false;
static float EaseInOut(float t, float accel, float decel)
{
    t = std::clamp(t, 0.0f, 1.0f);

    float a = std::max(0.01f, accel);
    float d = std::max(0.01f, decel);

    float inPart = std::pow(t, a);
    float outPart = 1.0f - std::pow(1.0f - t, d);

    return (inPart + outPart) * 0.5f;
}
std::vector<uint16_t> GetSelectedBoneGroupsABOT()
{
    std::vector<uint16_t> bones;
    auto* cfg = g_Config.Aimbot;
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
        bones.insert(bones.end(), {
            SKEL_L_UpperArm, SKEL_L_Forearm, SKEL_L_Hand,
            SKEL_R_UpperArm, SKEL_R_Forearm, SKEL_R_Hand
            });
    }
    if (cfg->BoneGroup_Legs)
    {
        bones.insert(bones.end(), {
            SKEL_L_Thigh, SKEL_L_Calf,
            SKEL_R_Thigh, SKEL_R_Calf
            });
    }
    if (cfg->BoneGroup_Feet)
    {
        bones.insert(bones.end(), {
            SKEL_L_Foot,
            SKEL_R_Foot
            });
    }
    if (bones.empty())
        bones.push_back(SKEL_Head);
    return bones;
}

D3DXVECTOR3 PredictPosition2(CPed* ped, D3DXVECTOR3 bonePos)
{
    if (!ped || !Core::SDK::Pointers::pLocalPlayer)
        return bonePos;

    CPed* local = Core::SDK::Pointers::pLocalPlayer;
    D3DXVECTOR3 targetVel{};

    if (ped->HasFlag(ePedConfigFlag::InVehicle))
    {
        uintptr_t veh = Mem.Read<uintptr_t>((uintptr_t)ped + 0x0D30);
        if (veh) targetVel = Mem.Read<D3DXVECTOR3>(veh + 0x0D40);
    }
    else
    {
        targetVel = Mem.Read<D3DXVECTOR3>((uintptr_t)ped + 0x300);
    }

    static D3DXVECTOR3 lastVel{};
    float speed = D3DXVec3Length(&targetVel);
    float smooth = (speed < 0.1f) ? 0.9f : 0.5f;
    targetVel = lastVel + (targetVel - lastVel) * smooth;
    lastVel = targetVel;

    auto* wm = local->GetWeaponManager();
    if (!wm) return bonePos;
    auto* wi = wm->GetWeaponInfo();
    if (!wi) return bonePos;

    float bulletSpeed = wi->GetWeaponBulletSpeed();
    if (bulletSpeed < 10.0f) return bonePos;

    if (bulletSpeed != 1300.0f)
    {
        uintptr_t wpM = (g_Offsets.CurrentBuild >= 2802) ? 0x10B8ULL : 0x10D8ULL;
        uintptr_t wpmng = Mem.Read<uintptr_t>((uintptr_t)local + wpM);
        if (wpmng)
        {
            uintptr_t wepI = Mem.Read<uintptr_t>(wpmng + 0x20);
            if (wepI) Mem.Write<float>(wepI + 0xE0, 1300.0f);
        }
    }

    D3DXVECTOR3 localPos = Mem.Read<D3DXVECTOR3>((uintptr_t)local + 0x90);
    D3DXVECTOR3 dir = bonePos - localPos;
    float distSq = D3DXVec3LengthSq(&dir);
    float dist = std::sqrt(distSq);
    if (dist < 1.0f) return bonePos;

    float a = D3DXVec3Dot(&targetVel, &targetVel) - (bulletSpeed * bulletSpeed);
    float b = 2.0f * D3DXVec3Dot(&dir, &targetVel);
    float c = distSq;
    float travelTime = 0.0f;

    if (std::fabs(a) < 0.001f)
    {
        if (std::fabs(b) > 0.001f)
            travelTime = -c / b;
    }
    else
    {
        float disc = b * b - 4.0f * a * c;
        if (disc >= 0.0f)
        {
            float sd = std::sqrt(disc);
            float t1 = (-b + sd) / (2.0f * a);
            float t2 = (-b - sd) / (2.0f * a);
            travelTime = (t1 > 0.0f && t2 > 0.0f) ? std::min(t1, t2) : (t1 > 0.0f ? t1 : (t2 > 0.0f ? t2 : 0.0f));
        }
    }

    if (travelTime <= 0.001f || travelTime > 5.0f)
        travelTime = dist / bulletSpeed;

    D3DXVECTOR3 predicted = bonePos + targetVel * travelTime;
    if (dist > 80.0f)
        predicted.z += (dist - 80.0f) / 400.0f * 0.4f;

    return predicted;
}

void Core::Features::cAimbot::SetViewAngles(CPed* Ped, D3DXVECTOR3 BonePos)
{
    uintptr_t CamFollowPedCamera = Mem.Read<uintptr_t>(Core::SDK::Pointers::pCamGamePlayDirector + 0x2C0);
    if (!CamFollowPedCamera) return;

    D3DXVECTOR3 CamPos = Mem.Read<D3DXVECTOR3>(CamFollowPedCamera + 0x60);

    ::SDK::CMetaData* metadata = (::SDK::CMetaData*)Mem.Read<uintptr_t>(CamFollowPedCamera + 0x10);
    bool isFirstPerson = metadata ? metadata->bIsFristPerson() : false;

    uintptr_t rotationOffset = isFirstPerson ? 0x40 : 0x3D0;

    D3DXVECTOR3 CurrentViewAngles = Mem.Read<D3DXVECTOR3>(CamFollowPedCamera + rotationOffset);
    D3DXVec3Normalize(&CurrentViewAngles, &CurrentViewAngles);

    if (metadata)
    {
        if (isFirstPerson)
        {
            float currentAlt = Mem.Read<float>((uintptr_t)metadata + 0x88);
            if (std::abs(currentAlt) > 0.001f)
                metadata->fSetAltitude(0.0f);
        }

        if (Core::SDK::Pointers::pLocalPlayer)
            metadata->SetVectorFix(Core::SDK::Pointers::pLocalPlayer->InVehicle());
    }

    D3DXVECTOR3 TargetViewAngles = BonePos - CamPos;
    D3DXVec3Normalize(&TargetViewAngles, &TargetViewAngles);

    if (g_Config.Aimbot->CurvyMode)
    {
        bool newTarget = (g_CurveTarget != Ped) || !g_CurveActive;

        if (newTarget)
        {
            g_CurveTarget = Ped;
            g_CurveStartAngles = CurrentViewAngles;   // echte start
            g_CurveGoalAngles = TargetViewAngles;
            g_CurveProgress = 0.0f;
            g_CurveActive = true;
        }
        else
        {
            // Goal updaten (target kan bewegen)
            g_CurveGoalAngles = TargetViewAngles;

            // Start NIET elke frame volledig overschrijven!
            // We updaten hem alleen een klein beetje, of we gebruiken een "pull" factor
            // Dit is de key voor "curve + free look"

            float pullStrength = 0.15f; // ← pas dit aan (0.05 = traag volgen, 0.30 = sneller volgen)

            g_CurveStartAngles = g_CurveStartAngles + (CurrentViewAngles - g_CurveStartAngles) * pullStrength;
            D3DXVec3Normalize(&g_CurveStartAngles, &g_CurveStartAngles);
        }

        // Progress alleen verhogen als we nog niet klaar zijn
        if (g_CurveProgress < 1.0f)
        {
            float speed = std::clamp((float)g_Config.Aimbot->AimbotSpeed / 80.0f, 0.008f, 1.0f); // iets lager dan eerst
            g_CurveProgress += speed * 0.06f;   // experimenteer met 0.04f ~ 0.08f
            g_CurveProgress = std::clamp(g_CurveProgress, 0.0f, 1.0f);
        }

        float accel = std::max(0.8f, g_Config.Aimbot->CurveAccel);   // vaak beter rond 1.5 - 3.0
        float decel = std::max(0.8f, g_Config.Aimbot->CurveDecel);
        float t = EaseInOut(g_CurveProgress, accel, decel);

        D3DXVECTOR3 FinalAngles = g_CurveStartAngles + (g_CurveGoalAngles - g_CurveStartAngles) * t;
        D3DXVec3Normalize(&FinalAngles, &FinalAngles);

        Mem.Write<D3DXVECTOR3>(CamFollowPedCamera + rotationOffset, FinalAngles);

        // third person fix
        if (!isFirstPerson)
        {
            D3DXVECTOR3 Cam3DAngles = Mem.Read<D3DXVECTOR3>(CamFollowPedCamera + 0x3D0);
            D3DXVECTOR3 ThirdPersonAngles = FinalAngles;
            ThirdPersonAngles.z = FinalAngles.z - (CurrentViewAngles.z - Cam3DAngles.z);
            Mem.Write<D3DXVECTOR3>(CamFollowPedCamera + 0x3D0, ThirdPersonAngles);
        }

        return;
    }
    else
    {
        g_CurveActive = false;
        g_CurveTarget = nullptr;
        g_CurveProgress = 0.0f;
    }

    if (g_Config.Aimbot->LegitMode) {
        if (g_Config.Aimbot->AimbotSpeed > 1)
        {
            D3DXVECTOR3 Delta = TargetViewAngles - CurrentViewAngles;
            float SmoothFactor = 1.0f / std::max(1.0f, (float)g_Config.Aimbot->AimbotSpeed * 2.0f);

            D3DXVECTOR3 FinalAngles = CurrentViewAngles + (Delta * SmoothFactor);

            Mem.Write<D3DXVECTOR3>(CamFollowPedCamera + rotationOffset, FinalAngles);
            if (!isFirstPerson)
            {
                D3DXVECTOR3 Cam3DAngles = Mem.Read<D3DXVECTOR3>(CamFollowPedCamera + 0x3D0);
                D3DXVECTOR3 ThirdPersonAngles = FinalAngles;
                ThirdPersonAngles.z = FinalAngles.z - (CurrentViewAngles.z - Cam3DAngles.z);
                Mem.Write<D3DXVECTOR3>(CamFollowPedCamera + 0x3D0, ThirdPersonAngles);
            }
        }
        else
        {
            Mem.Write<D3DXVECTOR3>(CamFollowPedCamera + rotationOffset, TargetViewAngles);
        }
    }
    else
    {
        if (g_Config.Aimbot->AimbotSpeed > 1)
        {
            D3DXVECTOR3 Delta = TargetViewAngles - CurrentViewAngles;
            float SmoothFactor = 1.0f / (float)g_Config.Aimbot->AimbotSpeed;
            D3DXVECTOR3 FinalAngles;
            FinalAngles.x = CurrentViewAngles.x + (Delta.x * SmoothFactor);
            FinalAngles.y = CurrentViewAngles.y + (Delta.y * SmoothFactor);
            FinalAngles.z = CurrentViewAngles.z + (Delta.z * SmoothFactor);

            Mem.Write<D3DXVECTOR3>(CamFollowPedCamera + rotationOffset, FinalAngles);
            if (!isFirstPerson)
            {
                D3DXVECTOR3 Cam3DAngles = Mem.Read<D3DXVECTOR3>(CamFollowPedCamera + 0x3D0);
                D3DXVECTOR3 ThirdPersonAngles = FinalAngles;
                ThirdPersonAngles.z = FinalAngles.z - (CurrentViewAngles.z - Cam3DAngles.z);
                Mem.Write<D3DXVECTOR3>(CamFollowPedCamera + 0x3D0, ThirdPersonAngles);
            }
        }
        else
        {
            Mem.Write<D3DXVECTOR3>(CamFollowPedCamera + rotationOffset, TargetViewAngles);
        }
    }
}
#include <Includes/CustomWidgets/Notify.hpp>

bool IsAimbotActive()
{
    auto* cfg = g_Config.Aimbot;
    HWND ActiveWindow = GetForegroundWindow();
    const bool InGameContext = !g_MenuInfo.IsOpen && ActiveWindow == g_Variables.g_hGameWindow;

    if (!InGameContext)
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

    bool down = (GetAsyncKeyState(cfg->KeyBind) & 0x8000) != 0;

    if (cfg->KeyBindMode == 1) // Hold
    {
        cfg->KeyBindToggled = down;
        cfg->KeyBindLast = down;
        return down;
    }

    // Toggle
    if (down && !cfg->KeyBindLast)
    {
        cfg->KeyBindToggled = !cfg->KeyBindToggled;

        if (cfg->KeyBindToggled)
            NotifyManager::Send("Aimbot Enabled", 200.0f);
        else
            NotifyManager::Send("Aimbot Disabled", 200.0f);
    }

    cfg->KeyBindLast = down;

    return cfg->KeyBindToggled;
}
void Core::Features::cAimbot::Start(uint64_t worker_epoch)
{
    constexpr int IDLE_SLEEP = 24;
    constexpr int ACTIVE_SLEEP = 10;

    while (Core::ShouldGameThreadRun(worker_epoch))
    {
        if (!IsAimbotActive())
        {
            Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::Aimbot);

            if (g_Config.Aimbot->StickyAim)
                g_StickyTarget = nullptr;

            g_CurveActive = false;
            g_CurveTarget = nullptr;
            g_CurveProgress = 0.0f;

            std::this_thread::sleep_for(std::chrono::milliseconds(IDLE_SLEEP));
            continue;
        }

        CPed* local = Core::SDK::Pointers::pLocalPlayer;
        if (!local)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(IDLE_SLEEP));
            continue;
        }

        D3DXVECTOR3 localPos = Mem.Read<D3DXVECTOR3>((uintptr_t)local + 0x90);

        uintptr_t wepI = 0;
        {
            uintptr_t wpM = (g_Offsets.CurrentBuild >= 2802) ? 0x10B8ULL : 0x10D8ULL;
            uintptr_t wpmng = Mem.Read<uintptr_t>((uintptr_t)local + wpM);
            if (wpmng)
                wepI = Mem.Read<uintptr_t>(wpmng + 0x20);
        }

        CPed* TargetPed = nullptr;

        if (g_Config.Aimbot->StickyAim && g_StickyTarget)
        {
            if (Core::SDK::Game::IsValidPed(g_StickyTarget) &&
                Core::SDK::Game::GetDistanceToPed(g_StickyTarget) <= g_Config.Aimbot->MaxDistance)
            {
                TargetPed = g_StickyTarget;
            }
            else
            {
                g_StickyTarget = nullptr;
            }
        }

        if (!TargetPed)
        {
            TargetPed = Core::SDK::Game::GetClosestPed(
                g_Config.Aimbot->MaxDistance,
                g_Config.Aimbot->IgnoreNPCs,
                g_Config.Aimbot->IgnoreDead,
                g_Config.Aimbot->OnlyVisible,
                true,
                g_Config.Aimbot->ExcludeGod
            );

            if (TargetPed && g_Config.Aimbot->StickyAim)
                g_StickyTarget = TargetPed;
        }

        if (!TargetPed)
        {
            Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::Aimbot);
            std::this_thread::sleep_for(std::chrono::milliseconds(IDLE_SLEEP));
            continue;
        }

        uintptr_t frag = Mem.Read<uintptr_t>((uintptr_t)TargetPed + g_Offsets.m_FragInst);
        uintptr_t v9 = Mem.Read<uintptr_t>(frag + 0x68);
        if (!v9)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(ACTIVE_SLEEP));
            continue;
        }

        Core::SDK::Game::cSkeleton_t Skeleton;

        Skeleton.m_pSkeleton = Mem.Read<uintptr_t>(v9 + 0x178);

        Skeleton.crSkeletonData.Ptr = Mem.Read<uintptr_t>(Skeleton.m_pSkeleton);
        Skeleton.crSkeletonData.m_Used = Mem.Read<unsigned int>(Skeleton.crSkeletonData.Ptr + 0x1A);
        Skeleton.crSkeletonData.m_NumBones = Mem.Read<unsigned int>(Skeleton.crSkeletonData.Ptr + 0x5E);
        Skeleton.crSkeletonData.m_BoneIdTable_Slots = Mem.Read<unsigned short>(Skeleton.crSkeletonData.Ptr + 0x18);

        if (!Skeleton.crSkeletonData.m_BoneIdTable_Slots)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(ACTIVE_SLEEP));
            continue;
        }

        Skeleton.crSkeletonData.m_BoneIdTable = Mem.Read<uintptr_t>(Skeleton.crSkeletonData.Ptr + 0x10);

        Skeleton.Arg1 = Mem.Read<D3DXMATRIX>(Mem.Read<uintptr_t>(Skeleton.m_pSkeleton + 0x8));
        Skeleton.Arg2 = Mem.Read<uintptr_t>(Skeleton.m_pSkeleton + 0x18);

        auto bones = GetSelectedBoneGroupsABOT();

        float bestDistSq = FLT_MAX;
        D3DXVECTOR3 bestPos{};
        bool found = false;

        for (uint16_t id : bones)
        {
            D3DXVECTOR3 pos = Core::SDK::Game::GetBonePosComplex(TargetPed, id, Skeleton);

            D3DXVECTOR3 diff = pos - localPos;
            float distance = D3DXVec3Length(&diff);

            if (g_Config.Aimbot->Prediction)
                pos = Core::SDK::Game::AplPredict((uintptr_t)TargetPed, distance, pos, wepI);

            D3DXVECTOR2 screen = Core::SDK::Game::WorldToScreen(pos);

            if (!Core::SDK::Game::IsOnScreen(screen))
                continue;

            float dx = screen.x - g_Variables.g_vGameWindowCenter.x;
            float dy = screen.y - g_Variables.g_vGameWindowCenter.y;

            const float distSq = Core::FovUtils::GetNormalizedDistanceSq(
                dx,
                dy,
                static_cast<float>(g_Config.Aimbot->FOVX),
                static_cast<float>(g_Config.Aimbot->FOVY));

            if (distSq < bestDistSq && distSq <= 1.0f)
            {
                bestDistSq = distSq;
                bestPos = pos;
                found = true;
            }
        }

        if (!found)
        {
            Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::Aimbot);
            std::this_thread::sleep_for(std::chrono::milliseconds(ACTIVE_SLEEP));
            continue;
        }

        D3DXVECTOR2 ScreenPos = Core::SDK::Game::WorldToScreen(bestPos);

        if (Core::SDK::Game::IsOnScreen(ScreenPos))
        {
            float dx = ScreenPos.x - g_Variables.g_vGameWindowCenter.x;
            float dy = ScreenPos.y - g_Variables.g_vGameWindowCenter.y;

            const float distSq = Core::FovUtils::GetNormalizedDistanceSq(
                dx,
                dy,
                static_cast<float>(g_Config.Aimbot->FOVX),
                static_cast<float>(g_Config.Aimbot->FOVY));

            if (distSq <= 1.0f)
            {
                Core::Features::TargetHighlight::Mark(Core::Features::TargetHighlight::Source::Aimbot, TargetPed);
                SetViewAngles(TargetPed, bestPos);
            }
            else
            {
                Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::Aimbot);
            }
        }
        else
        {
            Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::Aimbot);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(ACTIVE_SLEEP));
    }

    Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::Aimbot);
    g_StickyTarget = nullptr;
    g_CurveActive = false;
    g_CurveTarget = nullptr;
    g_CurveProgress = 0.0f;
}
