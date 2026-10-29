#pragma once
#include <vector>
#include <cstdint>
#include <mutex>
#include <Utils/DirectXCompat.hpp>

namespace Core {
    namespace Features {

        class cCameraBypass {
        public:

            struct Camera {
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                float pitch = 0.0f;
                float yaw = 0.0f;
                float roll = 0.0f;
            };

            void HookCamera(const D3DXVECTOR3& targetPos, const D3DXVECTOR3& playerPos);
            void UnhookCamera();
            void ToggleCamera(bool enable, const D3DXVECTOR3& targetPos = {}, const D3DXVECTOR3& playerPos = {});

            Camera g_CameraData{};

        private:
            uintptr_t m_HookAddr = 0;
            uintptr_t m_CaveAddr = 0;
            std::vector<uint8_t> m_OriginalBytes;

            bool m_Hooked = false;
            std::mutex mtx;
        };

        inline cCameraBypass g_CameraBypass;

    }
}
