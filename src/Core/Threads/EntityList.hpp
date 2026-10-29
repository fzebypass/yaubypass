#pragma once
#include <Includes/Includes.hpp>
#include <Includes/Utils.hpp>
#include <Core/Offsets.hpp>
#include <Core/SDK/SDK.hpp>
#include <Core/Lifecycle.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <Core/SDK/visible.hpp>

namespace Core
{
    namespace Threads
    {
        class cEntityList
        {
        private:
            std::mutex EntityListMutex;
            std::unordered_map<CPed*, std::chrono::steady_clock::time_point> LastRaycastRefresh;
            std::vector<Core::SDK::Game::EntityStruct> UpdatedEntities;
        private:
            CPedInterFace* CPedInterFace = nullptr;
            CPedList* CPedList = nullptr;
        public:
            std::unordered_map<CPed*, Core::SDK::Game::EntityStruct> CachedEntities;
        public:
            void Update(uint64_t worker_epoch)
            {
                int visibilityTick = 0;

                while (Core::ShouldGameThreadRun(worker_epoch))
                {
                    visibilityTick++;

                    if (!g_MenuInfo.IsLogged && !g_Variables.g_bPassedByThisVerify)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        continue;
                    }

                    if (!Core::SDK::Pointers::pReplayInterFace)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }

                    CPedInterFace = Core::SDK::Pointers::pReplayInterFace->InterfacePed();
                    if (!CPedInterFace)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(25));
                        continue;
                    }

                    CPedList = CPedInterFace->PedList();
                    if (!CPedList)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(25));
                        continue;
                    }

                    if (!Core::HasLocalPlayerCached())
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(25));
                        continue;
                    }

                    auto* localPlayer = Core::SDK::Pointers::pLocalPlayer;
                    if (!localPlayer)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(25));
                        continue;
                    }

                    const D3DXVECTOR3 localPos = localPlayer->GetPos();
                    const bool isAimTraceActive =  g_Config.ESP->IgnoreLocalPed && (GetAsyncKeyState(VK_RBUTTON) & 0x8000);

                    const int PedCount = CPedInterFace->MaxPed();
                    UpdatedEntities.clear();
                    if (UpdatedEntities.capacity() < static_cast<size_t>(PedCount))
                    {
                        UpdatedEntities.reserve(static_cast<size_t>(PedCount));
                    }

                    const auto getFriendState = [](CPed* ped) {
                        return Core::Features::cFriends::IsFriend(ped);
                    };

                    const auto hasResolvedPlayerName = [](const std::string& name) {
                        if (name.empty()) {
                            return false;
                        }

                        return _stricmp(name.c_str(), "npc") != 0 &&
                            _stricmp(name.c_str(), "unknown") != 0 &&
                            _stricmp(name.c_str(), "unknown player") != 0;
                    };

                    for (int i = 0; i < PedCount; i++)
                    {
                        CPed* CurrentPed = CPedList->Ped(i);

                        if (!CurrentPed)
                            continue;

                        Core::SDK::Game::EntityStruct Entity{};

                        Entity.Ped = CurrentPed;
                        Entity.Id = CurrentPed->GetID();
                        Entity.Index = i;

                        Entity.Pos = CurrentPed->GetPos();
                        Entity.IsVisible2 = Entity.Ped->IsTrueInvisible();
                        Entity.MaxHealth = CurrentPed->GetMaxHealth();
                        Entity.Health = CurrentPed->GetHealth();
                        Entity.Armor = CurrentPed->GetArmor();
                        Entity.PedType = CurrentPed->GetPedType();
                        Entity.Distance = CurrentPed->GetDistance(localPos, Entity.Pos);
                        Entity.NetworkInfo.UserName = Core::Features::UpdateNames::GetName(CurrentPed);
                        Entity.IsPlayer = Entity.Id > 0 || Entity.PedType == 2 || hasResolvedPlayerName(Entity.NetworkInfo.UserName);
                        Entity.IsFriend = getFriendState(CurrentPed);
                        Entity.IsGod;

                        uintptr_t Addr = reinterpret_cast<uintptr_t>(CurrentPed) + 0x188;
                        DWORD flag = Mem.Read<DWORD>(Addr);
                        if (flag & (1 << 9))
                            Entity.IsGod = true;

                        bool isVisible = false;
                        isVisible = Entity.Ped->IsVisible(1, false);

                        if (isAimTraceActive && CurrentPed && localPlayer)
                        {
                            isVisible = Core::Features::IsPlayerVisible(CurrentPed, localPlayer);
                        }

                        Entity.IsVisible = isVisible;
                        Entity.IsCheating = Entity.Ped->HasFlag(ePedConfigFlag::TreatAsFriendlyForTargetingAndDamage);
                        Entity.IsVisible3 = Core::SDK::Game::isVisible(Mem.ModBase + 0x600, Entity.Ped, localPlayer);

                        if (auto* weaponManager = CurrentPed->GetWeaponManager())
                        {
                            if (auto* weaponInfo = weaponManager->GetWeaponInfo())
                            {
                                Entity.WeaponName = weaponInfo->GetName();
                                if (Entity.WeaponName == "UNARMED" && g_Config.ESP->IgnoreUnarmed)
                                {
                                    Entity.WeaponName = "";
                                }
                            }
                        }

                        UpdatedEntities.push_back(std::move(Entity));
                    }

                    CachedEntities.clear();
                    if (!UpdatedEntities.empty())
                    {
                        CachedEntities.reserve(UpdatedEntities.size());
                        for (const auto& entity : UpdatedEntities)
                        {
                            CachedEntities.emplace(entity.Ped, entity);
                        }
                    }
                    else
                    {
                        LastRaycastRefresh.clear();
                    }

                    {
                        std::lock_guard<std::mutex> entityGuard(Core::SDK::Game::EntityMutex);
                        Core::SDK::Game::EntityList.swap(UpdatedEntities);
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                std::lock_guard<std::mutex> entityGuard(Core::SDK::Game::EntityMutex);
                Core::SDK::Game::EntityList.clear();
                UpdatedEntities.clear();
                CachedEntities.clear();
                LastRaycastRefresh.clear();
            }
        };

        inline cEntityList g_EntityList;
    }
}
