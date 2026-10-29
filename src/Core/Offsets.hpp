#pragma once
#include "../Includes/Includes.hpp"
namespace Core {

    class OffsetsClass {
    public:

        //Class Addrs
        uintptr_t m_World, //48 8B 05 ?? ?? ?? ?? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B
            m_ReplayInterFace, //48 8D 0D ?? ?? ?? ?? 48 ?? ?? E8 ?? ?? ?? ?? 48 8D 0D ?? ?? ?? ?? 8A D8 E8 ?? ?? ?? ?? 84 DB 75 13 48 8D 0D ?? ?? ?? ?? 48 8B D7 E8 ?? ?? ?? ?? 84 C0 74 BC 8B 8F
            m_ViewPort, //48 8B 15 ?? ?? ?? ?? 48 8D 2D ?? ?? ?? ?? 48 8B CD
            m_BlipList, //4C 8D 05 ?? ?? ?? ?? 0F B7 C1
            m_CamGameplayDirector, //4C 8B 35 ?? ?? ?? ?? 33 FF 32 DB
            m_LocalPlayer;

        //Function Addrs
        uintptr_t m_SilentAim, //48 8D 45 ?? F3 0F 10 00 F3 0F 10 48 ?? F3 0F 11 45
            m_InfiniteAmmo0, //41 2B C9 3B C8 0F 4D C8
            m_InfiniteAmmo1, //41 2B D1 E8
            m_MagicBulletsPatch, //0F 29 4F ?? 83 8F ?? ?? ?? ?? ?? 48 8B 4F
            m_ArmsKinematics, //E8 ?? ?? ?? ?? 48 83 C3 60 48 FF CF 75 E6 48 8B 5C 24
            m_LegsKinematics, //E8 ?? ?? ?? ?? 48 83 C3 60 48 FF CF 75 DF 48 8B 5C 24 ?? 48 8B 6C 24 ?? 48 8B 74 24
            m_InfiniteCombatRoll, //89 81 ?? ?? ?? ?? 8B 87 ?? ?? ?? ?? F7 D0
            m_GiveWeapon; //48 89 5c 24 ?? 48 89 6c 24 ?? 48 89 74 24 ? 57 48 83 ec ?? 41 8b f0 8b fa 48 8b d9 e8

        //Offsets
        uintptr_t m_LastVehicle,
            m_Handling,
            m_VehicleEngineHealth,
            m_PlayerInfo,
            m_PedFactory,
            m_FragInst,
            m_Armor,
            m_Recoil,
            m_PedFlag,
            m_Spread,
            m_WeaponManager,
            m_currentPedWeapon,
            m_isCutscene,
            m_Setentityhealth,
            m_isCamActive,
            m_getRenderingCam,
            m_OffsetPool, //0F B7 CA 83 F9 07 7F 5E
            m_WeaponInfo,
            m_VehicleDoorsLockState,
            m_EntityType,
            m_Speed,
            m_MaxHealth,
            m_VehicleDriver,
            m_VehicleGravity,
            m_CObject,
            m_NoRagDoll,
            m_CWeapon,
            m_PlayerId,
            m_CitizenNamesModBase,
            m_NetIdToNamesEntry,
            m_SeatBealt,
            m_AimCPedPatternResult,
            m_VehicleState,
            m_VehicleExtras = 0x58B,
            m_VehicleSpawn,
            m_RocketSpeed,
            m_FrameFlag,
            m_Camera,
            m_HandleBullet,
            m_BulletSpeed,
            m_IsPedShooting,
            m_CameraX,
            SET_AMMO,
            GET_AMMO,
            player_pos_write_data,
            m_CameraY,
            m_CameraZ,
            m_CamFinalRotAddr,
            m_stealthPoint,
            g_OcclusionOffset,
            m_Invisible,
            m_funcRot,
            m_VisibleFlag = 0x145C,
            m_Occlusion,
            m_LastVisibleTime = 0x1A0,
            playerPosWriteData,
            foundAddr,
            m_VehicleFuelLevel,
            g_IsPedRagdollFuncAddr,
            PatternAddr,
            m_solo_session_start,
            m_IsCutscenePlaying,
            m_IsFreeAimingatPed,
            m_solo_session_end,
            m_GetGameplayCamRot,
            m_GetGameplayCamCoord,
            m_GetFinalRenderedCamRot,
            PatternAddrAlt,
            Addr4;

        static constexpr DWORD VISIBILITY_FLAG_OFFSET = 0x145C;

        uint32_t CurrentBuild;
        std::string ServerIp;
    };

    inline OffsetsClass g_Offsets;
}