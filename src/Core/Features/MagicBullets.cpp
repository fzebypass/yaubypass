#include "MagicBullets.hpp"
#include <Core/Lifecycle.hpp>
#include <Includes/CustomWidgets/Notify.hpp>

bool IsSilentAimActive2()
{
	auto* cfg = g_Config.SilentAim;

	HWND ActiveWindow = GetForegroundWindow();
	if (ActiveWindow != g_Variables.g_hGameWindow)
		return false;

	if (!cfg->MagicBullets || cfg->KeyBind == 0)
		return false;

	if (cfg->KeyBindMode == 1) {// Hold
		if (!cfg->KeyBindToggled)
			cfg->KeyBindToggled = true;
		return (GetAsyncKeyState(cfg->KeyBind) & 0x8000) != 0;
	}

	// Toggle
	bool down = (GetAsyncKeyState(cfg->KeyBind) & 0x8000) != 0;

	if (down && !cfg->KeyBindLast)
	{
		cfg->KeyBindToggled = !cfg->KeyBindToggled;

		if (cfg->KeyBindToggled)
			NotifyManager::Send("Magic Bullet Enabled", 200.0f);
		else
			NotifyManager::Send("Magic Bullet Disabled", 200.0f);
	}

	cfg->KeyBindLast = down;

	return cfg->KeyBindToggled;
}

uintptr_t Core::Features::cMagicBullets::GetCWeaponObj()
{
	CWeaponManager* WeaponMgr = Core::SDK::Pointers::pLocalPlayer->GetWeaponManager();
	auto CObject = Mem.Read<uintptr_t>((uintptr_t)WeaponMgr + g_Offsets.m_CObject);

	return Mem.Read<uintptr_t>(CObject + g_Offsets.m_CWeapon);
}

void Core::Features::cMagicBullets::Initialize()
{
	CWeapon = this->GetCWeaponObj();
	if (!CWeapon || !g_Offsets.m_MagicBulletsPatch) return;

	Mem.PatchFunc(g_Offsets.m_MagicBulletsPatch, 4);
}

void Core::Features::cMagicBullets::Restore()
{
	std::vector<uint8_t> UpdateMuzzlePosTable
	{
		0x0F, 0x29, 0x4F, 0x20 //movaps [rdi+20], xmm1
	};

	Mem.WriteBytes(g_Offsets.m_MagicBulletsPatch, UpdateMuzzlePosTable);
}

void Core::Features::cMagicBullets::Start()
{
	while (!Core::IsShutDownRequested())
	{
		const bool active =
			IsSilentAimActive2() &&
			GetForegroundWindow() != g_Variables.g_hCheatWindow;

		if (active)
		{

			CPed* Ped = Core::SDK::Game::GetClosestPed(g_Config.SilentAim->MaxDistance, g_Config.SilentAim->IgnoreNPCs,g_Config.SilentAim->IgnoreDead, g_Config.SilentAim->OnlyVisible,true, g_Config.SilentAim->ExcludeGod
			);
			if (!Ped) { continue; }

			D3DXVECTOR3 HeadPos = Ped->GetBonePosDefault(0);
			D3DXVECTOR2 HeadToScreen = Core::SDK::Game::WorldToScreen(HeadPos);

			if (Core::SDK::Game::IsOnScreen(HeadToScreen))
			{
				const float dx = HeadToScreen.x - g_Variables.g_vGameWindowCenter.x;
				const float dy = HeadToScreen.y - g_Variables.g_vGameWindowCenter.y;
				if (Core::FovUtils::IsInsideEllipse(dx, dy, static_cast<float>(g_Config.SilentAim->FOVX), static_cast<float>(g_Config.SilentAim->FOVY))) {

					if (!Initialized) {
						Initialize();
						Initialized = true;
					}

					bool Miss = g_Config.SilentAim->MissChance >= Utils::GenRandomInt(0, 100);

                    D3DXVECTOR3 MuzzlePos = Core::SDK::Game::GetMuzzlePos(CWeapon);
                    D3DXVECTOR3 BulletStartPos = (MuzzlePos.x == 0.0f && MuzzlePos.y == 0.0f && MuzzlePos.z == 0.0f) ?
                        (HeadPos) : MuzzlePos;

                    D3DXVECTOR3 TargetPos = Miss ? HeadPos + D3DXVECTOR3(0.0f, 0.3f, 0.0f) : HeadPos;

                    Mem.Write(CWeapon + 0x20, BulletStartPos);
				}
				else {
					if (Initialized) {
						Restore();
						Initialized = false;
					}
				}
			}
			else {
				if (Initialized) {
					Restore();
					Initialized = false;
				}
			}
		}
		else
		{
			if (Initialized) {
				Restore();
				Initialized = false;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(active ? 8 : 20));
	}
}
