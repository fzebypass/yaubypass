#pragma once
#include <Includes/Includes.hpp>
#include <Core/SDK/Memory.hpp>
#include <Core/Offsets.hpp>
#include <Core/SDK/Structs/GameClasses.hpp>
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <unordered_map>
#include <mutex>

namespace Core {
    namespace SDK {
        namespace Pointers {
            // Object Pointers
            inline CPedFactory* pWorld = nullptr;
            inline CPed* pLocalPlayer = nullptr;
            inline CReplayInterFace* pReplayInterFace = nullptr;
            inline uintptr_t pBlipList = 0, pViewPort = 0, pCamGamePlayDirector = 0;
            inline uintptr_t pCWorld = 0;
        }

        namespace Game {
            struct cSkeleton_t {
                std::unordered_map<unsigned int, int> StoredBonesIdx;
                struct crSkeletonData_t {
                    uintptr_t Ptr, m_BoneIdTable;
                    unsigned int m_Used;
                    unsigned int m_NumBones;
                    unsigned short m_BoneIdTable_Slots;
                } crSkeletonData;
                uintptr_t m_pSkeleton, Arg2;
                D3DXMATRIX Arg1;
            };

            struct NetworkInfo {
                std::string UserName;
                std::string DiscordId;
                std::string SteamId;
            };

            struct EntityStruct {
                CPed* Ped;
                int Id;
                int Index;
                int PedType;
                bool IsVisible;
                bool IsVisible2;
                bool IsVisible3 = false;
                bool IsTargeting = false;
                D3DXVECTOR3 Pos;
                float Health = 0.0f;
                float MaxHealth = 0.0f;
                float Armor = 0.0f;
                float Distance = 0.0f;
                bool IsPlayer = false;
                bool IsFriend = false;
                bool IsGod = false;
                bool IsCheating;
                bool NoWeapon = false;
                std::string WeaponName;
                NetworkInfo NetworkInfo;
                float HealthAnim = 0.0f;
            };
            struct EspAnim {
                bool CanFadeOut;
                float Health;
                float Armor;
                float Alpha;
            };

            struct VehicleStructure {
                CVehicle* Pointer;
                CPed* Driver;
                std::string Name;
                float Dist;
                bool IsLocked;
                D3DXVECTOR3 Pos;

            };
            inline std::vector<EntityStruct> EntityList;
            inline std::vector<VehicleStructure> VehicleList;
            inline std::unordered_map<CPed*, bool> FriendMap;
            inline std::vector<CPed*> FriendList;
            inline std::mutex FriendMutex;
            inline std::mutex EntityMutex;
            inline std::mutex VehicleMutex;
            inline std::vector<std::string> PendingFriendRemovals;
            inline std::mutex PendingFriendMutex;

            inline std::vector<EntityStruct> GetEntityListSnapshot() {
                std::lock_guard<std::mutex> lock(EntityMutex);
                return EntityList;
            }

            inline const std::vector<EntityStruct>& GetEntityListSnapshotCached() {
                struct entity_snapshot_cache_t {
                    std::chrono::steady_clock::time_point updated_at{};
                    std::vector<EntityStruct> snapshot;
                    bool valid = false;
                };

                thread_local entity_snapshot_cache_t cache{};
                const auto now = std::chrono::steady_clock::now();
                if (!cache.valid || (now - cache.updated_at) >= std::chrono::milliseconds(8)) {
                    std::lock_guard<std::mutex> lock(EntityMutex);
                    cache.snapshot = EntityList;
                    cache.updated_at = now;
                    cache.valid = true;
                }

                return cache.snapshot;
            }

            inline D3DXMATRIX GetCachedViewMatrix() {
                struct view_matrix_cache_t {
                    uintptr_t viewport = 0;
                    std::chrono::steady_clock::time_point updated_at{};
                    D3DXMATRIX matrix{};
                    bool valid = false;
                };

                thread_local view_matrix_cache_t cache{};
                const uintptr_t viewport = Core::SDK::Pointers::pViewPort;
                if (!viewport) {
                    cache.valid = false;
                    return {};
                }

                const auto now = std::chrono::steady_clock::now();
                if (!cache.valid ||
                    cache.viewport != viewport ||
                    (now - cache.updated_at) >= std::chrono::milliseconds(4)) {
                    cache.viewport = viewport;
                    cache.matrix = Mem.Read<D3DXMATRIX>(viewport + 0x24C);
                    D3DXMatrixTranspose(&cache.matrix, &cache.matrix);
                    cache.updated_at = now;
                    cache.valid = true;
                }

                return cache.matrix;
            }

            inline D3DXVECTOR2 WorldToScreen(D3DXVECTOR3 World) {
                if (!Core::SDK::Pointers::pViewPort) {
                    return D3DXVECTOR2(0, 0);
                }

                const D3DXMATRIX ViewMatrix = GetCachedViewMatrix();
                auto VecX = D3DXVECTOR4(ViewMatrix._21, ViewMatrix._22, ViewMatrix._23, ViewMatrix._24),
                    VecY = D3DXVECTOR4(ViewMatrix._31, ViewMatrix._32, ViewMatrix._33, ViewMatrix._34),
                    VecZ = D3DXVECTOR4(ViewMatrix._41, ViewMatrix._42, ViewMatrix._43, ViewMatrix._44);
                D3DXVECTOR3 ScreenPos = D3DXVECTOR3(
                    (VecX.x * World.x) + (VecX.y * World.y) + (VecX.z * World.z) + VecX.w,
                    (VecY.x * World.x) + (VecY.y * World.y) + (VecY.z * World.z) + VecY.w,
                    (VecZ.x * World.x) + (VecZ.y * World.y) + (VecZ.z * World.z) + VecZ.w
                );
                if (ScreenPos.z <= 0.1f) return D3DXVECTOR2(0, 0);
                ScreenPos.z = 1.0f / ScreenPos.z;
                ScreenPos.x *= ScreenPos.z;
                ScreenPos.y *= ScreenPos.z;
                ScreenPos.x += (g_Variables.g_vGameWindowSize.x / 2) + float(0.5f * ScreenPos.x * g_Variables.g_vGameWindowSize.x + 0.5f);
                ScreenPos.y = (g_Variables.g_vGameWindowSize.y / 2) - float(0.5f * ScreenPos.y * g_Variables.g_vGameWindowSize.y + 0.5f);
                return D3DXVECTOR2(ScreenPos.x, ScreenPos.y);
            }

            inline std::unordered_map<uintptr_t, std::unordered_map<unsigned int, int>> StoredBonesIdx;
            inline std::mutex BonesCache;

            inline D3DXVECTOR3 GetMuzzlePos(uintptr_t pWeapon) {
                if (!pWeapon) return D3DXVECTOR3(0.0f, 0.0f, 0.0f);
                uintptr_t pWeaponSkeleton = Mem.Read<uintptr_t>(pWeapon + 0x30);
                if (!pWeaponSkeleton) return D3DXVECTOR3(0.0f, 0.0f, 0.0f);
                const uint16_t MUZZLE_BONE_ID = 0x49D9;
                uintptr_t boneArray = Mem.Read<uintptr_t>(pWeaponSkeleton + 0x8);
                if (!boneArray) return D3DXVECTOR3(0.0f, 0.0f, 0.0f);
                D3DXMATRIX boneMatrix = Mem.Read<D3DXMATRIX>(boneArray + MUZZLE_BONE_ID * sizeof(D3DXMATRIX));
                D3DXVECTOR3 muzzlePos(boneMatrix._41, boneMatrix._42, boneMatrix._43);
                return muzzlePos;
            }

            inline bool GetBoneIndex(cSkeleton_t cSkeleton, unsigned int boneId, int& outBoneIdx) {
                std::lock_guard<std::mutex> lock(BonesCache);
                {
                    auto& stored_bones_idx_cache = StoredBonesIdx[cSkeleton.crSkeletonData.Ptr];
                    if (stored_bones_idx_cache.find(boneId) != stored_bones_idx_cache.end()) {
                        outBoneIdx = stored_bones_idx_cache[boneId];
                        return true;
                    }
                    if (cSkeleton.crSkeletonData.m_Used != 0) {
                        unsigned short m_BoneIdTable_Slots = cSkeleton.crSkeletonData.m_BoneIdTable_Slots;
                        std::uintptr_t m_BoneIdTable_Hash = Mem.Read<std::uintptr_t>(cSkeleton.crSkeletonData.m_BoneIdTable + 0x8 * (boneId % (unsigned int)m_BoneIdTable_Slots));
                        for (std::uintptr_t i = m_BoneIdTable_Hash; i != 0; i = Mem.Read<std::uintptr_t>(i + 0x8)) {
                            int i_key = Mem.Read<int>(i);
                            if (boneId == i_key) {
                                int p_Data = Mem.Read<int>(i + 0x4);
                                if (p_Data) {
                                    outBoneIdx = p_Data;
                                    stored_bones_idx_cache[boneId] = p_Data;
                                    return true;
                                }
                            }
                        }
                    }
                    else if (boneId < cSkeleton.crSkeletonData.m_NumBones) {
                        outBoneIdx = boneId;
                        stored_bones_idx_cache[boneId] = boneId;
                        return true;
                    }
                    return false;
                }
            }

            inline D3DXVECTOR3 GetBonePosInstFrag(std::uintptr_t InstFrag, unsigned int BoneID, D3DXMATRIX Arg1, uintptr_t Arg2) {
                D3DXMATRIX v4;
                D3DXMATRIX Result;
                v4 = Arg1;
                if (!v4) {
                    return D3DXVECTOR3(0, 0, 0);
                }
                Result = Mem.Read<D3DXMATRIX>(Arg2 + ((unsigned __int64)BoneID << 6));
                if (!Result) {
                    return D3DXVECTOR3(0, 0, 0);
                }
                D3DXVECTOR3 vec1(v4._11, v4._12, v4._13);
                D3DXVECTOR3 vec2(v4._21, v4._22, v4._23);
                D3DXVECTOR3 vec3(v4._31, v4._32, v4._33);
                D3DXVECTOR3 vec4(v4._41, v4._42, v4._43);
                D3DXVECTOR3 vec5(Result._41, Result._42, Result._43);
                return D3DXVECTOR3(
                    vec1.x * vec5.x + vec4.x + vec2.x * vec5.y + vec3.x * vec5.z,
                    vec1.y * vec5.x + vec4.y + vec2.y * vec5.y + vec3.y * vec5.z,
                    vec1.z * vec5.x + vec4.z + vec2.z * vec5.y + vec3.z * vec5.z
                );
            }

           inline D3DXVECTOR3 getBonePosByInstFragAndID(uint64_t crSkeletonData, unsigned int BoneID) {
                uint64_t baseMatrixPtr = Mem.Read<uint64_t>(crSkeletonData + 0x8);
                uint64_t boneMatrixPtr = Mem.Read<uint64_t>(crSkeletonData + 0x18) + (BoneID << 6);

                D3DXMATRIX v4 = Mem.Read<D3DXMATRIX>(baseMatrixPtr);
                D3DXMATRIX Result = Mem.Read<D3DXMATRIX>(boneMatrixPtr);

                return D3DXVECTOR3(
                    v4._11 * Result._41 + v4._21 * Result._42 + v4._31 * Result._43 + v4._41,
                    v4._12 * Result._41 + v4._22 * Result._42 + v4._32 * Result._43 + v4._42,
                    v4._13 * Result._41 + v4._23 * Result._42 + v4._33 * Result._43 + v4._43
                );
            }

            inline D3DXVECTOR3 GetBonePosComplex(CPed* Ped, unsigned int Mask, cSkeleton_t cSkeleton_t) {
                bool result = false;
                int BoneId = 0;
                if (GetBoneIndex(cSkeleton_t, Mask, BoneId)) {
                    return GetBonePosInstFrag(cSkeleton_t.m_pSkeleton, BoneId, cSkeleton_t.Arg1, cSkeleton_t.Arg2);
                }
                return Ped->GetPos();
            }

            inline std::string GetPedName(CPed* Ped) {
                const auto& entities = GetEntityListSnapshotCached();
                for (const auto& Ent : entities) {
                    if (Ent.Ped != Ped) continue;
                    return Ent.NetworkInfo.UserName;
                }
                return std::string();
            }
            inline cSkeleton_t GetPedSkeleton(CPed* ped)
            {
                cSkeleton_t skeleton{};

                if (!ped) return skeleton;

                uintptr_t fragInst = Mem.Read<uintptr_t>((uintptr_t)ped + 0x30);
                if (!fragInst) return skeleton;

                uintptr_t skeletonRoot = Mem.Read<uintptr_t>(fragInst + 0x68);
                if (!skeletonRoot) return skeleton;

                skeleton.m_pSkeleton = Mem.Read<uintptr_t>(skeletonRoot + 0x178);
                if (!skeleton.m_pSkeleton) return skeleton;

                skeleton.crSkeletonData.Ptr = Mem.Read<uintptr_t>(skeleton.m_pSkeleton);

                skeleton.crSkeletonData.m_BoneIdTable_Slots = Mem.Read<unsigned short>(skeleton.crSkeletonData.Ptr + 0x18);
                skeleton.crSkeletonData.m_NumBones = Mem.Read<unsigned int>(skeleton.crSkeletonData.Ptr + 0x1A);
                skeleton.crSkeletonData.m_Used = 0;

                if (!skeleton.crSkeletonData.m_BoneIdTable_Slots)
                    return skeleton;

                skeleton.crSkeletonData.m_BoneIdTable = Mem.Read<uintptr_t>(skeleton.crSkeletonData.Ptr + 0x10);

                uintptr_t matrixPtr = Mem.Read<uintptr_t>(skeleton.m_pSkeleton + 0x8);

                skeleton.Arg1 = Mem.Read<D3DXMATRIX>(matrixPtr);
                skeleton.Arg2 = Mem.Read<uintptr_t>(skeleton.m_pSkeleton + 0x18);

                return skeleton;
            }
            inline uintptr_t resolveRelativeAddress(uintptr_t instr, int offsetOffset, int instrSize) {
                int32_t ripOffset = Mem.Read<int32_t>(instr + offsetOffset);
                return instr + instrSize + ripOffset;
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
                unsigned char mov_r9_1[7] = { 0x49, 0xC7, 0xC1, 0x01, 0x00, 0x00, 0x00 };
                unsigned char mov_stack_arg5[8] = { 0xC7, 0x44, 0x24, 0x20, 0x04, 0x20, 0x00, 0x00 };
                unsigned char mov_stack_arg6[8] = { 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00 };
                unsigned char mov_rax_raycast[2] = { 0x48, 0xB8 };
                uintptr_t raycast_func_addr;
                unsigned char call_rax_raycast[2] = { 0xFF, 0xD0 };
                unsigned char mov_rcx_rax[3] = { 0x48, 0x89, 0xC1 };
                unsigned char mov_rax_exitthread[2] = { 0x48, 0xB8 };
                uintptr_t exit_thread_addr;
                unsigned char call_rax_exitthread[2] = { 0xFF, 0xD0 };
            };
#pragma pack(pop)
            inline bool isVisible(uintptr_t codeCaveAddress, CPed* localPlayer, CPed* targetPlayer) {
                static uintptr_t xmword_raycast = Mem.FindSignatureStr(xorstr("0F 28 05 ? ? ? ? 88 44 24 ? 44 89 4C 24"), 0, 0);
                static uintptr_t raycast_function = Mem.FindSignatureStr(xorstr("48 8B C4 48 89 58 ? 48 89 70 ? 48 89 78 ? 55 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 0F 28 A2"), 0, 0);
                static uintptr_t xmword_raycast_absolute_address = resolveRelativeAddress(xmword_raycast, 3, 7);
                static uintptr_t exitThreadAddress = (uintptr_t)GetProcAddress(GetModuleHandleA(xorstr("kernel32.dll")), xorstr("ExitThread"));
                if (!xmword_raycast || !raycast_function || !xmword_raycast_absolute_address || !codeCaveAddress || !exitThreadAddress) {
                    return false;
                }
                RaycastShellcode shellcode;
                shellcode.local_player_addr = reinterpret_cast<uintptr_t>(localPlayer);
                shellcode.player_addr = reinterpret_cast<uintptr_t>(targetPlayer);
                shellcode.xmword_addr = xmword_raycast_absolute_address;
                shellcode.raycast_func_addr = raycast_function;
                shellcode.exit_thread_addr = exitThreadAddress;
                DWORD exitCode = Mem.execShellcode(codeCaveAddress, reinterpret_cast<const unsigned char*>(&shellcode), sizeof(shellcode));
                return exitCode != 0;
            }
            inline float GetDistanceToPed(CPed* Ped)
            {
                if (!Ped || !Core::SDK::Pointers::pLocalPlayer) return FLT_MAX;

                const auto& entities = GetEntityListSnapshotCached();
                for (const auto& Entity : entities)
                {
                    if (Entity.Ped == Ped)
                    {
                        return Entity.Distance;
                    }
                }

                D3DXVECTOR3 localPos = Core::SDK::Pointers::pLocalPlayer->GetPos();
                D3DXVECTOR3 pedPos = Ped->GetPos();

                D3DXVECTOR3 delta = pedPos - localPos;
                return D3DXVec3Length(&delta);
            }
            inline bool IsValidPed(CPed* Ped)
            {
                if (!Ped) return false;

                try {
                    DWORD test = Mem.Read<DWORD>((uintptr_t)Ped);
                }
                catch (...) {
                    return false;
                }

                if (Ped == Core::SDK::Pointers::pLocalPlayer) return false;

                const auto& entities = GetEntityListSnapshotCached();
                for (const auto& Entity : entities)
                {
                    if (Entity.Ped == Ped)
                    {
                        return (Entity.Health > 0.0f && Entity.Health <= 200.0f);
                    }
                }

                return false;
            }
            inline bool HasObstacleBetween(D3DXVECTOR3 start, D3DXVECTOR3 end) {
                return false;
            }
            inline CPed* GetClosestPed(int MaxDistance, bool IgnoreNpc, bool IgnoreDead, bool VisibleCheck, bool IngoreFriend,bool IgnoreGod) {
                CPed* ClosestPed = nullptr;
                float ClosestDistSq = FLT_MAX;
                const auto& entities = GetEntityListSnapshotCached();
                const float centerX = g_Variables.g_vGameWindowCenter.x;
                const float centerY = g_Variables.g_vGameWindowCenter.y;
                for (const auto& Entity : entities) {
                    CPed* Ped = Entity.Ped;
                    if (Ped == Core::SDK::Pointers::pLocalPlayer) continue;
                    if (IgnoreNpc && !Entity.IsPlayer) continue;
                    if (IngoreFriend && Entity.IsFriend) continue;
                    if (Entity.Distance > MaxDistance) continue;
                    if (IgnoreDead && std::abs(Entity.Health) <= 101.f) continue;
                    if (VisibleCheck && !Entity.IsVisible || VisibleCheck && Entity.IsVisible2)
                        continue;
                    //if (IgnoreGod && Entity.IsGod)
                    //    continue;
                    
                    D3DXVECTOR2 PedPos = Core::SDK::Game::WorldToScreen(Entity.Pos);
                    if (PedPos == D3DXVECTOR2(0, 0)) continue;
                    float x = PedPos.x - centerX;
                    float y = PedPos.y - centerY;
                    float AimDistSq = (x * x) + (y * y);
                    if (AimDistSq < ClosestDistSq) {
                        ClosestDistSq = AimDistSq;
                        ClosestPed = Ped;
                    }
                }
                return ClosestPed;
            }

            inline bool GetVehBoneIndex(cSkeleton_t& cSkeleton, unsigned int boneId, int& outBoneIdx) {
                auto& cache = StoredBonesIdx[cSkeleton.crSkeletonData.Ptr];

                auto it = cache.find(boneId);
                if (it != cache.end()) {
                    outBoneIdx = it->second;
                    return true;
                }

                unsigned short slots = cSkeleton.crSkeletonData.m_BoneIdTable_Slots;
                if (slots == 0) {
                    outBoneIdx = -1;
                    return true;
                }

                std::uintptr_t hash = Mem.Read<std::uintptr_t>(cSkeleton.crSkeletonData.m_BoneIdTable + 0x8 * (boneId % slots));

                for (std::uintptr_t i = hash; i != 0; i = Mem.Read<std::uintptr_t>(i + 0x8)) {
                    int key = Mem.Read<int>(i);

                    if (boneId == static_cast<unsigned int>(key)) {
                        int data = Mem.Read<int>(i + 0x4);

                        if (data) {
                            outBoneIdx = data;
                            cache[boneId] = data;
                            return true;
                        }
                    }
                }
                outBoneIdx = -1;
                return false;
            }

            inline cSkeleton_t createSkeleton(uintptr_t vehicle) {
                const uintptr_t fragInstNMGta = Mem.Read<uintptr_t>(vehicle + 0x30);
                const uintptr_t skeletonRoot = Mem.Read<uintptr_t>(fragInstNMGta + 0x68);

                cSkeleton_t skeleton;
                if (!skeletonRoot) return skeleton;

                skeleton.m_pSkeleton = Mem.Read<uintptr_t>(skeletonRoot + 0x178);
                skeleton.crSkeletonData.Ptr = Mem.Read<uintptr_t>(skeleton.m_pSkeleton);

                skeleton.crSkeletonData.m_BoneIdTable_Slots = Mem.Read<unsigned short>(skeleton.crSkeletonData.Ptr + 0x18);
                skeleton.crSkeletonData.m_NumBones = Mem.Read<unsigned int>(skeleton.crSkeletonData.Ptr + 0x1A);
                skeleton.crSkeletonData.m_Used = 0;

                if (!skeleton.crSkeletonData.m_BoneIdTable_Slots) return skeleton;

                skeleton.crSkeletonData.m_BoneIdTable = Mem.Read<uintptr_t>(skeleton.crSkeletonData.Ptr + 0x10);

                const uintptr_t matrixPtr = Mem.Read<uintptr_t>(skeleton.m_pSkeleton + 0x8);

                skeleton.Arg1 = Mem.Read<D3DXMATRIX>(matrixPtr);
                skeleton.Arg2 = Mem.Read<uintptr_t>(skeleton.m_pSkeleton + 0x18);

                return skeleton;
            }

            inline static unsigned atHash_const_charU(const char* s) {
                unsigned h = 0, g;
                while (*s) {
                    char c = *s++;
                    if (c >= 'a' && c <= 'z')
                        c -= 0x20;

                    h = (h << 4) + c;
                    if (((g = h & 0xf0000000)) != 0) {
                        h = h ^ (g >> 24);
                        h = h ^ g;
                    }
                }
                return h;
            }

            inline static unsigned short atHash16U(const char* s) {
                uint32_t hash = atHash_const_charU(s);
                uint32_t prime = 65167;
                return uint16_t((hash % prime) + (0xffff - prime));
            }

            inline std::vector<D3DXVECTOR3> getVehicleWheels(uintptr_t vehicle) {
                std::vector<D3DXVECTOR3> wheels;

                auto skeleton = createSkeleton(vehicle);
                if (!skeleton.m_pSkeleton || skeleton.crSkeletonData.m_NumBones == 0) return wheels;

                static const std::vector<const char*> allWheelBones = {
                    "wheel_lf", "wheel_rf", "wheel_lr", "wheel_rr", "wheel_f",  "wheel_r",
                    "wheel_lm1", "wheel_rm1", "wheel_lm2", "wheel_rm2", "wheel_lm3", "wheel_rm3",
                    "gear_f", "gear_rl", "gear_rr", "gear_lm1", "gear_rm1",
                };

                D3DXVECTOR3 vehiclePos = Mem.Read<D3DXVECTOR3>(vehicle + 0x90);

                for (const char* boneName : allWheelBones) {
                    int outIndex = -1;

                    GetVehBoneIndex(skeleton, atHash16U(boneName), outIndex);

                    if (outIndex <= 0 || outIndex >= skeleton.crSkeletonData.m_NumBones) continue;

                    D3DXVECTOR3 bonePos = getBonePosByInstFragAndID(skeleton.m_pSkeleton, outIndex);

                    if (std::isnan(bonePos.x) || std::isnan(bonePos.y) || std::isnan(bonePos.z)) continue;

                    float distFromCenter = sqrt(pow(bonePos.x - vehiclePos.x, 2) + pow(bonePos.y - vehiclePos.y, 2));
                    if (distFromCenter < 0.1f) continue;

                    wheels.push_back(bonePos);
                }

                return wheels;
            }

            inline float getWheelRadius(uintptr_t vehicle) {
                const uintptr_t modelInfo = Mem.Read<uintptr_t>(vehicle + 0x20);
                if (!modelInfo) return 0.35f;

                const float radius = Mem.Read<float>(modelInfo + 0x488);
                if (radius < 0.1f || radius > 1.0f) { return 0.35f; }

                return radius;
            }

            inline bool IsOnScreen(D3DXVECTOR2 Pos) {
                if (Pos == D3DXVECTOR2(0, 0)) return false;
                return true;
            }
            inline D3DXVECTOR3 bestWheel(int maxDistance, bool ignoreUnoccupied) {
                D3DXVECTOR3 bestWheelPos{};
                float closestCrossDist = FLT_MAX;

                std::lock_guard<std::mutex> lock(VehicleMutex);
                for (const auto& vehData : VehicleList) {
                    if (!vehData.Pointer) continue;
                    if (vehData.Dist > maxDistance) continue;
                    if (!vehData.Driver && ignoreUnoccupied) continue;

                    const auto wheels = getVehicleWheels(reinterpret_cast<uintptr_t>(vehData.Pointer));
                    const float radius = getWheelRadius(reinterpret_cast<uintptr_t>(vehData.Pointer));
                    const D3DXVECTOR3 vehiclePos = vehData.Pos;

                    for (const auto& wheelCenter : wheels) {
                        D3DXVECTOR3 dir(wheelCenter.x - vehiclePos.x, wheelCenter.y - vehiclePos.y, wheelCenter.z - vehiclePos.z);

                        const float len = std::sqrt((dir.x * dir.x) + (dir.y * dir.y) + (dir.z * dir.z));
                        if (len <= 0.0f) continue;

                        dir.x /= len;
                        dir.y /= len;
                        dir.z /= len;

                        const D3DXVECTOR3 wheelEdge(wheelCenter.x + dir.x * radius, wheelCenter.y + dir.y * radius, wheelCenter.z + dir.z * radius);

                        const D3DXVECTOR2 screen = WorldToScreen(wheelEdge);
                        if (!IsOnScreen(screen)) continue;

                        const float deltaX = screen.x - g_Variables.g_vGameWindowCenter.x;
                        const float deltaY = screen.y - g_Variables.g_vGameWindowCenter.y;
                        const float crossDist = std::sqrt(deltaX * deltaX + deltaY * deltaY);

                        if (crossDist < closestCrossDist) {
                            closestCrossDist = crossDist;
                            bestWheelPos = wheelEdge;
                        }
                    }
                }

                return bestWheelPos;
            }

            inline void RenderSpectatorList()
            {
                if (!g_Config.General->SpectatorList)
                    return;

                auto draw = ImGui::GetBackgroundDrawList();
                ImFont* font = ImGui::GetFont();

                ImVec2 windowSize = ImGui::GetIO().DisplaySize;
                float lineHeight = 16.0f;

                std::lock_guard<std::mutex> lock(EntityMutex);

                ImVec2 pos = ImVec2(windowSize.x - 220.0f, 40.0f);

                auto DrawTextOutlined = [&](const ImVec2& p, const ImU32 color, const ImU32 outlineColor, const char* text, float fontSize) {
                    float offset = 1.0f;

                    draw->AddText(font, fontSize, ImVec2(p.x - offset, p.y), outlineColor, text);
                    draw->AddText(font, fontSize, ImVec2(p.x + offset, p.y), outlineColor, text);
                    draw->AddText(font, fontSize, ImVec2(p.x, p.y - offset), outlineColor, text);
                    draw->AddText(font, fontSize, ImVec2(p.x, p.y + offset), outlineColor, text);

                    draw->AddText(font, fontSize, p, color, text);
                    };

                DrawTextOutlined(pos, IM_COL32(255, 255, 255, 255), IM_COL32(0, 0, 0, 255), "Spectator List", 15.0f);
                pos.y += lineHeight;

                if (!Pointers::pLocalPlayer)
                    return;

                D3DXVECTOR3 localPos = Pointers::pLocalPlayer->GetPos();
                const float horizontalTolerance = 5.0f;
                const float verticalTolerance = 5.0f;

                for (const auto& entity : EntityList)
                {
                    if (!entity.Ped)
                        continue;
                    if (entity.IsVisible)
                        continue;
                    if (entity.NetworkInfo.UserName == "NPC") continue;

                    D3DXVECTOR3 pedPos = entity.Ped->GetPos();

                    const float minZDifference = 10.0f;

                    if (std::fabs(pedPos.x - localPos.x) > horizontalTolerance)
                        continue;

                    if (std::fabs(pedPos.z - localPos.z) < minZDifference)
                        continue;

                    if (std::fabs(pedPos.y - localPos.y) < 0.01f && std::fabs(pedPos.z - localPos.z) < 0.01f)
                        continue;

                    std::string name = GetPedName(entity.Ped);
                    if (name.empty()) name = "Unknown";

                    float distance = entity.Distance;
                    if (distance <= 0.0f)
                    {
                        const auto dx = pedPos.x - localPos.x;
                        const auto dy = pedPos.y - localPos.y;
                        const auto dz = pedPos.z - localPos.z;
                        distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                    }

                    char buf[128];
                    std::snprintf(buf, sizeof(buf), "%s - %.1fm", name.c_str(), distance);

                    DrawTextOutlined(pos, IM_COL32(255, 255, 255, 255), IM_COL32(0, 0, 0, 255), buf, 14.0f);
                    pos.y += lineHeight;

                    if (pos.y > windowSize.y - 20.0f) break;
                }
            }

            inline static uintptr_t g_IsPedRagdollFuncAddr = 0;
            inline static uintptr_t g_IsPedRagdollShellcodeAddr = 0;
            inline static std::vector<uint8_t> g_OriginalIsPedRagdollBytes;
            inline static bool g_IsPedRagdollHooked = false;

            inline static uintptr_t g_EntityDamagedFuncAddr = 0;
            inline static uintptr_t g_EntityDamagedShellcodeAddr = 0;
            inline static std::vector<uint8_t> g_OriginalEntityDamagedBytes;
            inline static bool g_EntityDamagedHooked = false;

            inline static uintptr_t g_patched_addr2 = 0;
            inline static uintptr_t g_patched_addr4 = 0;
            inline static uintptr_t g_patched_IsPedShooting = 0;
            inline static std::vector<uint8_t> orig_addr2;
            inline static std::vector<uint8_t> orig_addr4;
            inline static std::vector<uint8_t> orig_IsPedShooting;

            inline uintptr_t AllocateNearbyMemory(uintptr_t targetAddr, size_t size) {
                uintptr_t allocationBase = targetAddr & 0xFFFFFFFFFFF00000;
                uintptr_t minAddr = (allocationBase > 0x7FFFFF00) ? allocationBase - 0x7FFFFF00 : 0;
                uintptr_t maxAddr = allocationBase + 0x7FFFFF00;
                MEMORY_BASIC_INFORMATION mbi;

                for (uintptr_t addr = allocationBase; addr < maxAddr; addr += 0x10000) {
                    if (VirtualQueryEx(Mem.AttachedProcessHandle, (LPCVOID)addr, &mbi, sizeof(mbi))) {
                        if (mbi.State == MEM_FREE) {
                            uintptr_t allocated = (uintptr_t)VirtualAllocEx(Mem.AttachedProcessHandle, (LPVOID)addr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                            if (allocated) return allocated;
                        }
                    }
                }
                for (uintptr_t addr = allocationBase; addr > minAddr; addr -= 0x10000) {
                    if (VirtualQueryEx(Mem.AttachedProcessHandle, (LPCVOID)addr, &mbi, sizeof(mbi))) {
                        if (mbi.State == MEM_FREE) {
                            uintptr_t allocated = (uintptr_t)VirtualAllocEx(Mem.AttachedProcessHandle, (LPVOID)addr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                            if (allocated) return allocated;
                        }
                    }
                }
                return 0;
            }

            inline void DisableAllFiringEvents()
            {
                uintptr_t addr4 = Mem.FindSignatureStr("FF 57 08 48 8B 4D ?? 33 FF 48 85 C9 74 ?? 48 8D 55 ?? E8 ?? ?? ?? ?? 48 8B"); // CEventGunShotBulletImpact
                if (addr4)
                {
                    orig_addr4 = Mem.ReadBytes(addr4, 3);
                    Mem.WriteBytes(addr4, { 0x90, 0x90, 0x90 });
                }
            }


            static void CreateEntityDamagedHook() {
                if (g_EntityDamagedShellcodeAddr) {
                    return;
                }

                std::vector<uint8_t> buffer = Mem.ReadBytes(g_EntityDamagedFuncAddr, 5);

                uintptr_t fivemTarget = g_EntityDamagedFuncAddr + 5;
                if (buffer.size() >= 5 && buffer[0] == 0xE9) {
                    int32_t offset = *(int32_t*)(&buffer[1]);
                    fivemTarget = g_EntityDamagedFuncAddr + 5 + offset;
                }

                std::vector<uint8_t> shellcode = {
                    0x48, 0x89, 0xCA,
                    0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0xFF, 0xE0
                };

                *(uintptr_t*)(&shellcode[5]) = fivemTarget;

                uintptr_t shellcodeAddr = AllocateNearbyMemory(g_EntityDamagedFuncAddr, shellcode.size() + 32);
                if (!shellcodeAddr) {
                    return;
                }

                if (Mem.WriteProcessMemoryImpl(shellcodeAddr, shellcode.data(), shellcode.size())) {
                    g_EntityDamagedShellcodeAddr = shellcodeAddr;
                }
            }
            inline bool InitalizeEntityDamagedHook() {
                if (g_EntityDamagedHooked) return true;

                // Znajdź pattern
                uintptr_t foundAddr = Mem.FindSignatureStr(xorstr("21 4D D8 21 4D DC 41 8B D8"));
                if (!foundAddr) {
                    return false;
                }

                // Odejmij 0x1F żeby trafić na początek funkcji
                g_EntityDamagedFuncAddr = foundAddr - 0x1F;

                // Zapisz oryginalne bajty (5 bajtów dla JMP)
                g_OriginalEntityDamagedBytes = Mem.ReadBytes(g_EntityDamagedFuncAddr, 5);

                // Utwórz hook
                CreateEntityDamagedHook();

                if (!g_EntityDamagedShellcodeAddr) {
                    return false;
                }

                // Oblicz offset dla JMP
                int64_t offset = (int64_t)g_EntityDamagedShellcodeAddr - (int64_t)g_EntityDamagedFuncAddr - 5;
                if (offset > 2147483647 || offset < -2147483648) {
                    return false;
                }

                // Wstaw JMP do naszego shellcode (używając WriteProcessMemoryImpl)
                uint8_t hook[5];
                hook[0] = 0xE9;
                *(int32_t*)(&hook[1]) = (int32_t)offset;

                DWORD oldProtect;
                VirtualProtectEx(Mem.AttachedProcessHandle, (LPVOID)g_EntityDamagedFuncAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
                Mem.WriteProcessMemoryImpl(g_EntityDamagedFuncAddr, hook, 5);
                VirtualProtectEx(Mem.AttachedProcessHandle, (LPVOID)g_EntityDamagedFuncAddr, 5, oldProtect, &oldProtect);

                FlushInstructionCache(GetCurrentProcess(), (LPCVOID)g_EntityDamagedFuncAddr, 16);
                g_EntityDamagedHooked = true;
                return true;
            }
            static void CreateIsPedRagdollHook() {
                if (g_IsPedRagdollShellcodeAddr) {
                    return;
                }

                std::vector<uint8_t> shellcode = {
                    0x48, 0x83, 0xEC, 0x28,              // sub rsp, 0x28
                    0x50, 0x51,                          // push rax, push rcx
                    0x48, 0x8B, 0x01,                   // mov rax, [rcx]
                    0xF3, 0x0F, 0x10, 0x05,             // movss xmm0, [rip+imm32]
                    0x00, 0x00, 0x00, 0x00,             // [RIP OFFSET]
                    0xF3, 0x0F, 0x11, 0x00,            // movss [rax], xmm0
                    0x59, 0x58,                        // pop rcx, pop rax
                    0x48, 0x83, 0xC4, 0x28,            // add rsp, 0x28
                    0x28, 0xC3,                        // sub bl, al
                    0xC3,                              // ret
                    0x00, 0x00, 0x80, 0x3F            // float 1.0
                };

                uintptr_t shellcodeAddr = Mem.CreateCodeCave(shellcode.size() + 20);
                if (!shellcodeAddr) {
                    return;
                }

                uintptr_t returnValueAddr = shellcodeAddr + shellcode.size() / 2 + 10;


                int32_t ripOffset = static_cast<int32_t>(returnValueAddr - (shellcodeAddr + 0x11));
                memcpy(&shellcode[0x0D], &ripOffset, 4);

                if (Mem.WriteBytes(shellcodeAddr, shellcode)) {
                    Mem.Write<float>(returnValueAddr, 1.0f);
                    g_IsPedRagdollShellcodeAddr = shellcodeAddr;
                }
            }

            static bool InitializeIsPedRagdollHook() {
                if (g_IsPedRagdollHooked = true)
                    return true;

                g_IsPedRagdollFuncAddr = Mem.FindSignatureStr(xorstr(
                    "48 8D 64 24 F8 48 89 2C 24 48 8D 2D ?? ?? ?? ?? 48 87 2C 24 48 8D 64 24 08 FF 64 24 F8 50 51 48 8B 44 24 10 48 B9 ?? ?? ?? ?? ?? ?? ?? ?? E9 ?? ?? ?? ?? 48 ?? ?? 89 01"
                ));

                if (!g_IsPedRagdollFuncAddr && !g_IsPedRagdollHooked) {
                    return false;
                }

                g_OriginalIsPedRagdollBytes = Mem.ReadBytes(g_IsPedRagdollFuncAddr, 32);

                CreateIsPedRagdollHook();
                if (!g_IsPedRagdollShellcodeAddr) {
                    return false;
                }

                if (!Mem.HookJMP(g_IsPedRagdollFuncAddr, g_IsPedRagdollShellcodeAddr)) {
                    return false;
                }

                FlushInstructionCache(GetCurrentProcess(), (LPCVOID)g_IsPedRagdollFuncAddr, 16);
                g_IsPedRagdollHooked = true;

                return true;
            }

            static void UnhookIsPedRagdoll() {
                if (!g_IsPedRagdollHooked || !g_IsPedRagdollFuncAddr) return;

                if (Mem.WriteBytes(g_IsPedRagdollFuncAddr, g_OriginalIsPedRagdollBytes)) {
                    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)g_IsPedRagdollFuncAddr, g_OriginalIsPedRagdollBytes.size());
                    g_IsPedRagdollHooked = false;
                }
            }

            inline void EnableAllFiringEvents()
            {
                if (g_patched_addr2) {
                    Mem.WriteBytes(g_patched_addr2, orig_addr2);
                    g_patched_addr2 = 0;
                }

                if (g_patched_addr4) {
                    Mem.WriteBytes(g_patched_addr4, orig_addr4);
                    g_patched_addr4 = 0;
                }

                if (g_patched_IsPedShooting) {
                    Mem.WriteBytes(g_patched_IsPedShooting, orig_IsPedShooting);
                    g_patched_IsPedShooting = 0;
                }
            }

            inline void SpoofCamera(bool enable)
            {
                static bool g_bCameraSpoofActive = false;
                static bool HookedCamera = false;
                static std::mutex cameraMutex;
                std::lock_guard<std::mutex> lock(cameraMutex);

                if (enable)
                {
                    if (!HookedCamera)
                    {
                        try {
                            DisableAllFiringEvents();
                            HookedCamera = true;
                        }
                        catch (...) {
                            HookedCamera = false;
                            return;
                        }
                    }
                    g_bCameraSpoofActive = true;
                }
                else
                {
                    if (HookedCamera)
                    {
                        try {
                            HookedCamera = false;
                        }
                        catch (...) {
                            HookedCamera = false;
                        }
                    }

                    g_bCameraSpoofActive = false;


                }
            }
            inline D3DXVECTOR3 AplPredict(uintptr_t pcea, float dis, const D3DXVECTOR3& target, uintptr_t wepI)
            {
                D3DXVECTOR3 aeaef = { 0.0f, 0.0f, 0.0f };
                CPed* local = Core::SDK::Pointers::pLocalPlayer;
                const auto lks = (g_Offsets.CurrentBuild >= 3095) ? 0x13C0 : (g_Offsets.CurrentBuild >= 2802) ? 0x1370 : (g_Offsets.CurrentBuild >= 2060) ? 0x1390 : 0x1350;
                const auto vhm = (g_Offsets.CurrentBuild >= 2802) ? 0x0D10 : (g_Offsets.CurrentBuild >= 2060) ? 0x0D30 : (g_Offsets.CurrentBuild >= 1604) ? 0x0D28 : 0x0D28;
                const auto tsk = (g_Offsets.CurrentBuild >= 2802) ? 0x144B : 0x146B;

                const auto task = Mem.Read<uint8_t>(pcea + tsk);

                if (task & (1 << 6))
                {
                    const auto veh = Mem.Read<uintptr_t>(pcea + vhm);
                    if (veh)
                        aeaef = Mem.Read<D3DXVECTOR3>(veh + (lks - 0x820));
                }
                else
                {
                    aeaef = Mem.Read<D3DXVECTOR3>(pcea + 0x300);
                }
                auto* wm = local->GetWeaponManager();
                auto* wi = wm->GetWeaponInfo();

                float blSp = wi->GetWeaponBulletSpeed();

                const auto timeToTravel = dis / (blSp * 2);

                const float extra = 1.5f;
                if (timeToTravel <= 0.0f)
                    return target;

                D3DXVECTOR3 ape = {
                    aeaef.x * timeToTravel * extra,
                    aeaef.y * timeToTravel * extra,
                    aeaef.z * timeToTravel
                };

                return D3DXVECTOR3{target.x + ape.x,target.y + ape.y,target.z + ape.z
                };
            }
        }
    }
}