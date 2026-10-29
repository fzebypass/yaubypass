#pragma once
#include <Includes/Includes.hpp>
#include <windows.h>
#include <iostream>
#include <thread>
#include <declar/declar.hpp>
#include "core/features/invoker.hpp"
#include <Includes/CustomWidgets/Notify.hpp>
using namespace std;
namespace Settings {

	static int iSubTabCount = 0;
	static float SubTabAlpha = 0.f;
	static int iSubTab = 0;

	void Render() {
		// Definir los sub-tabs correctamente
		ImGui::SetCursorPosX(850);
		if (Custom::SubTab(xorstr("Main"), iSubTabCount == 0)) {
			iSubTabCount = 0;
			iSubTab = iSubTabCount;
		}

		// Animaci�n de transici�n entre sub-tabs
		SubTabAlpha = ImClamp(SubTabAlpha + (5.f * ImGui::GetIO().DeltaTime * (iSubTabCount == iSubTab ? 1.f : -1.f)), 0.f, 1.f);

		if (SubTabAlpha == 0.f)
			iSubTab = iSubTabCount;

		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, SubTabAlpha * ImGui::GetStyle().Alpha);

		ImGui::SetCursorPos(ImVec2(226, 76));
		ImGui::BeginGroup();
		{
			switch (iSubTab)
			{
			case 0: // Main Settings
				ImGui::BeginGroup();
				{
					ImGui::BeginChild(xorstr("Globals"), ImVec2(340, 400), false, 0);
					{
						Custom::CheckBox(xorstr("WaterMark"), &g_Config.General->WaterMark);
						Custom::CheckBox(xorstr("KeybindList"), &g_Config.General->KeybindList);
						Custom::CheckBox(xorstr("Auto Config"), &g_Config.General->AutoConfig);
						Custom::CheckBox(xorstr("Second Monitor"), &g_Config.General->SecondMonitor);
						if (g_Config.General->SecondMonitor)
						{
							Custom::CheckBox(xorstr("Black Background"), &g_Config.General->MirrorFivem);
						}

                        if (Custom::CheckBox(xorstr("Stream Proof"), &g_Config.General->StreamProof));

						static int KeyMode = 1;
						ImGui::Keybind(xorstr("Menu Key"), &g_Config.General->MenuKey, &KeyMode);

						static bool bindingKey = false;  // Indica si estamos asignando una tecla
						static bool keyBindCompleted = false; // Flag para saber si ya termin� de asignar la tecla
						static bool keyPressedOnce = false;   // Flag para saber si la tecla ya fue presionada

						// Dibujar el keybind para Panic Key

						// Cuando se suelta el keybind, marcamos que termin� el proceso
						if (bindingKey && !ImGui::IsItemActive())
						{
							bindingKey = false;
							keyBindCompleted = true;  // Se complet� el bind
							keyPressedOnce = false;   // Reiniciamos el flag
						}

						// Verificar pulsaci�n solo si ya termin� el bind


						if (ImGui::ColorEdit4(xorstr("Menu Color"), accent_color, ImGuiColorEditFlags_AlphaBar))
						{
								accent_color[0], accent_color[1], accent_color[2], accent_color[3];
						}


						if (Custom::Button(xorstr("Invoke"), ImVec2(-1, 30), 0))
						{
						
						}

					}
					ImGui::EndChild();
				}
				ImGui::EndGroup();
				ImGui::SameLine();
				ImGui::BeginGroup();
				{
					ImGui::BeginChild(xorstr("Configs"), ImVec2(340, 400), false, 0);
					{
						static char ConfigName[64] = "default";
						auto GetConfigName = []() {
							std::string cfgName = ConfigName;
							return cfgName == xorstr("default") ? std::string() : cfgName;
						};
						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, 8));
						ImGui::InputText(xorstr("Name"), ConfigName, IM_ARRAYSIZE(ConfigName));
						ImGui::InputTextMultiline(
							xorstr("Config"),
							g_Variables.m_Config,
							IM_ARRAYSIZE(g_Variables.m_Config),
							ImVec2(-1, 220)
						);
						if (Custom::Button(xorstr("Save Config"), ImVec2(-1, 30), 0))
						{
							const std::string savedConfig = g_Config.SaveCurrentConfig(GetConfigName());
							if (!savedConfig.empty())
							{
								strncpy_s(g_Variables.m_Config, savedConfig.c_str(), _TRUNCATE);
								NotifyManager::Send(xorstr("Config saved"), 2000);
							}
							else
							{
								NotifyManager::Send(xorstr("Config save failed"), 3000);
							}
						}
						if (Custom::Button(xorstr("Load Config"), ImVec2(-1, 30), 0))
						{
							const std::string loadedConfig = g_Config.LoadCfg(GetConfigName(), "");
							if (!loadedConfig.empty())
							{
								strncpy_s(g_Variables.m_Config, loadedConfig.c_str(), _TRUNCATE);
								NotifyManager::Send(xorstr("Config loaded"), 2000);
							}
							else
							{
								NotifyManager::Send(xorstr("Config load failed"), 3000);
							}
						}
						if (Custom::Button(xorstr("Import Text"), ImVec2(-1, 30), 0))
						{
							const std::string importedConfig = g_Config.LoadCfg(GetConfigName(), g_Variables.m_Config);
							NotifyManager::Send(importedConfig.empty() ? xorstr("Config import failed") : xorstr("Config imported"), importedConfig.empty() ? 3000 : 2000);
						}
						ImGui::PopStyleVar();
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
