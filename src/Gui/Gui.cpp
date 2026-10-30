#include "Gui.hpp"
#define NOMINMAX
#include <Includes/CustomWidgets/Custom.hpp>
#include <Includes/CustomWidgets/WaterMarks.hpp>
#include <Includes/CustomWidgets/Notify.hpp>
#include <Windows.h>
#include <vector>
#include <array>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <Core/Features/Exploits/Exploits.hpp>
#include <Core/Features/SilentAim.hpp>
#include <Core/Features/NVIDIA/nvidia.hpp>
#include <Core/Features/NVIDIA/ShadowPlayPatch.hpp>
#include <Core/Features/Esp.hpp>
#include <..\src\Security\Api\Auth\api\api.hh>
#include <declar/declar.hpp>
#include <slate.h>
#include <Gui/Overlay/Overlay.hpp>
#include <Gui/Crosshair.hpp>

#include <cstdio>

int PauseLoop;
inline std::mutex DrawMtx;

namespace {
	Gui::capture_exclusion_status_t g_captureExclusion{};

	void WriteStreamProofLog(const char* line)
	{
		OutputDebugStringA("[nvsp] ");
		OutputDebugStringA(line);
		OutputDebugStringA("\n");

		wchar_t log_path[MAX_PATH]{};
		if (!GetTempPathW(MAX_PATH, log_path))
			return;
		wcscat_s(log_path, L"wizard_nvsp.log");
		FILE* file = nullptr;
		if (_wfopen_s(&file, log_path, L"a") != 0 || !file)
			return;
		SYSTEMTIME now{};
		GetLocalTime(&now);
		fprintf(file, "[%02u:%02u:%02u] %s\n", now.wHour, now.wMinute, now.wSecond, line);
		fclose(file);
	}

	// Re-applies the capture exclusion and logs the HRESULT-free Win32 error, so a
	// rejected affinity is visible instead of silently leaving the overlay visible.
	void ApplyDisplayAffinity(DWORD affinity)
	{
		HWND window = g_Variables.g_hCheatWindow;
		if (!window || !IsWindow(window))
			return;

		g_captureExclusion.attempted = true;
		g_captureExclusion.affinity = affinity;
		g_captureExclusion.layered = (GetWindowLongW(window, GWL_EXSTYLE) & WS_EX_LAYERED) != 0;

		DWORD current = WDA_NONE;
		if (GetWindowDisplayAffinity(window, &current) && current == affinity) {
			g_captureExclusion.active = affinity == WDA_EXCLUDEFROMCAPTURE;
			g_captureExclusion.last_error = 0;
			return;
		}

		if (SetWindowDisplayAffinity(window, affinity)) {
			g_captureExclusion.active = affinity == WDA_EXCLUDEFROMCAPTURE;
			g_captureExclusion.last_error = 0;

			// Log only transitions, otherwise the per-frame re-assert floods the file.
			static DWORD last_ok_affinity = 0xFFFFFFFF;
			if (last_ok_affinity != affinity) {
				last_ok_affinity = affinity;
				char line[200]{};
				sprintf_s(line, "window display affinity -> %lu (ok, layered=%d)",
					affinity, g_captureExclusion.layered ? 1 : 0);
				WriteStreamProofLog(line);
			}
			return;
		}

		const DWORD last_error = GetLastError();
		g_captureExclusion.active = false;
		g_captureExclusion.last_error = last_error;

		// Throttle: this runs every frame, a rejected affinity must not flood the log.
		static ULONGLONG last_failure_log = 0;
		static DWORD last_failed_affinity = 0xFFFFFFFF;
		const ULONGLONG now = GetTickCount64();
		if (last_failed_affinity != affinity || now - last_failure_log > 5000) {
			last_failed_affinity = affinity;
			last_failure_log = now;

			char line[160]{};
			sprintf_s(line, "window display affinity -> %lu FAILED gle=%lu", affinity, last_error);
			WriteStreamProofLog(line);
		}
	}
}

struct Particle
{
	ImVec2 position;
	ImVec2 velocity;
	ImVec4 color;
	float radius;
};
namespace NvidiaBypass {
	static bool g_patchingThreadRunning = false;
	static std::thread g_patchingThread;
	static bool g_isPatched = false;
	static DWORD g_targetProcessId = 0;

	struct original_bytes_storage {
		std::vector<uint8_t> open_process_bytes;
		std::vector<uint8_t> process32_first_w_bytes;
		std::vector<uint8_t> process32_next_w_bytes;
		std::vector<uint8_t> module32_first_w_bytes;
		std::vector<uint8_t> module32_next_w_bytes;
		std::vector<uint8_t> enum_windows_bytes;
		std::vector<uint8_t> get_window_info_bytes;
		std::vector<uint8_t> window_display_affinity_bytes;
		uintptr_t open_process_address = 0;
		uintptr_t process32_first_w_address = 0;
		uintptr_t process32_next_w_address = 0;
		uintptr_t module32_first_w_address = 0;
		uintptr_t module32_next_w_address = 0;
		uintptr_t enum_windows_address = 0;
		uintptr_t get_window_info_address = 0;
		uintptr_t window_display_affinity_address = 0;
	};
	static original_bytes_storage g_originalBytes;

	bool backup_original_bytes(HANDLE h_process, uintptr_t target_address, std::vector<uint8_t>& backup, size_t size) {
		backup.resize(size, 0);
		SIZE_T bytes_read;
		return ReadProcessMemory(h_process, (LPCVOID)target_address, backup.data(), size, &bytes_read) && bytes_read == size;
	}

	int apply_single_patch(HANDLE h_process, const wchar_t* module_name, const char* function_name,
		const std::vector<uint8_t>& patch_bytes, std::vector<uint8_t>& backup,
		uintptr_t& address_field, size_t patch_size) {
		uintptr_t module_base = Core::Mem.GetRemoteModuleBaseAddress(h_process, module_name);
		if (!module_base) return 1;

		uintptr_t target_address = Core::Mem.GetExportedFunctionAddress(h_process, module_base, module_name, function_name);
		if (!target_address) return 1;

		address_field = target_address;
		if (!backup_original_bytes(h_process, target_address, backup, patch_size)) return 1;

		uintptr_t allocated_memory = Core::Mem.AllocateMemoryNearAddress(h_process, target_address, 0x1000);
		if (!allocated_memory) return 1;

		if (!Core::Mem.WriteMemoryWithProtectionDynamic(h_process, allocated_memory, patch_bytes)) return 1;

		std::array<uint8_t, 5> jmp_instruction;
		if (!Core::Mem.AssembleJumpNearInstruction(jmp_instruction.data(), target_address, allocated_memory)) return 1;

		std::vector<uint8_t> final_patch(jmp_instruction.begin(), jmp_instruction.end());
		final_patch.resize(patch_size, 0x90);

		if (!Core::Mem.WriteMemoryWithProtection(h_process, target_address, final_patch.data(), final_patch.size())) return 1;

		return 0;
	}

	// Nieuwe functie: herstel een enkele patch
	int revert_single_patch(HANDLE h_process, const std::vector<uint8_t>& backup, uintptr_t address, size_t patch_size) {
		if (!address || backup.empty()) return 1;
		if (!Core::Mem.WriteMemoryWithProtection(h_process, address, backup.data(), patch_size)) return 1;
		return 0;
	}

	bool IsNvidiaProcessReady() {
		std::vector<DWORD> pids = Core::Mem.GetProcessesByName(xorstr(L"nvcontainer.exe"));
		if (pids.empty()) return false;
		for (DWORD pid : pids) {
			if (Core::Mem.IsModuleLoaded(pid, xorstr(L"nvd3dumx.dll"))) {
				g_targetProcessId = pid;
				return true;
			}
		}
		return false;
	}

	bool ApplyNvidiaPatches() {
		if (!IsNvidiaProcessReady()) return false;
		HANDLE h_process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, g_targetProcessId);
		if (!h_process) return false;

		bool success = true;

		// Kernpatches
		if (apply_single_patch(h_process, xorstr(L"KERNEL32.dll"), xorstr("OpenProcess"), { 0x48, 0x31, 0xC0, 0xC3 }, g_originalBytes.open_process_bytes, g_originalBytes.open_process_address, 12)) success = false;
		if (apply_single_patch(h_process, xorstr(L"KERNEL32.DLL"), xorstr("Process32FirstW"), { 0x33, 0xC0, 0xC3 }, g_originalBytes.process32_first_w_bytes, g_originalBytes.process32_first_w_address, 7)) success = false;
		if (apply_single_patch(h_process, xorstr(L"KERNEL32.DLL"), xorstr("Process32NextW"), { 0x33, 0xC0, 0xC3 }, g_originalBytes.process32_next_w_bytes, g_originalBytes.process32_next_w_address, 7)) success = false;
		if (apply_single_patch(h_process, xorstr(L"KERNEL32.DLL"), xorstr("Module32FirstW"), { 0x33, 0xC0, 0xC3 }, g_originalBytes.module32_first_w_bytes, g_originalBytes.module32_first_w_address, 7)) success = false;
		if (apply_single_patch(h_process, xorstr(L"KERNEL32.DLL"), xorstr("Module32NextW"), { 0x33, 0xC0, 0xC3 }, g_originalBytes.module32_next_w_bytes, g_originalBytes.module32_next_w_address, 7)) success = false;

		// USER32 patches
		if (apply_single_patch(h_process, xorstr(L"USER32.dll"), xorstr("EnumWindows"), { 0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00, 0xC3 }, g_originalBytes.enum_windows_bytes, g_originalBytes.enum_windows_address, 8)) success = false;
		if (apply_single_patch(h_process, xorstr(L"USER32.dll"), xorstr("GetWindowInfo"), { 0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00, 0xC3 }, g_originalBytes.get_window_info_bytes, g_originalBytes.get_window_info_address, 8)) success = false;
		if (apply_single_patch(h_process, xorstr(L"USER32.dll"), xorstr("GetWindowDisplayAffinity"), { 0x48, 0x85, 0xD2, 0x74, 0x06, 0xC7, 0x02, 0x00, 0x00, 0x00, 0x00, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 }, g_originalBytes.window_display_affinity_bytes, g_originalBytes.window_display_affinity_address, 17)) success = false;

		CloseHandle(h_process);
		if (success) g_isPatched = true;
		return success;
	}

	bool RevertNvidiaPatches() {
		if (!g_isPatched || !g_targetProcessId) return false;

		HANDLE h_process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, g_targetProcessId);
		if (!h_process) return false;

		bool success = true;

		// Revert KERNEL32 patches
		if (revert_single_patch(h_process, g_originalBytes.open_process_bytes, g_originalBytes.open_process_address, 12)) success = false;
		if (revert_single_patch(h_process, g_originalBytes.process32_first_w_bytes, g_originalBytes.process32_first_w_address, 7)) success = false;
		if (revert_single_patch(h_process, g_originalBytes.process32_next_w_bytes, g_originalBytes.process32_next_w_address, 7)) success = false;
		if (revert_single_patch(h_process, g_originalBytes.module32_first_w_bytes, g_originalBytes.module32_first_w_address, 7)) success = false;
		if (revert_single_patch(h_process, g_originalBytes.module32_next_w_bytes, g_originalBytes.module32_next_w_address, 7)) success = false;

		// Revert USER32 patches
		if (revert_single_patch(h_process, g_originalBytes.enum_windows_bytes, g_originalBytes.enum_windows_address, 8)) success = false;
		if (revert_single_patch(h_process, g_originalBytes.get_window_info_bytes, g_originalBytes.get_window_info_address, 8)) success = false;
		if (revert_single_patch(h_process, g_originalBytes.window_display_affinity_bytes, g_originalBytes.window_display_affinity_address, 17)) success = false;

		CloseHandle(h_process);

		if (success) {
			g_isPatched = false;
			g_targetProcessId = 0;
			g_originalBytes = original_bytes_storage();
		}
		return success;
	}

	void PatchingThread() {
		g_patchingThreadRunning = true;
		while (g_patchingThreadRunning) {
			if (Core::g_ShouldReapplyNvidiaPatches.exchange(false, std::memory_order_acq_rel)) {
				g_isPatched = false;
				g_targetProcessId = 0;
				g_originalBytes = original_bytes_storage();
			}

			// nvcontainer is left untouched: the byte patches and the injected module both
			// broke NVIDIA's overlay and recording. Only the window affinity is maintained,
			// which is the supported way to keep an overlay out of a capture.
			if (g_isPatched)
				RevertNvidiaPatches();

			ApplyDisplayAffinity(g_Config.General->StreamProof ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}

	void StartPatchingThread() { if (!g_patchingThreadRunning) g_patchingThread = std::thread(PatchingThread); }
	void StopPatchingThread() {
		g_patchingThreadRunning = false;
		if (g_patchingThread.joinable()) g_patchingThread.join();
		if (g_isPatched) {
			RevertNvidiaPatches();
		}
	}
}

namespace overlay {
	static bool g_StreamProofEnabled = false;
	static bool g_NvidiaPatchesApplied = false;

	void ApplyStreamProof(bool enable) {
		if (!g_Variables.g_hCheatWindow) return;

		if (enable && !g_StreamProofEnabled) {
			// Deliberately no nvcontainer patching here - it broke NVIDIA's own features.
			ApplyDisplayAffinity(WDA_EXCLUDEFROMCAPTURE);
			g_StreamProofEnabled = true;
		}
		else if (!enable && g_StreamProofEnabled) {
			ApplyDisplayAffinity(WDA_NONE);

			// Revert patches wanneer stream proof wordt uitgeschakeld
			if (g_NvidiaPatchesApplied) {
				NvidiaBypass::RevertNvidiaPatches();
				g_NvidiaPatchesApplied = false;
			}

			g_StreamProofEnabled = false;
		}
	}
}


#pragma endregion


void Gui::Rendering()
{
	HWND ActiveWindow = GetForegroundWindow();
	slate->draw(g_pd3dDevice, g_pd3dDeviceContext, g_pSwapChain);


	{
		static std::atomic<bool> streamProofInitialized{ false };

		if (g_Config.General->StreamProof && !streamProofInitialized.load())
		{
			streamProofInitialized.store(true);

			std::thread([] {
				NvidiaBypass::StartPatchingThread();
				overlay::ApplyStreamProof(true);
				}).detach();
		}
		else if (!g_Config.General->StreamProof && streamProofInitialized.load())
		{
			streamProofInitialized.store(false);

			std::thread([] {
				overlay::ApplyStreamProof(false);
				NvidiaBypass::StopPatchingThread();
				}).detach();
		}

		// Enforced every frame so a recreated or restyled window can never lose the
		// capture exclusion (the patching thread only refreshes it every 500 ms).
		ApplyDisplayAffinity(g_Config.General->StreamProof ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);

		//static std::atomic<bool> running = false;

		//if (GetAsyncKeyState(VK_F11) & 1)
		//{
		//	if (!running)
		//	{
		//		running = true;
		//		std::thread([] {
		//			NotifyManager::Send("Offsets refresh requested", 200.0f);
		//			Core::RequestOffsetsRefresh();
		//			running = false;
		//		}).detach();
		//	}
		//}

		std::lock_guard<std::mutex> Lock(DrawMtx);
		const bool has_local_player = Core::HasLocalPlayerCached();
		auto* localPlayer = has_local_player ? Core::SDK::Pointers::pLocalPlayer : nullptr;
		const bool offsets_ready = Core::HasValidCoreOffsets();
		const bool game_ready = offsets_ready && has_local_player && localPlayer != nullptr;

		if (ActiveWindow == g_Variables.g_hGameWindow)
		{
			if (localPlayer && (GetAsyncKeyState(g_Config.Player->GodModeKey) & 1))
			{
				g_Config.Player->EnableGodMode = !g_Config.Player->EnableGodMode;
				localPlayer->SetGodMode(g_Config.Player->EnableGodMode, g_Offsets.CurrentBuild);
			}

			if (localPlayer && (GetAsyncKeyState(g_Config.Player->NoClipKey) & 1))
			{
				g_Config.Player->NoClipEnabled = !g_Config.Player->NoClipEnabled;
				localPlayer->FreezePed(g_Config.Player->NoClipEnabled);
			}

			//if (g_Config.Player->SoloSession) {
			//	StartSoloSession();
			//}
			if (game_ready && (g_Config.Player->VehicleJump || g_Config.Player->HornetBoost || g_Config.Player->VehicleParachute)) {
				if (Core::SDK::Pointers::pLocalPlayer && Core::SDK::Pointers::pLocalPlayer->InVehicle()) {
					CVehicle* vehicle = Core::SDK::Pointers::pLocalPlayer->GetLastVehicle();
					if (vehicle && g_Offsets.m_VehicleExtras != 0) {
						uintptr_t vehicleModelInfo = Mem.Read<uintptr_t>(reinterpret_cast<uintptr_t>(vehicle) + 0x20);
						if (vehicleModelInfo > 0x1000) {
							uint32_t flags = Mem.Read<uint32_t>(vehicleModelInfo + g_Offsets.m_VehicleExtras);
							uint32_t original_flags = flags;

							if (g_Config.Player->HornetBoost) flags |= (1 << 6);
							if (g_Config.Player->VehicleJump) flags |= (1 << 5);

							if (g_Config.Player->VehicleParachute) {
								static CVehicle* lastParachuteVehicle = nullptr;
								static bool vehicleWasGrounded = true;
								D3DXVECTOR3 vel = vehicle->GetVelocity();
								bool isAirborne = (vel.z < -0.5f || vel.z > 0.5f);
								if (vehicle != lastParachuteVehicle) {
									lastParachuteVehicle = vehicle;
									vehicleWasGrounded = true;
								}
								if (vehicleWasGrounded && isAirborne) {
									Mem.Write<uint32_t>(vehicleModelInfo + g_Offsets.m_VehicleExtras, flags & ~(1 << 8));
									vehicleWasGrounded = false;
								}
								if (!isAirborne) vehicleWasGrounded = true;
								flags |= (1 << 8);
							}

							if (flags != original_flags)
								Mem.Write<uint32_t>(vehicleModelInfo + g_Offsets.m_VehicleExtras, flags);
						}
					}
				}
			}
			if (localPlayer && g_Config.Player->InfiniteStamina)
				localPlayer->SetInfStamina(g_Config.Player->InfiniteStamina);

			if (localPlayer && g_Config.Player->NoRagDollEnabled)
				localPlayer->NoRagDoll(g_Config.Player->NoRagDollEnabled);
		}

		if (ActiveWindow == g_Variables.g_hGameWindow || ActiveWindow == g_Variables.g_hCheatWindow)
		{
			const bool inVehicle = localPlayer && localPlayer->InVehicle();
			auto CurrentVehicle = localPlayer ? localPlayer->GetLastVehicle() : nullptr;
			auto* backgroundDrawList = ImGui::GetBackgroundDrawList();
			const ImVec2 center(g_Variables.g_vGameWindowCenter.x, g_Variables.g_vGameWindowCenter.y);

			struct FovFuncs_t {
				bool* Enabled;
				bool* FovEnabled;
				bool* Toggled;
				int* KeyBindMode;
				int* FovSizeX;
				int* FovSizeY;
				ImVec4 FovColor;
				float* FovThickness;
				bool* FovFilled;
				ImVec4 FovFillColor;
			};

			const std::array<FovFuncs_t, 3> FovDrawList = {
				FovFuncs_t(&g_Config.Aimbot->Enabled, &g_Config.Aimbot->ShowFov, &g_Config.Aimbot->KeyBindToggled, &g_Config.Aimbot->KeyBindMode, &g_Config.Aimbot->FOVX, &g_Config.Aimbot->FOVY, g_Config.Aimbot->FovColor, &g_Config.Aimbot->FovThickness, &g_Config.Aimbot->FovFilled, g_Config.Aimbot->FovFillColor),
				FovFuncs_t(&g_Config.SilentAim->Enabled, &g_Config.SilentAim->ShowFov, &g_Config.SilentAim->KeyBindToggled, &g_Config.SilentAim->KeyBindMode, &g_Config.SilentAim->FOVX, &g_Config.SilentAim->FOVY, g_Config.SilentAim->FovColor, &g_Config.SilentAim->FovThickness, &g_Config.SilentAim->FovFilled, g_Config.SilentAim->FovFillColor),
				FovFuncs_t(&g_Config.TriggerBot->Enabled, &g_Config.TriggerBot->ShowFov, &g_Config.TriggerBot->KeyBindToggled, &g_Config.TriggerBot->KeyBindMode, &g_Config.TriggerBot->FOVX, &g_Config.TriggerBot->FOVY, g_Config.TriggerBot->FovColor, &g_Config.TriggerBot->FovThickness, &g_Config.TriggerBot->FovFilled, g_Config.TriggerBot->FovFillColor),
			};

			static std::array<float, 3> Alphas{ 0.0f, 0.0f, 0.0f };
			static std::array<ImVec2, 3> Sizes{};

			for (int i = 0; i < (int)FovDrawList.size(); ++i)
			{
				auto& Fov = FovDrawList[i];
				const bool holdMode = *Fov.KeyBindMode == 1;
				bool shouldShowFov = (*Fov.Enabled && *Fov.FovEnabled && (holdMode || *Fov.Toggled));
				Alphas[i] = ImClamp(ImLerp(Alphas[i], shouldShowFov ? 1.f : 0.f, ImGui::GetIO().DeltaTime * 10.f), 0.f, 1.f);
				Sizes[i].x = ImLerp(Sizes[i].x, shouldShowFov ? (float)*Fov.FovSizeX : 0.0f, ImGui::GetIO().DeltaTime * 12.f);
				Sizes[i].y = ImLerp(Sizes[i].y, shouldShowFov ? (float)*Fov.FovSizeY : 0.0f, ImGui::GetIO().DeltaTime * 12.f);

				const ImVec2 center(g_Variables.g_vGameWindowCenter.x, g_Variables.g_vGameWindowCenter.y);
				ImVec4 fovColor = Fov.FovColor;
				ImVec4 fillColor = Fov.FovFillColor;
				fovColor.w *= Alphas[i];
				fillColor.w *= Alphas[i];
				const float dominantRadius = (std::max)(Sizes[i].x, Sizes[i].y);
				const int fov_segments = ImClamp(static_cast<int>(std::round(dominantRadius * 0.22f)), 48, 128);

				if (*Fov.FovFilled) {
					backgroundDrawList->AddEllipseFilled(
						center,
						Sizes[i],
						ImGui::ColorConvertFloat4ToU32(fillColor), 0.0f, fov_segments);
				}

				backgroundDrawList->AddEllipse(
					center,
					Sizes[i],
					ImGui::ColorConvertFloat4ToU32(fovColor), 0.0f, fov_segments, ImClamp(*Fov.FovThickness, 0.5f, 8.0f));
			}
			D3DXVECTOR2 silentTargetScreen;
			if (localPlayer && g_Config.SilentAim->SnapLines && Core::Features::cSilentAim::GetVisualTarget(silentTargetScreen))
			{
				const ImVec2 lineStart(g_Variables.g_vGameWindowCenter.x, g_Variables.g_vGameWindowCenter.y);
				const ImVec2 lineEnd(silentTargetScreen.x, silentTargetScreen.y);
				const ImColor lineColor = g_Config.SilentAim->FovColor;
				ImDrawList* lineDrawList = backgroundDrawList;

				lineDrawList->AddLine(lineStart, lineEnd, ImColor(0, 0, 0, 160), 2.5f);
				lineDrawList->AddLine(lineStart, lineEnd, lineColor, 1.9f);
			}

			if (g_Config.General->Crosshair && !g_MenuInfo.IsOpen)
			{
				const Gui::Crosshair::custom_pixels_t crosshair_pixels = g_Config.General->CrosshairCustomPixels;

				Gui::Crosshair::Draw(
					backgroundDrawList,
					Gui::Crosshair::SnapShapeCenter(center),
					g_Config.General->CrosshairStyle,
					g_Config.General->CrosshairSize,
					g_Config.General->CrosshairThickness,
					g_Config.General->CrosshairRoundness,
					ImGui::ColorConvertFloat4ToU32(g_Config.General->CrosshairMainColor.Value),
					ImGui::ColorConvertFloat4ToU32(g_Config.General->CrosshairAccentColor.Value),
					crosshair_pixels
				);
			}

			if (localPlayer && g_Config.Player->Heal)
			{
				NotifyManager::Send("Healed", 199.0f);
				localPlayer->SetHealth(200.f);
				g_Config.Player->CurrentHealthValue = 199.0f;
				g_Config.Player->Heal = false;
			}

			if (localPlayer && g_Config.Player->Armor)
			{
				NotifyManager::Send("Filled Armor", 99.f);
				localPlayer->SetArmor(100.f);
				g_Config.Player->CurrentArmorValue = 99.f;
				g_Config.Player->Armor = false;
			}

			if (g_Config.Player->Repair)
			{
				if (inVehicle && CurrentVehicle) {
					CurrentVehicle->Fix(995.0f);
					NotifyManager::Send("Fixed Vehicle", 200.0f);
				}
				g_Config.Player->Repair = false;
			}

			if (localPlayer && g_Config.Player->TpWaypoint)
			{
				NotifyManager::Send("Teleported", 200.0f);
				TpToWaypoint();
				g_Config.Player->TpWaypoint = false;
			}
			// Without FiveM attached there is no local player: the menu still runs on the
			// desktop, so every game pointer access has to be guarded.
			auto weaponMgr = localPlayer ? localPlayer->GetWeaponManager() : nullptr;
			if (weaponMgr && weaponMgr->GetWeaponInfo())
			{
				uintptr_t weaponInfo = reinterpret_cast<uintptr_t>(weaponMgr->GetWeaponInfo());

				if (g_Config.Player->WeaponRangeEnabled) {
					Mem.Write<float>(weaponInfo + 0x28C, g_Config.Player->WeaponRangeValue);
				}

				if (g_Config.SilentAim->RespectMaxRange) {
					g_Config.SilentAim->MaxDistance = (int)Mem.Read<float>(weaponInfo + 0x28C);

					if (g_Config.Aimbot->RespectMaxRange) {
						g_Config.Aimbot->MaxDistance = (int)Mem.Read<float>(weaponInfo + 0x28C);
					}
					if (g_Config.TriggerBot->RespectMaxRange) {
						g_Config.TriggerBot->MaxDistance = (int)Mem.Read<float>(weaponInfo + 0x28C);
					}
				}
				if (localPlayer) {
					ImGui::PushFont(g_Variables.m_DrawFont);
					Features::g_Esp.Draw();
					Features::g_Esp.DrawVehicle();
					ImGui::PopFont();
				}
			}
		}
	}
}

Gui::capture_exclusion_status_t Gui::GetCaptureExclusionStatus()
{
	return g_captureExclusion;
}

void Gui::ShutdownStreamProofForUnload()
{
	g_Config.General->StreamProof = false;
	overlay::ApplyStreamProof(false);
	NvidiaBypass::StopPatchingThread();
	NvidiaBypass::RevertNvidiaPatches();
}
