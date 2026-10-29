#pragma once
#define NOMINMAX

#include "Includes/Includes.hpp"
#include <core/Offsets.hpp>
#include <core/sdk/sdk.hpp>
#include <Windows.h>
#include <Core/SDK/SDK.hpp>
#include <mutex>
#include <vector>

namespace Core {
    namespace Features {

        class cSilentAim {
        public:
            static bool SilentAimInitialized;
            static uintptr_t StartAddy;
            static uintptr_t SilentAimHook;

            static std::vector<uint8_t> OriginalFuncTable;
            static std::vector<uint8_t> SilentAimShell;

            // Visual target for ESP / Highlight
            static std::mutex VisualTargetMutex;
            static D3DXVECTOR2 VisualTargetScreenPos;
            static bool HasVisualTarget;

            Core::SDK::Game::EntityStruct aim_target;

            std::vector<uint8_t> ReWriteTable = {
                0xF3, 0x41, 0x0F, 0x10, 0x19,
                0xF3, 0x41, 0x0F, 0x10, 0x41, 0x04,
                0xF3, 0x41, 0x0F, 0x10, 0x51, 0x08
            };

            std::vector<uint8_t> AngleReWriteTable = {
                0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
            };

            static bool HookedCamera;

            void RestoreSilent();
            void ApplySilent(D3DXVECTOR3 EndBulletPos, D3DXVECTOR3 localPos = {});

            void Start(uint64_t worker_epoch);

            static void SetVisualTarget(const D3DXVECTOR2& screenPos);
            static void ClearVisualTarget();
            static bool GetVisualTarget(D3DXVECTOR2& screenPos);

            static void SpoofCamera(bool enable);
            static void ResetProcessState();
            static D3DXVECTOR3 CalcRotation(D3DXVECTOR3 src, D3DXVECTOR3 dst);

        private:
            // Camera hook internals
            inline static uintptr_t g_GetFinalRenderedCamRotFuncAddr = 0;
            inline static uintptr_t g_GetFinalRenderedCamRotShellcodeAddr = 0;
            inline static std::vector<uint8_t> g_OriginalGetFinalRenderedCamRotBytes;
            inline static bool g_GetFinalRenderedCamRotHooked = false;

            inline static uintptr_t g_GetGameplayCamRotFuncAddr = 0;
            inline static uintptr_t g_GetGameplayCamRotShellcodeAddr = 0;
            inline static std::vector<uint8_t> g_OriginalGetGameplayCamRotBytes;
            inline static bool g_GetGameplayCamRotHooked = false;

            inline static uintptr_t g_GetEntityPlayerIsFreeAimingAtFuncAddr = 0;
            inline static uintptr_t g_GetEntityPlayerIsFreeAimingAtShellcodeAddr = 0;
            inline static std::vector<uint8_t> g_OriginalGetEntityPlayerIsFreeAimingAtBytes;
            inline static bool g_GetEntityPlayerIsFreeAimingAtHooked = false;

            static void CreateGetEntityPlayerIsFreeAimingAtHook();
            static bool InitializeGetEntityPlayerIsFreeAimingAtHook();

            static void CreateGetFinalRenderedCamRotHook();
            static void CreateGetGameplayCamRotHook();
            static bool InitalizeGetFinalRenderedCamRotHook();
            static bool InitalizeGetGameplayCamRotHook();

            static bool setfinalcamrot(float x, float y, float z);
            static bool setgameplaycamrot(float x, float y, float z);

            static void UnhookGetEntityPlayerIsFreeAimingAt();

            static void UnhookGetFinalRenderedCamRot();
            static void UnhookGetGameplayCamRot();
            static void UnhookAllCamRot();
        };

        inline cSilentAim g_SilentAim;

    }
}
