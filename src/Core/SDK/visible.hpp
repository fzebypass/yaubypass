#pragma once
#include <Includes/Includes.hpp>
#include <Core/SDK/Memory.hpp>
#include <Core/Offsets.hpp>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <deque>

namespace Core {
    namespace Features {

        namespace VischeckOffsets {
            constexpr uint32_t EntityFlags = 0xC0;
            constexpr uint32_t VisualPtr = 0x48;
            constexpr uint32_t OcclusionIndex = 0x2B;
            constexpr uint32_t bUseOcclusionQuery = 0x20000000;
            constexpr int EntrySize = 128;
            constexpr int PixelCountOffset = -8;
            constexpr int PixelThreshold = 100;
        }

        namespace BoneIds {
            constexpr unsigned int HEAD = 0x796e;
            constexpr unsigned int SPINE = 0x60f2;
            constexpr unsigned int NECK = 0x9995;
            constexpr unsigned int L_FOOT = 0xcc;
            constexpr unsigned int R_FOOT = 0x9000;
            constexpr unsigned int L_THIGH = 0xe39f;
            constexpr unsigned int R_THIGH = 0xca72;
            constexpr unsigned int PELVIS = 0x2e28;
        }

        class cVisibilityChecker {
        private:
            uintptr_t m_OcclusionQueryArray = 0;

            struct VisibilityEntry {
                bool isVisible;
                std::chrono::high_resolution_clock::time_point timestamp;
                std::deque<bool> recentResults;
                int consecutiveVisible;
                int consecutiveNotVisible;
            };

            std::unordered_map<uintptr_t, VisibilityEntry> m_VisibilityCache;
            std::unordered_map<uintptr_t, int> m_QueryEnableFrame;

            struct BoneVisEntry {
                bool isVisible;
                std::chrono::high_resolution_clock::time_point timestamp;
            };
            std::unordered_map<uintptr_t, std::unordered_map<unsigned int, BoneVisEntry>> m_BoneVisibilityCache;

            std::mutex m_CacheMutex;
            bool m_Initialized = false;
            int m_CurrentFrame = 0;

            static constexpr int CACHE_CLEANUP_INTERVAL = 60;
            static constexpr double CACHE_EXPIRE_MS = 0.0;
            static constexpr double BONE_CACHE_EXPIRE_MS = 30.0;
            static constexpr int SMOOTHING_SAMPLES = 1;
            static constexpr int VISIBILITY_THRESHOLD_VISIBLE = 1;
            static constexpr int VISIBILITY_THRESHOLD_NOT_VISIBLE = 1;
            int m_FramesSinceCleanup = 0;

        public:
            void Reset() {
                std::lock_guard<std::mutex> lock(m_CacheMutex);
                m_OcclusionQueryArray = 0;
                m_VisibilityCache.clear();
                m_QueryEnableFrame.clear();
                m_BoneVisibilityCache.clear();
                m_Initialized = false;
                m_CurrentFrame = 0;
                m_FramesSinceCleanup = 0;
            }

            bool Initialize() {
                if (m_Initialized) return true;

                std::vector<BYTE> pattern = { 0x48, 0x8D, 0x1D, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xC1 };
                uintptr_t patternAddr = Mem.FindSignature(pattern);

                if (!patternAddr) {
                    std::vector<BYTE> altPattern = { 0x48, 0x8D, 0x1D, 0x00, 0x00, 0x00, 0x00 };
                    patternAddr = Mem.FindSignature(altPattern);
                }

                if (patternAddr) {
                    int32_t offset = Mem.Read<int32_t>(patternAddr + 3);
                    m_OcclusionQueryArray = patternAddr + 3 + 4 + offset;
                    m_Initialized = true;
                    return true;
                }

                return false;
            }

            bool EnableOcclusionQuery(uintptr_t entityAddr) {
                if (!entityAddr) return false;

                uint32_t flags = Mem.Read<uint32_t>(entityAddr + VischeckOffsets::EntityFlags);
                if (!(flags & VischeckOffsets::bUseOcclusionQuery)) {
                    flags |= VischeckOffsets::bUseOcclusionQuery;
                    Mem.Write<uint32_t>(entityAddr + VischeckOffsets::EntityFlags, flags);
                }
                return true;
            }

            void UpdateFrame() {
                m_CurrentFrame++;

                m_FramesSinceCleanup++;
                if (m_FramesSinceCleanup >= CACHE_CLEANUP_INTERVAL) {
                    m_FramesSinceCleanup = 0;
                    CleanupOldEntries();
                }
            }

            void CleanupOldEntries() {
                auto now = std::chrono::high_resolution_clock::now();
                std::lock_guard<std::mutex> lock(m_CacheMutex);

                auto it = m_VisibilityCache.begin();
                while (it != m_VisibilityCache.end()) {
                    double ageMs = std::chrono::duration<double, std::milli>(now - it->second.timestamp).count();
                    if (ageMs > 500.0) {
                        it = m_VisibilityCache.erase(it);
                    }
                    else {
                        ++it;
                    }
                }

                auto boneIt = m_BoneVisibilityCache.begin();
                while (boneIt != m_BoneVisibilityCache.end()) {
                    auto& boneMap = boneIt->second;
                    auto innerIt = boneMap.begin();
                    while (innerIt != boneMap.end()) {
                        double ageMs = std::chrono::duration<double, std::milli>(now - innerIt->second.timestamp).count();
                        if (ageMs > 500.0) {
                            innerIt = boneMap.erase(innerIt);
                        }
                        else {
                            ++innerIt;
                        }
                    }
                    if (boneMap.empty()) {
                        boneIt = m_BoneVisibilityCache.erase(boneIt);
                    }
                    else {
                        ++boneIt;
                    }
                }

                auto qit = m_QueryEnableFrame.begin();
                while (qit != m_QueryEnableFrame.end()) {
                    if (m_CurrentFrame - qit->second > 30) {
                        qit = m_QueryEnableFrame.erase(qit);
                    }
                    else {
                        ++qit;
                    }
                }
            }

            bool IsEntityVisibleFast(uintptr_t entityAddr, float distance = 0.0f) {
                if (!m_Initialized || !entityAddr || !m_OcclusionQueryArray) {
                    return true;
                }

                auto now = std::chrono::high_resolution_clock::now();

                {
                    std::lock_guard<std::mutex> lock(m_CacheMutex);
                    auto it = m_VisibilityCache.find(entityAddr);
                    if (it != m_VisibilityCache.end()) {
                        double ageMs = std::chrono::duration<double, std::milli>(now - it->second.timestamp).count();
                        if (ageMs < CACHE_EXPIRE_MS) {
                            return it->second.isVisible;
                        }
                    }
                }

                EnableOcclusionQuery(entityAddr);
                int32_t pixelCount = CheckPixels(entityAddr);

                const float maxDistance = 500.0f;
                const float minThreshold = 5.0f;
                const float maxThreshold = 100.0f;

                float clampedDistance = (distance < 0.0f) ? 0.0f : (distance > maxDistance) ? maxDistance : distance;
                float distanceRatio = clampedDistance / maxDistance;
                float threshold = minThreshold + ((maxThreshold - minThreshold) * distanceRatio);

                bool isAiming1stPerson = false;
                if (g_Config.ESP->IgnoreLocalPed) {
                    if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0) {
                        isAiming1stPerson = true;
                    }
                }

                bool currentResult = false;
                if (isAiming1stPerson) {
                    threshold *= 0.3f;
                }
                else {
                    currentResult = (pixelCount > threshold);
                }


                {
                    std::lock_guard<std::mutex> lock(m_CacheMutex);
                    auto& entry = m_VisibilityCache[entityAddr];

                    entry.recentResults.push_back(currentResult);
                    if (entry.recentResults.size() > SMOOTHING_SAMPLES) {
                        entry.recentResults.pop_front();
                    }

                    if (currentResult) {
                        entry.consecutiveVisible++;
                        entry.consecutiveNotVisible = 0;
                    }
                    else {
                        entry.consecutiveNotVisible++;
                        entry.consecutiveVisible = 0;

                        if (pixelCount <= 1) {
                            entry.isVisible = false;
                            entry.recentResults.clear();
                            entry.timestamp = now;
                            return false;
                        }
                    }

                    bool smoothedResult = entry.isVisible;

                    if (entry.consecutiveNotVisible >= VISIBILITY_THRESHOLD_NOT_VISIBLE) {
                        smoothedResult = false;
                    }

                    else if (!entry.isVisible && entry.consecutiveVisible >= VISIBILITY_THRESHOLD_VISIBLE) {
                        smoothedResult = true;
                    }

                    else if (entry.isVisible && entry.consecutiveVisible > 0) {
                        smoothedResult = true;
                    }

                    entry.isVisible = smoothedResult;
                    entry.timestamp = now;

                    return smoothedResult;
                }
            }


            bool IsEntityVisible(uintptr_t entityAddr, float distance = 0.0f) {
                return IsEntityVisibleFast(entityAddr, distance);
            }


            bool IsBoneVisible(uintptr_t entityAddr, unsigned int boneId, const D3DXVECTOR3& boneWorldPos, float distance = 0.0f) {
                if (!m_Initialized || !entityAddr) {
                    return true;
                }

                auto now = std::chrono::high_resolution_clock::now();


                uintptr_t cacheKey = entityAddr ^ (static_cast<uintptr_t>(boneId) << 32);


                {
                    std::lock_guard<std::mutex> lock(m_CacheMutex);
                    auto entityIt = m_BoneVisibilityCache.find(entityAddr);
                    if (entityIt != m_BoneVisibilityCache.end()) {
                        auto boneIt = entityIt->second.find(boneId);
                        if (boneIt != entityIt->second.end()) {
                            double ageMs = std::chrono::duration<double, std::milli>(now - boneIt->second.timestamp).count();
                            if (ageMs < BONE_CACHE_EXPIRE_MS) {
                                return boneIt->second.isVisible;
                            }
                        }
                    }
                }


                bool entityVisible = IsEntityVisibleFast(entityAddr, distance);
                if (!entityVisible) {

                    std::lock_guard<std::mutex> lock(m_CacheMutex);
                    m_BoneVisibilityCache[entityAddr][boneId] = { false, now };
                    return false;
                }

                uintptr_t camDirector = Mem.Read<uintptr_t>(g_Offsets.m_CamGameplayDirector);
                if (camDirector == 0) {
                    std::lock_guard<std::mutex> lock(m_CacheMutex);
                    m_BoneVisibilityCache[entityAddr][boneId] = { true, now };
                    return true;
                }
                uintptr_t CamFollowPedCamera = Mem.Read<uintptr_t>(camDirector + 0x2C0);
                if (!CamFollowPedCamera) {
                    std::lock_guard<std::mutex> lock(m_CacheMutex);
                    m_BoneVisibilityCache[entityAddr][boneId] = { true, now };
                    return true;
                }

                D3DXVECTOR3 CamPos = Mem.Read<D3DXVECTOR3>(CamFollowPedCamera + 0x60);

                D3DXVECTOR3 dir = boneWorldPos - CamPos;
                float distToBone = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);

                if (distToBone < 0.5f) {
                    std::lock_guard<std::mutex> lock(m_CacheMutex);
                    m_BoneVisibilityCache[entityAddr][boneId] = { true, now };
                    return true;
                }

                uintptr_t visualPtr = Mem.Read<uintptr_t>(entityAddr + VischeckOffsets::VisualPtr);
                if (!visualPtr) {
                    std::lock_guard<std::mutex> lock(m_CacheMutex);
                    m_BoneVisibilityCache[entityAddr][boneId] = { true, now };
                    return true;
                }

                uint8_t occlusionIndex = Mem.Read<uint8_t>(visualPtr + VischeckOffsets::OcclusionIndex);

                uintptr_t pixelCountAddr1 = m_OcclusionQueryArray + (occlusionIndex * VischeckOffsets::EntrySize) - 8;
                int32_t pixelCount = Mem.Read<int32_t>(pixelCountAddr1);

                if (pixelCount < 0 || pixelCount > 100000) {
                    uintptr_t pixelCountAddr2 = m_OcclusionQueryArray + (occlusionIndex * VischeckOffsets::EntrySize) + 120;
                    pixelCount = Mem.Read<int32_t>(pixelCountAddr2);
                }

                float boneThreshold = 30.0f;
                if (boneId == BoneIds::HEAD || boneId == BoneIds::NECK) {
                    boneThreshold = 50.0f;
                }
                else if (boneId == BoneIds::SPINE) {
                    boneThreshold = 40.0f;
                }

                boneThreshold = boneThreshold * (1.0f - (distToBone / 200.0f) * 0.5f);
                if (boneThreshold < 10.0f) boneThreshold = 10.0f;

                bool boneVisible = (pixelCount > 0 && pixelCount > boneThreshold);

                {
                    std::lock_guard<std::mutex> lock(m_CacheMutex);
                    m_BoneVisibilityCache[entityAddr][boneId] = { boneVisible, now };
                }

                return boneVisible;
            }

            static unsigned int GetBoneIdFromHitbox(int hitboxType) {
                switch (hitboxType) {
                case 0: return BoneIds::HEAD;
                case 1: return BoneIds::SPINE;
                case 2: return (rand() % 2) ? BoneIds::L_FOOT : BoneIds::R_FOOT;
                default: return BoneIds::HEAD;
                }
            }

            bool GetCachedVisibility(uintptr_t entityAddr) {
                std::lock_guard<std::mutex> lock(m_CacheMutex);
                auto it = m_VisibilityCache.find(entityAddr);
                if (it != m_VisibilityCache.end()) {
                    return it->second.isVisible;
                }
                return true;
            }

            void ClearCache() {
                std::lock_guard<std::mutex> lock(m_CacheMutex);
                m_VisibilityCache.clear();
                m_QueryEnableFrame.clear();
                m_BoneVisibilityCache.clear();
            }


            bool IsInitialized() const { return m_Initialized; }

            int CheckPixels(uintptr_t entityAddr) {
                if (!m_Initialized || !entityAddr || !m_OcclusionQueryArray) return 0;

                uintptr_t visualPtr = Mem.Read<uintptr_t>(entityAddr + VischeckOffsets::VisualPtr);
                if (!visualPtr) return 0;

                uint8_t occlusionIndex = Mem.Read<uint8_t>(visualPtr + VischeckOffsets::OcclusionIndex);
                if (occlusionIndex < 1) return 0;

                uintptr_t pixelCountAddr1 = m_OcclusionQueryArray + (occlusionIndex * VischeckOffsets::EntrySize) - 8;
                uintptr_t pixelCountAddr2 = m_OcclusionQueryArray + (occlusionIndex * VischeckOffsets::EntrySize) + 120;

                int32_t pixelCount1 = Mem.Read<int32_t>(pixelCountAddr1);
                int32_t pixelCount2 = Mem.Read<int32_t>(pixelCountAddr2);

                if (pixelCount1 > 0 && pixelCount1 < 50000) return pixelCount1;
                if (pixelCount2 > 0 && pixelCount2 < 50000) return pixelCount2;

                return (pixelCount1 > pixelCount2) ? pixelCount1 : pixelCount2;
            }
        };

        inline cVisibilityChecker g_VisibilityChecker;

        inline uintptr_t resolveRelativeAddress(uintptr_t instr, int offsetOffset, int instrSize) {
            int32_t ripOffset = Mem.Read<int32_t>(instr + offsetOffset);
            return instr + instrSize + ripOffset;
        }
        inline uintptr_t ResolvePatternOffset(const std::string& pattern, int offsetPosition) {
            uintptr_t addr = Mem.FindSignatureStr(pattern);
            if (!addr) {
                return 0;
            }

            int32_t offset = Mem.Read<int32_t>(addr + offsetPosition);

            return offset;
        }
#pragma pack(push, 1)
        struct RaycastShellcode {
            unsigned char sub_rsp[4] = { 0x48, 0x83, 0xEC, 0x28 };
            unsigned char align_rsp[4] = { 0x48, 0x83, 0xE4, 0xF0 };

            unsigned char mov_rcx[2] = { 0x48, 0xB9 };
            uintptr_t local_player_addr;

            unsigned char mov_rdx[2] = { 0x48, 0xBA };
            uintptr_t player_addr;

            unsigned char mov_r8[2] = { 0x49, 0xB8 };
            uintptr_t xmword_addr;

            unsigned char mov_r9_1[7] = { 0x49, 0xC7, 0xC1, 0xFF, 0x01, 0x00, 0x00 };
            unsigned char mov_stack_arg5[8] = { 0xC7, 0x44, 0x24, 0x20, 0xFF, 0x01, 0x00, 0x00 };

            unsigned char mov_stack_arg6[8] = { 0xC7, 0x44, 0x24, 0x28, 0x07, 0x00, 0x00, 0x00 };

            unsigned char mov_rax_raycast[2] = { 0x48, 0xB8 };
            uintptr_t raycast_func_addr;
            unsigned char call_rax_raycast[2] = { 0xFF, 0xD0 };

            unsigned char mov_rcx_rax[3] = { 0x48, 0x89, 0xC1 };
            unsigned char mov_rax_exitthread[2] = { 0x48, 0xB8 };
            uintptr_t exit_thread_addr;
            unsigned char call_rax_exitthread[2] = { 0xFF, 0xD0 };
        };
#pragma pack(pop)

        inline bool isVisibleRaycast(CPed* player) {
            static uintptr_t codeCaveAddress = Mem.CreateCodeCave(1024);
            static uintptr_t xmword_raycast = Mem.FindSignatureStr(xorstr("0F 28 05 ? ? ? ? 88 44 24 ? 44 89 4C 24"), 0, 0);
            static uintptr_t raycast_function = Mem.FindSignatureStr(xorstr("48 8B C4 48 89 58 ? 48 89 70 ? 48 89 78 ? 55 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 0F 28 A2"), 0, 0);
            static uintptr_t xmword_raycast_absolute_address = xmword_raycast ? resolveRelativeAddress(xmword_raycast, 3, 7) : 0;
            static uintptr_t exitThreadAddress = (uintptr_t)GetProcAddress(GetModuleHandleA(xorstr("kernel32.dll")), xorstr("ExitThread"));

            if (!xmword_raycast || !raycast_function || !xmword_raycast_absolute_address || !codeCaveAddress || !exitThreadAddress) {
                return false;
            }

            RaycastShellcode shellcode;
            shellcode.local_player_addr = Mem.Read<uintptr_t>(ResolvePatternOffset(xorstr("48 8B 05 ? ? ? ? 8B 78 18 85 FF 74 58"), 3) + Mem.ModBase);
            if (!shellcode.local_player_addr) {
            }

            shellcode.player_addr = reinterpret_cast<uintptr_t>(player);
            shellcode.xmword_addr = xmword_raycast_absolute_address;
            shellcode.raycast_func_addr = raycast_function;
            shellcode.exit_thread_addr = exitThreadAddress;

            DWORD exitCode = Mem.execShellcode(codeCaveAddress, reinterpret_cast<const unsigned char*>(&shellcode), sizeof(shellcode));
            return exitCode != 0;
        }

        inline bool IsPlayerVisible(CPed* Current, CPed* LocalPlayer, int threshold = 100)
        {
            uintptr_t pointer = reinterpret_cast<uintptr_t>(Current);
            bool pixelVisible = g_VisibilityChecker.CheckPixels(pointer) > threshold;
            bool aiming = (g_Config.ESP->IgnoreLocalPed && (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);

            if (pixelVisible) return true;

            if (aiming && Current && LocalPlayer) {
                static uintptr_t codeCaveAddress = Mem.CreateCodeCave(1024);
                static uintptr_t xmword_raycast = Mem.FindSignatureStr(xorstr("0F 28 05 ? ? ? ? 88 44 24 ? 44 89 4C 24"), 0, 0);
                static uintptr_t raycast_function = Mem.FindSignatureStr(xorstr("48 8B C4 48 89 58 ? 48 89 70 ? 48 89 78 ? 55 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 0F 28 A2"), 0, 0);
                static uintptr_t xmword_raycast_absolute_address = xmword_raycast ? resolveRelativeAddress(xmword_raycast, 3, 7) : 0;
                static uintptr_t exitThreadAddress = (uintptr_t)GetProcAddress(GetModuleHandleA(xorstr("kernel32.dll")), xorstr("ExitThread"));

                if (raycast_function && codeCaveAddress && exitThreadAddress && xmword_raycast_absolute_address) {
                    RaycastShellcode shellcode;
                    shellcode.local_player_addr = reinterpret_cast<uintptr_t>(LocalPlayer);
                    shellcode.player_addr = reinterpret_cast<uintptr_t>(Current);
                    shellcode.xmword_addr = xmword_raycast_absolute_address;
                    shellcode.raycast_func_addr = raycast_function;
                    shellcode.exit_thread_addr = exitThreadAddress;

                    DWORD exitCode = Mem.execShellcode(codeCaveAddress, reinterpret_cast<const unsigned char*>(&shellcode), sizeof(shellcode));
                    return exitCode != 0;
                }
            }

            return false;
        }

    }
}