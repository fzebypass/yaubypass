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

namespace Core
{
    namespace Threads
    {
        class cUpdatePtrs {
        public:
            void Update()
            {
                bool hadLocalPlayer = false;
                bool hadGameSession = false;
                auto nextOffsetRefresh = std::chrono::steady_clock::now();

                const auto scheduleOffsetRefresh = [&](int delayMs) {
                    nextOffsetRefresh = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
                };

                const auto handleLocalPlayerLoss = [&](bool gameDisconnected) {
                    if (gameDisconnected && hadGameSession)
                    {
                        Core::CleanupDisconnectedGameState();
                    }
                    else if (hadLocalPlayer)
                    {
                        Core::StopGameThreads();
                    }

                    hadLocalPlayer = false;
                    if (gameDisconnected)
                    {
                        hadGameSession = false;
                        g_Variables.g_hGameWindow = nullptr;
                        Mem.ResetProcessContext();
                    }

                    Core::ClearSdkPointers();
                    Core::ClearRuntimeState();
                    Core::ClearOffsets();
                };

                while (!Core::IsShutDownRequested())
                {
                    const auto now = std::chrono::steady_clock::now();
                    const bool refreshRequested = Core::g_OffsetRefreshRequested.exchange(false, std::memory_order_acq_rel);

                    if (refreshRequested)
                    {
                        handleLocalPlayerLoss(false);
                        scheduleOffsetRefresh(0);
                    }

                    const bool gameWindowValid = g_Variables.g_hGameWindow && IsWindow(g_Variables.g_hGameWindow);
                    const bool processAlive = Mem.IsProcessAlive();

                    if (!gameWindowValid || !processAlive)
                    {
                        handleLocalPlayerLoss(true);

                        if (now >= nextOffsetRefresh)
                        {
                            Core::RefreshOffsets();
                            scheduleOffsetRefresh(1000);
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        continue;
                    }

                    hadGameSession = true;

                    if (!Mem.ProcHandle || !Mem.ModBase || !Core::HasValidCoreOffsets())
                    {
                        handleLocalPlayerLoss(false);

                        if (now >= nextOffsetRefresh)
                        {
                            Core::RefreshOffsets();
                            scheduleOffsetRefresh(1000);
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        continue;
                    }

                    auto* world = Mem.Read<CPedFactory*>(g_Offsets.m_World);
                    auto* replayInterface = Mem.Read<CReplayInterFace*>(g_Offsets.m_ReplayInterFace);
                    const auto viewPort = Mem.Read<uintptr_t>(g_Offsets.m_ViewPort);
                    const auto camGameplayDirector = Mem.Read<uintptr_t>(g_Offsets.m_CamGameplayDirector);

                    if (!world || reinterpret_cast<uintptr_t>(world) < 0x10000 ||
                        !replayInterface || reinterpret_cast<uintptr_t>(replayInterface) < 0x10000 ||
                        !viewPort || !camGameplayDirector)
                    {
                        handleLocalPlayerLoss(false);

                        if (now >= nextOffsetRefresh)
                        {
                            Core::RefreshOffsets();
                            scheduleOffsetRefresh(750);
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        continue;
                    }

                    auto* localPlayer = world->GetLocalPlayer();
                    if (!localPlayer || reinterpret_cast<uintptr_t>(localPlayer) < 0x10000)
                    {
                        handleLocalPlayerLoss(false);

                        if (now >= nextOffsetRefresh)
                        {
                            Core::RefreshOffsets();
                            scheduleOffsetRefresh(750);
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        continue;
                    }

                    if (!hadLocalPlayer)
                    {
                        Core::RefreshOffsets();

                        world = Mem.Read<CPedFactory*>(g_Offsets.m_World);
                        replayInterface = Mem.Read<CReplayInterFace*>(g_Offsets.m_ReplayInterFace);
                        localPlayer = world ? world->GetLocalPlayer() : nullptr;

                        if (!world || !replayInterface || !localPlayer ||
                            reinterpret_cast<uintptr_t>(world) < 0x10000 ||
                            reinterpret_cast<uintptr_t>(replayInterface) < 0x10000 ||
                            reinterpret_cast<uintptr_t>(localPlayer) < 0x10000)
                        {
                            handleLocalPlayerLoss(false);
                            scheduleOffsetRefresh(750);
                            std::this_thread::sleep_for(std::chrono::milliseconds(250));
                            continue;
                        }

                        hadLocalPlayer = true;
                        Core::SetLocalPlayerCached(true);
                        Core::StartGameThreads();
                    }

                    Core::SDK::Pointers::pWorld = world;
                    Core::SDK::Pointers::pLocalPlayer = localPlayer;
                    Core::SDK::Pointers::pReplayInterFace = replayInterface;
                    Core::SDK::Pointers::pViewPort = viewPort;
                    Core::SDK::Pointers::pCamGamePlayDirector = camGameplayDirector;
                    Core::SetLocalPlayerCached(true);
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                }
            }
        };

        inline cUpdatePtrs g_UpdatePtrs;
    }
}
