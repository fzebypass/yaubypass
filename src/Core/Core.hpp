#pragma once
#include <Includes/Includes.hpp>
#include <Core/Lifecycle.hpp>
#include <Core/Offsets.hpp>
#include <Core/SDK/SDK.hpp>
#include <Core/SDK/visible.hpp>

// Threads
#include "Threads/ScreenResolution.hpp"
#include "Threads/LocalPlayerState.hpp"
#include "Threads/UpdatePointers.hpp"
#include "Threads/UpdateNames.hpp"
#include "Threads/VehicleList.hpp"
#include "Threads/EntityList.hpp"
#include "Threads/GetConfig.hpp"


// Features
#include "Features/MagicBullets.hpp"
#include "Features/Triggerbot.hpp"
#include "Features/SilentAim.hpp"
#include "Features/Friends.hpp"
#include "Features/Aimbot.hpp"
#include "Features/ESP.hpp"
#include "Features/TargetHighlight.hpp"
//Exploits
#include "Features/Exploits/ResourceList.hpp"
#include "Features/Exploits/Exploits.hpp"
#include <lazyimporter.hpp>

#include <regex>
#include "Features/Exploits/GiveWeapon.hpp"

namespace Core {

	inline static bool ThreadsStarted = false;
	inline static std::mutex g_LifecycleMutex;
	inline bool SetupOffsets();

	inline bool HasValidCoreOffsets() {
		return Mem.IsProcessAlive() &&
			Mem.ModBase &&
			g_Offsets.m_World &&
			g_Offsets.m_ReplayInterFace &&
			g_Offsets.m_ViewPort &&
			g_Offsets.m_CamGameplayDirector;
	}

	inline void ClearSdkPointers() {
		Core::SDK::Pointers::pWorld = nullptr;
		Core::SDK::Pointers::pLocalPlayer = nullptr;
		Core::SDK::Pointers::pReplayInterFace = nullptr;
		Core::SDK::Pointers::pViewPort = 0;
		Core::SDK::Pointers::pCamGamePlayDirector = 0;
		Core::SDK::Pointers::pCWorld = 0;
		Core::SetLocalPlayerCached(false);
	}

	inline void ClearOffsets() {
		g_Offsets = OffsetsClass{};
	}

	inline void ClearRuntimeState() {
		{
			std::lock_guard<std::mutex> lock(Core::SDK::Game::EntityMutex);
			Core::SDK::Game::EntityList.clear();
		}

		{
			std::lock_guard<std::mutex> lock(Threads::g_VehicleList.vehicleListMutex);
			Core::SDK::Game::VehicleList.clear();
		}

		Features::UpdateNames::ClearCache();
		Features::TargetHighlight::Clear(Features::TargetHighlight::Source::Aimbot);
		Features::TargetHighlight::Clear(Features::TargetHighlight::Source::SilentAim);
		Features::TargetHighlight::Clear(Features::TargetHighlight::Source::Triggerbot);
		Features::cSilentAim::ResetProcessState();
	}

	inline void SetFreezeAmmoEnabled(bool enabled, bool suppress_errors) {
		static std::vector<uint8_t> original_ammo_bytes;
		static bool saved = false;
		static bool patched = false;
		static uintptr_t patched_address = 0;
		(void)suppress_errors;
		const uintptr_t ammo_address = g_Offsets.m_InfiniteAmmo1;

		if (enabled) {
			if (!SDK::Pointers::pLocalPlayer) {
				return;
			}

			if (!HasValidCoreOffsets() || !ammo_address) {
				return;
			}

			if (!saved || patched_address != ammo_address) {
				original_ammo_bytes = Mem.ReadBytes(ammo_address, 3);
				saved = original_ammo_bytes.size() == 3;
				patched = false;
				patched_address = ammo_address;
			}

			if (!saved) {
				return;
			}

			if (!patched) {
				Mem.PatchFunc(ammo_address, 3);
				patched = true;
			}

			return;
		}

		if (saved && patched && patched_address && HasValidCoreOffsets() && patched_address == g_Offsets.m_InfiniteAmmo1) {
			Mem.WriteBytes(patched_address, original_ammo_bytes);
		}

		saved = false;
		patched = false;
		patched_address = 0;
		original_ammo_bytes.clear();
	}

	inline void CleanupTransientGameState() {
		const bool can_touch_game_state = HasValidCoreOffsets() && SDK::Pointers::pLocalPlayer != nullptr;
		auto* local_player = can_touch_game_state ? SDK::Pointers::pLocalPlayer : nullptr;
		auto* current_vehicle = local_player && local_player->InVehicle() ? local_player->GetLastVehicle() : nullptr;

		if (local_player) {
			local_player->SetInfStamina(false);
			local_player->SetGodMode(false, g_Offsets.CurrentBuild);
			local_player->FreezePed(false);
			local_player->SetConfigFlag(ePedConfigFlag::DisablePlayerLockon, false);
			local_player->SetConfigFlag(ePedConfigFlag::AllowPlayerLockOnIfFriendly, false);
			local_player->SetConfigFlag(ePedConfigFlag::TreatAsFriendlyForTargetingAndDamage, false);

			if (local_player->InVehicle()) {
				local_player->SeatBealt(false);
			}
		}

		if (current_vehicle) {
			current_vehicle->SetGodMode(false);
		}

		SetFreezeAmmoEnabled(false, true);
		Mem.RapidFireUpdate(true);
		StopGameThreads();
	}

	inline void CleanupDisconnectedGameState() {
		CleanupTransientGameState();
	}

	inline void DisableAllFeaturesAndCleanup(bool mark_unload) {
		auto reset_flags = [](auto*... flags) {
			((*flags = false), ...);
		};

		if (mark_unload) {
			g_Config.General->ShutDown = true;
			g_ShutDown.store(true, std::memory_order_release);
		}

		reset_flags(
			&g_Config.General->Notifications,
			&g_Config.General->StreamProof,
			&g_Config.General->WaterMark,
			&g_Config.General->KeybindList,
			&g_Config.General->Crosshair,
			&g_Config.General->ArrayList,
			&g_Config.General->MirrorFivem,
			&g_Config.General->SecondMonitor,
			&g_Config.General->SpectatorList,
			&g_Config.General->AutoConfig,
			&g_Config.General->AddFriend
		);

		reset_flags(
			&g_Config.Aimbot->Enabled,
			&g_Config.TriggerBot->Enabled,
			&g_Config.SilentAim->Enabled,
			&g_Config.SilentAim->MagicBullets,
			&g_Config.ESP->Enabled,
			&g_Config.ESP->RadarEnabled,
			&g_Config.ESP->FriendsMarker,
			&g_Config.ESP->Observers,
			&g_Config.VehicleESP->Enabled,
			&g_Config.VehicleESP->SnapLines,
			&g_Config.VehicleESP->ShowLockUnlock,
			&g_Config.VehicleESP->VehName,
			&g_Config.VehicleESP->DistanceFromMe
		);

		g_Config.Aimbot->KeyBindToggled = false;
		g_Config.Aimbot->KeyBindLast = false;
		g_Config.TriggerBot->KeyBindToggled = false;
		g_Config.TriggerBot->KeyBindLast = false;
		g_Config.SilentAim->KeyBindToggled = false;
		g_Config.SilentAim->KeyBindLast = false;

		reset_flags(
			&g_Config.Player->SoloSession,
			&g_Config.Player->FastRun,
			&g_Config.Player->InfiniteStamina,
			&g_Config.Player->GetLocalPlayerInfo,
			&g_Config.Player->GetEntitiyListSafe,
			&g_Config.Player->WeaponOptions,
			&g_Config.Player->NoRecoilEnabled,
			&g_Config.Player->NoSpreadEnabled,
			&g_Config.Player->InfiniteAmmoEnabled,
			&g_Config.Player->NoReloadEnabled,
			&g_Config.Player->NCEnabled,
			&g_Config.Player->Heal,
			&g_Config.Player->Armor,
			&g_Config.Player->Repair,
			&g_Config.Player->TpWaypoint,
			&g_Config.Player->ReloadOffsets,
			&g_Config.Player->HandlingEditor,
			&g_Config.Player->InfiniteCombatRoll,
			&g_Config.Player->EnableGodMode,
			&g_Config.Player->DisableGodMode,
			&g_Config.Player->NoCollision,
			&g_Config.Player->ResourceList,
			&g_Config.Player->AutoStrafe,
			&g_Config.Player->PlayerList,
			&g_Config.Player->Immortality,
			&g_Config.Player->Absorption,
			&g_Config.Player->VehicleGodMode,
			&g_Config.Player->SeatBelt,
			&g_Config.Player->ForceWeaponWheel,
			&g_Config.Player->ShrinkEnabled,
			&g_Config.Player->NoRagDollEnabled,
			&g_Config.Player->AntiHSEnabled,
			&g_Config.Player->NoClipEnabled,
			&g_Config.Player->God1k,
			&g_Config.Player->StealCarEnabled,
			&g_Config.Player->SuperFistEnabled,
			&g_Config.Player->RapidFire,
			&g_Config.Player->DamageBoost,
			&g_Config.Player->FakeLag,
			&g_Config.Player->VehicleRain,
			&g_Config.Player->HornetBoost,
			&g_Config.Player->VehicleJump,
			&g_Config.Player->VehicleParachute
		);

		g_Config.Player->Toggled = 0;
		g_Config.Player->FakelagToggle = false;

		CleanupTransientGameState();
	}

	inline void RequestShutDown() {
		g_Config.General->ShutDown = true;
		g_ShutDown.store(true, std::memory_order_release);
	}

	inline void PerformShutDownCleanup() {
		g_MenuInfo.IsOpen = false;
		g_Variables.g_MenuKeyOverride = 0;

		if (g_Variables.g_hCheatWindow && IsWindow(g_Variables.g_hCheatWindow)) {
			const LONG ex_style = GetWindowLong(g_Variables.g_hCheatWindow, GWL_EXSTYLE);
			SetWindowLong(g_Variables.g_hCheatWindow, GWL_EXSTYLE, ex_style | WS_EX_TRANSPARENT);
			ShowWindow(g_Variables.g_hCheatWindow, SW_HIDE);
		}

		if (g_Variables.g_hGameWindow && IsWindow(g_Variables.g_hGameWindow)) {
			SetForegroundWindow(g_Variables.g_hGameWindow);
		}

		DisableAllFeaturesAndCleanup(false);
	}

	inline bool RefreshOffsets() {
		std::lock_guard<std::mutex> lock(g_LifecycleMutex);

		Features::cSilentAim::ResetProcessState();
		ClearOffsets();
		if (!SetupOffsets()) {
			ClearOffsets();
			return false;
		}

		Core::Features::g_VisibilityChecker.Reset();
		Core::Features::g_VisibilityChecker.Initialize();
		return HasValidCoreOffsets();
	}

	inline void StopGameThreads() {
		std::lock_guard<std::mutex> lock(g_LifecycleMutex);

		if (!g_GameThreadsRunning.load(std::memory_order_acquire)) {
			return;
		}

		g_GameThreadsRunning.store(false, std::memory_order_release);
		g_GameThreadEpoch.fetch_add(1, std::memory_order_acq_rel);
		Threads::g_GetConfig.running.store(false, std::memory_order_release);

		Features::TargetHighlight::Clear(Features::TargetHighlight::Source::Aimbot);
		Features::TargetHighlight::Clear(Features::TargetHighlight::Source::SilentAim);
		Features::TargetHighlight::Clear(Features::TargetHighlight::Source::Triggerbot);

		if (HasValidCoreOffsets() && g_Offsets.m_HandleBullet && Mem.ModBase) {
			Features::g_SilentAim.RestoreSilent();
		}

		::RestoreFakeLagPatch();
		ClearRuntimeState();
	}

	inline void StartGameThreads() {
		std::lock_guard<std::mutex> lock(g_LifecycleMutex);

		if (g_GameThreadsRunning.load(std::memory_order_acquire)) {
			return;
		}

		const uint64_t worker_epoch = g_GameThreadEpoch.load(std::memory_order_acquire);
		g_GameThreadsRunning.store(true, std::memory_order_release);
		Threads::g_GetConfig.running.store(true, std::memory_order_release);

		auto safe_thread = [worker_epoch](auto&& func) {
			std::thread([f = std::move(func), worker_epoch] {
				std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100 + 50));
				f(worker_epoch);
				}).detach();
			};

		safe_thread([](uint64_t epoch) { Threads::g_EntityList.Update(epoch); });
		safe_thread([](uint64_t epoch) { Threads::g_VehicleList.Update(epoch); });
		safe_thread([](uint64_t epoch) { Threads::g_GetConfig.Update(epoch); });
		safe_thread([](uint64_t epoch) { Threads::g_Exploits.Update(epoch); });
		safe_thread([](uint64_t epoch) { Features::g_Aimbot.Start(epoch); });
		safe_thread([](uint64_t epoch) { Features::g_SilentAim.Start(epoch); });
		safe_thread([](uint64_t epoch) { Features::g_Triggerbot.Start(epoch); });
	}

	inline void StartThreads() {
		std::lock_guard<std::mutex> lock(g_LifecycleMutex);

		if (ThreadsStarted) {
			return;
		}

		auto safe_thread = [](auto&& func) {
			std::thread([f = std::move(func)] {
				std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100 + 50));
				f();
				}).detach();
			};

		safe_thread([] { Threads::g_ScreenResolution.Update(); });
		safe_thread([] { Threads::g_LocalPlayerState.Update(); });
		safe_thread([] { Threads::g_UpdatePtrs.Update(); });

		ThreadsStarted = true;
	}

	inline std::pair<DWORD, std::wstring> GetProcessPidByName(const std::wstring& ProcessNamePattern) {
		HANDLE hSnapshot = SafeCall(CreateToolhelp32Snapshot)(TH32CS_SNAPPROCESS, 0);
		if (!hSnapshot || hSnapshot == INVALID_HANDLE_VALUE || hSnapshot == ((HANDLE)(LONG_PTR)ERROR_BAD_LENGTH)) {
			return { 0, L"" };
		}

		DWORD Pid = 0;
		std::wstring processName;
		PROCESSENTRY32 ProcessEntry;
		ProcessEntry.dwSize = sizeof(ProcessEntry);

		std::wregex pattern(ProcessNamePattern);
		if (SafeCall(Process32First)(hSnapshot, &ProcessEntry)) {
			do {
				wchar_t wExeFile[260];
				mbstowcs(wExeFile, ProcessEntry.szExeFile, sizeof(wExeFile) / sizeof(wchar_t));

				std::wstring exeFile(wExeFile);
				if (std::regex_match(exeFile, pattern)) {
					Pid = ProcessEntry.th32ProcessID;
					processName = exeFile;
					break;
				}
			} while (SafeCall(Process32Next)(hSnapshot, &ProcessEntry));
		}

		SafeCall(CloseHandle)(hSnapshot);
		return { Pid, processName };
	}

	inline std::string trim(const std::string& str) {
		size_t first = str.find_first_not_of(' ');
		if (first == std::string::npos) return "";
		size_t last = str.find_last_not_of(' ');
		return str.substr(first, last - first + 1);
	}

	inline uintptr_t ResolvePattern(const std::string& pattern, int Relative) {
		uintptr_t addr = Mem.FindSignatureStr(pattern);

		if (!addr) {
			return 0;
		}

		auto relativeOffset = Mem.Read<int32_t>(addr + Relative);

		uintptr_t resolvedAddr = addr + 7 + relativeOffset;

		return resolvedAddr;
	}

	inline uintptr_t Resolve(uintptr_t addr, int Relative) {

		auto relativeOffset = Mem.Read<int32_t>(addr + Relative);

		uintptr_t resolvedAddr = addr + 7 + relativeOffset;
		return resolvedAddr;
	}

	inline uintptr_t ResolvePatternOffset(const std::string& pattern, int offsetPosition) {
		uintptr_t addr = Mem.FindSignatureStr(pattern);
		if (!addr) {
			return 0;
		}

		int32_t offset = Mem.Read<int32_t>(addr + offsetPosition);

		return offset;
	}


	typedef struct _ModuleInfo {
		DWORD_PTR baseAddress;
		DWORD size;
	} ModuleInfo;

	inline int GetBuild() {
		std::wstring appDataPath = _wgetenv(L"LOCALAPPDATA");
		std::wstring iniFilePath = appDataPath + L"\\FiveM\\FiveM.app\\CitizenFX.ini";

		std::ifstream iniFile(iniFilePath);
		if (!iniFile.is_open()) {
			std::cerr << "Failed to open CitizenFX.ini" << std::endl;
			return -1;
		}

		std::string line;
		while (std::getline(iniFile, line)) {
			line = trim(line);

			if (line == "ReplaceExecutable=0") {
				return 3258;
			}
		}

		iniFile.clear();
		iniFile.seekg(0);
		while (std::getline(iniFile, line)) {
			line = trim(line);

			if (line.find(("SavedBuildNumber=")) != std::string::npos) {
				std::string buildNumberStr = line.substr(line.find("=") + 1);
				buildNumberStr = trim(buildNumberStr);
				return std::stoi(buildNumberStr);
			}
		}

		std::cout << ("Version not found") << std::endl;
		return -1;
	}

	inline bool SetupOffsets() {
		g_Offsets.CurrentBuild = GetBuild();
		std::wstring gtaProcessPattern = xorstr(L"FiveM_b\\d{4}_GTAProcess\\.exe");
		std::wstring gameProcessPattern = xorstr(L"FiveM_b\\d{4}_GameProcess\\.exe");
		std::wstring gtaProcess = xorstr(L"FiveM_GTAProcess.exe");
		std::wstring gameProcess = xorstr(L"FiveM_GameProcess.exe");


		auto [pid1, name1] = GetProcessPidByName(gtaProcessPattern);
		auto [pid2, name2] = GetProcessPidByName(gameProcessPattern);
		auto [pid3, name3] = GetProcessPidByName(gtaProcess);
		auto [pid4, name4] = GetProcessPidByName(gameProcess);

		if (pid1) {
			g_Variables.ProcIdFiveM = pid1;
			Mem.ProcName = name1;
		}
		else if (pid2) {
			g_Variables.ProcIdFiveM = pid2;
			Mem.ProcName = name2;
		}
		else if (pid3) {
			g_Variables.ProcIdFiveM = pid3;
			Mem.ProcName = name3;
		}
		else if (pid4) {
			g_Variables.ProcIdFiveM = pid4;
			Mem.ProcName = name4;
		}
		else {
			return false;
		}

		if (!Mem.OpenProcByPid()) {
			return 0;
		}

		if (!Mem.ModBase || !Mem.ModBaseSize || !g_Variables.ProcIdFiveM) {
			return false;
		}

		if (g_Offsets.CurrentBuild >= 2944)
			g_Offsets.m_PlayerId = 0xE8;
		else if (g_Offsets.CurrentBuild > 2189)
			g_Offsets.m_PlayerId = 0x78;
		else
			g_Offsets.m_PlayerId = 0x88;

		switch (g_Offsets.CurrentBuild) {

		default:
			g_Offsets.m_WeaponManager = 0x10D8;
			g_Offsets.m_WeaponInfo = 0x20;
			g_Offsets.m_LastVehicle = 0xD30;
			g_Offsets.m_PlayerInfo = 0x10C8;
			g_Offsets.m_PlayerId = 0x88;
			g_Offsets.m_FragInst = 0x1400;
			g_Offsets.m_Armor = 0x14E0;
			g_Offsets.m_PedFlag = 0x1414;

		case 3570:
			g_Offsets.m_World = Mem.ModBase + 0x25ec580;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1fb0418;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x2058ba0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x2059a48;
			g_Offsets.m_BlipList = Mem.ModBase + 0x2061870;

			g_Offsets.m_MaxHealth = 0x284;
			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10A8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_PlayerId = 0xE8;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 3570;
			break;
		case 3407:
			/*g_Offsets.m_World = Mem.ModBase + 0x25D7108;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F9A9D8;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x20431C0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x20440C8;
			g_Offsets.m_BlipList = Mem.ModBase + 0x2047D50;*/

			g_Offsets.m_World = ResolvePattern(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), 3);
			g_Offsets.m_ReplayInterFace = ResolvePattern(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"), 3);
			g_Offsets.m_ViewPort = ResolvePattern(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), 3);
			g_Offsets.m_CamGameplayDirector = ResolvePattern(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB"), 3);
			g_Offsets.m_BlipList = ResolvePattern(xorstr("4C 8D 05 ? ? ? ? 0F B7 C1"), 3);

			g_Offsets.m_MaxHealth = 0x284;
			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10A8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_PlayerId = 0xE8;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 3407;
			break;

		case 3323:
			/*g_Offsets.m_World = Mem.ModBase + 0x25C15B0;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F85458;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x202DC50;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x202EB48;
			g_Offsets.m_BlipList = Mem.ModBase + 0x20333E0;*/

			g_Offsets.m_World = ResolvePattern(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), 3);
			g_Offsets.m_ReplayInterFace = ResolvePattern(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"), 3);
			g_Offsets.m_ViewPort = ResolvePattern(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), 3);
			g_Offsets.m_CamGameplayDirector = ResolvePattern(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB"), 3);
			g_Offsets.m_BlipList = ResolvePattern(xorstr("4C 8D 05 ? ? ? ? 0F B7 C1"), 3);
			g_Offsets.m_MaxHealth = 0x284;
			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10A8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_PlayerId = 0xE8;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 3323;
			break;

		case 3258:
			/*g_Offsets.m_World = Mem.ModBase + 0x25b14b0;;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1FBD4F0;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x201dba0;;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x201ED50;
			g_Offsets.m_BlipList = Mem.ModBase + 0x2023400;*/

			g_Offsets.m_World = ResolvePattern(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), 3);
			g_Offsets.m_ReplayInterFace = ResolvePattern(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"), 3);
			g_Offsets.m_ViewPort = ResolvePattern(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), 3);
			g_Offsets.m_CamGameplayDirector = ResolvePattern(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB"), 3);
			g_Offsets.m_BlipList = ResolvePattern(xorstr("4C 8D 05 ? ? ? ? 0F B7 C1"), 3);
			g_Offsets.m_MaxHealth = 0x284;

			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10A8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_PlayerId = 0xE8;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 3258;
			break;

		case 3095:
			/*g_Offsets.m_World = Mem.ModBase + 0x2593320;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F58B58;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x20019E0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x2002888;
			g_Offsets.m_BlipList = Mem.ModBase + 0x2002FA0;*/

			g_Offsets.m_World = ResolvePattern(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), 3);
			g_Offsets.m_ReplayInterFace = ResolvePattern(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"), 3);
			g_Offsets.m_ViewPort = ResolvePattern(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), 3);
			g_Offsets.m_CamGameplayDirector = ResolvePattern(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB"), 3);
			g_Offsets.m_BlipList = ResolvePattern(xorstr("4C 8D 05 ? ? ? ? 0F B7 C1"), 3);
			g_Offsets.m_MaxHealth = 0x284;

			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10A8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_PlayerId = 0xE8;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 3095;
			break;

		case 2944:
			/*g_Offsets.m_World = Mem.ModBase + 0x25B14B0;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1FBD4F0;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x201DBA0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x201ED50;
			g_Offsets.m_BlipList = Mem.ModBase + 0x2023400;*/

			g_Offsets.m_World = ResolvePattern(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), 3);
			g_Offsets.m_ReplayInterFace = ResolvePattern(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"), 3);
			g_Offsets.m_ViewPort = ResolvePattern(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), 3);
			g_Offsets.m_CamGameplayDirector = ResolvePattern(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB"), 3);
			g_Offsets.m_BlipList = ResolvePattern(xorstr("4C 8D 05 ? ? ? ? 0F B7 C1"), 3);

			g_Offsets.m_MaxHealth = 0x284;
			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10A8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_PlayerId = 0xE8;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 2944;

			break;

		case 2802:
			/*g_Offsets.m_World = Mem.ModBase + 0x254D448;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F5B820;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x1FBC100;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x1FBCFA8;
			g_Offsets.m_BlipList = Mem.ModBase + 0x1FBD6E0;*/

			g_Offsets.m_World = ResolvePattern(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), 3);
			g_Offsets.m_ReplayInterFace = ResolvePattern(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"), 3);
			g_Offsets.m_ViewPort = ResolvePattern(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), 3);
			g_Offsets.m_CamGameplayDirector = ResolvePattern(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB"), 3);
			g_Offsets.m_BlipList = ResolvePattern(xorstr("4C 8D 05 ? ? ? ? 0F B7 C1"), 3);
			g_Offsets.m_MaxHealth = 0x284;

			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10C8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 2802;
			break;

		case 2699:
			/*g_Offsets.m_World = Mem.ModBase + 0x26684D8;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x20304C8;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x20D8C90;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x20D9B38;
			g_Offsets.m_BlipList = Mem.ModBase + 0x20E1420;*/

			g_Offsets.m_World = ResolvePattern(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), 3);
			g_Offsets.m_ReplayInterFace = ResolvePattern(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"), 3);
			g_Offsets.m_ViewPort = ResolvePattern(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), 3);
			g_Offsets.m_CamGameplayDirector = ResolvePattern(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB"), 3);
			g_Offsets.m_BlipList = ResolvePattern(xorstr("4C 8D 05 ? ? ? ? 0F B7 C1"), 3);
			g_Offsets.m_MaxHealth = 0x284;
			g_Offsets.m_EntityType = 0x10B8;
			g_Offsets.m_FragInst = 0x1450;
			g_Offsets.m_Armor = 0x1530;
			g_Offsets.m_PedFlag = 0x1464;

			g_Offsets.CurrentBuild = 2699;
			break;

		case 2612:
			/*g_Offsets.m_World = Mem.ModBase + 0x2567DB0;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F77EF0;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x1FD8570;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x1FD9418;
			g_Offsets.m_BlipList = Mem.ModBase + 0x1FDDD20;*/

			g_Offsets.m_World = ResolvePattern(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), 3);
			g_Offsets.m_ReplayInterFace = ResolvePattern(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"), 3);
			g_Offsets.m_ViewPort = ResolvePattern(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), 3);
			g_Offsets.m_CamGameplayDirector = ResolvePattern(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB"), 3);
			g_Offsets.m_BlipList = ResolvePattern(xorstr("4C 8D 05 ? ? ? ? 0F B7 C1"), 3);
			g_Offsets.m_MaxHealth = 0x284;
			g_Offsets.m_EntityType = 0x10B8;
			g_Offsets.m_FragInst = 0x1450;
			g_Offsets.m_Armor = 0x1530;
			g_Offsets.m_PedFlag = 0x1464;

			g_Offsets.CurrentBuild = 2612;
			break;

		case 2545:
			/*g_Offsets.m_World = Mem.ModBase + 0x25667E8;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F2E7A8;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x1FD6F70;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x1FD7E18;
			g_Offsets.m_BlipList = Mem.ModBase + 0x1FDF560;*/

			g_Offsets.m_World = ResolvePattern(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), 3);
			g_Offsets.m_ReplayInterFace = ResolvePattern(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"), 3);
			g_Offsets.m_ViewPort = ResolvePattern(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), 3);
			g_Offsets.m_CamGameplayDirector = ResolvePattern(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB"), 3);
			g_Offsets.m_BlipList = ResolvePattern(xorstr("4C 8D 05 ? ? ? ? 0F B7 C1"), 3);
			g_Offsets.m_MaxHealth = 0x2A0;
			g_Offsets.m_EntityType = 0x10B8;
			g_Offsets.m_FragInst = 0x1450;
			g_Offsets.m_Armor = 0x1530;
			g_Offsets.m_PedFlag = 0x1464;

			g_Offsets.CurrentBuild = 2545;
			break;

		case 2372:
			/*g_Offsets.m_World = Mem.ModBase + 0x252DCD8;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F05208;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x1F9E9F0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x1F9F898;
			g_Offsets.m_BlipList = Mem.ModBase + 0x1F9FFA0;*/

			g_Offsets.m_World = ResolvePattern(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), 3);
			g_Offsets.m_ReplayInterFace = ResolvePattern(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"), 3);
			g_Offsets.m_ViewPort = ResolvePattern(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), 3);
			g_Offsets.m_CamGameplayDirector = ResolvePattern(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB"), 3);
			g_Offsets.m_BlipList = ResolvePattern(xorstr("4C 8D 05 ? ? ? ? 0F B7 C1"), 3);
			g_Offsets.m_MaxHealth = 0x2A0;
			g_Offsets.m_EntityType = 0x10B8;
			g_Offsets.CurrentBuild = 2372;
			break;

		case 2189:
			/*g_Offsets.m_World = Mem.ModBase + 0x24E6D90;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1EE18A8;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x1F888C0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x1F89768;
			g_Offsets.m_BlipList = Mem.ModBase + 0x1F6EF80;*/

			g_Offsets.m_World = ResolvePattern(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), 3);
			g_Offsets.m_ReplayInterFace = ResolvePattern(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"), 3);
			g_Offsets.m_ViewPort = ResolvePattern(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), 3);
			g_Offsets.m_CamGameplayDirector = ResolvePattern(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB"), 3);
			g_Offsets.m_BlipList = ResolvePattern(xorstr("4C 8D 05 ? ? ? ? 0F B7 C1"), 3);
			g_Offsets.m_MaxHealth = 0x2A0;
			g_Offsets.m_EntityType = 0x10B8;
			g_Offsets.m_PlayerId = 0x68;
			g_Offsets.m_Speed = 0xCD0;

			g_Offsets.CurrentBuild = 2189;
			break;

		case 2060:
			/*g_Offsets.m_World = Mem.ModBase + 0x24C8858;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1EC3828;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x1F6A7E0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x1F6B940;
			g_Offsets.m_BlipList = Mem.ModBase + 0x1F4F940;*/

			g_Offsets.m_World = ResolvePattern(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), 3);
			g_Offsets.m_ReplayInterFace = ResolvePattern(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"), 3);
			g_Offsets.m_ViewPort = ResolvePattern(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), 3);
			g_Offsets.m_CamGameplayDirector = ResolvePattern(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB"), 3);
			g_Offsets.m_BlipList = ResolvePattern(xorstr("4C 8D 05 ? ? ? ? 0F B7 C1"), 3);
			g_Offsets.m_MaxHealth = 0x2A0;
			g_Offsets.m_EntityType = 0x10B8;
			g_Offsets.m_PlayerId = 0x68;
			g_Offsets.m_Speed = 0xCD0;

			g_Offsets.CurrentBuild = 2060;
			break;

		case 1604:
			/*g_Offsets.m_World = Mem.ModBase + 0x247F840;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1EFD4C8;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x2087780;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x20888E0;
			g_Offsets.m_BlipList = Mem.ModBase + 0x206B4E0;*/

			g_Offsets.m_World = ResolvePattern(xorstr("48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B"), 3);
			g_Offsets.m_ReplayInterFace = ResolvePattern(xorstr("48 8D 0D ? ? ? ? 48 ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? 8A D8 E8 ? ? ? ? 84 DB 75 13 48 8D 0D ? ? ? ? 48 8B D7 E8 ? ? ? ? 84 C0 74 BC 8B 8F"), 3);
			g_Offsets.m_ViewPort = ResolvePattern(xorstr("48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD"), 3);
			g_Offsets.m_CamGameplayDirector = ResolvePattern(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB"), 3);
			g_Offsets.m_BlipList = ResolvePattern(xorstr("4C 8D 05 ? ? ? ? 0F B7 C1"), 3);

			g_Offsets.m_MaxHealth = 0x284;

			g_Offsets.m_WeaponManager = 0x10C8;
			g_Offsets.m_LastVehicle = 0xD28;
			g_Offsets.m_PlayerInfo = 0x10B8;
			g_Offsets.m_PlayerId = 0x68;
			g_Offsets.m_FragInst = 0x13E0;

			g_Offsets.CurrentBuild = 1604;
			break;
		}

		if (g_Offsets.CurrentBuild != 0)
		{
			{
				uintptr_t playerSectorPosNode = Mem.FindSignatureStr("40 53 48 83 EC ? 48 8B 02 4C 8B C2 48 8B D9 48 8B D1 49 8B C8 FF 50 ? 33 C9 66 39 8B ? ? ? ? 89 8B");

				if (playerSectorPosNode) {
					uintptr_t start = Mem.ModBase;
					size_t size = 0x10000000;

					std::vector<uint8_t> buffer(size);
					Mem.ReadChunkToBuffer(start, (char*)buffer.data(), size);

					for (size_t i = 0; i < size - 8; i++) {
						uint8_t opcode = buffer[i];

						if (opcode == 0xE8 || opcode == 0xE9) {
							int32_t relOffset = *reinterpret_cast<int32_t*>(&buffer[i + 1]);
							uintptr_t addr = start + i;
							uintptr_t resolvedAddr = addr + 5 + relOffset;

							if (resolvedAddr == playerSectorPosNode) {
								if (buffer[i + 7] == 0x4B) {
									g_Offsets.playerPosWriteData = addr - 0x38;
									break;
								}
							}
						}
					}
				}
				g_Offsets.m_IsFreeAimingatPed = Mem.FindSignatureStr("48 89 5C 24 ? 57 48 83 EC ? 33 DB 38 1D ? ? ? ? 74");
				g_Offsets.m_IsCutscenePlaying = Mem.FindSignature({ 0x8A, 0x81, 0x63, 0x0C, 0x00, 0x00, 0xC3 });
				g_Offsets.m_solo_session_start = Mem.FindSignature({ 0x48, 0x83, 0xEC, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC0, 0x74, 0x00, 0x48, 0x8B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC9, 0x74, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4 });
				g_Offsets.m_solo_session_end = Mem.FindSignature({ 0x48, 0x83, 0xEC, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC0, 0x74, 0x00, 0x48, 0x8B, 0x88, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC9, 0x74, 0x00, 0x80, 0xB9 });
				g_Offsets.m_GetFinalRenderedCamRot = Mem.FindSignatureStr(xorstr("48 89 5C 24 08 57 48 83 EC 30 8B DA 48 8B F9 E8 ? ? ? ? 48 8D 4C 24 20"));
				g_Offsets.m_Occlusion = Mem.ScanOffset("48 8D 1D ? ? ? ? FF C1", true, false);
				g_Offsets.m_Invisible = Mem.FindSignatureStr("f3 0f 10 0a 48 8b 59");
				g_Offsets.PatternAddr = Mem.FindSignature({ 0x48, 0x8D, 0x1D, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xC1 });
				g_Offsets.PatternAddrAlt = Mem.FindSignature({ 0x48, 0x8D, 0x1D, 0x00, 0x00, 0x00, 0x00 });
				g_Offsets.g_IsPedRagdollFuncAddr = Mem.FindSignatureStr(xorstr("48 8D 64 24 F8 48 89 2C 24 48 8D 2D ?? ?? ?? ?? 48 87 2C 24 48 8D 64 24 08 FF 64 24 F8 50 51 48 8B 44 24 10 48 B9 ?? ?? ?? ?? ?? ?? ?? ?? E9 ?? ?? ?? ?? 48 ?? ?? 89 01"));
				g_Offsets.foundAddr = Mem.FindSignatureStr(xorstr("21 4D D8 21 4D DC 41 8B D8"));
				g_Offsets.Addr4 = Mem.FindSignatureStr(xorstr("FF 57 08 48 8B 4D ?? 33 FF 48 85 C9 74 ?? 48 8D 55 ?? E8 ?? ?? ?? ?? 48 8B"));

				g_Offsets.m_VehicleFuelLevel = 0x814;

				g_Offsets.m_GiveWeapon = Mem.FindSignature(
					{ 0x48, 0x89, 0x5c, 0x24, 0x00 , 0x48, 0x89, 0x6c, 0x24, 0x00 , 0x48, 0x89, 0x74, 0x24, 0x00 , 0x57, 0x48, 0x83, 0xec, 0x00 , 0x41, 0x8b, 0xf0, 0x8b, 0xfa, 0x48, 0x8b, 0xd9, 0xe8 }
				);

				g_Offsets.m_HandleBullet = Mem.FindSignature({ 0xF3, 0x41, 0x0F, 0x10, 0x19, 0xF3, 0x41, 0x0F, 0x10, 0x41, 0x04 });

				g_Offsets.m_Setentityhealth = Mem.FindSignature(
					{ 0x48, 0x89, 0x5C, 0x24, 0x00, 0x48, 0x89, 0x74, 0x24, 0x00,0x57, 0x48, 0x83, 0xEC, 0x00, 0x0F, 0x29, 0x74, 0x24, 0x00,0x0F, 0x28, 0xF1, 0x48, 0x8B, 0xF1, 0x0F, 0x2F, 0xB1 }
				);
				g_Offsets.m_Camera = Core::SDK::Game::resolveRelativeAddress(Mem.FindSignatureStr(xorstr("4C 8B 35 ? ? ? ? 33 FF 32 DB")), 3, 7);

				//0F 29 4F ?? 83 8F ?? ?? ?? ?? ?? 48 8B 4F
				g_Offsets.m_MagicBulletsPatch = Mem.FindSignature(
					{ 0x0F, 0x29, 0x4F, 0x00, 0x83, 0x8F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x4F }
				);

				//E8 ?? ?? ?? ?? 48 83 C3 60 48 FF CF 75 E6 48 8B 5C 24
				g_Offsets.m_ArmsKinematics = Mem.FindSignature(
					{ 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC3, 0x60, 0x48, 0xFF, 0xCF, 0x75, 0xE6, 0x48, 0x8B, 0x5C, 0x24 }
				);

				//E8 ?? ?? ?? ?? 48 83 C3 60 48 FF CF 75 DF 48 8B 5C 24 ?? 48 8B 6C 24 ?? 48 8B 74 24
				g_Offsets.m_LegsKinematics = Mem.FindSignature(
					{ 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC3, 0x60, 0x48, 0xFF, 0xCF, 0x75, 0xDF, 0x48, 0x8B, 0x5C, 0x24, 0x00, 0x48, 0x8B, 0x6C, 0x24, 0x00, 0x48, 0x8B, 0x74, 0x24 }
				);

				//48 8D 45 ?? F3 0F 10 00 F3 0F 10 48 ?? F3 0F 11 45
				g_Offsets.m_SilentAim = Mem.FindSignature(
					{ 0x48, 0x8D, 0x45, 0x00, 0xF3, 0x0F, 0x10, 0x00, 0xF3, 0x0F, 0x10, 0x48, 0x00, 0xF3, 0x0F, 0x11, 0x45 }
				);

				//89 81 ?? ?? ?? ?? 8B 87 ?? ?? ?? ?? F7 D0
				g_Offsets.m_InfiniteCombatRoll = Mem.FindSignature(
					{ 0x89, 0x81, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x87, 0x00, 0x00, 0x00, 0x00, 0xF7, 0xD0 }
				);

				//41 2B C9 3B C8 0F 4D C8
				g_Offsets.m_InfiniteAmmo0 = Mem.FindSignature(
					{ 0x41, 0x2B, 0xC9, 0x3B, 0xC8, 0x0F, 0x4D, 0xC8 }
				);

				//41 2B D1 E8
				g_Offsets.m_InfiniteAmmo1 = Mem.FindSignature(
					{ 0x41, 0x2B, 0xD1, 0xE8 }
				);


				//48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 48 85 C9 74 05 E8 ?? ?? ?? ?? 8A CB
				g_Offsets.m_AimCPedPatternResult = Mem.FindSignature(
					{ 0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC9, 0x74, 0x05, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x8A, 0xCB }
				);


			}

			//Offsets Update
			{
				//0f 2f c2 77 ? f3 0f 10 83 ? ? ? ? 48 83 c4
				uintptr_t SpeedSigAddr = Mem.FindSignature({ 0x0f, 0x2f, 0xc2, 0x77, 0x00, 0xf3, 0x0f, 0x10, 0x83, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xc4 });
				g_Offsets.m_Speed = Mem.Read<int>(SpeedSigAddr + 9);

				//f3 0f 11 80 ? ? ? ? 48 8b 87 ? ? ? ? f3 0f 10 05
				uintptr_t VehicleEngineSigAddr = Mem.FindSignature({ 0xf3, 0x0f, 0x11, 0x80, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0x87, 0x00, 0x00, 0x00, 0x00, 0xf3, 0x0f, 0x10, 0x05 });
				g_Offsets.m_VehicleEngineHealth = Mem.Read<int>(VehicleEngineSigAddr + 4);

				//f3 41 0f 10 b0 ? ? ? ? 48 8b ca
				uintptr_t SpreadSigAddr = Mem.FindSignature({ 0xf3, 0x41, 0x0f, 0x10, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0xca });
				g_Offsets.m_Spread = Mem.Read<int>(SpreadSigAddr + 5);

				//f3 0f 10 b0 ? ? ? ? 49 8b 87
				uintptr_t RecoilSigAddr = Mem.FindSignature({ 0xf3, 0x0f, 0x10, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x49, 0x8b, 0x87 });
				g_Offsets.m_Recoil = Mem.Read<int>(RecoilSigAddr + 4);

				//4c 8b 41 ? 4d 85 c9
				uintptr_t ObjectSigAddr = Mem.FindSignature({ 0x4c, 0x8b, 0x41, 0x00, 0x4d, 0x85, 0xc9 });
				g_Offsets.m_CObject = (int)Mem.Read<BYTE>(ObjectSigAddr + 3);


				//48 8b 8b ? ? ? ? 48 8b d7 e8 ? ? ? ? 48 8b 8b ? ? ? ? 48 8b d7 e8
				uintptr_t CWeaponSigAddr = Mem.FindSignature({ 0x48, 0x8b, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0xd7, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0xd7, 0xe8 });
				g_Offsets.m_CWeapon = Mem.Read<int>(CWeaponSigAddr + 3);

				//4c 8b 94 c7 ? ? ? ? eb ? 4d 8b d6 41 8a 82
				uintptr_t VehicleSigAddr = Mem.FindSignature({ 0x4c, 0x8b, 0x94, 0xc7, 0x00, 0x00, 0x00, 0x00, 0xeb, 0x00, 0x4d, 0x8b, 0xd6, 0x41, 0x8a, 0x82 });
				g_Offsets.m_VehicleDriver = Mem.Read<int>(VehicleSigAddr + 4);

				//8b 81 ? ? ? ? 83 e0 ? c1 e0 ? 3d ? ? ? ? 0f 8f
				uintptr_t RagDollSigAddr = Mem.FindSignature({ 0x8b, 0x81, 0x00, 0x00, 0x00, 0x00, 0x83, 0xe0, 0x00, 0xc1, 0xe0, 0x00, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x8f });
				g_Offsets.m_NoRagDoll = Mem.Read<int>(RagDollSigAddr + 2);

				//f6 81 ? ? ? ? ? 75 ? 8b 83
				uintptr_t SeatBealtAddr = Mem.FindSignature({ 0xf6, 0x81,0x00 ,0x00 ,0x00 ,0x00 ,0x00 , 0x75,0x00 , 0x8b, 0x83 });
				g_Offsets.m_SeatBealt = Mem.Read<int>(SeatBealtAddr + 2);

				//74 ? 83 b9 ? ? ? ? ? 75 ? 45 84 f6
				uintptr_t VehicleDoorsLockAddr = Mem.FindSignature({ 0x74, 0x00, 0x83, 0xb9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x75, 0x00, 0x45, 0x84, 0xf6 });
				g_Offsets.m_VehicleDoorsLockState = Mem.Read<int>(VehicleDoorsLockAddr + 4);

				//48 8b 83 ? ? ? ? 8b d5 48 89 44 24 ? 4c 89 74 24
				uintptr_t HandlingSigAddr = Mem.FindSignature({ 0x48, 0x8b, 0x83,0x00,0x00,0x00,0x00, 0x8b, 0xd5, 0x48, 0x89, 0x44, 0x24,0x00, 0x4c, 0x89, 0x74, 0x24 });
				g_Offsets.m_Handling = Mem.Read<int>(HandlingSigAddr + 3);

			}
		}

		return HasValidCoreOffsets();
	}

	inline void GetOffsets()
	{
		g_Offsets.m_WeaponManager = 0x10D8;
		g_Offsets.m_WeaponInfo = 0x20;
		g_Offsets.m_LastVehicle = 0xD30;
		g_Offsets.m_PlayerInfo = 0x10C8;
		g_Offsets.m_PlayerId = 0x88;
		g_Offsets.m_FragInst = 0x1400;
		g_Offsets.m_Armor = 0x14E0;
		g_Offsets.m_PedFlag = 0x1414;


		if (g_Offsets.CurrentBuild == 2060)
		{
			g_Offsets.m_World = Mem.ModBase + 0x24C8858;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1EC3828;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x1F6A7E0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x1F6B940;
			g_Offsets.m_BlipList = Mem.ModBase + 0x1F4F940;
			g_Offsets.m_MaxHealth = 0x2A0;
			g_Offsets.m_EntityType = 0x10B8;
			g_Offsets.m_PlayerId = 0x68;
			g_Offsets.m_Speed = 0xCD0;

			g_Offsets.CurrentBuild = 2060;
		}
		else if (g_Offsets.CurrentBuild == 2189)
		{
			g_Offsets.m_World = Mem.ModBase + 0x24E6D90;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1EE18A8;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x1F888C0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x1F89768;
			g_Offsets.m_BlipList = Mem.ModBase + 0x1F6EF80;
			g_Offsets.m_MaxHealth = 0x2A0;
			g_Offsets.m_EntityType = 0x10B8;
			g_Offsets.m_PlayerId = 0x68;
			g_Offsets.m_Speed = 0xCD0;

			g_Offsets.CurrentBuild = 2189;
		}
		else if (g_Offsets.CurrentBuild == 2372)
		{
			g_Offsets.m_World = Mem.ModBase + 0x252DCD8;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F05208;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x1F9E9F0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x1F9F898;
			g_Offsets.m_BlipList = Mem.ModBase + 0x1F9FFA0;
			g_Offsets.m_MaxHealth = 0x2A0;
			g_Offsets.m_EntityType = 0x10B8;
			g_Offsets.CurrentBuild = 2372;
		}
		else if (g_Offsets.CurrentBuild == 2545)
		{
			g_Offsets.m_World = Mem.ModBase + 0x25667E8;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F2E7A8;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x1FD6F70;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x1FD7E18;
			g_Offsets.m_BlipList = Mem.ModBase + 0x1FDF560;
			g_Offsets.m_MaxHealth = 0x2A0;
			g_Offsets.m_EntityType = 0x10B8;
			g_Offsets.m_FragInst = 0x1450;
			g_Offsets.m_Armor = 0x1530;
			g_Offsets.m_PedFlag = 0x1464;

			g_Offsets.CurrentBuild = 2545;
		}
		else if (g_Offsets.CurrentBuild == 2612)
		{
			g_Offsets.m_World = Mem.ModBase + 0x2567DB0;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F77EF0;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x1FD8570;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x1FD9418;
			g_Offsets.m_BlipList = Mem.ModBase + 0x1FDDD20;
			g_Offsets.m_MaxHealth = 0x284;
			g_Offsets.m_EntityType = 0x10B8;
			g_Offsets.m_FragInst = 0x1450;
			g_Offsets.m_Armor = 0x1530;
			g_Offsets.m_PedFlag = 0x1464;

			g_Offsets.CurrentBuild = 2612;
		}
		else if (g_Offsets.CurrentBuild == 2699)
		{
			g_Offsets.m_World = Mem.ModBase + 0x26684D8;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x20304C8;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x20D8C90;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x20D9B38;
			g_Offsets.m_BlipList = Mem.ModBase + 0x20E1420;
			g_Offsets.m_MaxHealth = 0x284;
			g_Offsets.m_EntityType = 0x10B8;
			g_Offsets.m_FragInst = 0x1450;
			g_Offsets.m_Armor = 0x1530;
			g_Offsets.m_PedFlag = 0x1464;

			g_Offsets.CurrentBuild = 2699;
		}
		else if (g_Offsets.CurrentBuild == 2802)
		{
			g_Offsets.m_World = Mem.ModBase + 0x254D448;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F5B820;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x1FBC100;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x1FBCFA8;
			g_Offsets.m_BlipList = Mem.ModBase + 0x1FBD6E0;
			g_Offsets.m_MaxHealth = 0x284;

			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10C8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 2802;
		}
		else if (g_Offsets.CurrentBuild == 2944)
		{
			g_Offsets.m_World = Mem.ModBase + 0x25B14B0;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1FBD4F0;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x201DBA0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x201ED50;
			g_Offsets.m_BlipList = Mem.ModBase + 0x2023400;
			g_Offsets.m_MaxHealth = 0x284;

			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10A8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_PlayerId = 0xE8;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 2944;

		}
		else if (g_Offsets.CurrentBuild == 3095)
		{
			g_Offsets.m_World = Mem.ModBase + 0x2593320;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F58B58;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x20019E0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x2002888;
			g_Offsets.m_BlipList = Mem.ModBase + 0x2002FA0;
			g_Offsets.m_MaxHealth = 0x284;

			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10A8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_PlayerId = 0xE8;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 3095;
		}
		else if (g_Offsets.CurrentBuild == 3258)
		{
			g_Offsets.m_World = Mem.ModBase + 0x25b14b0;;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1FBD4F0;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x201dba0;;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x201ED50;
			g_Offsets.m_BlipList = Mem.ModBase + 0x2023400;

			g_Offsets.m_MaxHealth = 0x284;
			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10A8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_PlayerId = 0xE8;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 3258;
		}
		else if (g_Offsets.CurrentBuild == 3323)
		{
			g_Offsets.m_World = Mem.ModBase + 0x25C15B0;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F85458;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x202DC50;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x202EB48;
			g_Offsets.m_BlipList = Mem.ModBase + 0x20333E0;

			g_Offsets.m_MaxHealth = 0x284;
			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10A8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_PlayerId = 0xE8;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 3323;
		}
		else if (g_Offsets.CurrentBuild == 3407)
		{
			g_Offsets.m_World = Mem.ModBase + 0x25D7108;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1F9A9D8;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x20431C0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x20440C8;
			g_Offsets.m_BlipList = Mem.ModBase + 0x2047D50;
			g_Offsets.m_MaxHealth = 0x284;

			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10A8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_PlayerId = 0xE8;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 3407;
		}
		else if (g_Offsets.CurrentBuild == 3570)
		{
			g_Offsets.m_World = Mem.ModBase + 0x25EC580;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1FB0418;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x2058BA0;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x2059778;
			g_Offsets.m_BlipList = Mem.ModBase + 0x0;
			g_Offsets.m_MaxHealth = 0x284;
			g_Offsets.m_WeaponManager = 0x10B8;
			g_Offsets.m_EntityType = 0x1098;
			g_Offsets.m_LastVehicle = 0xD10;
			g_Offsets.m_PlayerInfo = 0x10A8;
			g_Offsets.m_FragInst = 0x1430;
			g_Offsets.m_PlayerId = 0xE8;
			g_Offsets.m_Armor = 0x150C;
			g_Offsets.m_PedFlag = 0x1444;

			g_Offsets.CurrentBuild = 3570;
		}

		else {
			g_Offsets.m_World = Mem.ModBase + 0x247F840;
			g_Offsets.m_ReplayInterFace = Mem.ModBase + 0x1EFD4C8;
			g_Offsets.m_ViewPort = Mem.ModBase + 0x2087780;
			g_Offsets.m_CamGameplayDirector = Mem.ModBase + 0x20888E0;
			g_Offsets.m_BlipList = Mem.ModBase + 0x206B4E0;
			g_Offsets.m_MaxHealth = 0x284;

			g_Offsets.m_WeaponManager = 0x10C8;
			g_Offsets.m_LastVehicle = 0xD28;
			g_Offsets.m_PlayerInfo = 0x10B8;
			g_Offsets.m_PlayerId = 0x68;
			g_Offsets.m_FragInst = 0x13E0;

			g_Offsets.CurrentBuild = 1604;
		}

		if (g_Offsets.CurrentBuild != 0)
		{
			//Functions Signatures
			{
				//48 89 5c 24 ? 48 89 6c 24 ? 48 89 74 24 ? 57 48 83 ec ? 41 8b f0 8b fa 48 8b d9 e8
				g_Offsets.m_GiveWeapon = Mem.FindSignature(
					{ 0x48, 0x89, 0x5c, 0x24, 0x00 , 0x48, 0x89, 0x6c, 0x24, 0x00 , 0x48, 0x89, 0x74, 0x24, 0x00 , 0x57, 0x48, 0x83, 0xec, 0x00 , 0x41, 0x8b, 0xf0, 0x8b, 0xfa, 0x48, 0x8b, 0xd9, 0xe8 }
				);

				//0F 29 4F ?? 83 8F ?? ?? ?? ?? ?? 48 8B 4F
				g_Offsets.m_MagicBulletsPatch = Mem.FindSignature(
					{ 0x0F, 0x29, 0x4F, 0x00, 0x83, 0x8F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x4F }
				);

				//E8 ?? ?? ?? ?? 48 83 C3 60 48 FF CF 75 E6 48 8B 5C 24
				g_Offsets.m_ArmsKinematics = Mem.FindSignature(
					{ 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC3, 0x60, 0x48, 0xFF, 0xCF, 0x75, 0xE6, 0x48, 0x8B, 0x5C, 0x24 }
				);

				//E8 ?? ?? ?? ?? 48 83 C3 60 48 FF CF 75 DF 48 8B 5C 24 ?? 48 8B 6C 24 ?? 48 8B 74 24
				g_Offsets.m_LegsKinematics = Mem.FindSignature(
					{ 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC3, 0x60, 0x48, 0xFF, 0xCF, 0x75, 0xDF, 0x48, 0x8B, 0x5C, 0x24, 0x00, 0x48, 0x8B, 0x6C, 0x24, 0x00, 0x48, 0x8B, 0x74, 0x24 }
				);

				//48 8D 45 ?? F3 0F 10 00 F3 0F 10 48 ?? F3 0F 11 45
				g_Offsets.m_SilentAim = Mem.FindSignature(
					{ 0x48, 0x8D, 0x45, 0x00, 0xF3, 0x0F, 0x10, 0x00, 0xF3, 0x0F, 0x10, 0x48, 0x00, 0xF3, 0x0F, 0x11, 0x45 }
				);

				//89 81 ?? ?? ?? ?? 8B 87 ?? ?? ?? ?? F7 D0
				g_Offsets.m_InfiniteCombatRoll = Mem.FindSignature(
					{ 0x89, 0x81, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x87, 0x00, 0x00, 0x00, 0x00, 0xF7, 0xD0 }
				);

				//41 2B C9 3B C8 0F 4D C8
				g_Offsets.m_InfiniteAmmo0 = Mem.FindSignature(
					{ 0x41, 0x2B, 0xC9, 0x3B, 0xC8, 0x0F, 0x4D, 0xC8 }
				);

				//41 2B D1 E8
				g_Offsets.m_InfiniteAmmo1 = Mem.FindSignature(
					{ 0x41, 0x2B, 0xD1, 0xE8 }
				);
				//48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 48 85 C9 74 05 E8 ?? ?? ?? ?? 8A CB
				g_Offsets.m_AimCPedPatternResult = Mem.FindSignature(
					{ 0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC9, 0x74, 0x05, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x8A, 0xCB }
				);
			}
			//Offsets Update
			{
				//0f 2f c2 77 ? f3 0f 10 83 ? ? ? ? 48 83 c4
				uintptr_t SpeedSigAddr = Mem.FindSignature({ 0x0f, 0x2f, 0xc2, 0x77, 0x00, 0xf3, 0x0f, 0x10, 0x83, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xc4 });
				g_Offsets.m_Speed = Mem.Read<int>(SpeedSigAddr + 9);

				//f3 0f 11 80 ? ? ? ? 48 8b 87 ? ? ? ? f3 0f 10 05
				uintptr_t VehicleEngineSigAddr = Mem.FindSignature({ 0xf3, 0x0f, 0x11, 0x80, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0x87, 0x00, 0x00, 0x00, 0x00, 0xf3, 0x0f, 0x10, 0x05 });
				g_Offsets.m_VehicleEngineHealth = Mem.Read<int>(VehicleEngineSigAddr + 4);

				//f3 41 0f 10 b0 ? ? ? ? 48 8b ca
				uintptr_t SpreadSigAddr = Mem.FindSignature({ 0xf3, 0x41, 0x0f, 0x10, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0xca });
				g_Offsets.m_Spread = Mem.Read<int>(SpreadSigAddr + 5);

				//f3 0f 10 b0 ? ? ? ? 49 8b 87
				uintptr_t RecoilSigAddr = Mem.FindSignature({ 0xf3, 0x0f, 0x10, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x49, 0x8b, 0x87 });
				g_Offsets.m_Recoil = Mem.Read<int>(RecoilSigAddr + 4);

				//4c 8b 41 ? 4d 85 c9
				uintptr_t ObjectSigAddr = Mem.FindSignature({ 0x4c, 0x8b, 0x41, 0x00, 0x4d, 0x85, 0xc9 });
				g_Offsets.m_CObject = (int)Mem.Read<BYTE>(ObjectSigAddr + 3);


				//48 8b 8b ? ? ? ? 48 8b d7 e8 ? ? ? ? 48 8b 8b ? ? ? ? 48 8b d7 e8
				uintptr_t CWeaponSigAddr = Mem.FindSignature({ 0x48, 0x8b, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0xd7, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0xd7, 0xe8 });
				g_Offsets.m_CWeapon = Mem.Read<int>(CWeaponSigAddr + 3);

				//4c 8b 94 c7 ? ? ? ? eb ? 4d 8b d6 41 8a 82
				uintptr_t VehicleSigAddr = Mem.FindSignature({ 0x4c, 0x8b, 0x94, 0xc7, 0x00, 0x00, 0x00, 0x00, 0xeb, 0x00, 0x4d, 0x8b, 0xd6, 0x41, 0x8a, 0x82 });
				g_Offsets.m_VehicleDriver = Mem.Read<int>(VehicleSigAddr + 4);

				//8b 81 ? ? ? ? 83 e0 ? c1 e0 ? 3d ? ? ? ? 0f 8f
				uintptr_t RagDollSigAddr = Mem.FindSignature({ 0x8b, 0x81, 0x00, 0x00, 0x00, 0x00, 0x83, 0xe0, 0x00, 0xc1, 0xe0, 0x00, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x8f });
				g_Offsets.m_NoRagDoll = Mem.Read<int>(RagDollSigAddr + 2);

				//f6 81 ? ? ? ? ? 75 ? 8b 83
				uintptr_t SeatBealtAddr = Mem.FindSignature({ 0xf6, 0x81,0x00 ,0x00 ,0x00 ,0x00 ,0x00 , 0x75,0x00 , 0x8b, 0x83 });
				g_Offsets.m_SeatBealt = Mem.Read<int>(SeatBealtAddr + 2);

				//74 ? 83 b9 ? ? ? ? ? 75 ? 45 84 f6
				uintptr_t VehicleDoorsLockAddr = Mem.FindSignature({ 0x74, 0x00, 0x83, 0xb9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x75, 0x00, 0x45, 0x84, 0xf6 });
				g_Offsets.m_VehicleDoorsLockState = Mem.Read<int>(VehicleDoorsLockAddr + 4);

				//48 8b 83 ? ? ? ? 8b d5 48 89 44 24 ? 4c 89 74 24
				uintptr_t HandlingSigAddr = Mem.FindSignature({ 0x48, 0x8b, 0x83,0x00,0x00,0x00,0x00, 0x8b, 0xd5, 0x48, 0x89, 0x44, 0x24,0x00, 0x4c, 0x89, 0x74, 0x24 });
				g_Offsets.m_Handling = Mem.Read<int>(HandlingSigAddr + 3);

			}
		}
	}

}
