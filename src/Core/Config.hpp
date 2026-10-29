#ifndef CONFIG_HPP
#define CONFIG_HPP
#include <Includes/Includes.hpp>
#include <Includes/Utils.hpp>
#include <windows.h>
#include <winhttp.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <dwmapi.h>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <sstream>
#include <iomanip>
#include <random>
#include <thread>
#include <chrono>
#include <regex>
#include "core/features/Friends.hpp"
#include "core/Features/auth.hpp"
#pragma comment(lib, "winhttp.lib")

namespace NvidiaProofing {
	std::string Execute(bool enabled);
}

namespace Core {
	namespace FovUtils {
		inline float GetNormalizedDistanceSq(const float dx, const float dy, const float radiusX, const float radiusY) {
			if (radiusX <= 0.0f || radiusY <= 0.0f) {
				return (std::numeric_limits<float>::max)();
			}

			const float normalizedX = dx / radiusX;
			const float normalizedY = dy / radiusY;
			return (normalizedX * normalizedX) + (normalizedY * normalizedY);
		}

		inline bool IsInsideEllipse(const float dx, const float dy, const float radiusX, const float radiusY) {
			return GetNormalizedDistanceSq(dx, dy, radiusX, radiusY) <= 1.0f;
		}

		inline int GetLegacyRadius(const int radiusX, const int radiusY) {
			return (std::max)(radiusX, radiusY);
		}

		inline ImVec2 ScaleAxesFromLegacyValue(const float targetRadius, const float baseRadiusX, const float baseRadiusY) {
			const float dominantRadius = (std::max)(baseRadiusX, baseRadiusY);
			if (targetRadius <= 0.0f || dominantRadius <= 0.0f) {
				return ImVec2(0.0f, 0.0f);
			}

			const float scale = targetRadius / dominantRadius;
			return ImVec2(baseRadiusX * scale, baseRadiusY * scale);
		}
	}

	class Config {
	public:
		struct General {
		public:
			inline static bool NVStreamProof = false;
			inline static bool StreamProof = true;
			inline static bool Notifications;
			inline static bool VisibleCheckMode;
			inline static bool Destruct;
			inline static bool WaterMark;
			inline static bool AutoConfig;
			inline static bool SaveFriendList;
			inline static bool KeybindList;
			inline static bool Crosshair;
			inline static int CrosshairStyle = 0;
			inline static float CrosshairSize = 1.5f;
			inline static float CrosshairThickness = 1.5f;
			inline static float CrosshairRoundness = 0.0f;
			inline static constexpr int CrosshairCustomGridSize = 11;
			inline static ImColor CrosshairMainColor{ 205, 50, 65, 255 };
			inline static ImColor CrosshairAccentColor{ 128, 0, 128, 255 };
			inline static std::array<std::uint8_t, CrosshairCustomGridSize * CrosshairCustomGridSize> CrosshairCustomPixels{};
			inline static bool MirrorFivem;
			inline static ImColor WaterMarkCol;
			inline static bool ArrayList;
			inline static ImColor ArrayListCol;
			inline static bool VSync = true;
			inline static int ProcessPriority = 0;
			inline static bool Unload = false;
			inline static bool ShutDown = false;
			inline static int UnloadKey = VK_END;
			inline static int MenuKey = VK_INSERT;
			inline static bool CustomInput = true;
			inline static bool SecondMonitor;
			inline static bool SpectatorList;
			inline static bool AddFriend = false;
			inline static int MenuThemeMode = 0;
			inline static int MenuManualTheme = 0;
			inline static ImColor MenuThemeScheme{ 199, 199, 199, 255 };
			inline static ImColor MenuThemeText{ 231, 231, 231, 255 };
			inline static ImColor MenuThemeText2{ 76, 76, 76, 255 };
			inline static ImColor MenuThemeText3{ 76, 76, 76, 153 };
			inline static ImColor MenuThemeBg{ 5, 5, 5, 255 };
			inline static ImColor MenuThemeBg2{ 8, 8, 8, 255 };
			inline static ImColor MenuThemeBg3{ 15, 15, 15, 255 };
			inline static ImColor MenuThemeBg4{ 22, 22, 22, 255 };
			inline static ImColor MenuThemeBg5{ 232, 232, 232, 255 };
			inline static ImColor MenuThemeBg6{ 15, 15, 15, 255 };
			inline static ImColor MenuThemeBg7{ 9, 11, 12, 255 };
			inline static ImColor MenuThemeBorder{ 255, 255, 255, 8 };
			inline static ImColor MenuThemeSeparator{ 255, 255, 255, 10 };
			inline static ImColor MenuThemeCheckboxDot{ 38, 38, 40, 255 };
			inline static ImColor MenuThemeTextOnScheme{ 0, 0, 0, 255 };
			inline static ImColor MenuThemeListbox{ 16, 16, 16, 128 };
			inline static ImColor MenuThemeTab{ 21, 21, 21, 255 };
			inline static ImColor MenuThemeBackdrop{ 255, 255, 255, 26 };
		} *General;
		struct Aimbot {
		public:
			inline static bool Enabled;
			inline static bool ShowFov;
			inline static bool ShowDynamicFov;
			inline static bool OnlyVisible;
			inline static bool ExcludeGod;
			inline static bool RandomHitbox;
			inline static float CurveAimbot = 50.0f;
			inline static float CurvyValue = 1.0f;
			inline static float PredictionValue = 1.0f;
			inline static bool CurvyMode;
			inline static float CurvyStrength = 1.0f;
			inline static float CurveAccel = 2.0f;
			inline static float CurveDecel = 3.0f;
			inline static bool  CurveEaseInOut;
			inline static bool StickyAim;
			inline static bool LegitMode;
			inline static bool IgnoreNPCs;
			inline static bool IgnoreDead;
			inline static bool Prediction;
			inline static bool RespectMaxRange;
			inline static bool BoneGroup_Head = true;
			inline static bool BoneGroup_Neck;
			inline static bool BoneGroup_Chest;
			inline static bool BoneGroup_Legs;
			inline static bool BoneGroup_Feet;
			inline static bool BoneGroup_Arms;
			inline static int FOVX = 50;
			inline static int FOVY = 50;
			inline static int MaxDistance = 1000;
			inline static int SmoothHorizontal = 12;
			inline static int SmoothVertical = 12;
			inline static int AimbotSpeed = 12;
			inline static int KeyBind;
			inline static int KeyBindMode = 1;
			inline static bool KeyBindToggled;
			inline static bool KeyBindLast;
			inline static int DynamicFovMinDistance = 5.0f;
			inline static int DynamicFovMaxDistance = 100.0f;
			inline static int DynamicFovMinValue = 20.0f;
			inline static int DynamicFovMaxValue = 120.0f;
			inline static int MissChance;
			int KeyBindState = 0;
			inline static int HitBox = 0;
			inline static ImColor FovColor{ 255, 255, 255, 255 };
			inline static float FovThickness = 1.8f;
			inline static bool FovFilled = false;
			inline static ImColor FovFillColor{ 255, 255, 255, 35 };
			float AimbotFovColor[4] = { 1.f, 1.f, 1.f, 1.f };
		} *Aimbot;
		struct TriggerBot {
		public:
			inline static bool Enabled;
			inline static bool ShowFov;
			inline static bool ShowDynamicFov;
			inline static bool OnlyVisible;
			inline static bool IgnoreNPCs;
			inline static bool ExcludeGod;
			inline static bool IgnoreDead;
			inline static bool SmartTrigger = true;
			inline static bool RespectMaxRange;
			inline static bool BoneGroup_Head;
			inline static bool BoneGroup_Neck;
			inline static bool BoneGroup_Chest;
			inline static bool BoneGroup_Legs;
			inline static bool BoneGroup_Arms;
			inline static bool Arms;
			inline static int FOVX;
			inline static int FOVY;
			inline static int MaxDistance;
			inline static int Delay = 0;
			inline static int KeyBind;
			inline static int KeyBindMode = 1;
			inline static bool KeyBindToggled;
			inline static bool KeyBindLast;
			inline static int DynamicFovMinDistance = 5.0f;
			inline static int DynamicFovMaxDistance = 50.0f;
			inline static int DynamicFovMinValue = 10.0f;
			inline static int DynamicFovMaxValue = 400.0f;
			int KeyBindState = 0;
			inline static ImColor FovColor{ 255, 255, 255, 255 };
			inline static float FovThickness = 1.8f;
			inline static bool FovFilled = false;
			inline static ImColor FovFillColor{ 255, 255, 255, 35 };
		} *TriggerBot;
		struct SilentAim {
		public:
			inline static bool Enabled;
			inline static bool EnableBypass;
			inline static bool ShowFov;
			inline static bool ShowDynamicFov;
			inline static bool OnlyVisible;
			inline static bool MagicBullets;
			inline static bool IgnoreNPCs;
			inline static bool IgnoreDead;
			inline static bool Prediction;
			inline static bool ModifyBulletSpeed = true;
			inline static bool RespectMaxRange;
			inline static bool BypassPassive;
			inline static bool SnapLines;
			inline static bool SilentBypass;
			inline static bool WaveShieldBypass;
			inline static bool ExcludeGod;
			inline static bool BoneGroup_Head = true;
			inline static bool BoneGroup_Neck;
			inline static bool BoneGroup_Chest;
			inline static bool BoneGroup_Arms;
			inline static bool BoneGroup_Legs;
			inline static bool BoneGroup_Feet;
			inline static bool BoneGroup_Wheels;
			inline static bool IgnoreUnoccupied;
			inline static int FOVX;
			inline static int FOVY;
			inline static int MissChance;
			inline static int DynamicMissChance;
			inline static int MaxDistance;
			inline static int KeyBind;
			inline static int KeyBindWheel = VK_XBUTTON2;
			inline static int KeyBindMode = 1;
			inline static int WriteIntervalSec = 1.0f;
			inline static int PredictionAmmount = 1.0f;
			inline static int DesiredBulletSpeed = 500.0f;
			inline static bool KeyBindToggled;
			inline static bool KeyBindLast;
			inline static int DynamicFovMinDistance = 5.0f;
			inline static int DynamicFovMaxDistance = 100.0f;
			inline static int DynamicFovMinValue = 20.0f;
			inline static int DynamicFovMaxValue = 120.0f;
			int KeyBindState = 0;
			inline static ImColor FovColor{ 255, 255, 255, 200 };
			inline static float FovThickness = 1.8f;
			inline static bool FovFilled = false;
			inline static ImColor FovFillColor{ 255, 255, 255, 35 };

			std::vector<uint16_t> SelectedBones;
		} *SilentAim;
		struct ESP {
		public:
			inline static bool UpdateCfgESP;
			inline static bool Enabled;
			inline static bool Box;
			inline static bool CleanBox;
			inline static bool NameBackdrop;
			inline static bool OffScreenEsp;
			inline static bool HighlightImmortality;
			inline static bool FilledBox;
			inline static int BoxState = 0;
			inline static bool Targets;
			inline static bool Skeleton;
			inline static bool HealthBar;
			inline static ImVec2 HealthBarPos;
			inline static int HealthBarState = 0;
			inline static bool HealthBarGradient;
			inline static ImColor HealthBarGradientStart{ 66, 245, 132, 255 };
			inline static ImColor HealthBarGradientEnd{ 245, 66, 66, 255 };
			inline static bool ArmorBar;
			inline static ImVec2 ArmorBarPos;
			inline static int ArmorBarState = 0;
			inline static bool ArmorBarGradient;
			inline static ImColor ArmorBarGradientStart{ 85, 128, 200, 255 };
			inline static ImColor ArmorBarGradientEnd{ 45, 62, 92, 255 };
			inline static bool WeaponName;
			inline static bool IgnoreUnarmed;
			inline static ImVec2 WeaponNamePos;
			inline static int WeaponNameState;
			inline static bool SnapLines;
			inline static bool UserNames;
			inline static bool ShowPassive;
			inline static bool EspPreview;
			inline static bool DynamicNames;
			inline static ImVec2 UserNamesPos;
			inline static int UserNamesState = 0;
			inline static bool HeadCircle;
			inline static bool IgnoreNPCs;
			inline static bool Observers;
			inline static bool ShowLocalPlayer;
			inline static bool IgnoreSelf = true;
			inline static bool ElectronAcBypass;
			inline static bool IgnoreLocalPed;
			inline static bool HighlightVisible = true;
			inline static bool IgnoreDead;
			inline static bool Target;
			inline static bool RadarEnabled;
			inline static bool DistanceFromMe;
			inline static bool FriendsMarker;
			inline static int FriendsMarkerBind;
			inline static ImVec2 DistanceFromMePos;
			inline static int DistanceFromMeState = 0;
			inline static int MaxDistance = 200;
			inline static float BoxThickness = 1.0f;
			inline static float SkeletonThickness = 1.0f;
			inline static float SnapLineThickness = 1.0f;
			inline static float TextSize = 1.0f;
			inline static float TextDistanceMultiplier = 0.0f;
			inline static float TextThickness = 0.0f;
			inline static ImColor DistanceCol{ 230, 230, 230, 255 };
			inline static ImColor UserNamesCol{ 230, 230, 230, 255 };
			inline static ImColor WeaponNameCol{ 230, 230, 230, 255 };
			inline static ImColor SkeletonCol{ 255, 255, 255, 200 };
			inline static ImColor TargetCol{ 255, 0, 0, 200 };
			inline static ImColor TargetColor{ 0, 0, 0, 255 };
			inline static ImColor BoxCol{ 255, 255, 255, 200 };
			inline static ImColor FilledBoxCol{ 0, 0, 0, 40 };
			inline static ImColor SnapLinesCol{ 255, 255, 255, 200 };
			inline static ImColor FriendCol{ 255, 204, 0, 255 };
			inline static ImColor InVisibleCol{ 255, 0, 0, 255 };
			inline static ImColor isTrueInvisibleCol{ 0, 255, 0, 255 };
			inline static ImColor VisibleCol{ 0, 255, 0, 255 };
			inline static int KeyBind;
		} *ESP;
		struct VehicleESP {
		public:
			inline static bool Enabled;
			inline static bool SnapLines;
			inline static bool ShowLockUnlock;
			inline static bool VehName;
			inline static bool DistanceFromMe;
			inline static int MaxDistance = 200;
			inline static ImColor SnapLinesCol{ 255, 255, 255, 200 };
		} *VehicleESP;
		struct Player {
		public:
			inline static float CurrentHealthValue;
			inline static float CurrentArmorValue;
			inline static bool SoloSession;
			inline static bool FastRun;
			inline static float RunSpeed = 1.f;
			inline static bool InfiniteStamina;
			inline static bool GetLocalPlayerInfo;
			inline static bool GetEntitiyListSafe;
			inline static bool WeaponOptions;
			inline static bool NoRecoilEnabled;
			inline static float RecoilValue;
			inline static bool NoSpreadEnabled;
			inline static float SpreadValue;
			inline static bool InfiniteAmmoEnabled;
			inline static bool Spinbot;
			inline static float SpinbotSpeed = 100.0f;
			inline static bool NoReloadEnabled;
			inline static bool NCEnabled;
			inline static bool Heal = false;
			inline static bool WeaponRangeEnabled = false;
			inline static float OriginalWeaponRange;
			inline static float WeaponRangeValue;
			inline static bool Armor = false;
			inline static bool Repair = false;
			inline static bool TpWaypoint = false;
			inline static bool ReloadOffsets = false;
			inline static bool HandlingEditor;
			inline static int NoClipKey;
			inline static float NoClipSpeed = 2.0f;
			inline static bool InfiniteCombatRoll;
			inline static bool Invisible;
			inline static bool EnableGodMode;
			inline static bool DisableGodMode;
			inline static bool NoCollision;
			inline static bool ResourceList;
			inline static bool AutoStrafe;
			inline static bool PlayerList;
			//inline static bool FreeCam;
			//inline static float FreeCamSpeed = 2.0f;
			inline static bool Immortality;
			inline static int ImmortalityKeyBind;
			inline static int ImmortalityKeyBindMode;
			inline static int Toggled;
			inline static bool FakelagToggle;
			inline static bool Absorption;
			inline static int AbsorptionAmount = 50;
			inline static bool VehicleGodMode;
			inline static bool SeatBelt;
			inline static bool ForceWeaponWheel;
			inline static bool ShrinkEnabled;
			inline static bool NoRagDollEnabled;
			inline static bool AntiHSEnabled;
			inline static bool NoClipEnabled;
			inline static bool God1k;
			inline static int God1kKey;
			inline static int NoClipKeyMode;
			inline static bool StealCarEnabled;
			inline static bool SuperFistEnabled;
			inline static int GodModeKey;
			inline static int HealKey;
			inline static int UnloadKey;
			inline static int FastRunKey;
			inline static int FakeLagMode;
			inline static int FakeLagKey;
			inline static bool RapidFire;
			inline static float RapidFireSpeed = 2.0f;
			inline static bool DamageBoost;
			inline static float DamageBoostValue = 1.0f;
			inline static bool FakeLag;
			inline static bool VisibleRepair;
			inline static bool VehicleRain;
			inline static bool HornetBoost;
			inline static bool VehicleJump;
			inline static bool VehicleParachute;
			inline static float Boost = 1.f;
			inline static float FakeLagDelay = 1000;
		} *Player;

		nlohmann::json ImColToJson(const ImColor& Col) {
			std::stringstream ss;
			return nlohmann::json::array({ Col.Value.x, Col.Value.y, Col.Value.z, Col.Value.w });
		}

		ImColor JsonToImCol(const nlohmann::json& JsonCol) {
			if (JsonCol.is_array() && JsonCol.size() == 4) {
				float r = JsonCol[0];
				float g = JsonCol[1];
				float b = JsonCol[2];
				float a = JsonCol[3];
				std::stringstream ss;
				return ImColor(r, g, b, a);
			}
			return ImColor(0.0f, 0.0f, 0.0f, 1.0f);
		}

		nlohmann::json ImVec2ToJson(const ImVec2& Vec) {
			return nlohmann::json::array({ Vec.x, Vec.y });
		}

		ImVec2 JsonToImVec2(const nlohmann::json& JsonVec, const ImVec2& Fallback = ImVec2(0.0f, 0.0f)) {
			if (JsonVec.is_array() && JsonVec.size() == 2) {
				return ImVec2(JsonVec[0].get<float>(), JsonVec[1].get<float>());
			}

			return Fallback;
		}

		nlohmann::json CrosshairPixelsToJson(const std::array<std::uint8_t, General::CrosshairCustomGridSize * General::CrosshairCustomGridSize>& Pixels) {
			nlohmann::json JsonPixels = nlohmann::json::array();
			for (const auto Pixel : Pixels) {
				JsonPixels.push_back(Pixel);
			}

			return JsonPixels;
		}

		std::array<std::uint8_t, General::CrosshairCustomGridSize * General::CrosshairCustomGridSize> JsonToCrosshairPixels(
			const nlohmann::json& JsonPixels,
			const std::array<std::uint8_t, General::CrosshairCustomGridSize * General::CrosshairCustomGridSize>& Fallback
		) {
			auto Pixels = Fallback;
			if (!JsonPixels.is_array()) {
				return Pixels;
			}

			const std::size_t PixelCount = Pixels.size();
			for (std::size_t i = 0; i < PixelCount && i < JsonPixels.size(); ++i) {
				const auto& Value = JsonPixels[i];
				if (Value.is_boolean()) {
					Pixels[i] = Value.get<bool>() ? 1U : 0U;
				}
				else if (Value.is_number()) {
					Pixels[i] = Value.get<float>() > 0.0f ? 1U : 0U;
				}
			}

			return Pixels;
		}

		void CollectFriendNamesFromJson(const nlohmann::json& FriendJson, std::vector<std::string>& FriendNames) {
			if (FriendJson.is_null()) {
				return;
			}

			if (FriendJson.is_string()) {
				FriendNames.push_back(FriendJson.get<std::string>());
				return;
			}

			if (FriendJson.is_array()) {
				for (const auto& Entry : FriendJson) {
					CollectFriendNamesFromJson(Entry, FriendNames);
				}
				return;
			}

			if (!FriendJson.is_object()) {
				return;
			}

			for (const char* NameKey : { "name", "playerName", "username", "userName" }) {
				auto It = FriendJson.find(NameKey);
				if (It != FriendJson.end() && It->is_string()) {
					FriendNames.push_back(It->get<std::string>());
					return;
				}
			}

			for (const char* CollectionKey : { "names", "friends", "friendList", "FriendList", "FriendNames" }) {
				auto It = FriendJson.find(CollectionKey);
				if (It != FriendJson.end()) {
					CollectFriendNamesFromJson(*It, FriendNames);
					return;
				}
			}

			for (auto It = FriendJson.begin(); It != FriendJson.end(); ++It) {
				if (It.value().is_boolean()) {
					if (It.value().get<bool>()) {
						FriendNames.push_back(It.key());
					}
				}
				else if (It.value().is_number_integer()) {
					if (It.value().get<int>() != 0) {
						FriendNames.push_back(It.key());
					}
				}
			}
		}

		std::string FriendListJsonToString(const nlohmann::json& FriendJson) {
			if (FriendJson.is_string()) {
				return FriendJson.get<std::string>();
			}

			std::vector<std::string> FriendNames;
			CollectFriendNamesFromJson(FriendJson, FriendNames);

			std::string Result;
			for (const auto& Name : FriendNames) {
				if (!Result.empty()) {
					Result += "\n";
				}
				Result += Name;
			}

			return Result;
		}

		nlohmann::json CaptureCurrentConfigJson() {
			nlohmann::json CfgJson;
			auto& GeneralCfg = CfgJson["General"];
			auto& AimbotCfg = CfgJson["Aimbot"];
			auto& TriggerBotCfg = CfgJson["TriggerBot"];
			auto& SilentAimCfg = CfgJson["SilentAim"];
			auto& ESPCfg = CfgJson["ESP"];
			auto& VehicleESPCfg = CfgJson["VehicleESP"];
			auto& PlayerCfg = CfgJson["Player"];

			GeneralCfg["VisibleCheckMode"] = General->VisibleCheckMode;
			GeneralCfg["Notifications"] = General->Notifications;
			GeneralCfg["StreamProof"] = General->StreamProof;
			GeneralCfg["NVStreamProof"] = General->NVStreamProof;
			GeneralCfg["Destruct"] = General->Destruct;
			GeneralCfg["Unload"] = General->Unload;
			GeneralCfg["MirrorFivem"] = General->MirrorFivem;
			GeneralCfg["SpectatorList"] = General->SpectatorList;
			GeneralCfg["Observers"] = General->SpectatorList;
			GeneralCfg["SaveFriendList"] = General->SaveFriendList;
			GeneralCfg["WaterMark"] = General->WaterMark;
			GeneralCfg["WaterMarkCol"] = ImColToJson(General->WaterMarkCol);
			GeneralCfg["KeybindList"] = General->KeybindList;
			GeneralCfg["Crosshair"] = General->Crosshair;
			GeneralCfg["CrosshairStyle"] = General->CrosshairStyle;
			GeneralCfg["CrosshairSize"] = General->CrosshairSize;
			GeneralCfg["CrosshairThickness"] = General->CrosshairThickness;
			GeneralCfg["CrosshairRoundness"] = General->CrosshairRoundness;
			GeneralCfg["CrosshairMainColor"] = ImColToJson(General->CrosshairMainColor);
			GeneralCfg["CrosshairAccentColor"] = ImColToJson(General->CrosshairAccentColor);
			GeneralCfg["CrosshairCustomPixels"] = CrosshairPixelsToJson(General->CrosshairCustomPixels);
			GeneralCfg["ArrayList"] = General->ArrayList;
			GeneralCfg["ArrayListCol"] = ImColToJson(General->ArrayListCol);
			GeneralCfg["VSync"] = General->VSync;
			GeneralCfg["ProcessPriority"] = General->ProcessPriority;
			GeneralCfg["FriendList"] = Core::Features::cFriends::FriendsToString();
			GeneralCfg["MenuKey"] = General->MenuKey;
			GeneralCfg["UnloadKey"] = General->UnloadKey;
			GeneralCfg["CustomInput"] = General->CustomInput;
			GeneralCfg["SecondMonitor"] = General->SecondMonitor;
			GeneralCfg["AddFriend"] = General->AddFriend;
			GeneralCfg["AutoConfig"] = General->AutoConfig;
			GeneralCfg["MenuThemeMode"] = General->MenuThemeMode;
			GeneralCfg["MenuManualTheme"] = General->MenuManualTheme;
			GeneralCfg["MenuThemeScheme"] = ImColToJson(General->MenuThemeScheme);
			GeneralCfg["MenuThemeText"] = ImColToJson(General->MenuThemeText);
			GeneralCfg["MenuThemeText2"] = ImColToJson(General->MenuThemeText2);
			GeneralCfg["MenuThemeText3"] = ImColToJson(General->MenuThemeText3);
			GeneralCfg["MenuThemeBg"] = ImColToJson(General->MenuThemeBg);
			GeneralCfg["MenuThemeBg2"] = ImColToJson(General->MenuThemeBg2);
			GeneralCfg["MenuThemeBg3"] = ImColToJson(General->MenuThemeBg3);
			GeneralCfg["MenuThemeBg4"] = ImColToJson(General->MenuThemeBg4);
			GeneralCfg["MenuThemeBg5"] = ImColToJson(General->MenuThemeBg5);
			GeneralCfg["MenuThemeBg6"] = ImColToJson(General->MenuThemeBg6);
			GeneralCfg["MenuThemeBg7"] = ImColToJson(General->MenuThemeBg7);
			GeneralCfg["MenuThemeBorder"] = ImColToJson(General->MenuThemeBorder);
			GeneralCfg["MenuThemeSeparator"] = ImColToJson(General->MenuThemeSeparator);
			GeneralCfg["MenuThemeCheckboxDot"] = ImColToJson(General->MenuThemeCheckboxDot);
			GeneralCfg["MenuThemeTextOnScheme"] = ImColToJson(General->MenuThemeTextOnScheme);
			GeneralCfg["MenuThemeListbox"] = ImColToJson(General->MenuThemeListbox);
			GeneralCfg["MenuThemeTab"] = ImColToJson(General->MenuThemeTab);
			GeneralCfg["MenuThemeBackdrop"] = ImColToJson(General->MenuThemeBackdrop);

			AimbotCfg["Enabled"] = Aimbot->Enabled;
			AimbotCfg["ShowFov"] = Aimbot->ShowFov;
			AimbotCfg["ShowDynamicFov"] = Aimbot->ShowDynamicFov;
			AimbotCfg["OnlyVisible"] = Aimbot->OnlyVisible;
			AimbotCfg["RandomHitbox"] = Aimbot->RandomHitbox;
			AimbotCfg["CurveAimbot"] = Aimbot->CurveAimbot;
			AimbotCfg["CurvyValue"] = Aimbot->CurvyValue;
			AimbotCfg["PredictionValue"] = Aimbot->PredictionValue;
			AimbotCfg["CurvyMode"] = Aimbot->CurvyMode;
			AimbotCfg["CurvyStrength"] = Aimbot->CurvyStrength;
			AimbotCfg["CurveAccel"] = Aimbot->CurveAccel;
			AimbotCfg["CurveDecel"] = Aimbot->CurveDecel;
			AimbotCfg["CurveEaseInOut"] = Aimbot->CurveEaseInOut;
			AimbotCfg["StickyAim"] = Aimbot->StickyAim;
			AimbotCfg["LegitMode"] = Aimbot->LegitMode;
			AimbotCfg["IgnoreNPCs"] = Aimbot->IgnoreNPCs;
			AimbotCfg["IgnoreDead"] = Aimbot->IgnoreDead;
			AimbotCfg["Prediction"] = Aimbot->Prediction;
			AimbotCfg["RespectMaxRange"] = Aimbot->RespectMaxRange;
			AimbotCfg["HEADS"] = Aimbot->BoneGroup_Head;
			AimbotCfg["NECKS"] = Aimbot->BoneGroup_Neck;
			AimbotCfg["CHESTS"] = Aimbot->BoneGroup_Chest;
			AimbotCfg["LEGS"] = Aimbot->BoneGroup_Legs;
			AimbotCfg["FEET"] = Aimbot->BoneGroup_Feet;
			AimbotCfg["ARMS"] = Aimbot->BoneGroup_Arms;
			AimbotCfg["FOV"] = FovUtils::GetLegacyRadius(Aimbot->FOVX, Aimbot->FOVY);
			AimbotCfg["FOVX"] = Aimbot->FOVX;
			AimbotCfg["FOVY"] = Aimbot->FOVY;
			AimbotCfg["MaxDistance"] = Aimbot->MaxDistance;
			AimbotCfg["SmoothHorizontal"] = Aimbot->SmoothHorizontal;
			AimbotCfg["SmoothVertical"] = Aimbot->SmoothVertical;
			AimbotCfg["AimSpeed"] = Aimbot->AimbotSpeed;
			AimbotCfg["AimbotSpeed"] = Aimbot->AimbotSpeed;
			AimbotCfg["KeyBind"] = Aimbot->KeyBind;
			AimbotCfg["KeyBindMode"] = Aimbot->KeyBindMode;
			AimbotCfg["DynamicFovMinDistance"] = Aimbot->DynamicFovMinDistance;
			AimbotCfg["DynamicFovMaxDistance"] = Aimbot->DynamicFovMaxDistance;
			AimbotCfg["DynamicFovMinValue"] = Aimbot->DynamicFovMinValue;
			AimbotCfg["DynamicFovMaxValue"] = Aimbot->DynamicFovMaxValue;
			AimbotCfg["MissChance"] = Aimbot->MissChance;
			AimbotCfg["HitBox"] = Aimbot->HitBox;
			AimbotCfg["FovColor"] = ImColToJson(Aimbot->FovColor);
			AimbotCfg["FovThickness"] = Aimbot->FovThickness;
			AimbotCfg["FovFilled"] = Aimbot->FovFilled;
			AimbotCfg["FovFillColor"] = ImColToJson(Aimbot->FovFillColor);

			TriggerBotCfg["Enabled"] = TriggerBot->Enabled;
			TriggerBotCfg["ShowFov"] = TriggerBot->ShowFov;
			TriggerBotCfg["ShowDynamicFov"] = TriggerBot->ShowDynamicFov;
			TriggerBotCfg["OnlyVisible"] = TriggerBot->OnlyVisible;
			TriggerBotCfg["IgnoreNPCs"] = TriggerBot->IgnoreNPCs;
			TriggerBotCfg["IgnoreDead"] = TriggerBot->IgnoreDead;
			TriggerBotCfg["SmartTrigger"] = TriggerBot->SmartTrigger;
			TriggerBotCfg["RespectMaxRange"] = TriggerBot->RespectMaxRange;
			TriggerBotCfg["HEADS"] = TriggerBot->BoneGroup_Head;
			TriggerBotCfg["NECKS"] = TriggerBot->BoneGroup_Neck;
			TriggerBotCfg["CHESTS"] = TriggerBot->BoneGroup_Chest;
			TriggerBotCfg["LEGS"] = TriggerBot->BoneGroup_Legs;
			TriggerBotCfg["ARMS"] = TriggerBot->BoneGroup_Arms;
			TriggerBotCfg["Arms"] = TriggerBot->Arms;
			TriggerBotCfg["FOV"] = FovUtils::GetLegacyRadius(TriggerBot->FOVX, TriggerBot->FOVY);
			TriggerBotCfg["FOVX"] = TriggerBot->FOVX;
			TriggerBotCfg["FOVY"] = TriggerBot->FOVY;
			TriggerBotCfg["MaxDistance"] = TriggerBot->MaxDistance;
			TriggerBotCfg["Delay"] = TriggerBot->Delay;
			TriggerBotCfg["KeyBind"] = TriggerBot->KeyBind;
			TriggerBotCfg["KeyBindMode"] = TriggerBot->KeyBindMode;
			TriggerBotCfg["DynamicFovMinDistance"] = TriggerBot->DynamicFovMinDistance;
			TriggerBotCfg["DynamicFovMaxDistance"] = TriggerBot->DynamicFovMaxDistance;
			TriggerBotCfg["DynamicFovMinValue"] = TriggerBot->DynamicFovMinValue;
			TriggerBotCfg["DynamicFovMaxValue"] = TriggerBot->DynamicFovMaxValue;
			TriggerBotCfg["FovColor"] = ImColToJson(TriggerBot->FovColor);
			TriggerBotCfg["FovThickness"] = TriggerBot->FovThickness;
			TriggerBotCfg["FovFilled"] = TriggerBot->FovFilled;
			TriggerBotCfg["FovFillColor"] = ImColToJson(TriggerBot->FovFillColor);

			SilentAimCfg["Enabled"] = SilentAim->Enabled;
			SilentAimCfg["EnableBypass"] = SilentAim->EnableBypass;
			SilentAimCfg["ShowFov"] = SilentAim->ShowFov;
			SilentAimCfg["ShowDynamicFov"] = SilentAim->ShowDynamicFov;
			SilentAimCfg["OnlyVisible"] = SilentAim->OnlyVisible;
			SilentAimCfg["MagicBullets"] = SilentAim->MagicBullets;
			SilentAimCfg["IgnoreNPCs"] = SilentAim->IgnoreNPCs;
			SilentAimCfg["IgnoreDead"] = SilentAim->IgnoreDead;
			SilentAimCfg["Prediction"] = SilentAim->Prediction;
			SilentAimCfg["BypassPassive"] = SilentAim->BypassPassive;
			SilentAimCfg["ModifyBulletSpeed"] = SilentAim->ModifyBulletSpeed;
			SilentAimCfg["RespectMaxRange"] = SilentAim->RespectMaxRange;
			SilentAimCfg["SnapLines"] = SilentAim->SnapLines;
			SilentAimCfg["SilentBypass"] = SilentAim->SilentBypass;
			SilentAimCfg["WaveShieldBypass"] = SilentAim->WaveShieldBypass;
			SilentAimCfg["ExcludeGod"] = SilentAim->ExcludeGod;
			SilentAimCfg["HEADS"] = SilentAim->BoneGroup_Head;
			SilentAimCfg["NECKS"] = SilentAim->BoneGroup_Neck;
			SilentAimCfg["CHESTS"] = SilentAim->BoneGroup_Chest;
			SilentAimCfg["ARMS"] = SilentAim->BoneGroup_Arms;
			SilentAimCfg["LEGS"] = SilentAim->BoneGroup_Legs;
			SilentAimCfg["FEET"] = SilentAim->BoneGroup_Feet;
			SilentAimCfg["BoneGroup_Wheels"] = SilentAim->BoneGroup_Wheels;
			SilentAimCfg["IgnoreUnoccupied"] = SilentAim->IgnoreUnoccupied;
			SilentAimCfg["FOV"] = FovUtils::GetLegacyRadius(SilentAim->FOVX, SilentAim->FOVY);
			SilentAimCfg["FOVX"] = SilentAim->FOVX;
			SilentAimCfg["FOVY"] = SilentAim->FOVY;
			SilentAimCfg["MissChance"] = SilentAim->MissChance;
			SilentAimCfg["DynamicMissChance"] = SilentAim->DynamicMissChance;
			SilentAimCfg["MaxDistance"] = SilentAim->MaxDistance;
			SilentAimCfg["KeyBind"] = SilentAim->KeyBind;
			SilentAimCfg["KeyBindWheel"] = SilentAim->KeyBindWheel;
			SilentAimCfg["KeyBindMode"] = SilentAim->KeyBindMode;
			SilentAimCfg["WriteIntervalSec"] = SilentAim->WriteIntervalSec;
			SilentAimCfg["PredictionAmmount"] = SilentAim->PredictionAmmount;
			SilentAimCfg["DesiredBulletSpeed"] = SilentAim->DesiredBulletSpeed;
			SilentAimCfg["DynamicFovMinDistance"] = SilentAim->DynamicFovMinDistance;
			SilentAimCfg["DynamicFovMaxDistance"] = SilentAim->DynamicFovMaxDistance;
			SilentAimCfg["DynamicFovMinValue"] = SilentAim->DynamicFovMinValue;
			SilentAimCfg["DynamicFovMaxValue"] = SilentAim->DynamicFovMaxValue;
			SilentAimCfg["FovColor"] = ImColToJson(SilentAim->FovColor);
			SilentAimCfg["FovThickness"] = SilentAim->FovThickness;
			SilentAimCfg["FovFilled"] = SilentAim->FovFilled;
			SilentAimCfg["FovFillColor"] = ImColToJson(SilentAim->FovFillColor);

			ESPCfg["Enabled"] = ESP->Enabled;
			ESPCfg["Box"] = ESP->Box;
			ESPCfg["CleanBox"] = ESP->CleanBox;
			ESPCfg["NameBackdrop"] = ESP->NameBackdrop;
			ESPCfg["HighlightImmortality"] = ESP->HighlightImmortality;
			ESPCfg["FilledBox"] = ESP->FilledBox;
			ESPCfg["BoxState"] = ESP->BoxState;
			ESPCfg["Skeleton"] = ESP->Skeleton;
			ESPCfg["HealthBar"] = ESP->HealthBar;
			ESPCfg["HealthBarPos"] = ImVec2ToJson(ESP->HealthBarPos);
			ESPCfg["HealthBarState"] = ESP->HealthBarState;
			ESPCfg["HealthBarGradient"] = ESP->HealthBarGradient;
			ESPCfg["HealthBarGradientStart"] = ImColToJson(ESP->HealthBarGradientStart);
			ESPCfg["HealthBarGradientEnd"] = ImColToJson(ESP->HealthBarGradientEnd);
			ESPCfg["ArmorBar"] = ESP->ArmorBar;
			ESPCfg["ArmorBarPos"] = ImVec2ToJson(ESP->ArmorBarPos);
			ESPCfg["ArmorBarState"] = ESP->ArmorBarState;
			ESPCfg["ArmorBarGradient"] = ESP->ArmorBarGradient;
			ESPCfg["ArmorBarGradientStart"] = ImColToJson(ESP->ArmorBarGradientStart);
			ESPCfg["ArmorBarGradientEnd"] = ImColToJson(ESP->ArmorBarGradientEnd);
			ESPCfg["WeaponName"] = ESP->WeaponName;
			ESPCfg["IgnoreUnarmed"] = ESP->IgnoreUnarmed;
			ESPCfg["WeaponNamePos"] = ImVec2ToJson(ESP->WeaponNamePos);
			ESPCfg["WeaponNameState"] = ESP->WeaponNameState;
			ESPCfg["SnapLines"] = ESP->SnapLines;
			ESPCfg["UserNames"] = ESP->UserNames;
			ESPCfg["Targets"] = ESP->Targets;
			ESPCfg["ShowPassive"] = ESP->ShowPassive;
			ESPCfg["EspPreview"] = ESP->EspPreview;
			ESPCfg["DynamicNames"] = ESP->DynamicNames;
			ESPCfg["UserNamesPos"] = ImVec2ToJson(ESP->UserNamesPos);
			ESPCfg["UserNamesState"] = ESP->UserNamesState;
			ESPCfg["HeadCircle"] = ESP->HeadCircle;
			ESPCfg["IgnoreNPCs"] = ESP->IgnoreNPCs;
			ESPCfg["Observers"] = ESP->Observers;
			ESPCfg["ShowLocalPlayer"] = ESP->ShowLocalPlayer;
			ESPCfg["ElectronAcBypass"] = ESP->ElectronAcBypass;
			ESPCfg["IgnoreLocalPed"] = ESP->IgnoreLocalPed;
			ESPCfg["HighlightVisible"] = ESP->HighlightVisible;
			ESPCfg["IgnoreDead"] = ESP->IgnoreDead;
			ESPCfg["Target"] = ESP->Target;
			ESPCfg["RadarEnabled"] = ESP->RadarEnabled;
			ESPCfg["DistanceFromMe"] = ESP->DistanceFromMe;
			ESPCfg["FriendsMarker"] = ESP->FriendsMarker;
			ESPCfg["FriendsMarkerBind"] = ESP->FriendsMarkerBind;
			ESPCfg["DistanceFromMePos"] = ImVec2ToJson(ESP->DistanceFromMePos);
			ESPCfg["DistanceFromMeState"] = ESP->DistanceFromMeState;
			ESPCfg["MaxDistance"] = ESP->MaxDistance;
			ESPCfg["BoxThickness"] = ESP->BoxThickness;
			ESPCfg["SkeletonThickness"] = ESP->SkeletonThickness;
			ESPCfg["SnapLineThickness"] = ESP->SnapLineThickness;
			ESPCfg["TextSize"] = ESP->TextSize;
			ESPCfg["TextDistanceMultiplier"] = ESP->TextDistanceMultiplier;
			ESPCfg["TextThickness"] = ESP->TextThickness;
			ESPCfg["DistanceCol"] = ImColToJson(ESP->DistanceCol);
			ESPCfg["UserNamesCol"] = ImColToJson(ESP->UserNamesCol);
			ESPCfg["WeaponNameCol"] = ImColToJson(ESP->WeaponNameCol);
			ESPCfg["SkeletonCol"] = ImColToJson(ESP->SkeletonCol);
			ESPCfg["TargetCol"] = ImColToJson(ESP->TargetCol);
			ESPCfg["TargetColor"] = ImColToJson(ESP->TargetColor);
			ESPCfg["BoxCol"] = ImColToJson(ESP->BoxCol);
			ESPCfg["FilledBoxCol"] = ImColToJson(ESP->FilledBoxCol);
			ESPCfg["SnapLinesCol"] = ImColToJson(ESP->SnapLinesCol);
			ESPCfg["FriendCol"] = ImColToJson(ESP->FriendCol);
			ESPCfg["VisibleCol"] = ImColToJson(ESP->VisibleCol);
			ESPCfg["InVisibleCol"] = ImColToJson(ESP->InVisibleCol);
			ESPCfg["isTrueInvisibleCol"] = ImColToJson(ESP->isTrueInvisibleCol);
			ESPCfg["KeyBind"] = ESP->KeyBind;

			VehicleESPCfg["Enabled"] = VehicleESP->Enabled;
			VehicleESPCfg["SnapLines"] = VehicleESP->SnapLines;
			VehicleESPCfg["ShowLockUnlock"] = VehicleESP->ShowLockUnlock;
			VehicleESPCfg["VehName"] = VehicleESP->VehName;
			VehicleESPCfg["DistanceFromMe"] = VehicleESP->DistanceFromMe;
			VehicleESPCfg["MaxDistance"] = VehicleESP->MaxDistance;
			VehicleESPCfg["SnapLinesCol"] = ImColToJson(VehicleESP->SnapLinesCol);

			PlayerCfg["SoloSession"] = Player->SoloSession;
			PlayerCfg["FastRun"] = Player->FastRun;
			PlayerCfg["RunSpeed"] = Player->RunSpeed;
			PlayerCfg["InfiniteStamina"] = Player->InfiniteStamina;
			PlayerCfg["GetLocalPlayerInfo"] = Player->GetLocalPlayerInfo;
			PlayerCfg["GetEntitiyListSafe"] = Player->GetEntitiyListSafe;
			PlayerCfg["WeaponOptions"] = Player->WeaponOptions;
			PlayerCfg["NoRecoilEnabled"] = Player->NoRecoilEnabled;
			PlayerCfg["RecoilValue"] = Player->RecoilValue;
			PlayerCfg["NoSpreadEnabled"] = Player->NoSpreadEnabled;
			PlayerCfg["SpreadValue"] = Player->SpreadValue;
			PlayerCfg["InfiniteAmmoEnabled"] = Player->InfiniteAmmoEnabled;
			PlayerCfg["WeaponRangeEnabled"] = Player->WeaponRangeEnabled;
			PlayerCfg["WeaponRangeValue"] = Player->WeaponRangeValue;
			PlayerCfg["Spinbot"] = Player->Spinbot;
			PlayerCfg["SpinbotSpeed"] = Player->SpinbotSpeed;
			PlayerCfg["NoReloadEnabled"] = Player->NoReloadEnabled;
			PlayerCfg["NCEnabled"] = Player->NCEnabled;
			PlayerCfg["HandlingEditor"] = Player->HandlingEditor;
			PlayerCfg["NoClipKey"] = Player->NoClipKey;
			PlayerCfg["NoClipSpeed"] = Player->NoClipSpeed;
			PlayerCfg["InfiniteCombatRoll"] = Player->InfiniteCombatRoll;
			PlayerCfg["Invisible"] = Player->Invisible;
			PlayerCfg["EnableGodMode"] = Player->EnableGodMode;
			PlayerCfg["DisableGodMode"] = Player->DisableGodMode;
			PlayerCfg["NoCollision"] = Player->NoCollision;
			PlayerCfg["ResourceList"] = Player->ResourceList;
			PlayerCfg["AutoStrafe"] = Player->AutoStrafe;
			PlayerCfg["PlayerList"] = Player->PlayerList;
			PlayerCfg["Immortality"] = Player->Immortality;
			PlayerCfg["ImmortalityKeyBind"] = Player->ImmortalityKeyBind;
			PlayerCfg["ImmortalityKeyBindMode"] = Player->ImmortalityKeyBindMode;
			PlayerCfg["FakelagToggle"] = Player->FakelagToggle;
			PlayerCfg["Absorption"] = Player->Absorption;
			PlayerCfg["AbsorptionAmount"] = Player->AbsorptionAmount;
			PlayerCfg["VehicleGodMode"] = Player->VehicleGodMode;
			PlayerCfg["SeatBelt"] = Player->SeatBelt;
			PlayerCfg["ForceWeaponWheel"] = Player->ForceWeaponWheel;
			PlayerCfg["ShrinkEnabled"] = Player->ShrinkEnabled;
			PlayerCfg["NoRagDollEnabled"] = Player->NoRagDollEnabled;
			PlayerCfg["AntiHSEnabled"] = Player->AntiHSEnabled;
			PlayerCfg["NoClipEnabled"] = Player->NoClipEnabled;
			PlayerCfg["God1k"] = Player->God1k;
			PlayerCfg["God1kKey"] = Player->God1kKey;
			PlayerCfg["NoClipKeyMode"] = Player->NoClipKeyMode;
			PlayerCfg["StealCarEnabled"] = Player->StealCarEnabled;
			PlayerCfg["SuperFistEnabled"] = Player->SuperFistEnabled;
			PlayerCfg["GodModeKey"] = Player->GodModeKey;
			PlayerCfg["HealKey"] = Player->HealKey;
			PlayerCfg["UnloadKey"] = Player->UnloadKey;
			PlayerCfg["FastRunKey"] = Player->FastRunKey;
			PlayerCfg["FakeLagMode"] = Player->FakeLagMode;
			PlayerCfg["FakeLagKey"] = Player->FakeLagKey;
			PlayerCfg["RapidFire"] = Player->RapidFire;
			PlayerCfg["RapidFireSpeed"] = Player->RapidFireSpeed;
			PlayerCfg["DamageBoost"] = Player->DamageBoost;
			PlayerCfg["DamageBoostValue"] = Player->DamageBoostValue;
			PlayerCfg["FakeLag"] = Player->FakeLag;
			PlayerCfg["VisibleRepair"] = Player->VisibleRepair;
			PlayerCfg["VehicleRain"] = Player->VehicleRain;
			PlayerCfg["HornetBoost"] = Player->HornetBoost;
			PlayerCfg["VehicleJump"] = Player->VehicleJump;
			PlayerCfg["VehicleParachute"] = Player->VehicleParachute;
			PlayerCfg["Boost"] = Player->Boost;
			PlayerCfg["FakeLagDelay"] = Player->FakeLagDelay;

			return CfgJson;
		}

		std::string ExportCurrentConfigJsonString() {
			try {
				return CaptureCurrentConfigJson().dump();
			}
			catch (const std::exception&) {
				return "";
			}
		}

		bool ApplyConfigJsonObject(const nlohmann::json& CfgJson) {
			try {
				auto& GeneralCfg = CfgJson["General"];
				if (!GeneralCfg.is_null()) {
					General->VisibleCheckMode = GeneralCfg.value("VisibleCheckMode", General->VisibleCheckMode);
					General->Notifications = GeneralCfg.value("Notifications", General->Notifications);
					General->StreamProof = GeneralCfg.value("StreamProof", General->StreamProof);
					General->NVStreamProof = GeneralCfg.value("NVStreamProof", General->NVStreamProof);
					if (!General->StreamProof) {
						General->NVStreamProof = false;
					}
					General->Destruct = GeneralCfg.value("Destruct", General->Destruct);
					General->Unload = GeneralCfg.value("Unload", General->Unload);
					General->MirrorFivem = GeneralCfg.value("MirrorFivem", General->MirrorFivem);
					General->SpectatorList = GeneralCfg.value("SpectatorList", GeneralCfg.value("Observers", General->SpectatorList));
					General->SaveFriendList = GeneralCfg.value("SaveFriendList", General->SaveFriendList);
					General->WaterMark = GeneralCfg.value("WaterMark", General->WaterMark);
					General->WaterMarkCol = JsonToImCol(GeneralCfg.value("WaterMarkCol", ImColToJson(General->WaterMarkCol)));
					General->KeybindList = GeneralCfg.value("KeybindList", General->KeybindList);
					General->Crosshair = GeneralCfg.value("Crosshair", General->Crosshair);
					General->CrosshairStyle = GeneralCfg.value("CrosshairStyle", General->CrosshairStyle);
					General->CrosshairSize = GeneralCfg.value("CrosshairSize", General->CrosshairSize);
					General->CrosshairThickness = GeneralCfg.value("CrosshairThickness", General->CrosshairThickness);
					General->CrosshairRoundness = GeneralCfg.value("CrosshairRoundness", General->CrosshairRoundness);
					General->CrosshairMainColor = JsonToImCol(GeneralCfg.value("CrosshairMainColor", ImColToJson(General->CrosshairMainColor)));
					General->CrosshairAccentColor = JsonToImCol(GeneralCfg.value("CrosshairAccentColor", ImColToJson(General->CrosshairAccentColor)));
					General->CrosshairCustomPixels = JsonToCrosshairPixels(
						GeneralCfg.value("CrosshairCustomPixels", nlohmann::json::array()),
						General->CrosshairCustomPixels
					);
					const nlohmann::json* FriendListJson = nullptr;
					if (GeneralCfg.contains("FriendList")) {
						FriendListJson = &GeneralCfg["FriendList"];
					}
					else if (GeneralCfg.contains("FriendNames")) {
						FriendListJson = &GeneralCfg["FriendNames"];
					}
					else if (GeneralCfg.contains("Friends")) {
						FriendListJson = &GeneralCfg["Friends"];
					}

					if (FriendListJson) {
						Core::Features::cFriends::StringToFriends(FriendListJsonToString(*FriendListJson));
					}
					General->ArrayList = GeneralCfg.value("ArrayList", General->ArrayList);
					General->ArrayListCol = JsonToImCol(GeneralCfg.value("ArrayListCol", ImColToJson(General->ArrayListCol)));
					General->VSync = GeneralCfg.value("VSync", General->VSync);
					General->ProcessPriority = GeneralCfg.value("ProcessPriority", General->ProcessPriority);
					General->MenuKey = GeneralCfg.value("MenuKey", General->MenuKey);
					General->UnloadKey = GeneralCfg.value("UnloadKey", General->UnloadKey);
					General->CustomInput = GeneralCfg.value("CustomInput", General->CustomInput);
					General->SecondMonitor = GeneralCfg.value("SecondMonitor", General->SecondMonitor);
					General->AddFriend = GeneralCfg.value("AddFriend", General->AddFriend);
					General->AutoConfig = GeneralCfg.value("AutoConfig", General->AutoConfig);
					General->MenuThemeMode = GeneralCfg.value("MenuThemeMode", General->MenuThemeMode);
					General->MenuManualTheme = GeneralCfg.value("MenuManualTheme", General->MenuManualTheme);
					General->MenuThemeScheme = JsonToImCol(GeneralCfg.value("MenuThemeScheme", ImColToJson(General->MenuThemeScheme)));
					General->MenuThemeText = JsonToImCol(GeneralCfg.value("MenuThemeText", ImColToJson(General->MenuThemeText)));
					General->MenuThemeText2 = JsonToImCol(GeneralCfg.value("MenuThemeText2", ImColToJson(General->MenuThemeText2)));
					General->MenuThemeText3 = JsonToImCol(GeneralCfg.value("MenuThemeText3", ImColToJson(General->MenuThemeText3)));
					General->MenuThemeBg = JsonToImCol(GeneralCfg.value("MenuThemeBg", ImColToJson(General->MenuThemeBg)));
					General->MenuThemeBg2 = JsonToImCol(GeneralCfg.value("MenuThemeBg2", ImColToJson(General->MenuThemeBg2)));
					General->MenuThemeBg3 = JsonToImCol(GeneralCfg.value("MenuThemeBg3", ImColToJson(General->MenuThemeBg3)));
					General->MenuThemeBg4 = JsonToImCol(GeneralCfg.value("MenuThemeBg4", ImColToJson(General->MenuThemeBg4)));
					General->MenuThemeBg5 = JsonToImCol(GeneralCfg.value("MenuThemeBg5", ImColToJson(General->MenuThemeBg5)));
					General->MenuThemeBg6 = JsonToImCol(GeneralCfg.value("MenuThemeBg6", ImColToJson(General->MenuThemeBg6)));
					General->MenuThemeBg7 = JsonToImCol(GeneralCfg.value("MenuThemeBg7", ImColToJson(General->MenuThemeBg7)));
					General->MenuThemeBorder = JsonToImCol(GeneralCfg.value("MenuThemeBorder", ImColToJson(General->MenuThemeBorder)));
					General->MenuThemeSeparator = JsonToImCol(GeneralCfg.value("MenuThemeSeparator", ImColToJson(General->MenuThemeSeparator)));
					General->MenuThemeCheckboxDot = JsonToImCol(GeneralCfg.value("MenuThemeCheckboxDot", ImColToJson(General->MenuThemeCheckboxDot)));
					General->MenuThemeTextOnScheme = JsonToImCol(GeneralCfg.value("MenuThemeTextOnScheme", ImColToJson(General->MenuThemeTextOnScheme)));
					General->MenuThemeListbox = JsonToImCol(GeneralCfg.value("MenuThemeListbox", ImColToJson(General->MenuThemeListbox)));
					General->MenuThemeTab = JsonToImCol(GeneralCfg.value("MenuThemeTab", ImColToJson(General->MenuThemeTab)));
					General->MenuThemeBackdrop = JsonToImCol(GeneralCfg.value("MenuThemeBackdrop", ImColToJson(General->MenuThemeBackdrop)));
					ESP->Observers = GeneralCfg.value("Observers", General->SpectatorList);

				}

				if (CfgJson.contains("Aimbot")) {
					auto& AimbotCfg = CfgJson["Aimbot"];
					Aimbot->Enabled = AimbotCfg.value("Enabled", Aimbot->Enabled);
					Aimbot->ShowFov = AimbotCfg.value("ShowFov", Aimbot->ShowFov);
					Aimbot->ShowDynamicFov = AimbotCfg.value("ShowDynamicFov", Aimbot->ShowDynamicFov);
					Aimbot->OnlyVisible = AimbotCfg.value("OnlyVisible", Aimbot->OnlyVisible);
					Aimbot->CurveAimbot = AimbotCfg.value("CurveAimbot", Aimbot->CurveAimbot);
					Aimbot->IgnoreNPCs = AimbotCfg.value("IgnoreNPCs", Aimbot->IgnoreNPCs);
					Aimbot->IgnoreDead = AimbotCfg.value("IgnoreDead", Aimbot->IgnoreDead);
					Aimbot->RandomHitbox = AimbotCfg.value("RandomHitbox", Aimbot->RandomHitbox);
					Aimbot->Prediction = AimbotCfg.value("Prediction", Aimbot->Prediction);
					Aimbot->RespectMaxRange = AimbotCfg.value("RespectMaxRange", Aimbot->RespectMaxRange);
					Aimbot->LegitMode = AimbotCfg.value("LegitMode", Aimbot->LegitMode);
					Aimbot->StickyAim = AimbotCfg.value("StickyAim", Aimbot->StickyAim);
					Aimbot->CurvyMode = AimbotCfg.value("CurvyMode", Aimbot->CurvyMode);
					Aimbot->CurvyValue = AimbotCfg.value("CurvyValue", Aimbot->CurvyValue);
					Aimbot->PredictionValue = AimbotCfg.value("PredictionValue", Aimbot->PredictionValue);
					Aimbot->CurvyStrength = AimbotCfg.value("CurvyStrength", Aimbot->CurvyStrength);
					Aimbot->CurveDecel = AimbotCfg.value("CurveDecel", Aimbot->CurveDecel);
					Aimbot->CurveAccel = AimbotCfg.value("CurveAccel", Aimbot->CurveAccel);
					Aimbot->CurveEaseInOut = AimbotCfg.value("CurveEaseInOut", Aimbot->CurveEaseInOut);
					Aimbot->BoneGroup_Arms = AimbotCfg.value("ARMS", Aimbot->BoneGroup_Arms);
					Aimbot->BoneGroup_Neck = AimbotCfg.value("NECKS", Aimbot->BoneGroup_Neck);
					Aimbot->BoneGroup_Chest = AimbotCfg.value("CHESTS", Aimbot->BoneGroup_Chest);
					Aimbot->BoneGroup_Head = AimbotCfg.value("HEADS", Aimbot->BoneGroup_Head);
					Aimbot->BoneGroup_Legs = AimbotCfg.value("LEGS", Aimbot->BoneGroup_Legs);
					Aimbot->BoneGroup_Feet = AimbotCfg.value("FEET", Aimbot->BoneGroup_Feet);
					const int legacyAimbotFov = AimbotCfg.value("FOV", FovUtils::GetLegacyRadius(Aimbot->FOVX, Aimbot->FOVY));
					Aimbot->FOVX = AimbotCfg.value("FOVX", legacyAimbotFov);
					Aimbot->FOVY = AimbotCfg.value("FOVY", legacyAimbotFov);
					Aimbot->MaxDistance = AimbotCfg.value("MaxDistance", Aimbot->MaxDistance);
					Aimbot->SmoothHorizontal = AimbotCfg.value("SmoothHorizontal", Aimbot->SmoothHorizontal);
					Aimbot->SmoothVertical = AimbotCfg.value("SmoothVertical", Aimbot->SmoothVertical);
					Aimbot->AimbotSpeed = AimbotCfg.value("AimbotSpeed", AimbotCfg.value("AimSpeed", Aimbot->AimbotSpeed));
					Aimbot->KeyBind = AimbotCfg.value("KeyBind", Aimbot->KeyBind);
					Aimbot->KeyBindMode = AimbotCfg.value("KeyBindMode", Aimbot->KeyBindMode);
					Aimbot->DynamicFovMinDistance = AimbotCfg.value("DynamicFovMinDistance", Aimbot->DynamicFovMinDistance);
					Aimbot->DynamicFovMaxDistance = AimbotCfg.value("DynamicFovMaxDistance", Aimbot->DynamicFovMaxDistance);
					Aimbot->DynamicFovMinValue = AimbotCfg.value("DynamicFovMinValue", Aimbot->DynamicFovMinValue);
					Aimbot->DynamicFovMaxValue = AimbotCfg.value("DynamicFovMaxValue", Aimbot->DynamicFovMaxValue);
					Aimbot->HitBox = AimbotCfg.value("HitBox", Aimbot->HitBox);
					Aimbot->FovColor = JsonToImCol(AimbotCfg.value("FovColor", ImColToJson(Aimbot->FovColor)));
					Aimbot->FovThickness = AimbotCfg.value("FovThickness", Aimbot->FovThickness);
					Aimbot->FovFilled = AimbotCfg.value("FovFilled", Aimbot->FovFilled);
					Aimbot->FovFillColor = JsonToImCol(AimbotCfg.value("FovFillColor", ImColToJson(Aimbot->FovFillColor)));
					Aimbot->MissChance = AimbotCfg.value("MissChance", Aimbot->MissChance);
				}

				if (CfgJson.contains("TriggerBot")) {
					auto& TriggerBotCfg = CfgJson["TriggerBot"];
					TriggerBot->Enabled = TriggerBotCfg.value("Enabled", TriggerBot->Enabled);
					TriggerBot->ShowFov = TriggerBotCfg.value("ShowFov", TriggerBot->ShowFov);
					TriggerBot->ShowDynamicFov = TriggerBotCfg.value("ShowDynamicFov", TriggerBot->ShowDynamicFov);
					TriggerBot->SmartTrigger = TriggerBotCfg.value("SmartTrigger", TriggerBot->SmartTrigger);
					TriggerBot->OnlyVisible = TriggerBotCfg.value("OnlyVisible", TriggerBot->OnlyVisible);
					TriggerBot->IgnoreNPCs = TriggerBotCfg.value("IgnoreNPCs", TriggerBot->IgnoreNPCs);
					TriggerBot->IgnoreDead = TriggerBotCfg.value("IgnoreDead", TriggerBot->IgnoreDead);
					TriggerBot->RespectMaxRange = TriggerBotCfg.value("RespectMaxRange", TriggerBot->RespectMaxRange);
					TriggerBot->BoneGroup_Head = TriggerBotCfg.value("HEADS", TriggerBot->BoneGroup_Head);
					TriggerBot->BoneGroup_Neck = TriggerBotCfg.value("NECKS", TriggerBot->BoneGroup_Neck);
					TriggerBot->BoneGroup_Chest = TriggerBotCfg.value("CHESTS", TriggerBot->BoneGroup_Chest);
					TriggerBot->BoneGroup_Legs = TriggerBotCfg.value("LEGS", TriggerBot->BoneGroup_Legs);
					TriggerBot->BoneGroup_Arms = TriggerBotCfg.value("ARMS", TriggerBot->BoneGroup_Arms);
					TriggerBot->Arms = TriggerBotCfg.value("Arms", TriggerBot->Arms);
					const int legacyTriggerbotFov = TriggerBotCfg.value("FOV", FovUtils::GetLegacyRadius(TriggerBot->FOVX, TriggerBot->FOVY));
					TriggerBot->FOVX = TriggerBotCfg.value("FOVX", legacyTriggerbotFov);
					TriggerBot->FOVY = TriggerBotCfg.value("FOVY", legacyTriggerbotFov);
					TriggerBot->MaxDistance = TriggerBotCfg.value("MaxDistance", TriggerBot->MaxDistance);
					TriggerBot->Delay = TriggerBotCfg.value("Delay", TriggerBot->Delay);
					TriggerBot->KeyBind = TriggerBotCfg.value("KeyBind", TriggerBot->KeyBind);
					TriggerBot->KeyBindMode = TriggerBotCfg.value("KeyBindMode", TriggerBot->KeyBindMode);
					TriggerBot->DynamicFovMinDistance = TriggerBotCfg.value("DynamicFovMinDistance", TriggerBot->DynamicFovMinDistance);
					TriggerBot->DynamicFovMaxDistance = TriggerBotCfg.value("DynamicFovMaxDistance", TriggerBot->DynamicFovMaxDistance);
					TriggerBot->DynamicFovMinValue = TriggerBotCfg.value("DynamicFovMinValue", TriggerBot->DynamicFovMinValue);
					TriggerBot->DynamicFovMaxValue = TriggerBotCfg.value("DynamicFovMaxValue", TriggerBot->DynamicFovMaxValue);
					TriggerBot->FovColor = JsonToImCol(TriggerBotCfg.value("FovColor", ImColToJson(TriggerBot->FovColor)));
					TriggerBot->FovThickness = TriggerBotCfg.value("FovThickness", TriggerBot->FovThickness);
					TriggerBot->FovFilled = TriggerBotCfg.value("FovFilled", TriggerBot->FovFilled);
					TriggerBot->FovFillColor = JsonToImCol(TriggerBotCfg.value("FovFillColor", ImColToJson(TriggerBot->FovFillColor)));
				}

				if (CfgJson.contains("SilentAim")) {
					auto& SilentAimCfg = CfgJson["SilentAim"];
					SilentAim->Enabled = SilentAimCfg.value("Enabled", SilentAim->Enabled);
					SilentAim->EnableBypass = SilentAimCfg.value("EnableBypass", SilentAim->EnableBypass);
					SilentAim->ShowFov = SilentAimCfg.value("ShowFov", SilentAim->ShowFov);
					SilentAim->ShowDynamicFov = SilentAimCfg.value("ShowDynamicFov", SilentAim->ShowDynamicFov);
					SilentAim->OnlyVisible = SilentAimCfg.value("OnlyVisible", SilentAim->OnlyVisible);
					SilentAim->IgnoreNPCs = SilentAimCfg.value("IgnoreNPCs", SilentAim->IgnoreNPCs);
					SilentAim->IgnoreDead = SilentAimCfg.value("IgnoreDead", SilentAim->IgnoreDead);
					SilentAim->BypassPassive = SilentAimCfg.value("BypassPassive", SilentAim->BypassPassive);
					SilentAim->Prediction = SilentAimCfg.value("Prediction", SilentAim->Prediction);
					SilentAim->ModifyBulletSpeed = SilentAimCfg.value("ModifyBulletSpeed", SilentAim->ModifyBulletSpeed);
					SilentAim->RespectMaxRange = SilentAimCfg.value("RespectMaxRange", SilentAim->RespectMaxRange);
					SilentAim->SnapLines = SilentAimCfg.value("SnapLines", SilentAim->SnapLines);
					const int legacySilentAimFov = SilentAimCfg.value("FOV", FovUtils::GetLegacyRadius(SilentAim->FOVX, SilentAim->FOVY));
					SilentAim->FOVX = SilentAimCfg.value("FOVX", legacySilentAimFov);
					SilentAim->FOVY = SilentAimCfg.value("FOVY", legacySilentAimFov);
					SilentAim->SilentBypass = SilentAimCfg.value("SilentBypass", SilentAim->SilentBypass);
					SilentAim->WaveShieldBypass = SilentAimCfg.value("WaveShieldBypass", SilentAim->WaveShieldBypass);
					SilentAim->ExcludeGod = SilentAimCfg.value("ExcludeGod", SilentAim->ExcludeGod);
					SilentAim->BoneGroup_Wheels = SilentAimCfg.value("BoneGroup_Wheels", SilentAim->BoneGroup_Wheels);
					SilentAim->BoneGroup_Legs = SilentAimCfg.value("LEGS", SilentAim->BoneGroup_Legs);
					SilentAim->BoneGroup_Feet = SilentAimCfg.value("FEET", SilentAim->BoneGroup_Feet);
					SilentAim->BoneGroup_Arms = SilentAimCfg.value("ARMS", SilentAim->BoneGroup_Arms);
					SilentAim->BoneGroup_Chest = SilentAimCfg.value("CHESTS", SilentAim->BoneGroup_Chest);
					SilentAim->BoneGroup_Neck = SilentAimCfg.value("NECKS", SilentAim->BoneGroup_Neck);
					SilentAim->BoneGroup_Head = SilentAimCfg.value("HEADS", SilentAim->BoneGroup_Head);
					SilentAim->IgnoreUnoccupied = SilentAimCfg.value("IgnoreUnoccupied", SilentAim->IgnoreUnoccupied);
					SilentAim->MaxDistance = SilentAimCfg.value("MaxDistance", SilentAim->MaxDistance);
					SilentAim->KeyBind = SilentAimCfg.value("KeyBind", SilentAim->KeyBind);
					SilentAim->KeyBindWheel = SilentAimCfg.value("KeyBindWheel", SilentAim->KeyBindWheel);
					SilentAim->KeyBindMode = SilentAimCfg.value("KeyBindMode", SilentAim->KeyBindMode);
					SilentAim->WriteIntervalSec = SilentAimCfg.value("WriteIntervalSec", SilentAim->WriteIntervalSec);
					SilentAim->PredictionAmmount = SilentAimCfg.value("PredictionAmmount", SilentAim->PredictionAmmount);
					SilentAim->DesiredBulletSpeed = SilentAimCfg.value("DesiredBulletSpeed", SilentAim->DesiredBulletSpeed);
					SilentAim->MissChance = SilentAimCfg.value("MissChance", SilentAim->MissChance);
					SilentAim->DynamicMissChance = SilentAimCfg.value("DynamicMissChance", SilentAim->DynamicMissChance);
					SilentAim->MagicBullets = SilentAimCfg.value("MagicBullets", SilentAim->MagicBullets);
					SilentAim->DynamicFovMinDistance = SilentAimCfg.value("DynamicFovMinDistance", SilentAim->DynamicFovMinDistance);
					SilentAim->DynamicFovMaxDistance = SilentAimCfg.value("DynamicFovMaxDistance", SilentAim->DynamicFovMaxDistance);
					SilentAim->DynamicFovMinValue = SilentAimCfg.value("DynamicFovMinValue", SilentAim->DynamicFovMinValue);
					SilentAim->DynamicFovMaxValue = SilentAimCfg.value("DynamicFovMaxValue", SilentAim->DynamicFovMaxValue);
					SilentAim->FovColor = JsonToImCol(SilentAimCfg.value("FovColor", ImColToJson(SilentAim->FovColor)));
					SilentAim->FovThickness = SilentAimCfg.value("FovThickness", SilentAim->FovThickness);
					SilentAim->FovFilled = SilentAimCfg.value("FovFilled", SilentAim->FovFilled);
					SilentAim->FovFillColor = JsonToImCol(SilentAimCfg.value("FovFillColor", ImColToJson(SilentAim->FovFillColor)));
				}

				if (CfgJson.contains("ESP")) {
					auto& ESPCfg = CfgJson["ESP"];
					ESP->Enabled = ESPCfg.value("Enabled", ESP->Enabled);
					ESP->Observers = ESPCfg.value("Observers", GeneralCfg.value("Observers", ESP->Observers));
					ESP->Box = ESPCfg.value("Box", ESP->Box);
					ESP->CleanBox = ESPCfg.value("CleanBox", ESP->CleanBox);
					ESP->NameBackdrop = ESPCfg.value("NameBackdrop", ESP->NameBackdrop);
					ESP->HighlightImmortality = ESPCfg.value("HighlightImmortality", ESP->HighlightImmortality);
					ESP->FilledBox = ESPCfg.value("FilledBox", ESP->FilledBox);
					ESP->BoxState = ESPCfg.value("BoxState", ESP->BoxState);
					ESP->Skeleton = ESPCfg.value("Skeleton", ESP->Skeleton);
					ESP->HealthBar = ESPCfg.value("HealthBar", ESP->HealthBar);
					ESP->HealthBarPos = JsonToImVec2(ESPCfg.value("HealthBarPos", ImVec2ToJson(ESP->HealthBarPos)), ESP->HealthBarPos);
					ESP->HealthBarState = ESPCfg.value("HealthBarState", ESP->HealthBarState);
					ESP->HealthBarGradient = ESPCfg.value("HealthBarGradient", ESP->HealthBarGradient);
					ESP->HealthBarGradientStart = JsonToImCol(ESPCfg.value("HealthBarGradientStart", ImColToJson(ESP->HealthBarGradientStart)));
					ESP->HealthBarGradientEnd = JsonToImCol(ESPCfg.value("HealthBarGradientEnd", ImColToJson(ESP->HealthBarGradientEnd)));
					ESP->ArmorBar = ESPCfg.value("ArmorBar", ESP->ArmorBar);
					ESP->ArmorBarPos = JsonToImVec2(ESPCfg.value("ArmorBarPos", ImVec2ToJson(ESP->ArmorBarPos)), ESP->ArmorBarPos);
					ESP->ArmorBarState = ESPCfg.value("ArmorBarState", ESP->ArmorBarState);
					ESP->ArmorBarGradient = ESPCfg.value("ArmorBarGradient", ESP->ArmorBarGradient);
					ESP->ArmorBarGradientStart = JsonToImCol(ESPCfg.value("ArmorBarGradientStart", ImColToJson(ESP->ArmorBarGradientStart)));
					ESP->ArmorBarGradientEnd = JsonToImCol(ESPCfg.value("ArmorBarGradientEnd", ImColToJson(ESP->ArmorBarGradientEnd)));
					ESP->WeaponName = ESPCfg.value("WeaponName", ESP->WeaponName);
					ESP->IgnoreUnarmed = ESPCfg.value("IgnoreUnarmed", ESP->IgnoreUnarmed);
					ESP->WeaponNamePos = JsonToImVec2(ESPCfg.value("WeaponNamePos", ImVec2ToJson(ESP->WeaponNamePos)), ESP->WeaponNamePos);
					ESP->WeaponNameState = ESPCfg.value("WeaponNameState", ESP->WeaponNameState);
					ESP->SnapLines = ESPCfg.value("SnapLines", ESP->SnapLines);
					ESP->UserNames = ESPCfg.value("UserNames", ESP->UserNames);
					ESP->EspPreview = ESPCfg.value("EspPreview", ESP->EspPreview);
					ESP->DynamicNames = ESPCfg.value("DynamicNames", ESP->DynamicNames);
					ESP->UserNamesPos = JsonToImVec2(ESPCfg.value("UserNamesPos", ImVec2ToJson(ESP->UserNamesPos)), ESP->UserNamesPos);
					ESP->UserNamesState = ESPCfg.value("UserNamesState", ESP->UserNamesState);
					ESP->HeadCircle = ESPCfg.value("HeadCircle", ESP->HeadCircle);
					ESP->ShowPassive = ESPCfg.value("ShowPassive", ESP->ShowPassive);
					ESP->IgnoreNPCs = ESPCfg.value("IgnoreNPCs", ESP->IgnoreNPCs);
					ESP->ShowLocalPlayer = ESPCfg.value("ShowLocalPlayer", ESP->ShowLocalPlayer);
					ESP->ElectronAcBypass = ESPCfg.value("ElectronAcBypass", ESP->ElectronAcBypass);
					ESP->IgnoreLocalPed = ESPCfg.value("IgnoreLocalPed", ESP->IgnoreLocalPed);
					ESP->HighlightVisible = ESPCfg.value("HighlightVisible", ESP->HighlightVisible);
					ESP->IgnoreDead = ESPCfg.value("IgnoreDead", ESP->IgnoreDead);
					ESP->Target = ESPCfg.value("Target", ESP->Target);
					ESP->Targets = ESPCfg.value("Targets", ESP->Targets);
					ESP->RadarEnabled = ESPCfg.value("RadarEnabled", ESP->RadarEnabled);
					ESP->DistanceFromMe = ESPCfg.value("DistanceFromMe", ESP->DistanceFromMe);
					ESP->DistanceFromMePos = JsonToImVec2(ESPCfg.value("DistanceFromMePos", ImVec2ToJson(ESP->DistanceFromMePos)), ESP->DistanceFromMePos);
					ESP->DistanceFromMeState = ESPCfg.value("DistanceFromMeState", ESP->DistanceFromMeState);
					ESP->MaxDistance = ESPCfg.value("MaxDistance", ESP->MaxDistance);
					ESP->BoxThickness = ESPCfg.value("BoxThickness", ESP->BoxThickness);
					ESP->SkeletonThickness = ESPCfg.value("SkeletonThickness", ESP->SkeletonThickness);
					ESP->SnapLineThickness = ESPCfg.value("SnapLineThickness", ESP->SnapLineThickness);
					ESP->TextSize = ESPCfg.value("TextSize", ESP->TextSize);
					ESP->TextDistanceMultiplier = ESPCfg.value("TextDistanceMultiplier", ESP->TextDistanceMultiplier);
					ESP->TextThickness = ESPCfg.value("TextThickness", ESP->TextThickness);
					ESP->FriendsMarker = ESPCfg.value("FriendsMarker", ESP->FriendsMarker);
					ESP->FriendsMarkerBind = ESPCfg.value("FriendsMarkerBind", ESP->FriendsMarkerBind);
					ESP->DistanceCol = JsonToImCol(ESPCfg.value("DistanceCol", ImColToJson(ESP->DistanceCol)));
					ESP->UserNamesCol = JsonToImCol(ESPCfg.value("UserNamesCol", ImColToJson(ESP->UserNamesCol)));
					ESP->WeaponNameCol = JsonToImCol(ESPCfg.value("WeaponNameCol", ImColToJson(ESP->WeaponNameCol)));
					ESP->SkeletonCol = JsonToImCol(ESPCfg.value("SkeletonCol", ImColToJson(ESP->SkeletonCol)));
					ESP->TargetCol = JsonToImCol(ESPCfg.value("TargetCol", ImColToJson(ESP->TargetCol)));
					ESP->TargetColor = JsonToImCol(ESPCfg.value("TargetColor", ImColToJson(ESP->TargetColor)));
					ESP->BoxCol = JsonToImCol(ESPCfg.value("BoxCol", ImColToJson(ESP->BoxCol)));
					ESP->FilledBoxCol = JsonToImCol(ESPCfg.value("FilledBoxCol", ImColToJson(ESP->FilledBoxCol)));
					ESP->SnapLinesCol = JsonToImCol(ESPCfg.value("SnapLinesCol", ImColToJson(ESP->SnapLinesCol)));
					ESP->FriendCol = JsonToImCol(ESPCfg.value("FriendCol", ImColToJson(ESP->FriendCol)));
					ESP->VisibleCol = JsonToImCol(ESPCfg.value("VisibleCol", ImColToJson(ESP->VisibleCol)));
					ESP->InVisibleCol = JsonToImCol(ESPCfg.value("InVisibleCol", ImColToJson(ESP->InVisibleCol)));
					ESP->isTrueInvisibleCol = JsonToImCol(ESPCfg.value("isTrueInvisibleCol", ImColToJson(ESP->isTrueInvisibleCol)));
					ESP->KeyBind = ESPCfg.value("KeyBind", ESP->KeyBind);
				}

				if (CfgJson.contains("VehicleESP")) {
					auto& VehicleESPCfg = CfgJson["VehicleESP"];
					VehicleESP->Enabled = VehicleESPCfg.value("Enabled", VehicleESP->Enabled);
					VehicleESP->SnapLines = VehicleESPCfg.value("SnapLines", VehicleESP->SnapLines);
					VehicleESP->ShowLockUnlock = VehicleESPCfg.value("ShowLockUnlock", VehicleESP->ShowLockUnlock);
					VehicleESP->VehName = VehicleESPCfg.value("VehName", VehicleESP->VehName);
					VehicleESP->DistanceFromMe = VehicleESPCfg.value("DistanceFromMe", VehicleESP->DistanceFromMe);
					VehicleESP->MaxDistance = VehicleESPCfg.value("MaxDistance", VehicleESP->MaxDistance);
					VehicleESP->SnapLinesCol = JsonToImCol(VehicleESPCfg.value("SnapLinesCol", ImColToJson(VehicleESP->SnapLinesCol)));
				}

				if (CfgJson.contains("Player")) {
					auto& PlayerCfg = CfgJson["Player"];
					Player->SoloSession = PlayerCfg.value("SoloSession", Player->SoloSession);
					Player->FastRun = PlayerCfg.value("FastRun", Player->FastRun);
					Player->RunSpeed = PlayerCfg.value("RunSpeed", Player->RunSpeed);
					Player->InfiniteStamina = PlayerCfg.value("InfiniteStamina", Player->InfiniteStamina);
					Player->GetLocalPlayerInfo = PlayerCfg.value("GetLocalPlayerInfo", Player->GetLocalPlayerInfo);
					Player->GetEntitiyListSafe = PlayerCfg.value("GetEntitiyListSafe", Player->GetEntitiyListSafe);
					Player->WeaponOptions = PlayerCfg.value("WeaponOptions", Player->WeaponOptions);
					Player->NoRecoilEnabled = PlayerCfg.value("NoRecoilEnabled", Player->NoRecoilEnabled);
					Player->RecoilValue = PlayerCfg.value("RecoilValue", Player->RecoilValue);
					Player->NoSpreadEnabled = PlayerCfg.value("NoSpreadEnabled", Player->NoSpreadEnabled);
					Player->SpreadValue = PlayerCfg.value("SpreadValue", Player->SpreadValue);
					Player->InfiniteAmmoEnabled = PlayerCfg.value("InfiniteAmmoEnabled", Player->InfiniteAmmoEnabled);
					Player->WeaponRangeEnabled = PlayerCfg.value("WeaponRangeEnabled", Player->WeaponRangeEnabled);
					Player->WeaponRangeValue = PlayerCfg.value("WeaponRangeValue", Player->WeaponRangeValue);
					Player->Spinbot = PlayerCfg.value("Spinbot", Player->Spinbot);
					Player->SpinbotSpeed = PlayerCfg.value("SpinbotSpeed", Player->SpinbotSpeed);
					Player->NoReloadEnabled = PlayerCfg.value("NoReloadEnabled", Player->NoReloadEnabled);
					Player->NCEnabled = PlayerCfg.value("NCEnabled", Player->NCEnabled);
					Player->HandlingEditor = PlayerCfg.value("HandlingEditor", Player->HandlingEditor);
					Player->NoClipKey = PlayerCfg.value("NoClipKey", Player->NoClipKey);
					Player->NoClipEnabled = PlayerCfg.value("NoClipEnabled", Player->NoClipEnabled);
					Player->AutoStrafe = PlayerCfg.value("AutoStrafe", Player->AutoStrafe);
					Player->NoClipSpeed = PlayerCfg.value("NoClipSpeed", Player->NoClipSpeed);
					Player->InfiniteCombatRoll = PlayerCfg.value("InfiniteCombatRoll", Player->InfiniteCombatRoll);
					Player->Invisible = PlayerCfg.value("Invisible", Player->Invisible);
					Player->EnableGodMode = PlayerCfg.value("EnableGodMode", Player->EnableGodMode);
					Player->DisableGodMode = PlayerCfg.value("DisableGodMode", Player->DisableGodMode);
					Player->NoCollision = PlayerCfg.value("NoCollision", Player->NoCollision);
					Player->ResourceList = PlayerCfg.value("ResourceList", Player->ResourceList);
					Player->PlayerList = PlayerCfg.value("PlayerList", Player->PlayerList);
					Player->Immortality = PlayerCfg.value("Immortality", Player->Immortality);
					Player->ImmortalityKeyBind = PlayerCfg.value("ImmortalityKeyBind", Player->ImmortalityKeyBind);
					Player->ImmortalityKeyBindMode = PlayerCfg.value("ImmortalityKeyBindMode", Player->ImmortalityKeyBindMode);
					Player->FakelagToggle = PlayerCfg.value("FakelagToggle", Player->FakelagToggle);
					Player->Absorption = PlayerCfg.value("Absorption", Player->Absorption);
					Player->AbsorptionAmount = PlayerCfg.value("AbsorptionAmount", Player->AbsorptionAmount);
					Player->VehicleGodMode = PlayerCfg.value("VehicleGodMode", Player->VehicleGodMode);
					Player->SeatBelt = PlayerCfg.value("SeatBelt", Player->SeatBelt);
					Player->ForceWeaponWheel = PlayerCfg.value("ForceWeaponWheel", Player->ForceWeaponWheel);
					Player->ShrinkEnabled = PlayerCfg.value("ShrinkEnabled", Player->ShrinkEnabled);
					Player->NoRagDollEnabled = PlayerCfg.value("NoRagDollEnabled", Player->NoRagDollEnabled);
					Player->AntiHSEnabled = PlayerCfg.value("AntiHSEnabled", Player->AntiHSEnabled);
					Player->God1k = PlayerCfg.value("God1k", Player->God1k);
					Player->God1kKey = PlayerCfg.value("God1kKey", Player->God1kKey);
					Player->NoClipKeyMode = PlayerCfg.value("NoClipKeyMode", Player->NoClipKeyMode);
					Player->StealCarEnabled = PlayerCfg.value("StealCarEnabled", Player->StealCarEnabled);
					Player->SuperFistEnabled = PlayerCfg.value("SuperFistEnabled", Player->SuperFistEnabled);
					Player->GodModeKey = PlayerCfg.value("GodModeKey", Player->GodModeKey);
					Player->HealKey = PlayerCfg.value("HealKey", Player->HealKey);
					Player->UnloadKey = PlayerCfg.value("UnloadKey", Player->UnloadKey);
					Player->FastRunKey = PlayerCfg.value("FastRunKey", Player->FastRunKey);
					Player->RapidFire = PlayerCfg.value("RapidFire", Player->RapidFire);
					Player->RapidFireSpeed = PlayerCfg.value("RapidFireSpeed", Player->RapidFireSpeed);
					Player->DamageBoost = PlayerCfg.value("DamageBoost", Player->DamageBoost);
					Player->DamageBoostValue = PlayerCfg.value("DamageBoostValue", Player->DamageBoostValue);
					Player->FakeLagMode = PlayerCfg.value("FakeLagMode", Player->FakeLagMode);
					Player->FakeLagKey = PlayerCfg.value("FakeLagKey", Player->FakeLagKey);
					Player->FakeLag = PlayerCfg.value("FakeLag", Player->FakeLag);
					Player->VisibleRepair = PlayerCfg.value("VisibleRepair", Player->VisibleRepair);
					Player->VehicleRain = PlayerCfg.value("VehicleRain", Player->VehicleRain);
					Player->HornetBoost = PlayerCfg.value("HornetBoost", Player->HornetBoost);
					Player->VehicleJump = PlayerCfg.value("VehicleJump", Player->VehicleJump);
					Player->VehicleParachute = PlayerCfg.value("VehicleParachute", Player->VehicleParachute);
					Player->Boost = PlayerCfg.value("Boost", Player->Boost);
					Player->FakeLagDelay = PlayerCfg.value("FakeLagDelay", Player->FakeLagDelay);
				}

				ESP->UpdateCfgESP = true;
				return true;
			}
			catch (const std::exception&) {
				return false;
			}
		}

		bool ApplyConfigJsonString(const std::string& ConfigStr) {
			if (ConfigStr.empty()) {
				return false;
			}

			nlohmann::json CfgJson = nlohmann::json::parse(ConfigStr, nullptr, false);
			if (CfgJson.is_discarded()) {
				return false;
			}

			return ApplyConfigJsonObject(CfgJson);
		}

		std::string NormalizeConfigName(const std::string& CfgName) {
			if (CfgName.empty()) {
				return xorstr("default");
			}

			if (!std::regex_match(CfgName, std::regex("^[a-zA-Z0-9_-]+$"))) {
				return "";
			}

			return CfgName;
		}

		std::string SaveCurrentConfig(const std::string& CfgName) {
			try {
				if (NormalizeConfigName(CfgName).empty()) {
					return "";
				}

				std::string CfgJsonStr = CaptureCurrentConfigJson().dump();

				std::string hwid = auth::get_hwid();
				std::string payload =
					"{\"hwid\":\"" + auth::json_escape(hwid) +
					"\",\"action\":\"save\"";

				if (!CfgName.empty()) {
					payload += ",\"config_name\":\"" + auth::json_escape(CfgName) + "\"";
				}

				payload += ",\"config\":\"" + auth::json_escape(CfgJsonStr) + "\"}";

				std::string response;
				if (!auth::http_post_json(auth::CONFIG_API_URL, payload, &response)) {
					return "";
				}

				nlohmann::json responseJson = nlohmann::json::parse(response, nullptr, false);
				if (responseJson.is_discarded()) {
					return "";
				}

				if (responseJson.contains("error")) {
					return "";
				}

				return CfgJsonStr;
			}
			catch (const std::exception&) {
				return "";
			}
		}

		std::string LoadCfg(const std::string& CfgName, const std::string& ConfigStr) {
			try {
				if (!ConfigStr.empty()) {
					return ApplyConfigJsonString(ConfigStr) ? ConfigStr : "";
				}

				if (NormalizeConfigName(CfgName).empty()) {
					return "";
				}

				std::string hwid = auth::get_hwid();
				std::string payload =
					"{\"hwid\":\"" + auth::json_escape(hwid) +
					"\",\"action\":\"load\"";

				if (!CfgName.empty()) {
					payload += ",\"config_name\":\"" + auth::json_escape(CfgName) + "\"";
				}

				payload += "}";
				std::string response;
				if (!auth::http_post_json(auth::CONFIG_API_URL, payload, &response)) {
					return "";
				}
				nlohmann::json responseJson = nlohmann::json::parse(response, nullptr, false);
				if (responseJson.is_discarded()) {
					return "";
				}

				if (responseJson.contains("error")) {
					return "";
				}

				if (!responseJson.contains("config") || !responseJson["config"].is_string()) {
					return "";
				}

				std::string configStr = responseJson["config"].get<std::string>();
				return ApplyConfigJsonString(configStr) ? configStr : "";
			}
			catch (const std::exception&) {
				return "";
			}
		}
	};
	inline Config g_Config;
}

#endif
