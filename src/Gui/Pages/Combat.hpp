#pragma once
#include <Includes/Includes.hpp>
#include <windows.h>
#include <iostream>
#include <thread>

using namespace std;
using namespace Core;
namespace Combat {

	static int iSubTabCount = 0;
	static float SubTabAlpha = 0.f;
	static int iSubTab = 0;

	void Render() {


		//ImGui::PushStyleVar( ImGuiStyleVar_::ImGuiStyleVar_ItemSpacing, ImVec2( 6, 0 ) );
		ImGui::SetCursorPosX(675 - 50);
		if (Custom::SubTab(xorstr("Aimbot"), 0 == iSubTabCount)) {
			iSubTabCount = 0;
		}
		ImGui::SameLine();
		if (Custom::SubTab(xorstr("SilentAim"), 1 == iSubTabCount)) {
			iSubTabCount = 1;
		}
		ImGui::SameLine();
		if (Custom::SubTab(xorstr("TriggerBot"), 2 == iSubTabCount)) {
			iSubTabCount = 2;
		}

		SubTabAlpha = ImClamp(SubTabAlpha + (5.f * ImGui::GetIO().DeltaTime * (iSubTabCount == iSubTab ? 1.f : -1.f)), 0.f, 1.f);

		if (SubTabAlpha == 0.f)
			iSubTab = iSubTabCount;

		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, SubTabAlpha * ImGui::GetStyle().Alpha);

		ImGui::SetCursorPos(ImVec2(226, 76));
		ImGui::BeginGroup();
		{
			switch (iSubTab)
			{
			case 0: //Aimbot
				ImGui::BeginGroup();
				{
					ImGui::BeginChild(xorstr("Aimbot"), ImVec2(340, 400), false, 0);
					{
						static int KeyMode = 1;

						Custom::CheckBox(xorstr("Enable"), &g_Config.Aimbot->Enabled);
						Custom::CheckBox(xorstr("Only Visible"), &g_Config.Aimbot->OnlyVisible);
						Custom::CheckBox(xorstr("Ignore NPCs"), &g_Config.Aimbot->IgnoreNPCs);
						Custom::CheckBox(xorstr("Show Fov"), &g_Config.Aimbot->ShowFov);

						ImGui::Keybind(xorstr("Bind"), &g_Config.Aimbot->KeyBind, &KeyMode);

					}
					ImGui::EndChild();
				}
				ImGui::EndGroup();
				ImGui::SameLine();
				ImGui::BeginGroup();
				{
					ImGui::BeginChild(xorstr("Settings"), ImVec2(340, 400), false, 0);
					{
						ImGui::SliderInt(xorstr("Field of View X"), &g_Config.Aimbot->FOVX, 0, 400, xorstr("%d"), 0);
						ImGui::SliderInt(xorstr("Field of View Y"), &g_Config.Aimbot->FOVY, 0, 400, xorstr("%d"), 0);
						ImGui::SliderInt(xorstr("Smooth"), &g_Config.Aimbot->AimbotSpeed, 0, 100, xorstr("%d"), 0);
						ImGui::SliderInt(xorstr("Max Distance"), &g_Config.Aimbot->MaxDistance, 0, 1000, xorstr("%dm"), 0);
						ImGui::SliderFloat(xorstr("Fov Thickness"), &g_Config.Aimbot->FovThickness, 0.5f, 8.0f, xorstr("%.1fpx"), 0);
						Custom::CheckBox(xorstr("Fill Fov"), &g_Config.Aimbot->FovFilled);
						static float AimbotFovCol[4] = { g_Config.Aimbot->FovColor.Value.x, g_Config.Aimbot->FovColor.Value.y, g_Config.Aimbot->FovColor.Value.z,g_Config.Aimbot->FovColor.Value.w };
						if (ImGui::ColorEdit4(xorstr("Aimbot Fov"), AimbotFovCol, ImGuiColorEditFlags_AlphaBar))
						{
							g_Config.Aimbot->FovColor.Value.x = AimbotFovCol[0];
							g_Config.Aimbot->FovColor.Value.y = AimbotFovCol[1];
							g_Config.Aimbot->FovColor.Value.z = AimbotFovCol[2];
							g_Config.Aimbot->FovColor.Value.w = AimbotFovCol[3];
						}
						static float AimbotFovFillCol[4] = { g_Config.Aimbot->FovFillColor.Value.x, g_Config.Aimbot->FovFillColor.Value.y, g_Config.Aimbot->FovFillColor.Value.z,g_Config.Aimbot->FovFillColor.Value.w };
						if (ImGui::ColorEdit4(xorstr("Aimbot Fov Fill"), AimbotFovFillCol, ImGuiColorEditFlags_AlphaBar))
						{
							g_Config.Aimbot->FovFillColor.Value.x = AimbotFovFillCol[0];
							g_Config.Aimbot->FovFillColor.Value.y = AimbotFovFillCol[1];
							g_Config.Aimbot->FovFillColor.Value.z = AimbotFovFillCol[2];
							g_Config.Aimbot->FovFillColor.Value.w = AimbotFovFillCol[3];
						}
					}
					ImGui::EndChild();
				}
				ImGui::EndGroup();

				break;
			case 1: //Silent
				ImGui::BeginGroup();
				{
					ImGui::BeginChild(xorstr("Silent"), ImVec2(340, 400), false, 0);
					{
						Custom::CheckBox(xorstr("Enable"), &g_Config.SilentAim->Enabled);
						Custom::CheckBox(xorstr("Magic Bullets"), &g_Config.SilentAim->MagicBullets);
						Custom::CheckBox(xorstr("Only Visible"), &g_Config.SilentAim->OnlyVisible);
						Custom::CheckBox(xorstr("Ignore NPCs"), &g_Config.SilentAim->IgnoreNPCs);
						Custom::CheckBox(xorstr("Show Fov"), &g_Config.SilentAim->ShowFov);
						static int KeyMode = 1;

						ImGui::Keybind(xorstr("Bind"), &g_Config.SilentAim->KeyBind, &KeyMode);

					}
					ImGui::EndChild();
				}
				ImGui::EndGroup();
				ImGui::SameLine();
				ImGui::BeginGroup();
				{
					ImGui::BeginChild(xorstr("Settings"), ImVec2(340, 400), false, 0);
					{
						ImGui::SliderInt(xorstr("Field of View X"), &g_Config.SilentAim->FOVX, 0, 400, xorstr("%d"), 0);
						ImGui::SliderInt(xorstr("Field of View Y"), &g_Config.SilentAim->FOVY, 0, 400, xorstr("%d"), 0);
						ImGui::SliderInt(xorstr("Miss Chance"), &g_Config.SilentAim->MissChance, 0, 100, xorstr("%dx"), 0);
						ImGui::SliderInt(xorstr("Max Distance"), &g_Config.SilentAim->MaxDistance, 0, 1000, xorstr("%dm"), 0);
						ImGui::SliderFloat(xorstr("Fov Thickness"), &g_Config.SilentAim->FovThickness, 0.5f, 8.0f, xorstr("%.1fpx"), 0);
						Custom::CheckBox(xorstr("Fill Fov"), &g_Config.SilentAim->FovFilled);
						static float SilentAimFovCol[4] = { g_Config.SilentAim->FovColor.Value.x, g_Config.SilentAim->FovColor.Value.y, g_Config.SilentAim->FovColor.Value.z,g_Config.SilentAim->FovColor.Value.w };
						if (ImGui::ColorEdit4(xorstr("SilentAim Fov"), SilentAimFovCol, ImGuiColorEditFlags_AlphaBar))
						{
							g_Config.SilentAim->FovColor.Value.x = SilentAimFovCol[0];
							g_Config.SilentAim->FovColor.Value.y = SilentAimFovCol[1];
							g_Config.SilentAim->FovColor.Value.z = SilentAimFovCol[2];
							g_Config.SilentAim->FovColor.Value.w = SilentAimFovCol[3];
						}
						static float SilentAimFovFillCol[4] = { g_Config.SilentAim->FovFillColor.Value.x, g_Config.SilentAim->FovFillColor.Value.y, g_Config.SilentAim->FovFillColor.Value.z,g_Config.SilentAim->FovFillColor.Value.w };
						if (ImGui::ColorEdit4(xorstr("SilentAim Fov Fill"), SilentAimFovFillCol, ImGuiColorEditFlags_AlphaBar))
						{
							g_Config.SilentAim->FovFillColor.Value.x = SilentAimFovFillCol[0];
							g_Config.SilentAim->FovFillColor.Value.y = SilentAimFovFillCol[1];
							g_Config.SilentAim->FovFillColor.Value.z = SilentAimFovFillCol[2];
							g_Config.SilentAim->FovFillColor.Value.w = SilentAimFovFillCol[3];
						}
					}
					ImGui::EndChild();
				}
				ImGui::EndGroup();
				break;
			case 2: //Trigger
				ImGui::BeginGroup();
				{
					ImGui::BeginChild(xorstr("Trigger"), ImVec2(340, 400), false, 0);
					{
						Custom::CheckBox(xorstr("Enable"), &g_Config.TriggerBot->Enabled);
						Custom::CheckBox(xorstr("Smart Triggerbot"), &g_Config.TriggerBot->SmartTrigger);
						Custom::CheckBox(xorstr("Only Visible"), &g_Config.TriggerBot->OnlyVisible);
						Custom::CheckBox(xorstr("Ignore NPCs"), &g_Config.TriggerBot->IgnoreNPCs);
						if (!g_Config.TriggerBot->SmartTrigger) {
							Custom::CheckBox(xorstr("Show Fov"), &g_Config.TriggerBot->ShowFov);
						}
						else
						{
							g_Config.TriggerBot->ShowFov = false;
						}
						static int KeyMode = 1;
						ImGui::Keybind(xorstr("Bind"), &g_Config.TriggerBot->KeyBind, &KeyMode);
					}
					ImGui::EndChild();
				}
				ImGui::EndGroup();
				ImGui::SameLine();
				ImGui::BeginGroup();
				{
					ImGui::BeginChild(xorstr("Settings"), ImVec2(340, 400), false, 0);
					{

						if (!g_Config.TriggerBot->SmartTrigger) {
							ImGui::SliderInt(xorstr("Field of View X"), &g_Config.TriggerBot->FOVX, 0, 400, xorstr("%d"), 0);
							ImGui::SliderInt(xorstr("Field of View Y"), &g_Config.TriggerBot->FOVY, 0, 400, xorstr("%d"), 0);
							ImGui::SliderInt(xorstr("Max Distance"), &g_Config.TriggerBot->MaxDistance, 0, 1000, xorstr("%dm"), 0);
							ImGui::SliderFloat(xorstr("Fov Thickness"), &g_Config.TriggerBot->FovThickness, 0.5f, 8.0f, xorstr("%.1fpx"), 0);
							Custom::CheckBox(xorstr("Fill Fov"), &g_Config.TriggerBot->FovFilled);
						}

						ImGui::SliderInt(xorstr("Delay"), &g_Config.TriggerBot->Delay, 0, 10, xorstr("%d"), 0);


						static float TriggerFovCol[4] = { g_Config.TriggerBot->FovColor.Value.x, g_Config.TriggerBot->FovColor.Value.y, g_Config.TriggerBot->FovColor.Value.z,g_Config.TriggerBot->FovColor.Value.w };
						if (ImGui::ColorEdit4(xorstr("Triggerbot Fov"), TriggerFovCol, ImGuiColorEditFlags_AlphaBar))
						{
							g_Config.TriggerBot->FovColor.Value.x = TriggerFovCol[0];
							g_Config.TriggerBot->FovColor.Value.y = TriggerFovCol[1];
							g_Config.TriggerBot->FovColor.Value.z = TriggerFovCol[2];
							g_Config.TriggerBot->FovColor.Value.w = TriggerFovCol[3];
						}
						static float TriggerFovFillCol[4] = { g_Config.TriggerBot->FovFillColor.Value.x, g_Config.TriggerBot->FovFillColor.Value.y, g_Config.TriggerBot->FovFillColor.Value.z,g_Config.TriggerBot->FovFillColor.Value.w };
						if (ImGui::ColorEdit4(xorstr("Triggerbot Fov Fill"), TriggerFovFillCol, ImGuiColorEditFlags_AlphaBar))
						{
							g_Config.TriggerBot->FovFillColor.Value.x = TriggerFovFillCol[0];
							g_Config.TriggerBot->FovFillColor.Value.y = TriggerFovFillCol[1];
							g_Config.TriggerBot->FovFillColor.Value.z = TriggerFovFillCol[2];
							g_Config.TriggerBot->FovFillColor.Value.w = TriggerFovFillCol[3];
						}
					}
					ImGui::EndChild();
				}
				ImGui::EndGroup();
				break;
			default:
				break;
			}

		}
		ImGui::EndGroup();

		ImGui::PopStyleVar();
	}
}
