#pragma once

#include <atomic>
#include <cstdint>

namespace Core
{
    inline std::atomic<bool> g_GameThreadsRunning{ false };
    inline std::atomic<uint64_t> g_GameThreadEpoch{ 1 };
    inline std::atomic<bool> g_OffsetRefreshRequested{ false };
    inline std::atomic<bool> g_HasLocalPlayer{ false };
    inline std::atomic<bool> g_ShutDown{ false };

    inline bool IsShutDownRequested()
    {
        return g_ShutDown.load(std::memory_order_acquire);
    }

    inline bool ShouldGameThreadRun(uint64_t epoch)
    {
        return !IsShutDownRequested() &&
            g_GameThreadsRunning.load(std::memory_order_acquire) &&
            g_GameThreadEpoch.load(std::memory_order_acquire) == epoch;
    }

    inline uint64_t CaptureGameThreadEpoch()
    {
        return g_GameThreadEpoch.load(std::memory_order_acquire);
    }

    inline void RequestOffsetsRefresh()
    {
        g_OffsetRefreshRequested.store(true, std::memory_order_release);
    }

    inline bool HasLocalPlayerCached()
    {
        return g_HasLocalPlayer.load(std::memory_order_acquire);
    }

    inline void SetLocalPlayerCached(bool has_local_player)
    {
        g_HasLocalPlayer.store(has_local_player, std::memory_order_release);
    }

    bool HasValidCoreOffsets();
    bool RefreshOffsets();
    void ClearOffsets();
    void ClearSdkPointers();
    void ClearRuntimeState();
    void SetFreezeAmmoEnabled(bool enabled, bool suppress_errors = false);
    void CleanupDisconnectedGameState();
    void DisableAllFeaturesAndCleanup(bool mark_unload = false);
    void RequestShutDown();
    void PerformShutDownCleanup();
    void StartGameThreads();
    void StopGameThreads();
    void StartThreads();
}
