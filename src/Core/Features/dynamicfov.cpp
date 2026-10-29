#pragma once
#include <cmath>
#include <vector>
#include <cfloat>
#include "Core/SDK/SDK.hpp"


float GetSilentAimTargetDistance()
{
    if (!Core::SDK::Pointers::pLocalPlayer) return 0.0f;

    CPed* Ped = Core::SDK::Game::GetClosestPed(
        g_Config.SilentAim->MaxDistance,
        g_Config.SilentAim->IgnoreNPCs,
        g_Config.SilentAim->IgnoreDead,
        g_Config.SilentAim->OnlyVisible,true, g_Config.SilentAim->ExcludeGod
    );
    if (!Ped) return 0.0f;

    uintptr_t fragInst = Mem.Read<uintptr_t>((uintptr_t)Ped + g_Offsets.m_FragInst);
    uintptr_t v9 = Mem.Read<uintptr_t>(fragInst + 0x68);
    if (!v9) return 0.0f;

    Core::SDK::Game::cSkeleton_t Skeleton{};
    Skeleton.m_pSkeleton = Mem.Read<uintptr_t>(v9 + 0x178);
    Skeleton.crSkeletonData.Ptr = Mem.Read<uintptr_t>(Skeleton.m_pSkeleton);
    if (!Skeleton.crSkeletonData.Ptr) return 0.0f;

    Skeleton.crSkeletonData.m_Used = Mem.Read<unsigned int>(Skeleton.crSkeletonData.Ptr + 0x1A);
    Skeleton.crSkeletonData.m_NumBones = Mem.Read<unsigned int>(Skeleton.crSkeletonData.Ptr + 0x5E);
    Skeleton.crSkeletonData.m_BoneIdTable_Slots = Mem.Read<unsigned short>(Skeleton.crSkeletonData.Ptr + 0x18);
    Skeleton.crSkeletonData.m_BoneIdTable = Mem.Read<uintptr_t>(Skeleton.crSkeletonData.Ptr + 0x10);
    Skeleton.Arg1 = Mem.Read<D3DXMATRIX>(Mem.Read<uintptr_t>(Skeleton.m_pSkeleton + 0x8));
    Skeleton.Arg2 = Mem.Read<uintptr_t>(Skeleton.m_pSkeleton + 0x18);

    uint16_t BoneId = SKEL_Head;

    std::vector<uint16_t> BoneList;
    auto* cfg = g_Config.SilentAim;

    if (cfg->BoneGroup_Head)   BoneList.push_back(SKEL_Head);
    if (cfg->BoneGroup_Neck)   BoneList.push_back(SKEL_Neck_1);
    if (cfg->BoneGroup_Chest) {
        BoneList.push_back(SKEL_Spine3);
        BoneList.push_back(SKEL_Spine2);
        BoneList.push_back(SKEL_Spine1);
    }
    if (cfg->BoneGroup_Arms) {
        BoneList.push_back(SKEL_L_UpperArm);
        BoneList.push_back(SKEL_L_Forearm);
        BoneList.push_back(SKEL_L_Hand);
        BoneList.push_back(SKEL_R_UpperArm);
        BoneList.push_back(SKEL_R_Forearm);
        BoneList.push_back(SKEL_R_Hand);
    }
    if (cfg->BoneGroup_Legs) {
        BoneList.push_back(SKEL_L_Thigh);
        BoneList.push_back(SKEL_L_Calf);
        BoneList.push_back(SKEL_R_Thigh);
        BoneList.push_back(SKEL_R_Calf);
    }
    if (cfg->BoneGroup_Feet) {
        BoneList.push_back(SKEL_L_Foot);
        BoneList.push_back(SKEL_R_Foot);
    }

    if (!BoneList.empty())
    {
        float best = FLT_MAX;
        uint16_t bestBone = BoneList[0];

        for (uint16_t id : BoneList)
        {
            D3DXVECTOR3 p3 = Core::SDK::Game::GetBonePosComplex(Ped, id, Skeleton);
            D3DXVECTOR2 p2 = Core::SDK::Game::WorldToScreen(p3);
            if (!Core::SDK::Game::IsOnScreen(p2)) continue;

            const float dx = p2.x - g_Variables.g_vGameWindowCenter.x;
            const float dy = p2.y - g_Variables.g_vGameWindowCenter.y;
            const float dist = Core::FovUtils::GetNormalizedDistanceSq(
                dx,
                dy,
                static_cast<float>(g_Config.SilentAim->FOVX),
                static_cast<float>(g_Config.SilentAim->FOVY));
            if (dist < best && dist <= 1.0f)
            {
                best = dist;
                bestBone = id;
            }
        }
        BoneId = bestBone;
    }

    D3DXVECTOR3 TargetPos = Core::SDK::Game::GetBonePosComplex(Ped, BoneId, Skeleton);
    D3DXVECTOR3 LocalPos = Core::SDK::Pointers::pLocalPlayer->GetPos();
    D3DXVECTOR3 Delta = TargetPos - LocalPos;
    return D3DXVec3Length(&Delta);
}

float GetAimbotTargetDistance()
{
    if (!Core::SDK::Pointers::pLocalPlayer) return 0.0f;

    CPed* Ped = Core::SDK::Game::GetClosestPed(
        g_Config.Aimbot->MaxDistance,
        g_Config.Aimbot->IgnoreNPCs,
        g_Config.Aimbot->IgnoreDead,
        g_Config.Aimbot->OnlyVisible,true, g_Config.Aimbot->ExcludeGod

    );
    if (!Ped) return 0.0f;

    uintptr_t FragInst = Mem.Read<uintptr_t>((uintptr_t)Ped + g_Offsets.m_FragInst);
    uintptr_t v9 = Mem.Read<uintptr_t>(FragInst + 0x68);
    if (!v9) return 0.0f;

    Core::SDK::Game::cSkeleton_t Skeleton{};
    Skeleton.m_pSkeleton = Mem.Read<uintptr_t>(v9 + 0x178);
    Skeleton.crSkeletonData.Ptr = Mem.Read<uintptr_t>(Skeleton.m_pSkeleton);
    if (!Skeleton.crSkeletonData.Ptr) return 0.0f;

    Skeleton.crSkeletonData.m_Used = Mem.Read<unsigned int>(Skeleton.crSkeletonData.Ptr + 0x1A);
    Skeleton.crSkeletonData.m_NumBones = Mem.Read<unsigned int>(Skeleton.crSkeletonData.Ptr + 0x5E);
    Skeleton.crSkeletonData.m_BoneIdTable_Slots = Mem.Read<unsigned short>(Skeleton.crSkeletonData.Ptr + 0x18);
    Skeleton.crSkeletonData.m_BoneIdTable = Mem.Read<uintptr_t>(Skeleton.crSkeletonData.Ptr + 0x10);
    Skeleton.Arg1 = Mem.Read<D3DXMATRIX>(Mem.Read<uintptr_t>(Skeleton.m_pSkeleton + 0x8));
    Skeleton.Arg2 = Mem.Read<uintptr_t>(Skeleton.m_pSkeleton + 0x18);

    uint16_t BoneId = SKEL_Head;
    if (g_Config.Aimbot->RandomHitbox)
    {
        std::vector<uint16_t> BoneList = {
            SKEL_ROOT, SKEL_Pelvis, SKEL_L_Thigh, SKEL_L_Calf, SKEL_L_Foot,
            SKEL_R_Thigh, SKEL_R_Calf, SKEL_R_Foot, SKEL_Spine_Root, SKEL_Spine0,
            SKEL_Spine1, SKEL_Spine2, SKEL_Spine3, SKEL_L_Clavicle, SKEL_L_UpperArm,
            SKEL_L_Forearm, SKEL_L_Hand, SKEL_R_Clavicle, SKEL_R_UpperArm, SKEL_R_Forearm,
            SKEL_R_Hand, SKEL_Neck_1, SKEL_Head
        };
        float ClosestDist = FLT_MAX; uint16_t ClosestBone = SKEL_Head;
        for (auto& Bone : BoneList)
        {
            D3DXVECTOR3 BonePosTest = Core::SDK::Game::GetBonePosComplex(Ped, Bone, Skeleton);
            D3DXVECTOR2 BoneScreen = Core::SDK::Game::WorldToScreen(BonePosTest);
            if (!Core::SDK::Game::IsOnScreen(BoneScreen)) continue;
            float DistToCrosshair = std::hypot(BoneScreen.x - g_Variables.g_vGameWindowCenter.x, BoneScreen.y - g_Variables.g_vGameWindowCenter.y);
            if (DistToCrosshair < ClosestDist) { ClosestDist = DistToCrosshair; ClosestBone = Bone; }
        }
        BoneId = ClosestBone;
    }
    else {
        switch (g_Config.Aimbot->HitBox) { case 0: BoneId = SKEL_Head; break; case 1: BoneId = SKEL_Neck_1; break; case 2: BoneId = SKEL_Spine3; break; default: BoneId = SKEL_Head; break; }
    }

    D3DXVECTOR3 TargetPos = Core::SDK::Game::GetBonePosComplex(Ped, BoneId, Skeleton);
    D3DXVECTOR3 LocalPos = Core::SDK::Pointers::pLocalPlayer->GetPos();
    D3DXVECTOR3 Delta = TargetPos - LocalPos;
    return D3DXVec3Length(&Delta);
}

float GetTriggerbotTargetDistance()
{
    if (!Core::SDK::Pointers::pLocalPlayer) return 0.0f;

    if (!g_Config.TriggerBot->SmartTrigger)
    {
        CPed* Ped = Core::SDK::Game::GetClosestPed(g_Config.TriggerBot->MaxDistance, g_Config.TriggerBot->IgnoreNPCs, g_Config.TriggerBot->IgnoreDead, g_Config.TriggerBot->OnlyVisible,true, g_Config.TriggerBot->ExcludeGod);
        if (!Ped) return 0.0f;

        D3DXVECTOR3 HeadPos = Ped->GetBonePosDefault(0);
        D3DXVECTOR3 LocalPos = Core::SDK::Pointers::pLocalPlayer->GetPos();
        D3DXVECTOR3 Delta = HeadPos - LocalPos;
        return D3DXVec3Length(&Delta);
    }
    else
    {
        CPed* Ped = Mem.Read<CPed*>(g_Offsets.m_AimCPedPatternResult + Mem.Read<int>(g_Offsets.m_AimCPedPatternResult + 3) + 7);
        if (!Ped) return 0.0f;

        D3DXVECTOR3 HeadPos = Ped->GetBonePosDefault(0);
        D3DXVECTOR3 LocalPos = Core::SDK::Pointers::pLocalPlayer->GetPos();
        D3DXVECTOR3 Delta = HeadPos - LocalPos;
        return D3DXVec3Length(&Delta);
    }
}
