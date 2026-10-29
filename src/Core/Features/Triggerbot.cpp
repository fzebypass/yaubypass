#include "Triggerbot.hpp"
#include "dynamicfov.hpp"
#include "TargetHighlight.hpp"
#include <Core/Lifecycle.hpp>
#include <Includes/CustomWidgets/Notify.hpp>

bool IsTriggerBotActive()
{
    auto* cfg = g_Config.TriggerBot;
    const HWND active_window = GetForegroundWindow();
    const bool in_game_context = !g_MenuInfo.IsOpen && active_window == g_Variables.g_hGameWindow;

    if (!in_game_context) {
        cfg->KeyBindToggled = false;
        cfg->KeyBindLast = false;
        return false;
    }

    if (!cfg->Enabled || cfg->KeyBind == 0)
    {
        cfg->KeyBindToggled = false;
        cfg->KeyBindLast = false;
        return false;
    }

    if (cfg->KeyBindMode == 1) // Hold
    {
        const bool down = (GetAsyncKeyState(cfg->KeyBind) & 0x8000) != 0;
        cfg->KeyBindToggled = down;
        cfg->KeyBindLast = down;
        return down;
    }

    // Toggle
    bool down = (GetAsyncKeyState(cfg->KeyBind) & 0x8000) != 0;

    if (down && !cfg->KeyBindLast)
    {
        cfg->KeyBindToggled = !cfg->KeyBindToggled;

        if (cfg->KeyBindToggled)
            NotifyManager::Send("Triggerbot Enabled", 200.0f);
        else
            NotifyManager::Send("Triggerbot Disabled", 200.0f);
    }

    cfg->KeyBindLast = down;
    return cfg->KeyBindToggled;
}

inline float my_clampf(float value, float minVal, float maxVal)
{
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

void Core::Features::cTriggerbot::Shoot(int delay)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}

void Core::Features::cTriggerbot::Start(uint64_t worker_epoch)
{
    while (Core::ShouldGameThreadRun(worker_epoch))
    {
        bool triggerActive = IsTriggerBotActive();

        if (!triggerActive) {
            Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::Triggerbot);
            std::this_thread::sleep_for(std::chrono::milliseconds(24));
            continue;
        }

        CPed* ped = nullptr;

        if (!g_Config.TriggerBot->SmartTrigger)
        {
            ped = Core::SDK::Game::GetClosestPed(
                g_Config.TriggerBot->MaxDistance,
                g_Config.TriggerBot->IgnoreNPCs,
                g_Config.TriggerBot->IgnoreDead,
                g_Config.TriggerBot->OnlyVisible,
                true,
                g_Config.TriggerBot->ExcludeGod
            );
            if (!ped) {
                Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::Triggerbot);
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
            }

            D3DXVECTOR3 HeadPos = ped->GetBonePosDefault(0);
            D3DXVECTOR2 ScreenPos = Core::SDK::Game::WorldToScreen(HeadPos);
            if (!Core::SDK::Game::IsOnScreen(ScreenPos)) {
                Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::Triggerbot);
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
            }

            ImVec2 currentFovAxes(
                static_cast<float>(g_Config.TriggerBot->FOVX),
                static_cast<float>(g_Config.TriggerBot->FOVY));
            if (g_Config.TriggerBot->ShowDynamicFov)
            {
                float distance = GetTriggerbotTargetDistance();
                float minDist = static_cast<float>(g_Config.TriggerBot->DynamicFovMinDistance);
                float maxDist = static_cast<float>(g_Config.TriggerBot->DynamicFovMaxDistance);
                float minFov = static_cast<float>(g_Config.TriggerBot->DynamicFovMinValue);
                float maxFov = static_cast<float>(g_Config.TriggerBot->DynamicFovMaxValue);
                distance = my_clampf(distance, minDist, maxDist);
                const float currentFov = maxFov - (distance - minDist) / (maxDist - minDist) * (maxFov - minFov);
                currentFovAxes = Core::FovUtils::ScaleAxesFromLegacyValue(currentFov, currentFovAxes.x, currentFovAxes.y);
            }

            if (ped->HasFlag(ePedConfigFlag::DisablePlayerLockon))
                ped->SetConfigFlag(ePedConfigFlag::DisablePlayerLockon, false);
            if (ped->HasFlag(ePedConfigFlag::AllowPlayerLockOnIfFriendly))
                ped->SetConfigFlag(ePedConfigFlag::AllowPlayerLockOnIfFriendly, false);
            if (ped->HasFlag(ePedConfigFlag::TreatAsFriendlyForTargetingAndDamage))
                ped->SetConfigFlag(ePedConfigFlag::TreatAsFriendlyForTargetingAndDamage, false);

            const float dx = ScreenPos.x - g_Variables.g_vGameWindowCenter.x;
            const float dy = ScreenPos.y - g_Variables.g_vGameWindowCenter.y;
            if (Core::FovUtils::IsInsideEllipse(dx, dy, currentFovAxes.x, currentFovAxes.y))
            {
                Core::Features::TargetHighlight::Mark(Core::Features::TargetHighlight::Source::Triggerbot, ped);
                Shoot(g_Config.TriggerBot->Delay);
            }
            else
            {
                Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::Triggerbot);
            }
        }
        else
        {
            CPed* PedAim = Mem.Read<CPed*>(g_Offsets.m_AimCPedPatternResult + Mem.Read<int>(g_Offsets.m_AimCPedPatternResult + 3) + 7);
            if (!PedAim) {
                Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::Triggerbot);
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
            }
            Core::Features::TargetHighlight::Mark(Core::Features::TargetHighlight::Source::Triggerbot, PedAim);
            Shoot(g_Config.TriggerBot->Delay);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(12));
    }

    Core::Features::TargetHighlight::Clear(Core::Features::TargetHighlight::Source::Triggerbot);
}
