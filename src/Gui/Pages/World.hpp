#pragma once
#include <Includes/includes.hpp>
#include <windows.h>
#include <iostream>
#include <thread>

using namespace std;

namespace World {

	static int iSubTabCount = 0;
	static float SubTabAlpha = 0.f;
	static int iSubTab = 0;
	static int iPlayerSubTabCount = 0;
	static float PlayerSubTabAlpha = 0.f;
	static int iPlayerSubTab = 0;

	static std::vector<Core::SDK::Game::EntityStruct> GetFilteredPlayers(bool friendsOnly)
	{
		const auto players = Core::SDK::Game::GetEntityListSnapshot();
		std::vector<Core::SDK::Game::EntityStruct> filteredPlayers;
		filteredPlayers.reserve(players.size());

		for (const auto& player : players)
		{
			if (player.Ped == Core::SDK::Pointers::pLocalPlayer)
				continue;

			if (player.PedType != 2)
				continue;

			if (friendsOnly && !player.IsFriend)
				continue;

			if (!friendsOnly && player.IsFriend)
				continue;

			filteredPlayers.push_back(player);
		}

		return filteredPlayers;
	}

	static void RenderPlayerPanel(const char* panelId, std::vector<Core::SDK::Game::EntityStruct>& players, int& selectedIndex)
	{
		if (selectedIndex >= static_cast<int>(players.size()))
			selectedIndex = -1;

		const std::string listId = std::string(panelId) + xorstr("List");
		const std::string infoId = std::string(panelId) + xorstr("Info");
		const std::string actionsId = std::string(panelId) + xorstr("Actions");

		ImGui::BeginGroup();
		{
			ImGui::BeginChild(listId.c_str(), ImVec2(340, 400), false, 0);
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, 8));

				if (players.empty())
				{
					ImGui::TextColored(ImColor(g_Col.SecundaryText), xorstr("No players found"));
				}
				else
				{
					for (int i = 0; i < static_cast<int>(players.size()); i++)
					{
						bool isSelected = selectedIndex == i;
						if (ImGui::ListSelectable(players[i].NetworkInfo.UserName.c_str(), &isSelected))
							selectedIndex = i;
					}
				}

				ImGui::PopStyleVar();
			}
			ImGui::EndChild();
		}
		ImGui::EndGroup();
		ImGui::SameLine();
		ImGui::BeginGroup();
		{
			ImGui::BeginChild(infoId.c_str(), ImVec2(340, 180), false, 0);
			{
				if (selectedIndex == -1)
				{
					ImGui::TextColored(ImColor(g_Col.SecundaryText), xorstr("Select Player"));
				}
				else
				{
					auto& selectedPed = players[selectedIndex];
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, 4));

					ImGui::TextColored(ImColor(g_Col.FeaturesText), xorstr("Name:")); ImGui::SameLine();
					ImGui::TextColored(ImColor(g_Col.SecundaryText), selectedPed.NetworkInfo.UserName.c_str());
					ImGui::TextColored(ImColor(g_Col.FeaturesText), xorstr("Id:")); ImGui::SameLine();
					ImGui::TextColored(ImColor(g_Col.SecundaryText), std::to_string(selectedPed.Id).c_str());
					ImGui::TextColored(ImColor(g_Col.FeaturesText), xorstr("Distance:")); ImGui::SameLine();
					ImGui::TextColored(ImColor(g_Col.SecundaryText), std::to_string(selectedPed.Distance).c_str());
					ImGui::TextColored(ImColor(g_Col.FeaturesText), xorstr("Weapon:")); ImGui::SameLine();
					ImGui::TextColored(ImColor(g_Col.SecundaryText), selectedPed.WeaponName.c_str());
					ImGui::TextColored(ImColor(g_Col.FeaturesText), xorstr("Friend:")); ImGui::SameLine();
					ImGui::TextColored(ImColor(g_Col.SecundaryText), selectedPed.IsFriend ? xorstr("True") : xorstr("False"));

					ImGui::PopStyleVar();
				}
			}
			ImGui::EndChild();

			ImGui::BeginChild(actionsId.c_str(), ImVec2(340, 210), false, 0);
			{
				if (selectedIndex == -1)
				{
					ImGui::TextColored(ImColor(g_Col.SecundaryText), xorstr("Select Player"));
				}
				else
				{
					auto& selectedPed = players[selectedIndex];

					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, 8));

					bool isFriend = selectedPed.IsFriend;
					if (Custom::CheckBox(xorstr("Friend"), &isFriend))
					{
						if (isFriend)
							Core::Features::g_Friends.AddFriend(selectedPed.Ped);
						else
							Core::Features::g_Friends.RemoveFriend(selectedPed.Ped);

						selectedIndex = -1;
					}

					ImGui::Spacing();

					if (Custom::Button(xorstr("Teleport"), ImVec2(-1, 30), 0))
					{
						Core::SDK::Pointers::pLocalPlayer->SetPos(selectedPed.Pos);
					}

					ImGui::PopStyleVar();
				}
			}
			ImGui::EndChild();
		}
		ImGui::EndGroup();
	}

	void Render()
	{

		//ImGui::PushStyleVar( ImGuiStyleVar_::ImGuiStyleVar_ItemSpacing, ImVec2( 6, 0 ) );
		ImGui::SetCursorPosX(769 - 30);
		if (Custom::SubTab(xorstr("Players"), 0 == iSubTabCount)) {
			iSubTabCount = 0;
		}
		ImGui::SameLine();
		if (Custom::SubTab(xorstr("Vehicles"), 1 == iSubTabCount)) {
			iSubTabCount = 1;
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
			case 0:
			{
				static int playerSelectedIndex = -1;
				static int friendSelectedIndex = -1;

				if (Custom::SubTab(xorstr("Player List"), iPlayerSubTabCount == 0)) {
					iPlayerSubTabCount = 0;
				}
				ImGui::SameLine();
				if (Custom::SubTab(xorstr("Friend List"), iPlayerSubTabCount == 1)) {
					iPlayerSubTabCount = 1;
				}

				PlayerSubTabAlpha = ImClamp(PlayerSubTabAlpha + (5.f * ImGui::GetIO().DeltaTime * (iPlayerSubTabCount == iPlayerSubTab ? 1.f : -1.f)), 0.f, 1.f);

				if (PlayerSubTabAlpha == 0.f)
					iPlayerSubTab = iPlayerSubTabCount;

				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, PlayerSubTabAlpha * ImGui::GetStyle().Alpha);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12.f);

				switch (iPlayerSubTab)
				{
				case 0:
				{
					auto players = GetFilteredPlayers(false);
					RenderPlayerPanel(xorstr("Players"), players, playerSelectedIndex);
				}
				break;
				case 1:
				{
					auto players = GetFilteredPlayers(true);
					RenderPlayerPanel(xorstr("Friends"), players, friendSelectedIndex);
				}
				break;
				default:
					break;
				}

				ImGui::PopStyleVar();
			}
			break;
			case 1: //Vehicles
			{

				static int SeletedIndex = -1;
				bool IsSelected;


				ImGui::BeginGroup();
				{
					ImGui::BeginChild(xorstr("List"), ImVec2(340, 400), false, 0);
					{
						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, 8));


						{
							std::lock_guard<std::mutex> lock(Core::Threads::g_VehicleList.vehicleListMutex); // Protege a lista durante a renderiza��o

							for (int i = 0; i < Core::SDK::Game::VehicleList.size(); i++)
							{
								IsSelected = SeletedIndex == i;
								std::string Name = Core::SDK::Game::VehicleList[i].Name + xorstr(" (") + std::to_string((int)Core::SDK::Game::VehicleList[i].Dist) + xorstr("m)");
								if (ImGui::ListSelectable(Name.c_str(), &IsSelected)) SeletedIndex = i;
							}
						}

						ImGui::PopStyleVar();
					}
					ImGui::EndChild();
				}
				ImGui::EndGroup();
				ImGui::SameLine();
				ImGui::BeginGroup();
				{
					ImGui::BeginChild(xorstr("Info"), ImVec2(340, 140), false, 0);
					{
						if (SeletedIndex == -1) {
							ImGui::TextColored(ImColor(g_Col.SecundaryText), xorstr("Select Vehicle"));
						}
						else
						{
							ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, 4));

							ImGui::TextColored(ImColor(g_Col.FeaturesText), xorstr("Name:")); ImGui::SameLine();
							ImGui::TextColored(ImColor(g_Col.SecundaryText), Core::SDK::Game::VehicleList[SeletedIndex].Name.c_str());
							ImGui::TextColored(ImColor(g_Col.FeaturesText), xorstr("Distance:")); ImGui::SameLine();
							ImGui::TextColored(ImColor(g_Col.SecundaryText), (std::to_string((int)Core::SDK::Game::VehicleList[SeletedIndex].Dist) + xorstr("m")).c_str());
							ImGui::TextColored(ImColor(g_Col.FeaturesText), xorstr("Driver:")); ImGui::SameLine();
							ImGui::TextColored(ImColor(g_Col.SecundaryText), Core::SDK::Game::VehicleList[SeletedIndex].Driver == 0 ? xorstr("Unknown") : Core::SDK::Game::VehicleList[SeletedIndex].Driver->GetPedType() != 2 ? xorstr("NPC") : Core::SDK::Game::GetPedName(Core::SDK::Game::VehicleList[SeletedIndex].Driver).c_str());
							ImGui::TextColored(ImColor(g_Col.FeaturesText), xorstr("Doors Locked:")); ImGui::SameLine();
							ImGui::TextColored(ImColor(g_Col.SecundaryText), Core::SDK::Game::VehicleList[SeletedIndex].IsLocked ? xorstr("Yes") : xorstr("No"));

							ImGui::PopStyleVar();
						}
					}
					ImGui::EndChild();
					ImGui::BeginChild(xorstr("Actions"), ImVec2(340, 250), false, 0);
					{
						if (SeletedIndex == -1) {
							ImGui::TextColored(ImColor(g_Col.SecundaryText), xorstr("Select Vehicle"));
						}
						else
						{
							auto Veh = Core::SDK::Game::VehicleList[SeletedIndex].Pointer;
							ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, 8));

							if (Core::SDK::Game::VehicleList[SeletedIndex].Pointer->IsLocked())
							{
								if (Custom::Button(xorstr("Unlock"), ImVec2(-1, 30), 0)) {
									Veh->DoorState(true);
								}
							}
							else {
								if (Custom::Button(xorstr("Lock"), ImVec2(-1, 30), 0)) {
									Veh->DoorState(false);
								}
							}

							if (Custom::Button(xorstr("Teleport"), ImVec2(-1, 30), 0)) {
								Core::SDK::Pointers::pLocalPlayer->SetPos(Veh->GetPos());
							}


							ImGui::PopStyleVar();
						}
					}
					ImGui::EndChild();

				}
				ImGui::EndGroup();
			}
			break;
			default:
				break;
			}

		}
		ImGui::EndGroup();
		ImGui::PopStyleVar();

	}
}
