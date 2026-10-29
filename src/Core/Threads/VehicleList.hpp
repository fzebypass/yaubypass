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
#include <unordered_map>


namespace Core
{
    namespace Threads
    {
        class cVehicleList {
        private:
            CVehInterFace * VehInterface = nullptr;
            CVehicleList * VehList = nullptr;
            int MaxVehicles = 0;
            uintptr_t LastReplayInterfacePtr = 0;
            std::vector<Core::SDK::Game::VehicleStructure> UpdatedVehicleList;
            std::unordered_map<uintptr_t, std::string> VehicleNameCache;
        public:
            std::mutex vehicleListMutex;
        public:
            void Update(uint64_t worker_epoch)
            {
                while (Core::ShouldGameThreadRun(worker_epoch))
                {
                    const bool need_fast_vehicle_updates =
                        g_Config.VehicleESP->Enabled ||
                        g_Config.Player->VehicleGodMode ||
                        g_Config.Player->SeatBelt ||
                        g_Config.Player->VehicleParachute ||
                        g_Config.Player->VehicleJump ||
                        g_Config.Player->HornetBoost;
                    const int vehicle_sleep_ms = need_fast_vehicle_updates ? 45 : 90;

                   // if ( !g_MenuInfo.IsLogged && !g_Variables.g_bPassedByThisVerify )
                        //continue;

                    auto* replayInterface = Core::SDK::Pointers::pReplayInterFace;
                    if (!replayInterface)
                    {
                        VehInterface = nullptr;
                        VehList = nullptr;
                        MaxVehicles = 0;
                        LastReplayInterfacePtr = 0;
                        VehicleNameCache.clear();
                        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
                        continue;
                    }

                    const uintptr_t replayInterfacePtr = reinterpret_cast<uintptr_t>(replayInterface);
                    if (LastReplayInterfacePtr != replayInterfacePtr)
                    {
                        LastReplayInterfacePtr = replayInterfacePtr;
                        VehInterface = nullptr;
                        VehList = nullptr;
                        MaxVehicles = 0;
                        VehicleNameCache.clear();
                    }

                    if (!Core::HasLocalPlayerCached())
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(25));
                        continue;
                    }

                    auto* localPlayer = Core::SDK::Pointers::pLocalPlayer;
                    if ( !localPlayer )
                    {
                        std::this_thread::sleep_for( std::chrono::milliseconds( 25 ) );
                        continue;
                    }

                    if ( !VehInterface || !VehList )
                    {
                        VehInterface = replayInterface->InterfaceVeh( );
                        if ( !VehInterface ) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(vehicle_sleep_ms));
                            continue;
                        }

                        VehList = VehInterface->VehicleList( );
                        if ( !VehList ) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(vehicle_sleep_ms));
                            continue;
                        }
                    };

                    if ( !MaxVehicles )
                        MaxVehicles = VehInterface->MaxVehicles( );

                    const D3DXVECTOR3 localPos = localPlayer->GetPos();
                    UpdatedVehicleList.clear();
                    if (UpdatedVehicleList.capacity() < static_cast<size_t>(MaxVehicles))
                    {
                        UpdatedVehicleList.reserve(static_cast<size_t>(MaxVehicles));
                    }

                    for ( int i = 0; i < MaxVehicles; i++ )
                    {
                        CVehicle * CurrentVeh = VehList->Vehicle( i );
                        if ( !CurrentVeh )
                            continue;

                        //if ( CurrentVeh == Core::SDK::Pointers::pLocalPlayer->GetLastVehicle( ) )
                        //    continue;

                        uintptr_t vehicleModelInfo = Mem.Read<uintptr_t>( ( uintptr_t ) CurrentVeh + 0x20 );
                        if (!vehicleModelInfo)
                            continue;

                        std::string vehicleName;
                        auto cachedName = VehicleNameCache.find(vehicleModelInfo);
                        if (cachedName != VehicleNameCache.end())
                        {
                            vehicleName = cachedName->second;
                        }
                        else
                        {
                            std::string rawVehicleName = Mem.ReadString(vehicleModelInfo + 0x298);
                            if (rawVehicleName.empty())
                                continue;

                            vehicleName = Utils::StringToFirstUpperCase(rawVehicleName);
                            VehicleNameCache.emplace(vehicleModelInfo, vehicleName);
                        }

                        const D3DXVECTOR3 vehiclePos = CurrentVeh->GetPos();
                        D3DXVECTOR3 DistanceCalculation = vehiclePos - localPos;
                        float Distance = sqrtf( DistanceCalculation.x * DistanceCalculation.x + DistanceCalculation.y * DistanceCalculation.y + DistanceCalculation.z * DistanceCalculation.z );

                        Core::SDK::Game::VehicleStructure Vehicle;

                        Vehicle.Pointer = CurrentVeh;
                        Vehicle.Name = vehicleName;
                        Vehicle.Dist = Distance;
                        Vehicle.IsLocked = CurrentVeh->IsLocked( );
                        Vehicle.Driver = CurrentVeh->GetDriver( );
                        Vehicle.Pos = vehiclePos;

                        UpdatedVehicleList.push_back( Vehicle );
                    }

                    if (UpdatedVehicleList.size() > 1)
                    {
                        std::sort( UpdatedVehicleList.begin( ), UpdatedVehicleList.end( ), [ ] ( const auto & lhs, const auto & rhs ) { return lhs.Dist < rhs.Dist; } );
                    }

                    {
                        std::lock_guard<std::mutex> lock( vehicleListMutex );
                        Core::SDK::Game::VehicleList.swap(UpdatedVehicleList);
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(vehicle_sleep_ms));

                }

                std::lock_guard<std::mutex> lock(vehicleListMutex);
                Core::SDK::Game::VehicleList.clear();
                UpdatedVehicleList.clear();
            }
        };

        inline cVehicleList g_VehicleList;
    }
}
