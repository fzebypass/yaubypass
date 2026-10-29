#define NOMINMAX
#include "Esp.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include "friends.hpp"
#include "TargetHighlight.hpp"
#include "WizardVisuals.hpp"

namespace
{
	ImVec2 SnapTextPos(const ImVec2& pos)
	{
		return ImVec2(std::roundf(pos.x), std::roundf(pos.y));
	}

	void DrawOutlinedText(ImDrawList* drawList, ImFont* font, float size, const ImVec2& pos, ImColor color, const char* text, float outlineThickness = 0.0f)
	{
		if (!drawList || !font || !text || !text[0])
			return;

		const float textSize = std::max(0.2f, size);
		const float clampedOutline = std::clamp(outlineThickness, 0.0f, 1.25f);

		if (clampedOutline > 0.0f)
		{
			const float outlineOffset = std::max(0.65f, clampedOutline * 0.85f);
			const int outlineAlpha = std::clamp(static_cast<int>(std::roundf(color.Value.w * 255.0f * (0.42f + clampedOutline * 0.18f))), 0, 255);
			const ImColor outlineColor(0, 0, 0, outlineAlpha);
			const std::array<ImVec2, 4> outlineOffsets = {
				ImVec2(-outlineOffset, 0.0f),
				ImVec2(outlineOffset, 0.0f),
				ImVec2(0.0f, -outlineOffset),
				ImVec2(0.0f, outlineOffset)
			};

			for (const auto& offset : outlineOffsets)
			{
				drawList->AddText(font, textSize, SnapTextPos(ImVec2(pos.x + offset.x, pos.y + offset.y)), outlineColor, text);
			}
		}

		drawList->AddText(font, textSize, SnapTextPos(pos), color, text);
	}

	void DrawOutlinedText(ImDrawList* drawList, const ImVec2& pos, ImColor color, const char* text)
	{
		DrawOutlinedText(drawList, ImGui::GetFont(), ImGui::GetFontSize(), pos, color, text);
	}

	ImColor GetRainbowColor()
	{
		const float hue = std::fmodf(static_cast<float>(GetTickCount64()) * 0.002f, 1.0f);
		return ImColor::HSV(hue, 0.90f, 1.00f, 1.00f);
	}
}

struct LookingPlayer {
	std::string name;
	float distance;
	int lookPercentage;
	float currentLookPct;
	float targetLookPct;
	bool isAimingAtYou;
};

struct SpectatorInfo {
	std::string name;
	float distance;
	float currentPct;
	float targetPct;
};

void Core::Features::cEsp::Observers()
{
	if (g_Config.General->SpectatorList)
	{
		static int lookFrameCounter = 0;
		static std::vector<LookingPlayer> currentLookingPlayers;

		lookFrameCounter++;
		bool shouldRecalculateLook = (lookFrameCounter >= 20);

		if (shouldRecalculateLook)
		{
			lookFrameCounter = 0;

			if (Core::SDK::Pointers::pLocalPlayer)
			{
				D3DXVECTOR3 localPos = Core::SDK::Pointers::pLocalPlayer->GetPos();
				std::vector<LookingPlayer> newLookingPlayers;
				const auto entities = Core::SDK::Game::GetEntityListSnapshot();

				for (const auto& entity : entities)
				{
					CPed* Ped = entity.Ped;

					if (!entity.Ped || entity.Ped == Core::SDK::Pointers::pLocalPlayer)
						continue;

					if (!entity.IsPlayer || entity.IsFriend)
						continue;

					D3DXVECTOR3 playerPos = entity.Pos;
					float dx = playerPos.x - localPos.x;
					float dy = playerPos.y - localPos.y;
					float dz = playerPos.z - localPos.z;
					float distance = sqrt(dx * dx + dy * dy + dz * dz);

					if (distance > 200.0f)
						continue;

					uintptr_t navigation = Mem.Read<uintptr_t>((uintptr_t)entity.Ped + 0x30);
					if (!navigation)
						continue;

					struct Rotation { float yaw, pitch, roll; };
					Rotation rotation = Mem.Read<Rotation>(navigation + 0x30);

					D3DXVECTOR3 forward(rotation.yaw, rotation.pitch, rotation.roll);
					float len = D3DXVec3Length(&forward);
					if (len < 0.001f)
						continue;

					forward /= len;

					D3DXVECTOR3 toLocal = localPos - playerPos;
					float toLen = D3DXVec3Length(&toLocal);
					if (toLen < 0.001f)
						continue;

					toLocal /= toLen;

					float lookingDot = D3DXVec3Dot(&forward, &toLocal);
					const float FOV_THRESHOLD = 0.833f;

					if (lookingDot > FOV_THRESHOLD && entity.IsVisible3)
					{
						LookingPlayer lp;
						lp.name = Utils::ValidateTextForRendering(entity.NetworkInfo.UserName);
						lp.distance = distance;
						lp.isAimingAtYou = entity.Ped->HasFlag(ePedConfigFlag::IsAimingGun);

						float normalized = std::clamp(
							(lookingDot - FOV_THRESHOLD) / (1.0f - FOV_THRESHOLD),
							0.0f,
							1.0f
						);

						float boosted = powf(normalized, 2.0f);

						float distanceFactor = std::clamp(1.0f - (distance / 200.0f), 0.0f, 1.0f);
						float final = boosted * (0.7f + distanceFactor * 0.3f);

						float targetPct = final * 100.0f;

						auto it = std::find_if(currentLookingPlayers.begin(), currentLookingPlayers.end(),
							[&](const LookingPlayer& oldLp) { return oldLp.name == lp.name; });

						if (it != currentLookingPlayers.end())
						{
							lp.currentLookPct = it->currentLookPct;
							lp.targetLookPct = targetPct;
						}
						else
						{
							lp.currentLookPct = targetPct;
							lp.targetLookPct = targetPct;
						}

						lp.lookPercentage = static_cast<int>(lp.currentLookPct);
						newLookingPlayers.push_back(lp);
					}
				}

				currentLookingPlayers = newLookingPlayers;
			}
		}

		for (auto& lp : currentLookingPlayers)
		{
			if (lp.currentLookPct < lp.targetLookPct)
				lp.currentLookPct = std::min(lp.currentLookPct + 1.0f, lp.targetLookPct);
			else if (lp.currentLookPct > lp.targetLookPct)
				lp.currentLookPct = std::max(lp.currentLookPct - 1.0f, lp.targetLookPct);

			lp.lookPercentage = static_cast<int>(lp.currentLookPct);
		}

		if (!currentLookingPlayers.empty())
		{
			ImDrawList* draw = ImGui::GetBackgroundDrawList();
			float padding = 8.0f;
			ImVec2 iconSize = ImVec2(24, 24);

			ImVec2 screen_size = ImGui::GetIO().DisplaySize;
			ImVec2 center(screen_size.x * 0.5f, screen_size.y * 0.5f);

			float offset = 100.f;
			float yOffset = 10.f;

			int aimingCount = 0;
			std::string firstAimerName = "";
			int lookingCount = 0;
			std::string firstLookerName = "";

			for (const auto& lp : currentLookingPlayers)
			{
				if (lp.isAimingAtYou)
				{
					aimingCount++;
					if (firstAimerName.empty())
						firstAimerName = lp.name;
				}
				else
				{
					lookingCount++;
					if (firstLookerName.empty())
						firstLookerName = lp.name;
				}
			}

			if (aimingCount > 0)
			{
				ImGui::PushFont(g_Variables.FontAwesomeSolid);
				ImVec2 textPos(center.x + offset, center.y - yOffset);
				DrawOutlinedText(draw, textPos, IM_COL32(255, 80, 80, 255), ICON_FA_CROSSHAIRS);
				ImGui::PopFont();

				std::string percentText = "";

				if (aimingCount == 1)
				{
					for (const auto& lp : currentLookingPlayers)
					{
						if (lp.isAimingAtYou)
						{
							percentText = " (" + std::to_string((int)lp.lookPercentage) + "%)";
							break;
						}
					}
				}

				ImGui::PushFont(g_Variables.m_FontSecundary);
				ImVec2 namePos(textPos.x + iconSize.x + padding, textPos.y);

				std::string nameText = aimingCount == 1 ? firstAimerName + percentText : std::to_string(aimingCount) + " players";
				std::string actionText = aimingCount == 1 ? " is aiming at you" : " are aiming at you";

				DrawOutlinedText(draw, namePos, IM_COL32(240, 240, 240, 255), nameText.c_str());
				ImVec2 actionPos(namePos.x + ImGui::CalcTextSize(nameText.c_str()).x + padding, namePos.y);
				DrawOutlinedText(draw, actionPos, IM_COL32(200, 200, 200, 255), actionText.c_str());
				ImGui::PopFont();
			}
			else if (lookingCount > 0)
			{
				ImGui::PushFont(g_Variables.FontAwesomeSolid);
				ImVec2 textPos(center.x + offset, center.y - yOffset);
				DrawOutlinedText(draw, textPos, IM_COL32(80, 255, 120, 255), ICON_FA_EYE);
				ImGui::PopFont();

				std::string percentText = "";

				if (lookingCount == 1)
				{
					for (const auto& lp : currentLookingPlayers)
					{
						if (!lp.isAimingAtYou)
						{
							percentText = " (" + std::to_string((int)lp.lookPercentage) + "%)";
							break;
						}
					}
				}

				ImGui::PushFont(g_Variables.m_FontSecundary);
				ImVec2 namePos(textPos.x + iconSize.x + padding, textPos.y);

				std::string nameText = lookingCount == 1 ? firstLookerName + percentText : std::to_string(lookingCount) + " players";
				std::string actionText = lookingCount == 1 ? " is looking at you" : " are looking at you";

				DrawOutlinedText(draw, namePos, IM_COL32(240, 240, 240, 255), nameText.c_str());
				ImVec2 actionPos(namePos.x + ImGui::CalcTextSize(nameText.c_str()).x + padding, namePos.y);
				DrawOutlinedText(draw, actionPos, IM_COL32(200, 200, 200, 255), actionText.c_str());
				ImGui::PopFont();
			}
		}
	}

	if (g_Config.General->SpectatorList)
	{
		static int specFrameCounter = 0;
		static int cachedSpectators = 0;
		static std::string cachedSpectatorName = "";

		specFrameCounter++;
		bool shouldRecalculateSpec = (specFrameCounter >= 20);

		if (shouldRecalculateSpec)
		{
			specFrameCounter = 0;

			int spectators = 0;
			std::string spectatorName = "";

			if (Core::SDK::Pointers::pLocalPlayer)
			{
				const auto entities = Core::SDK::Game::GetEntityListSnapshot();

				for (const auto& entity : entities)
				{
					if (!entity.Ped || entity.Ped == Core::SDK::Pointers::pLocalPlayer)
						continue;

					if (!entity.IsPlayer)
						continue;

					bool isAdmin = entity.Ped->IsTrueInvisible();

					if (!isAdmin)
						continue;

					float distance = entity.Distance;

					if (distance > 100.0f)
						continue;

					spectators++;

					if (spectators == 1)
						spectatorName = Utils::ValidateTextForRendering(entity.NetworkInfo.UserName);
				}
			}

			cachedSpectators = spectators;
			cachedSpectatorName = spectatorName;
		}

		if (cachedSpectators > 0)
		{
			ImDrawList* draw = ImGui::GetBackgroundDrawList();

			std::string nameText =
				cachedSpectators == 1 ?
				cachedSpectatorName :
				std::to_string(cachedSpectators) + " players";

			std::string actionText =
				cachedSpectators == 1 ?
				" is spectating you" :
				" are spectating you";

			float padding = 8.0f;
			ImVec2 iconSize(24, 24);

			ImVec2 nameSize = ImGui::CalcTextSize(nameText.c_str());
			ImVec2 actionSize = ImGui::CalcTextSize(actionText.c_str());

			float totalWidth = iconSize.x + padding + nameSize.x + padding + actionSize.x;

			ImVec2 screen_size = ImGui::GetIO().DisplaySize;
			ImVec2 center(screen_size.x * 0.5f, screen_size.y * 0.5f);

			float offset = 100.f;
			float yOffset = 10.f;

			ImVec2 textPos(center.x - offset - totalWidth, center.y - yOffset);

			ImGui::PushFont(g_Variables.FontAwesomeSolid);
			DrawOutlinedText(draw, textPos, IM_COL32(255, 70, 70, 255), ICON_FA_EYE);
			ImGui::PopFont();

			ImGui::PushFont(g_Variables.m_FontSecundary);
			ImVec2 namePos(textPos.x + iconSize.x + padding, textPos.y);
			DrawOutlinedText(draw, namePos, IM_COL32(240, 240, 240, 255), nameText.c_str());

			ImVec2 actionPos(namePos.x + nameSize.x + padding, namePos.y);
			DrawOutlinedText(draw, actionPos, IM_COL32(200, 200, 200, 200), actionText.c_str());
			ImGui::PopFont();
		}
	}
}

void Core::Features::cEsp::Draw()
{
	auto Cfg = g_Config.ESP;

	if (!Cfg->Enabled)
		return;

	static std::unordered_map<CPed*, Core::SDK::Game::EspAnim> vEspAnimations;
	const auto ApplyAlpha = [](ImColor color, float alphaScale) -> ImColor
	{
		ImVec4 value = color.Value;
		value.w = ImClamp(value.w * alphaScale, 0.0f, 1.0f);
		return ImColor(value);
	};
	const auto entities = Core::SDK::Game::GetEntityListSnapshot();
	for (const auto& Entity : entities)
	{
		CPed* Ped = Entity.Ped;
		auto DrawList = ImGui::GetBackgroundDrawList();

		auto& CurrentESPAnim = vEspAnimations[Ped];
		CurrentESPAnim.CanFadeOut = false;

		float Distance = Entity.Distance;
		float Health = Entity.Health;
		int PedType = Entity.PedType;
		bool IsFriend = Entity.IsFriend;
		bool IsCheating = Entity.IsCheating;
		bool IsTargeting = Entity.IsTargeting;
		bool IsVisible = Entity.IsVisible;
		bool IsInVisible = Entity.Ped->IsTrueInvisible();
		bool IsLocalPlayer = Ped == Core::SDK::Pointers::pLocalPlayer;
		const bool showWeaponNameLabel = Cfg->WeaponName && !Entity.WeaponName.empty();

		if (Cfg->IgnoreSelf && IsLocalPlayer)
			continue;

		if (Distance > Cfg->MaxDistance)
			continue;

		if (Cfg->IgnoreNPCs && !Entity.IsPlayer && !IsLocalPlayer)
			continue;

		if (Cfg->IgnoreDead && !IsLocalPlayer)
		{
			if (PedType == 2)
			{
				if (Health > 400.f || Health <= 101.f)
					continue;
			}
			else
			{
				if (Health > 200.f || Health <= 1.f)
					continue;
			}
		}

		D3DXVECTOR3 HeadPos = Ped->GetBonePosDefault(0);
		D3DXVECTOR2 EntityTop = Core::SDK::Game::WorldToScreen(HeadPos + D3DXVECTOR3(0, 0, 0.2)); //Head

		D3DXVECTOR2 EntityBottom = Core::SDK::Game::WorldToScreen(
			Ped->GetBonePosDefault(8) - (g_Offsets.CurrentBuild >= 2802 ? D3DXVECTOR3(0, 0, 2) : D3DXVECTOR3(0, 0, 1.2)) //Abdomen
		);


		if (!Core::SDK::Game::IsOnScreen(EntityTop) || !Core::SDK::Game::IsOnScreen(EntityBottom))
			continue;

		float Height = EntityBottom.y - EntityTop.y;
		float Width = Height * 0.2f;

		const float BoxLeft = EntityTop.x - Width;
		const float BoxRight = EntityBottom.x + Width;

		ImVec2 BoxMin(BoxLeft, EntityTop.y);
		ImVec2 BoxMax(BoxRight, EntityBottom.y);
		ImVec2 BoxCenter = ImVec2((BoxMin.x + BoxMax.x) * 0.5f, (BoxMin.y + BoxMax.y) * 0.5f);
		const float maxVisualDistance = static_cast<float>(std::max(Cfg->MaxDistance, 50));
		const float distanceFactor = ImClamp((Distance - 15.0f) / std::max(1.0f, maxVisualDistance - 15.0f), 0.0f, 1.0f);
		const float visualScale = ImLerp(1.0f, 0.65f, distanceFactor);
		const float textDistanceScale = 1.0f + (Distance * std::clamp(Cfg->TextDistanceMultiplier, 0.0f, 0.005f));
		const float textScale = std::clamp(Cfg->TextSize * textDistanceScale, 0.50f, 3.0f);
		const float textThickness = std::clamp(Cfg->TextThickness, 0.0f, 1.0f);
		const float boxThickness = std::clamp(Cfg->BoxThickness, 0.5f, 5.0f);
		const float skeletonThickness = std::clamp(Cfg->SkeletonThickness, 0.5f, 5.0f);
		const float snapLineThickness = std::clamp(Cfg->SnapLineThickness, 0.5f, 5.0f);
		ImFont* textFont = g_Variables.m_DrawFont;
		ImFont* nameFont = g_Variables.m_DrawFont;
		std::array fonts = {
			g_Variables.m_DrawFont,
			g_Variables.m_DrawFont2,
			g_Variables.m_DrawFont3
		};

		auto PickFontByDistance = [&](float distanceFactor, auto currentFont)
			{
				float t = std::clamp(distanceFactor, 0.0f, 1.0f);

				int count = static_cast<int>(fonts.size());
				int index = static_cast<int>((1.0f - t) * count);

				if (index >= count)
					index = count - 1;
				if (index < 0)
					index = 0;

				for (int i = index; i >= 0; --i)
				{
					if (fonts[i])
						return fonts[i];
				}

				return currentFont;
			};

		textFont = PickFontByDistance(distanceFactor, textFont);
		nameFont = PickFontByDistance(distanceFactor, nameFont);

		const float baseTextFontSize = textFont ? textFont->FontSize : g_Variables.m_DrawFont->FontSize;
		const float baseNameFontSize = nameFont ? nameFont->FontSize : g_Variables.m_DrawFont->FontSize;
		const float textFontSize = std::max(baseTextFontSize * textScale, 12.0f);
		const float nameFontSize = std::max(baseNameFontSize * textScale, 12.0f);
		const float fontScaleRatio = textFontSize / std::max(1.0f, g_Variables.m_DrawFont->FontSize);
		const float textTopOffset = std::max(12.0f, 18.0f * visualScale * textScale);
		const float textBottomOffset = std::max(3.0f, 4.0f * visualScale * textScale);
		const float stackedTextOffset = std::max(8.0f, 15.0f * fontScaleRatio);
		const float barThickness = std::max(1.0f, 2.0f * visualScale);
		const float barOffset = std::max(4.0f, 6.0f * visualScale);
		const float barStackOffset = std::max(8.0f, 12.0f * visualScale);


		float FirstTextBoxTop = BoxMin.y - textTopOffset;

		float FirstTextBoxBottom = BoxMax.y + textBottomOffset;
		float SecondTextBoxBottom = BoxMax.y + (textBottomOffset * 2.0f) + (stackedTextOffset * 0.55f);
		float ThirdTextBoxBottom = BoxMax.y + (textBottomOffset * 3.0f) + stackedTextOffset;

		if (Cfg->HealthBarState == 2 && Cfg->ArmorBarState == 2) {
			FirstTextBoxBottom += barStackOffset + barOffset;
			SecondTextBoxBottom += barStackOffset + barOffset;
			ThirdTextBoxBottom += barStackOffset + barOffset;
		}
		else if (Cfg->HealthBarState == 2) {
			FirstTextBoxBottom += barOffset + barThickness;
			SecondTextBoxBottom += barOffset + barThickness;
			ThirdTextBoxBottom += barOffset + barThickness;
		}
		else if (Cfg->ArmorBarState == 2) {
			FirstTextBoxBottom += barStackOffset + barOffset;
			SecondTextBoxBottom += barStackOffset + barOffset;
			ThirdTextBoxBottom += barStackOffset + barOffset;
		}

		if (Cfg->HealthBarState == 0 && Cfg->ArmorBarState == 0) {
			FirstTextBoxTop -= textBottomOffset;
		}
		else if (Cfg->HealthBarState == 0) {
			FirstTextBoxTop -= textBottomOffset;
		}
		else if (Cfg->ArmorBarState == 0) {
			FirstTextBoxTop -= textBottomOffset;
		}

		ImTextureID wizardLogoTexture = reinterpret_cast<ImTextureID>(Core::Features::WizardVisuals::GetLogoTexture());
		if (!wizardLogoTexture)
			wizardLogoTexture = reinterpret_cast<ImTextureID>(g_Variables.Logo);

		const bool showWizardLogo =
			Core::Features::WizardVisuals::IsEnabled() &&
			Entity.IsPlayer &&
			wizardLogoTexture &&
			Core::Features::WizardVisuals::MatchesName(Entity.NetworkInfo.UserName);

		if (showWizardLogo)
		{
			const float logoSize = std::clamp(18.0f * visualScale, 12.0f, 22.0f);
			const float logoGap = std::max(5.0f, 6.0f * visualScale);
			const float logoY = FirstTextBoxTop - logoSize - logoGap;
			const ImVec2 logoMin(BoxCenter.x - (logoSize * 0.5f), logoY);
			const ImVec2 logoMax(logoMin.x + logoSize, logoMin.y + logoSize);

			DrawList->AddImage(
				wizardLogoTexture,
				logoMin,
				logoMax,
				ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f),
				IM_COL32(255, 255, 255, 235)
			);
		}

		if (Cfg->FriendsMarker && !IsLocalPlayer)
		{
			float radius = 2 / Distance;

			const static int FovRadius = 60;
			const static int Delay = 300;
			int Key = Cfg->FriendsMarkerBind;
			//std::string PlayerSetFriend = xorstr( "Press ") + std::string(keys[ Key ]) + xorstr(" to Add Friend" );
			std::string PlayerSetFriend = xorstr("Add Friend");
			std::string PlayerUnFriend = xorstr("Unfriend");


			int FovSize = std::hypot(BoxCenter.x - g_Variables.g_vGameWindowCenter.x, BoxCenter.y - g_Variables.g_vGameWindowCenter.y);

			ImGui::PushFont(g_Variables.m_FontSmaller);

			if (IsFriend && FovSize < FovRadius)
			{
				DrawList->AddCircleFilled(ImVec2(BoxCenter.x, BoxCenter.y), radius + Distance < 1 ? 1 : 4, ImColor(0, 0, 0), 24);
				DrawList->AddCircleFilled(ImVec2(BoxCenter.x, BoxCenter.y), radius + Distance < 1 ? 0 : 3, ImColor(0, 255, 0), 24);

				//ImVec2 TextSize = g_Variables.m_FontSmaller->CalcTextSizeA( g_Variables.m_FontSmaller->FontSize, FLT_MAX, 0.0f, PlayerUnFriend.c_str( ) );
				//DrawList->AddText( ImVec2( BoxCenter.x - ( TextSize.x / 2 ), BoxCenter.y ), ImColor( 0, 0, 0 ), PlayerUnFriend.c_str( ) );
				//DrawList->AddText( ImVec2( BoxCenter.x - ( TextSize.x / 2 ), BoxCenter.y - 2 ), ImColor( 255, 255, 255 ), PlayerUnFriend.c_str( ) );

				if (Utils::KeyPressedWithDelay(Key, Delay))
				{
					Core::Features::g_Friends.RemoveFriend(Ped);
					IsFriend = false;
				}
			}
			else if (!IsFriend && FovSize < FovRadius)
			{
				DrawList->AddCircleFilled(ImVec2(BoxCenter.x, BoxCenter.y), radius + Distance < 1 ? 1 : 4, ImColor(0, 0, 0), 24);
				DrawList->AddCircleFilled(ImVec2(BoxCenter.x, BoxCenter.y), radius + Distance < 1 ? 0 : 3, ImColor(255, 255, 255), 24);

				//ImVec2 TextSize = g_Variables.m_FontSmaller->CalcTextSizeA( g_Variables.m_FontSmaller->FontSize, FLT_MAX, 0.0f, PlayerSetFriend.c_str( ) );
				//DrawList->AddText( ImVec2( BoxCenter.x - ( TextSize.x / 2 ), BoxCenter.y ), ImColor( 0, 0, 0 ), PlayerSetFriend.c_str( ) );
				//DrawList->AddText( ImVec2( BoxCenter.x - ( TextSize.x / 2 ), BoxCenter.y - 1 ), ImColor( 255, 255, 255 ), PlayerSetFriend.c_str( ) );

				if (Utils::KeyPressedWithDelay(Key, Delay))
				{
					Core::Features::g_Friends.AddFriend(Ped);
					IsFriend = true;
				}
			}
			ImGui::PopFont();
		}
		//if (IsTargeting)
		//{
		//	Cfg->HeadCircle = true;
		//}
		ImColor boxColor = Cfg->BoxCol;
		ImColor skeletonColor = Cfg->SkeletonCol;
		ImColor highlightColor = Cfg->InVisibleCol;
		const bool isHighlightedTarget = Core::Features::TargetHighlight::IsPedHighlighted(Ped);

		if (IsFriend) {
			highlightColor = Cfg->FriendCol;
		}
		else if (IsInVisible) {
			highlightColor = Cfg->isTrueInvisibleCol;
		}
		else if (Cfg->HighlightVisible) {
			highlightColor = (IsVisible ? Cfg->VisibleCol : Cfg->InVisibleCol);
		}

		if (IsFriend || Cfg->HighlightVisible) {
			boxColor = highlightColor;
			skeletonColor = highlightColor;
		}

		if (Cfg->SnapLines)
		{
			const ImVec2 lineStart(g_Variables.g_vGameWindowCenter.x, g_Variables.g_vGameWindowSize.y);
			const ImVec2 lineEnd(BoxCenter.x, BoxMax.y);

			DrawList->AddLine(lineStart, lineEnd, ApplyAlpha(Cfg->SnapLinesCol, 0.22f), 2.0f * snapLineThickness);
			DrawList->AddLine(lineStart, lineEnd, Cfg->SnapLinesCol, 1.0f * snapLineThickness);
		}

		if (Cfg->Box)
		{
			if (Cfg->CleanBox)
			{
				if (Cfg->FilledBox)
					DrawList->AddRectFilled(BoxMin, BoxMax, Cfg->FilledBoxCol);

				const float boxW = BoxMax.x - BoxMin.x;
				const float boxH = BoxMax.y - BoxMin.y;
				const float segLen = ImClamp((std::min)(boxW, boxH) * 0.24f, 4.0f, 24.0f);

				const ImVec2 corners[4] = {
					ImVec2(BoxMin.x, BoxMin.y),
					ImVec2(BoxMax.x, BoxMin.y),
					ImVec2(BoxMax.x, BoxMax.y),
					ImVec2(BoxMin.x, BoxMax.y)
				};
				const float dirX[4] = { 1.0f, -1.0f, -1.0f, 1.0f };
				const float dirY[4] = { 1.0f, 1.0f, -1.0f, -1.0f };

				for (int i = 0; i < 4; ++i)
				{
					const ImVec2 hEnd(corners[i].x + dirX[i] * segLen, corners[i].y);
					const ImVec2 vEnd(corners[i].x, corners[i].y + dirY[i] * segLen);

					DrawList->AddLine(corners[i], hEnd, ApplyAlpha(boxColor, 0.30f), 2.6f * boxThickness);
					DrawList->AddLine(corners[i], vEnd, ApplyAlpha(boxColor, 0.30f), 2.6f * boxThickness);
					DrawList->AddLine(corners[i], hEnd, boxColor, 1.1f * boxThickness);
					DrawList->AddLine(corners[i], vEnd, boxColor, 1.1f * boxThickness);
				}
			}
			else
			{
				ImColor fillColor = Cfg->FilledBox ? Cfg->FilledBoxCol : ApplyAlpha(boxColor, 0.08f);
				DrawList->AddRectFilled(BoxMin, BoxMax, fillColor);
				DrawList->AddRect(BoxMin, BoxMax, ApplyAlpha(boxColor, 0.35f), 0.0f, 0, 2.0f * boxThickness);
				DrawList->AddRect(BoxMin, BoxMax, boxColor, 0.0f, 0, 1.0f * boxThickness);
			}
		}

		if (Cfg->ShowPassive && IsCheating)
		{
			const char* PassiveText = xorstr("PASSIVE");

			ImVec2 TextSize = nameFont->CalcTextSizeA(
				nameFontSize,
				FLT_MAX,
				0.0f,
				PassiveText
			);

			ImVec2 TextPos = ImVec2(
				BoxCenter.x - (TextSize.x / 2),
				FirstTextBoxTop - TextSize.y - 4.0f
			);

			DrawOutlinedText(
				DrawList,
				nameFont,
				nameFontSize,
				TextPos,
				ImColor(255, 0, 0, 255),
				PassiveText,
				textThickness
			);
		}

		if (Cfg->Skeleton)
		{
			if (isHighlightedTarget)
			{
				skeletonColor = GetRainbowColor();
			}

			uintptr_t FragInstNMGta = Mem.Read<uintptr_t>((uintptr_t)Ped + g_Offsets.m_FragInst);
			uintptr_t v9 = Mem.Read<uintptr_t>(FragInstNMGta + 0x68);

			if (!v9)
				continue;

			Core::SDK::Game::cSkeleton_t Skeleton;

			Skeleton.m_pSkeleton = Mem.Read<uintptr_t>(v9 + 0x178);

			Skeleton.crSkeletonData.Ptr = Mem.Read<uintptr_t>(Skeleton.m_pSkeleton);
			Skeleton.crSkeletonData.m_Used = Mem.Read<unsigned int>(Skeleton.crSkeletonData.Ptr + 0x1A);
			Skeleton.crSkeletonData.m_NumBones = Mem.Read<unsigned int>(Skeleton.crSkeletonData.Ptr + 0x5E);
			Skeleton.crSkeletonData.m_BoneIdTable_Slots = Mem.Read<unsigned short>(Skeleton.crSkeletonData.Ptr + 0x18);

			if (!Skeleton.crSkeletonData.m_BoneIdTable_Slots)
				continue;

			Skeleton.crSkeletonData.m_BoneIdTable = Mem.Read<uintptr_t>(Skeleton.crSkeletonData.Ptr + 0x10);

			Skeleton.Arg1 = Mem.Read<D3DXMATRIX>(Mem.Read<uintptr_t>(Skeleton.m_pSkeleton + 0x8));
			Skeleton.Arg2 = Mem.Read<uintptr_t>(Skeleton.m_pSkeleton + 0x18);


			D3DXVECTOR3 PelvisPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_Pelvis, Skeleton);
			D3DXVECTOR3 SpineRootPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_Spine_Root, Skeleton);
			D3DXVECTOR3 Spine0Pos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_Spine0, Skeleton);
			D3DXVECTOR3 Spine1Pos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_Spine1, Skeleton);
			D3DXVECTOR3 Spine2Pos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_Spine2, Skeleton);
			D3DXVECTOR3 Spine3Pos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_Spine3, Skeleton);
			D3DXVECTOR3 NeckPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_Neck_1, Skeleton);
			D3DXVECTOR3 HeadBonePos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_Head, Skeleton);
			D3DXVECTOR3 LeftClaviclePos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_L_Clavicle, Skeleton);
			D3DXVECTOR3 RightClaviclePos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_R_Clavicle, Skeleton);
			D3DXVECTOR3 LeftUperarmPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_L_UpperArm, Skeleton);
			D3DXVECTOR3 RightUperarmPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_R_UpperArm, Skeleton);
			D3DXVECTOR3 RightFormArmPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_R_Forearm, Skeleton);
			D3DXVECTOR3 LeftFormArmPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_L_Forearm, Skeleton);
			D3DXVECTOR3 RightHandPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_R_Hand, Skeleton);
			D3DXVECTOR3 LeftHandPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_L_Hand, Skeleton);
			D3DXVECTOR3 LeftThighPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_L_Thigh, Skeleton);
			D3DXVECTOR3 LeftCalfPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_L_Calf, Skeleton);
			D3DXVECTOR3 RightThighPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_R_Thigh, Skeleton);
			D3DXVECTOR3 RightCalfPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_R_Calf, Skeleton);
			D3DXVECTOR3 LfootPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_L_Foot, Skeleton);
			D3DXVECTOR3 RfootPos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_R_Foot, Skeleton);
			D3DXVECTOR3 LtoePos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_L_Toe0, Skeleton);
			D3DXVECTOR3 RtoePos = Core::SDK::Game::GetBonePosComplex(Ped, SKEL_R_Toe0, Skeleton);

			D3DXVECTOR2 Pelvis = Core::SDK::Game::WorldToScreen(PelvisPos);
			D3DXVECTOR2 SpineRoot = Core::SDK::Game::WorldToScreen(SpineRootPos);
			D3DXVECTOR2 Spine0 = Core::SDK::Game::WorldToScreen(Spine0Pos);
			D3DXVECTOR2 Spine1 = Core::SDK::Game::WorldToScreen(Spine1Pos);
			D3DXVECTOR2 Spine2 = Core::SDK::Game::WorldToScreen(Spine2Pos);
			D3DXVECTOR2 Spine3 = Core::SDK::Game::WorldToScreen(Spine3Pos);
			D3DXVECTOR2 Neck = Core::SDK::Game::WorldToScreen(NeckPos);
			D3DXVECTOR2 HeadBone = Core::SDK::Game::WorldToScreen(HeadBonePos);
			D3DXVECTOR2 LeftClavicle = Core::SDK::Game::WorldToScreen(LeftClaviclePos);
			D3DXVECTOR2 RightClavicle = Core::SDK::Game::WorldToScreen(RightClaviclePos);
			D3DXVECTOR2 LeftUperarm = Core::SDK::Game::WorldToScreen(LeftUperarmPos);
			D3DXVECTOR2 RightUperarm = Core::SDK::Game::WorldToScreen(RightUperarmPos);
			D3DXVECTOR2 RightFormArm = Core::SDK::Game::WorldToScreen(RightFormArmPos);
			D3DXVECTOR2 LeftFormArm = Core::SDK::Game::WorldToScreen(LeftFormArmPos);
			D3DXVECTOR2 RightHand = Core::SDK::Game::WorldToScreen(RightHandPos);
			D3DXVECTOR2 LeftHand = Core::SDK::Game::WorldToScreen(LeftHandPos);
			D3DXVECTOR2 LeftThigh = Core::SDK::Game::WorldToScreen(LeftThighPos);
			D3DXVECTOR2 LeftCalf = Core::SDK::Game::WorldToScreen(LeftCalfPos);
			D3DXVECTOR2 RightThigh = Core::SDK::Game::WorldToScreen(RightThighPos);
			D3DXVECTOR2 RightCalf = Core::SDK::Game::WorldToScreen(RightCalfPos);
			D3DXVECTOR2 Lfoot = Core::SDK::Game::WorldToScreen(LfootPos);
			D3DXVECTOR2 Rfoot = Core::SDK::Game::WorldToScreen(RfootPos);
			D3DXVECTOR2 Ltoe = Core::SDK::Game::WorldToScreen(LtoePos);
			D3DXVECTOR2 Rtoe = Core::SDK::Game::WorldToScreen(RtoePos);

			const auto DrawBoneLine = [&](const D3DXVECTOR2& from, const D3DXVECTOR2& to)
			{
				if (!Core::SDK::Game::IsOnScreen(from) || !Core::SDK::Game::IsOnScreen(to))
					return;

				const ImVec2 lineStart(from.x, from.y);
				const ImVec2 lineEnd(to.x, to.y);
				DrawList->AddLine(lineStart, lineEnd, ApplyAlpha(skeletonColor, 0.30f), 2.4f * skeletonThickness);
				DrawList->AddLine(lineStart, lineEnd, skeletonColor, 1.0f * skeletonThickness);
			};

			DrawBoneLine(Pelvis, SpineRoot);
			DrawBoneLine(SpineRoot, Spine0);
			DrawBoneLine(Spine0, Spine1);
			DrawBoneLine(Spine1, Spine2);
			DrawBoneLine(Spine2, Spine3);
			DrawBoneLine(Spine3, Neck);
			DrawBoneLine(Neck, HeadBone);

			DrawBoneLine(Spine3, LeftClavicle);
			DrawBoneLine(LeftClavicle, LeftUperarm);
			DrawBoneLine(LeftUperarm, LeftFormArm);
			DrawBoneLine(LeftFormArm, LeftHand);

			DrawBoneLine(Spine3, RightClavicle);
			DrawBoneLine(RightClavicle, RightUperarm);
			DrawBoneLine(RightUperarm, RightFormArm);
			DrawBoneLine(RightFormArm, RightHand);

			DrawBoneLine(Pelvis, LeftThigh);
			DrawBoneLine(LeftThigh, LeftCalf);
			DrawBoneLine(LeftCalf, Lfoot);
			DrawBoneLine(Lfoot, Ltoe);

			DrawBoneLine(Pelvis, RightThigh);
			DrawBoneLine(RightThigh, RightCalf);
			DrawBoneLine(RightCalf, Rfoot);
			DrawBoneLine(Rfoot, Rtoe);

		}

		if (Cfg->HeadCircle)
		{
			if (HeadPos != D3DXVECTOR3(0, 0, 0))
			{
				D3DXVECTOR2 head_screen_pos = SDK::Game::WorldToScreen(HeadPos);
				if (head_screen_pos != D3DXVECTOR2(0, 0))
				{
					int CircleValue = static_cast<int>((Height / 15) * visualScale);
					if (CircleValue <= 10)
						CircleValue = static_cast<int>((Height / 20) * visualScale);

					ImVec2 center(head_screen_pos.x, head_screen_pos.y);
					const ImColor headCircleColor = (IsFriend || Cfg->HighlightVisible) ? highlightColor : Cfg->InVisibleCol;
					ImU32 col = headCircleColor;
					ImU32 black = IM_COL32(0, 0, 0, 255);

					float r = (float)CircleValue;
					float th = 2.0f;


					ImVec2 cranium_center(center.x, center.y - r * 0.28f);
					float cranium_r = r * 0.88f;
					DrawList->AddCircle(cranium_center, cranium_r, col, 64, th);

					DrawList->AddLine(
						ImVec2(cranium_center.x, cranium_center.y - cranium_r * 0.72f),
						ImVec2(cranium_center.x, cranium_center.y + cranium_r * 0.12f),
						col, th * 0.85f);

					float eye_x = r * 0.37f;
					float eye_y = r * 0.20f;
					float eye_r = r * 0.245f;

					ImVec2 left_eye(center.x - eye_x, center.y - eye_y);
					ImVec2 right_eye(center.x + eye_x, center.y - eye_y);

					DrawList->AddCircleFilled(left_eye, eye_r, black);
					DrawList->AddCircleFilled(right_eye, eye_r, black);
					DrawList->AddCircle(left_eye, eye_r, col, 32, th);
					DrawList->AddCircle(right_eye, eye_r, col, 32, th);

					ImVec2 nose_top(center.x, center.y + r * 0.08f);
					ImVec2 nose_left(center.x - r * 0.19f, center.y + r * 0.37f);
					ImVec2 nose_right(center.x + r * 0.19f, center.y + r * 0.37f);

					DrawList->AddTriangleFilled(nose_top, nose_left, nose_right, black);
					DrawList->AddTriangle(nose_top, nose_left, nose_right, col, th);

					float mouth_top = center.y + r * 0.41f;
					float mouth_bottom = center.y + r * 0.58f;
					float mouth_w = r * 0.58f;

					ImVec2 mouth_min(center.x - mouth_w * 0.5f, mouth_top);
					ImVec2 mouth_max(center.x + mouth_w * 0.5f, mouth_bottom);

					DrawList->AddRectFilled(mouth_min, mouth_max, black);

					int teeth = 7;
					float tooth_w = mouth_w / (teeth + 1.0f);
					float tooth_h = r * 0.135f;
					for (int i = 0; i < teeth; i++)
					{
						float tx = center.x - mouth_w * 0.5f + tooth_w * (i + 0.5f);
						DrawList->AddLine(
							ImVec2(tx, mouth_top),
							ImVec2(tx, mouth_top + tooth_h),
							col, th * 0.95f);
					}

					float jaw_y = center.y + r * 0.70f;
					float jaw_w_half = r * 0.78f;

					ImVec2 jaw_left(center.x - mouth_w * 0.48f, mouth_top);
					ImVec2 jaw_right(center.x + mouth_w * 0.48f, mouth_top);
					ImVec2 jaw_tip_l(center.x - jaw_w_half * 0.48f, jaw_y);
					ImVec2 jaw_tip_r(center.x + jaw_w_half * 0.48f, jaw_y);

					ImVec2 cheek_l_start(left_eye.x + eye_r * 0.7f, left_eye.y + eye_r * 0.5f);
					ImVec2 cheek_l_end(jaw_tip_l.x + r * 0.08f, jaw_y - r * 0.08f);
					DrawList->AddBezierCubic(cheek_l_start,
						ImVec2(cheek_l_start.x - r * 0.1f, cheek_l_start.y + r * 0.15f),
						ImVec2(cheek_l_end.x - r * 0.15f, cheek_l_end.y - r * 0.1f),
						cheek_l_end, col, th * 0.9f, 16);

					ImVec2 cheek_r_start(right_eye.x - eye_r * 0.7f, right_eye.y + eye_r * 0.5f);
					ImVec2 cheek_r_end(jaw_tip_r.x - r * 0.08f, jaw_y - r * 0.08f);
					DrawList->AddBezierCubic(cheek_r_start,
						ImVec2(cheek_r_start.x + r * 0.1f, cheek_r_start.y + r * 0.15f),
						ImVec2(cheek_r_end.x + r * 0.15f, cheek_r_end.y - r * 0.1f),
						cheek_r_end, col, th * 0.9f, 16);

					DrawList->AddBezierCubic(jaw_left,
						ImVec2(jaw_left.x - r * 0.12f, jaw_y - r * 0.05f),
						ImVec2(jaw_right.x + r * 0.12f, jaw_y - r * 0.05f),
						jaw_right, col, th, 20);

					DrawList->AddBezierCubic(jaw_tip_l,
						ImVec2(jaw_tip_l.x + r * 0.12f, jaw_y + r * 0.06f),
						ImVec2(jaw_tip_r.x - r * 0.12f, jaw_y + r * 0.06f),
						jaw_tip_r, col, th, 16);
				}
			}
		}

		if (Cfg->DistanceFromMe)
		{

			std::string DistStr = std::to_string((int)Distance) + xorstr("m");

			ImVec2 TextSize = textFont->CalcTextSizeA(textFontSize, FLT_MAX, 0.0f, DistStr.c_str());
			ImVec2 TextPos = ImVec2(BoxCenter.x - (TextSize.x / 2), FirstTextBoxBottom);

			switch (Cfg->DistanceFromMeState)
			{

			case 0: //Top

				if (Cfg->UserNames && Cfg->UserNamesState == 0)
				{
					TextPos.y = FirstTextBoxTop - stackedTextOffset;
				}

				if (showWeaponNameLabel && Cfg->WeaponNameState == 0)
				{
					TextPos.y = FirstTextBoxTop - stackedTextOffset;
				}

				if (showWeaponNameLabel && Cfg->WeaponNameState == 0 && Cfg->UserNames && Cfg->UserNamesState == 0)
				{
					TextPos.y = FirstTextBoxTop - (stackedTextOffset * 2.0f);
				}

				if (showWeaponNameLabel && Cfg->WeaponNameState == 2 && Cfg->UserNames && Cfg->UserNamesState == 2)
				{
					TextPos.y = FirstTextBoxTop;
				}


				if (!Cfg->UserNames && !showWeaponNameLabel)
				{
					TextPos.y = FirstTextBoxTop;
				}

				break;
			case 2: //Bottom
				if (Cfg->UserNames && Cfg->UserNamesState == 2)
				{
					TextPos.y = FirstTextBoxBottom + stackedTextOffset;
				}

				if (showWeaponNameLabel && Cfg->WeaponNameState == 2)
				{
					TextPos.y = FirstTextBoxBottom + stackedTextOffset;
				}

				if (showWeaponNameLabel && Cfg->WeaponNameState == 2 && Cfg->UserNames && Cfg->UserNamesState == 2)
				{
					TextPos.y = FirstTextBoxBottom + (stackedTextOffset * 2.0f);
				}

				if (!Cfg->UserNames && !showWeaponNameLabel)
				{
					TextPos.y = FirstTextBoxBottom;
				}

				break;
			default:
				TextPos.y = FirstTextBoxBottom;
				break;
			}

			/*DrawList->AddText( g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2( TextPos.x + 1, TextPos.y + 1 ), ImColor( 0, 0, 0, 100 ), DistStr.c_str( ) );
			DrawList->AddText( g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2( TextPos.x - 1, TextPos.y + 1 ), ImColor( 0, 0, 0, 100 ), DistStr.c_str( ) );
			DrawList->AddText( g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2( TextPos.x + 1, TextPos.y - 1 ), ImColor( 0, 0, 0, 100 ), DistStr.c_str( ) );
			DrawList->AddText( g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2( TextPos.x - 1, TextPos.y - 1 ), ImColor( 0, 0, 0, 100 ), DistStr.c_str( ) );*/

			DrawList->AddText(textFont, textFontSize, ImVec2(TextPos.x + 1, TextPos.y + 1), ImColor(0.f, 0.f, 0.f, Cfg->DistanceCol.Value.w), DistStr.c_str());
			DrawList->AddText(textFont, textFontSize, TextPos, Cfg->DistanceCol, DistStr.c_str());
		}

		if (showWeaponNameLabel) {

			std::string weaponName = Entity.WeaponName;

			auto to_lower = [](const char* str) {
				std::string message = str;
				if (!message.empty()) {
					std::transform(message.begin(), message.end(), message.begin(), ::tolower);
					message[0] = std::toupper(message[0]);
				}
				return message;
				};


			ImVec2 TextSize = textFont->CalcTextSizeA(textFontSize, FLT_MAX, 0.0f, to_lower(weaponName.c_str()).c_str());
			ImVec2 TextPos = ImVec2(BoxCenter.x - (TextSize.x / 2), FirstTextBoxBottom);

			if (Cfg->DistanceFromMe)
				TextPos.y = FirstTextBoxBottom;

			if (Cfg->UserNamesState == 1)
				TextPos.y = SecondTextBoxBottom;

			switch (Cfg->WeaponNameState)
			{

			case 0: //Top
				if (Cfg->UserNamesState == 0 && Cfg->UserNames)
				{
					TextPos.y = FirstTextBoxTop - stackedTextOffset;
				}
				else
				{
					TextPos.y = FirstTextBoxTop;
				}

				break;
			case 2: //Bottom
				if (Cfg->UserNamesState == 2 && Cfg->UserNames)
				{
					TextPos.y = FirstTextBoxBottom + stackedTextOffset;
				}
				else
				{
					TextPos.y = FirstTextBoxBottom;
				}
				break;
			default:
				TextPos.y = FirstTextBoxBottom;
				break;
			}

			std::string WeaponNameUnder = to_lower(weaponName.c_str());

			/*DrawList->AddText( g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2( TextPos.x + 1, TextPos.y + 1 ), ImColor( 0, 0, 0, 100 ), WeaponNameUnder.c_str( ) );
			DrawList->AddText( g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2( TextPos.x - 1, TextPos.y + 1 ), ImColor( 0, 0, 0, 100 ), WeaponNameUnder.c_str( ) );
			DrawList->AddText( g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2( TextPos.x + 1, TextPos.y - 1 ), ImColor( 0, 0, 0, 100 ), WeaponNameUnder.c_str( ) );
			DrawList->AddText( g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2( TextPos.x - 1, TextPos.y - 1 ), ImColor( 0, 0, 0, 100 ), WeaponNameUnder.c_str( ) );*/

			DrawList->AddText(textFont, textFontSize, ImVec2(TextPos.x + 1, TextPos.y + 1), ImColor(0.f, 0.f, 0.f, Cfg->WeaponNameCol.Value.w), WeaponNameUnder.c_str());
			DrawList->AddText(textFont, textFontSize, TextPos, Cfg->WeaponNameCol, WeaponNameUnder.c_str());
		}

		if (Cfg->UserNames)
		{

			std::string PlayerName = Utils::ValidateTextForRendering(Entity.NetworkInfo.UserName);

			// Unresolved player names used to render nothing useful ("NPC" / empty).
			// Fall back to a stable label so every player always gets a visible tag.
			if (Entity.IsPlayer &&
				(PlayerName.empty() || PlayerName == "NPC" || PlayerName == "npc" || PlayerName == "Unknown"))
			{
				const int networkId = Ped ? Ped->GetID() : 0;
				PlayerName = networkId > 0
					? std::string("Player ") + std::to_string(networkId)
					: std::string("Player");
			}

			ImVec2 TextSize = nameFont->CalcTextSizeA(nameFontSize, FLT_MAX, 0.0f, PlayerName.c_str());
			ImVec2 TextPos = ImVec2(BoxCenter.x - (TextSize.x / 2), FirstTextBoxTop);

			switch (Cfg->UserNamesState)
			{

			case 0: //Top
				TextPos.y = FirstTextBoxTop;
				break;
			case 2: //Bottom
				TextPos.y = FirstTextBoxBottom;
				break;
			default:
				TextPos.y = FirstTextBoxTop;
				break;
			}

			if (Cfg->NameBackdrop && !PlayerName.empty())
			{
				const float padX = 4.0f * textScale;
				const float padY = 2.0f * textScale;
				DrawList->AddRectFilled(
					ImVec2(TextPos.x - padX, TextPos.y - padY),
					ImVec2(TextPos.x + TextSize.x + padX, TextPos.y + TextSize.y + padY),
					ImColor(0, 0, 0, 130),
					3.0f
				);
			}

			/*DrawList->AddText( g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2( TextPos.x + 1, TextPos.y + 1 ), ImColor( 0, 0, 0, 100 ), PlayerName.c_str( ) );
			DrawList->AddText( g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2( TextPos.x - 1, TextPos.y + 1 ), ImColor( 0, 0, 0, 100 ), PlayerName.c_str( ) );
			DrawList->AddText( g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2( TextPos.x + 1, TextPos.y - 1 ), ImColor( 0, 0, 0, 100 ), PlayerName.c_str( ) );
			DrawList->AddText( g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2( TextPos.x - 1, TextPos.y - 1 ), ImColor( 0, 0, 0, 100 ), PlayerName.c_str( ) );*/

			DrawList->AddText(nameFont, nameFontSize, ImVec2(TextPos.x + 1, TextPos.y + 1), ImColor(0.f, 0.f, 0.f, Cfg->UserNamesCol.Value.w), PlayerName.c_str());
			DrawList->AddText(nameFont, nameFontSize, ImVec2(TextPos.x - 1, TextPos.y + 1), ImColor(0.f, 0.f, 0.f, Cfg->UserNamesCol.Value.w * 0.6f), PlayerName.c_str());
			DrawList->AddText(nameFont, nameFontSize, TextPos, Cfg->UserNamesCol, PlayerName.c_str());
		}

		const auto DrawHealthBarV = [&DrawList](ImVec2 pos, ImVec2 dim, ImColor col, int background) {
			if (background == 1) { //Bar
				DrawList->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + dim.x, pos.y - (dim.y + 1)), col);
			}
			else { //BackGround
				DrawList->AddRectFilled(ImVec2(pos.x - 1, pos.y + 1), ImVec2(pos.x + dim.x + 1, pos.y - (dim.y + 2)), ImColor(0, 0, 0, 255));
				DrawList->AddRectFilled(ImVec2(pos.x, pos.y - 1), ImVec2(pos.x + dim.x, pos.y - (dim.y + 2)), ImColor(80, 80, 80, 125));
			}
			};
		const auto DrawHealthBarH = [&DrawList](ImVec2 pos, ImVec2 dim, ImColor col, int background) {
			if (background == 1) { //Bar
				DrawList->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + dim.y, pos.y + dim.x), col);
			}
			else { //BackGround
				DrawList->AddRectFilled(ImVec2(pos.x - 1, pos.y - 1), ImVec2(pos.x + dim.y + 1, pos.y + dim.x + 1), ImColor(0, 0, 0, 255));
				DrawList->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + dim.y, pos.y + dim.x), ImColor(80, 80, 80, 125));
			}
			};
		const auto EaseGradient = [](float t) {
			t = ImClamp(t, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
			};
		const auto LerpGradientColor = [](const ImVec4& start, const ImVec4& end, float t) {
			return ImVec4(
				ImLerp(start.x, end.x, t),
				ImLerp(start.y, end.y, t),
				ImLerp(start.z, end.z, t),
				ImLerp(start.w, end.w, t)
			);
			};
		const auto DrawGradientBarV = [&DrawList, &EaseGradient, &LerpGradientColor](ImVec2 pos, ImVec2 dim, ImColor startCol, ImColor endCol) {
			const ImVec2 min(pos.x, pos.y - (dim.y + 1));
			const ImVec2 max(pos.x + dim.x, pos.y);
			const float height = max.y - min.y;
			const int segments = static_cast<int>(ImClamp(height / 3.0f, 14.0f, 40.0f));

			for (int i = 0; i < segments; ++i) {
				const float t0 = static_cast<float>(i) / segments;
				const float t1 = static_cast<float>(i + 1) / segments;
				const float eased0 = EaseGradient(t0);
				const float eased1 = EaseGradient(t1);
				const float y0 = ImLerp(max.y, min.y, t0);
				const float y1 = ImLerp(max.y, min.y, t1);

				const ImU32 col0 = ImGui::ColorConvertFloat4ToU32(LerpGradientColor(startCol.Value, endCol.Value, eased0));
				const ImU32 col1 = ImGui::ColorConvertFloat4ToU32(LerpGradientColor(startCol.Value, endCol.Value, eased1));

				DrawList->AddRectFilledMultiColor(ImVec2(min.x, y1), ImVec2(max.x, y0), col1, col1, col0, col0);
			}

			DrawList->AddRectFilledMultiColor(
				min,
				max,
				IM_COL32(255, 255, 255, 18),
				IM_COL32(255, 255, 255, 10),
				IM_COL32(255, 255, 255, 0),
				IM_COL32(255, 255, 255, 0)
			);
			};
		const auto DrawGradientBarH = [&DrawList, &EaseGradient, &LerpGradientColor](ImVec2 pos, ImVec2 dim, ImColor startCol, ImColor endCol) {
			const ImVec2 min(pos.x, pos.y);
			const ImVec2 max(pos.x + dim.y, pos.y + dim.x);
			const float width = max.x - min.x;
			const int segments = static_cast<int>(ImClamp(width / 3.0f, 14.0f, 40.0f));

			for (int i = 0; i < segments; ++i) {
				const float t0 = static_cast<float>(i) / segments;
				const float t1 = static_cast<float>(i + 1) / segments;
				const float eased0 = EaseGradient(t0);
				const float eased1 = EaseGradient(t1);
				const float x0 = ImLerp(min.x, max.x, t0);
				const float x1 = ImLerp(min.x, max.x, t1);

				const ImU32 col0 = ImGui::ColorConvertFloat4ToU32(LerpGradientColor(startCol.Value, endCol.Value, eased0));
				const ImU32 col1 = ImGui::ColorConvertFloat4ToU32(LerpGradientColor(startCol.Value, endCol.Value, eased1));

				DrawList->AddRectFilledMultiColor(ImVec2(x0, min.y), ImVec2(x1, max.y), col0, col1, col1, col0);
			}

			DrawList->AddRectFilledMultiColor(
				min,
				max,
				IM_COL32(255, 255, 255, 18),
				IM_COL32(255, 255, 255, 18),
				IM_COL32(255, 255, 255, 0),
				IM_COL32(255, 255, 255, 0)
			);
			};

		if (Cfg->HealthBar)
		{
			float MaxHealth = Entity.MaxHealth;

			CurrentESPAnim.Health = ImLerp(CurrentESPAnim.Health, Health, ImGui::GetIO().DeltaTime * 4);
			float AnimHealth = CurrentESPAnim.Health;

			float FullHealthBar = Height / 100 * 100;
			float DecreaseHealthBar = FullHealthBar * (AnimHealth / MaxHealth);

			float Width2 = BoxRight - BoxLeft;
			float FullHealthBarH = Width2 / 100 * 100;
			float DecreaseHealthBarH = FullHealthBarH * (AnimHealth / MaxHealth);

			if (DecreaseHealthBarH > FullHealthBarH)
			{
				DecreaseHealthBarH = FullHealthBarH;
			}

			if (DecreaseHealthBar > FullHealthBar)
			{
				DecreaseHealthBar = FullHealthBar;
			}

			ImColor BarColor;
			ImColor FullHealth = (ImVec4(ImColor(80, 80, 80, 200)));

			if (Health > (MaxHealth / 2))
			{
				BarColor = (ImVec4(ImColor(66, 245, 132, 255)));
			}
			else if (Health <= (MaxHealth / 2) && MaxHealth == 200 ? Health > 50 : Health > 150)
			{
				BarColor = (ImVec4(ImColor(245, 135, 66, 255)));
			}
			else
			{
				BarColor = (ImVec4(ImColor(245, 66, 66, 255)));
			}

			switch (Cfg->HealthBarState)
			{
			case 0: //Top
				DrawHealthBarH(ImVec2(BoxMin.x, BoxMin.y - textBottomOffset), ImVec2(barThickness, FullHealthBarH), (ImVec4(FullHealth)), 0);
				if (Cfg->HealthBarGradient)
					DrawGradientBarH(ImVec2(BoxMin.x, BoxMin.y - textBottomOffset), ImVec2(barThickness, DecreaseHealthBarH), Cfg->HealthBarGradientStart, Cfg->HealthBarGradientEnd);
				else
					DrawHealthBarH(ImVec2(BoxMin.x, BoxMin.y - textBottomOffset), ImVec2(barThickness, DecreaseHealthBarH), (ImVec4(BarColor)), 1);
				break;
			case 1: //Right
				DrawHealthBarV(ImVec2(BoxMax.x + barOffset, BoxMax.y), ImVec2(barThickness, FullHealthBar), (ImVec4(FullHealth)), 0);
				if (Cfg->HealthBarGradient)
					DrawGradientBarV(ImVec2(BoxMax.x + barOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), Cfg->HealthBarGradientStart, Cfg->HealthBarGradientEnd);
				else
					DrawHealthBarV(ImVec2(BoxMax.x + barOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), (ImVec4(BarColor)), 1);
				break;
			case 2: //Bottom
				DrawHealthBarH(ImVec2(BoxMin.x, BoxMax.y + barOffset), ImVec2(barThickness, FullHealthBarH), (ImVec4(FullHealth)), 0);
				if (Cfg->HealthBarGradient)
					DrawGradientBarH(ImVec2(BoxMin.x, BoxMax.y + barOffset), ImVec2(barThickness, DecreaseHealthBarH), Cfg->HealthBarGradientStart, Cfg->HealthBarGradientEnd);
				else
					DrawHealthBarH(ImVec2(BoxMin.x, BoxMax.y + barOffset), ImVec2(barThickness, DecreaseHealthBarH), (ImVec4(BarColor)), 1);
				break;
			case 3: //Left
				DrawHealthBarV(ImVec2(BoxMin.x - barOffset, BoxMax.y), ImVec2(barThickness, FullHealthBar), (ImVec4(FullHealth)), 0);
				if (Cfg->HealthBarGradient)
					DrawGradientBarV(ImVec2(BoxMin.x - barOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), Cfg->HealthBarGradientStart, Cfg->HealthBarGradientEnd);
				else
					DrawHealthBarV(ImVec2(BoxMin.x - barOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), (ImVec4(BarColor)), 1);
				break;

			default:
				DrawHealthBarV(ImVec2(BoxMin.x - barOffset, BoxMax.y), ImVec2(barThickness, FullHealthBar), (ImVec4(FullHealth)), 0);
				if (Cfg->HealthBarGradient)
					DrawGradientBarV(ImVec2(BoxMin.x - barOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), Cfg->HealthBarGradientStart, Cfg->HealthBarGradientEnd);
				else
					DrawHealthBarV(ImVec2(BoxMin.x - barOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), (ImVec4(BarColor)), 1);
				break;
			}
		}
		if (Cfg->OffScreenEsp)
		{
			ImVec2 screen = ImGui::GetIO().DisplaySize;
			ImVec2 center(screen.x * 0.5f, screen.y * 0.5f);

			D3DXVECTOR3 localPos = Core::SDK::Pointers::pLocalPlayer->GetPos();

			D3DXVECTOR3 enemyPos = Ped->GetPos();

			float dx = enemyPos.x - localPos.x;
			float dy = enemyPos.y - localPos.y;

			float angle = atan2f(dy, dx);
			float radius = 140.0f;
			ImVec2 arrowPos(
				center.x + cosf(angle) * radius,
				center.y + sinf(angle) * radius
			);
			float arrowSize = 14.0f;
			ImVec2 tip(
				arrowPos.x + cosf(angle) * arrowSize,
				arrowPos.y + sinf(angle) * arrowSize
			);

			ImVec2 left(
				arrowPos.x + cosf(angle + 2.5f) * arrowSize,
				arrowPos.y + sinf(angle + 2.5f) * arrowSize
			);

			ImVec2 right(
				arrowPos.x + cosf(angle - 2.5f) * arrowSize,
				arrowPos.y + sinf(angle - 2.5f) * arrowSize
			);
			D3DXVECTOR2 screenPos = Core::SDK::Game::WorldToScreen(enemyPos);

			if (!Core::SDK::Game::IsOnScreen(screenPos))
			{
				ImColor arrowColor = boxColor;

				DrawList->AddTriangleFilled(
					ImVec2(tip.x + 1, tip.y + 1),
					ImVec2(left.x + 1, left.y + 1),
					ImVec2(right.x + 1, right.y + 1),
					IM_COL32(0, 0, 0, 255)
				);

				DrawList->AddTriangleFilled(
					tip,
					left,
					right,
					arrowColor
				);
				DrawList->AddCircle(
					center,
					radius,
					IM_COL32(255, 255, 255, 40),
					100,
					1.0f
				);
			}
		}
		if (Cfg->ArmorBar)
		{
			float MaxHealth = Entity.MaxHealth;
			float Armor = Entity.Armor;

			if (Armor > 0)
			{
				MaxHealth = MaxHealth / 2;

				CurrentESPAnim.Armor = ImLerp(CurrentESPAnim.Armor, Armor, ImGui::GetIO().DeltaTime * 4);
				float AnimArmor = CurrentESPAnim.Armor;

				float FullArmorBar = Height / 100 * 100;
				float DecreaseHealthBar = FullArmorBar * (AnimArmor / MaxHealth);

				float Width2 = BoxRight - BoxLeft;
				float FullArmorBarH = Width2 / 100 * 100;
				float DecreaseArmorBarH = FullArmorBarH * (AnimArmor / MaxHealth);


				if (DecreaseHealthBar > FullArmorBar)
				{
					DecreaseHealthBar = FullArmorBar;
				}

				if (DecreaseArmorBarH > FullArmorBarH)
				{
					DecreaseArmorBarH = FullArmorBarH;
				}

				ImColor BarColor;
				ImColor FullArmor = ImVec4(ImColor(80, 80, 80, 200));

				if (Armor > (MaxHealth / 2)) {
					BarColor = ImVec4(ImColor(85, 128, 200, 255));
				}
				else if (Armor <= (MaxHealth / 2) && MaxHealth == 200 ? Armor > 50 : Armor > 150) {
					BarColor = ImVec4(ImColor(69, 102, 157, 255));
				}
				else {
					BarColor = ImVec4(ImColor(45, 62, 92, 255));
				}

				switch (Cfg->ArmorBarState)
				{

				case 0: //Top
					if (Cfg->HealthBar && Cfg->HealthBarState == Cfg->ArmorBarState)
					{
						DrawHealthBarH(ImVec2(BoxMin.x, BoxMin.y - barStackOffset), ImVec2(barThickness, FullArmorBarH), FullArmor, 0);
						if (Cfg->ArmorBarGradient)
							DrawGradientBarH(ImVec2(BoxMin.x, BoxMin.y - barStackOffset), ImVec2(barThickness, DecreaseArmorBarH), Cfg->ArmorBarGradientStart, Cfg->ArmorBarGradientEnd);
						else
							DrawHealthBarH(ImVec2(BoxMin.x, BoxMin.y - barStackOffset), ImVec2(barThickness, DecreaseArmorBarH), BarColor, 1);
					}
					else
					{
						DrawHealthBarH(ImVec2(BoxMin.x, BoxMin.y - textBottomOffset), ImVec2(barThickness, FullArmorBarH), FullArmor, 0);
						if (Cfg->ArmorBarGradient)
							DrawGradientBarH(ImVec2(BoxMin.x, BoxMin.y - textBottomOffset), ImVec2(barThickness, DecreaseArmorBarH), Cfg->ArmorBarGradientStart, Cfg->ArmorBarGradientEnd);
						else
							DrawHealthBarH(ImVec2(BoxMin.x, BoxMin.y - textBottomOffset), ImVec2(barThickness, DecreaseArmorBarH), BarColor, 1);
					}
					break;
				case 1: //Right
					if (Cfg->HealthBar && Cfg->HealthBarState == Cfg->ArmorBarState)
					{
						DrawHealthBarV(ImVec2(BoxMax.x + barStackOffset, BoxMax.y), ImVec2(barThickness, FullArmorBar), FullArmor, 0);
						if (Cfg->ArmorBarGradient)
							DrawGradientBarV(ImVec2(BoxMax.x + barStackOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), Cfg->ArmorBarGradientStart, Cfg->ArmorBarGradientEnd);
						else
							DrawHealthBarV(ImVec2(BoxMax.x + barStackOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), BarColor, 1);
					}
					else
					{
						DrawHealthBarV(ImVec2(BoxMax.x + barOffset, BoxMax.y), ImVec2(barThickness, FullArmorBar), FullArmor, 0);
						if (Cfg->ArmorBarGradient)
							DrawGradientBarV(ImVec2(BoxMax.x + barOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), Cfg->ArmorBarGradientStart, Cfg->ArmorBarGradientEnd);
						else
							DrawHealthBarV(ImVec2(BoxMax.x + barOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), BarColor, 1);
					}
					break;
				case 2: //Bottom
					if (Cfg->HealthBar && Cfg->HealthBarState == Cfg->ArmorBarState)
					{
						DrawHealthBarH(ImVec2(BoxMin.x, BoxMax.y + barStackOffset), ImVec2(barThickness, FullArmorBarH), FullArmor, 0);
						if (Cfg->ArmorBarGradient)
							DrawGradientBarH(ImVec2(BoxMin.x, BoxMax.y + barStackOffset), ImVec2(barThickness, DecreaseArmorBarH), Cfg->ArmorBarGradientStart, Cfg->ArmorBarGradientEnd);
						else
							DrawHealthBarH(ImVec2(BoxMin.x, BoxMax.y + barStackOffset), ImVec2(barThickness, DecreaseArmorBarH), BarColor, 1);
					}
					else
					{
						DrawHealthBarH(ImVec2(BoxMin.x, BoxMax.y + barOffset), ImVec2(barThickness, FullArmorBarH), FullArmor, 0);
						if (Cfg->ArmorBarGradient)
							DrawGradientBarH(ImVec2(BoxMin.x, BoxMax.y + barOffset), ImVec2(barThickness, DecreaseArmorBarH), Cfg->ArmorBarGradientStart, Cfg->ArmorBarGradientEnd);
						else
							DrawHealthBarH(ImVec2(BoxMin.x, BoxMax.y + barOffset), ImVec2(barThickness, DecreaseArmorBarH), BarColor, 1);
					}
					break;
				case 3: //Left
					if (Cfg->HealthBar && Cfg->HealthBarState == Cfg->ArmorBarState)
					{
						DrawHealthBarV(ImVec2(BoxMin.x - barStackOffset, BoxMax.y), ImVec2(barThickness, FullArmorBar), FullArmor, 0);
						if (Cfg->ArmorBarGradient)
							DrawGradientBarV(ImVec2(BoxMin.x - barStackOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), Cfg->ArmorBarGradientStart, Cfg->ArmorBarGradientEnd);
						else
							DrawHealthBarV(ImVec2(BoxMin.x - barStackOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), BarColor, 1);
					}
					else
					{
						DrawHealthBarV(ImVec2(BoxMin.x - barOffset, BoxMax.y), ImVec2(barThickness, FullArmorBar), FullArmor, 0);
						if (Cfg->ArmorBarGradient)
							DrawGradientBarV(ImVec2(BoxMin.x - barOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), Cfg->ArmorBarGradientStart, Cfg->ArmorBarGradientEnd);
						else
							DrawHealthBarV(ImVec2(BoxMin.x - barOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), BarColor, 1);
					}
					break;

				default:
					DrawHealthBarV(ImVec2(BoxMin.x - barOffset, BoxMax.y), ImVec2(barThickness, FullArmorBar), FullArmor, 0);
					if (Cfg->ArmorBarGradient)
						DrawGradientBarV(ImVec2(BoxMin.x - barOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), Cfg->ArmorBarGradientStart, Cfg->ArmorBarGradientEnd);
					else
						DrawHealthBarV(ImVec2(BoxMin.x - barOffset, BoxMax.y), ImVec2(barThickness, DecreaseHealthBar), BarColor, 1);
					break;
				}

			}

		}
	}
}

void Core::Features::cEsp::DrawVehicle()
{
	auto Cfg = g_Config.VehicleESP;

	if (!Cfg->Enabled)
		return;

	for (auto Entity : Core::SDK::Game::VehicleList)
	{

		CVehicle* Vehicle = Entity.Pointer;
		auto DrawList = ImGui::GetBackgroundDrawList();

		float Distance = Entity.Dist;

		if (Distance > Cfg->MaxDistance)
			continue;

		D3DXVECTOR2 VehicleLocation = Core::SDK::Game::WorldToScreen(
			Vehicle->GetPos()
		);

		if (!VehicleLocation)
			continue;

		if (!SDK::Game::IsOnScreen(VehicleLocation))
			continue;

		if (Cfg->SnapLines)
		{
			DrawList->AddLine(ImVec2(g_Variables.g_vGameWindowCenter.x, g_Variables.g_vGameWindowSize.y), ImVec2(VehicleLocation.x, VehicleLocation.y + 30), ImColor(0, 0, 0), 2.5);
			DrawList->AddLine(ImVec2(g_Variables.g_vGameWindowCenter.x, g_Variables.g_vGameWindowSize.y), ImVec2(VehicleLocation.x, VehicleLocation.y + 30), Cfg->SnapLinesCol, 1.5);
		}

		if (Cfg->VehName)
		{
			ImVec2 text_size = g_Variables.m_DrawFont->CalcTextSizeA(g_Variables.m_DrawFont->FontSize, FLT_MAX, 0.0f, Entity.Name.c_str());
			DrawOutlinedText(DrawList, g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2(VehicleLocation.x - (text_size.x / 2), VehicleLocation.y), ImColor(255, 255, 255), Entity.Name.c_str());
		}

		if (Cfg->ShowLockUnlock) {
			bool IsLocked = Entity.IsLocked;
			float radius = 2 / Distance;

			const static std::string VehicleLocked = xorstr("Press L to Unlock");
			const static std::string VehicleUnlocked = xorstr("Press L to Lock");
			const static int fovRadius = 40;
			const static int L = 0x4C;
			const static int Delay = 500;


			int FovSize = std::hypot(VehicleLocation.x - g_Variables.g_vGameWindowCenter.x, VehicleLocation.y - g_Variables.g_vGameWindowCenter.y);

			if (IsLocked)
			{
				DrawList->AddCircleFilled(ImVec2(VehicleLocation.x, VehicleLocation.y - 6), radius + Distance < 1 ? 1 : 4, ImColor(0, 0, 0), 24);
				DrawList->AddCircleFilled(ImVec2(VehicleLocation.x, VehicleLocation.y - 6), radius + Distance < 1 ? 0 : 3, ImColor(250, 72, 62, 255), 24);

				if (FovSize < fovRadius)
				{
					ImVec2 text_size = g_Variables.m_DrawFont->CalcTextSizeA(g_Variables.m_DrawFont->FontSize, FLT_MAX, 0.0f, VehicleLocked.c_str());
					DrawOutlinedText(DrawList, g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2(VehicleLocation.x - (text_size.x / 2), VehicleLocation.y - 30), ImColor(255, 255, 255), VehicleLocked.c_str());

					if (Utils::KeyPressedWithDelay(L, Delay))
						Vehicle->DoorState(true);
				}



			}
			else if (!IsLocked)
			{
				DrawList->AddCircleFilled(ImVec2(VehicleLocation.x, VehicleLocation.y - 6), radius + Distance < 1 ? 1 : 4, ImColor(0, 0, 0), 24);
				DrawList->AddCircleFilled(ImVec2(VehicleLocation.x, VehicleLocation.y - 6), radius + Distance < 1 ? 0 : 3, ImColor(102, 255, 133, 255), 24);

				if (FovSize < fovRadius)
				{
					ImVec2 text_size = g_Variables.m_DrawFont->CalcTextSizeA(g_Variables.m_DrawFont->FontSize, FLT_MAX, 0.0f, VehicleUnlocked.c_str());
					DrawOutlinedText(DrawList, g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2(VehicleLocation.x - (text_size.x / 2), VehicleLocation.y - 30), ImColor(255, 255, 255), VehicleUnlocked.c_str());

					if (Utils::KeyPressedWithDelay(L, Delay))
						Vehicle->DoorState(false);
				}
			}
		}

		if (Cfg->DistanceFromMe)
		{
			std::string vehicleDistance = std::to_string(int(Distance)) + xorstr("m");
			ImVec2 text_size = g_Variables.m_DrawFont->CalcTextSizeA(g_Variables.m_DrawFont->FontSize, FLT_MAX, 0.0f, vehicleDistance.c_str());
			DrawOutlinedText(DrawList, g_Variables.m_DrawFont, g_Variables.m_DrawFont->FontSize, ImVec2(VehicleLocation.x - (text_size.x / 2), VehicleLocation.y + 14), ImColor(255, 255, 255), vehicleDistance.c_str());
		}

	}
}
