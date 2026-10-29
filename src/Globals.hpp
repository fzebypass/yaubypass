#pragma once
#include <Includes/Includes.hpp>
#include <declar/declar.hpp>

// forward declarations for game classes used in globals
class CPed;

class cColors {
public:

	// Definir los colores correctamente
	ImVec4 ButtonHovered = ImVec4(accent_color[0], accent_color[1], accent_color[2], 0.85f); // 85% de opacidad



	ImVec4 PrimaryText = ImColor(200, 200, 200);
	ImVec4 SecundaryText = ImColor(70, 72, 89);
	ImVec4 TestingTest = ImColor(255, 201, 224);

	ImVec4 FeaturesText = ImColor(70, 72, 89);
	ImVec4 SecundaryFeaturesText = ImColor(70, 72, 89);

	ImVec4 BorderCol = ImColor(17, 17, 21);
	ImVec4 LinesCol = ImColor(26, 26, 36);
	ImVec4 BackgroundCol = ImColor(17, 17, 21);

	ImVec4 ChildCol = ImColor(23, 23, 30);
	ImVec4 ChildBorderCol = ImColor(23, 23, 30);

	ImVec4 TitleBar = ImColor(20, 20, 25);
	ImVec4 TitleBarBorder = ImColor(21, 21, 24);

	ImVec4 SideBar = ImColor(14, 14, 16);
	ImVec4 SideBarBorder = ImColor(14, 14, 16);


	ImVec4 InputBackground = ImColor(16, 16, 16, 255);
	ImVec4 InputBorder = ImColor(22, 22, 22, 255);


};

inline cColors g_Col;

class c_globals {
public:
	bool g_bPassedByThisVerify;
	uintptr_t g_VerifyLogin;
	bool done;
	bool IsOpen;
	DWORD ProcIdFiveM = 0;
	std::string ServerIp;
	std::string UserName;
	std::string Role;
	std::string gameVersionSTR;
	int gameNumber = 0;
	ImVec2 TestePos;
	char userLogin[64];
	char passLogin[64];
	char keyLogin[64];
	char m_Config[6000];
	int g_MenuKeyOverride = -1;
	HWND g_hCheatWindow;
	HWND g_hGameWindow;
	HWND g_hMirrorWindow;

	ImVec2 g_vGameWindowSize;
	ImVec2 g_vGameWindowPos;
	ImVec2 g_vGameWindowCenter;

	ImFont* m_FontBig;
	ImFont* m_FontBigSmall;
	ImFont* m_Interbold;
	ImFont* m_FontNormal;
	ImFont* m_FontSecundary;
	ImFont* m_FontSmaller;
	ImFont* m_FontSmaller2;
	ImFont* m_DrawFont;
	ImFont* m_DrawFont2;
	ImFont* m_DrawFont3;
	ImFont* m_Expand;

	ImFont* FontAwesomeSolid;
	ImFont* FontAwesomeSolidSmall;
	ImFont* FontAwesomeRegular;
	ImFont* FontAwesomeBrands;

	ImFont* open_sans_sm = nullptr;
	ImFont* open_sans = nullptr;


	ID3D11ShaderResourceView* Logo = nullptr;



	struct FiveM_Weapons_t {
		ID3D11ShaderResourceView* gadget_parachute = nullptr;
		ID3D11ShaderResourceView* weapon_advancedrifle = nullptr;
		ID3D11ShaderResourceView* weapon_appistol = nullptr;
		ID3D11ShaderResourceView* weapon_assaultrifle = nullptr;
		ID3D11ShaderResourceView* weapon_assaultrifle_mk2 = nullptr;
		ID3D11ShaderResourceView* weapon_assaultshotgun = nullptr;
		ID3D11ShaderResourceView* weapon_assaultsmg = nullptr;
		ID3D11ShaderResourceView* weapon_autoshotgun = nullptr;
		ID3D11ShaderResourceView* weapon_ball = nullptr;
		ID3D11ShaderResourceView* weapon_bat = nullptr;
		ID3D11ShaderResourceView* weapon_battleaxe = nullptr;
		ID3D11ShaderResourceView* weapon_bottle = nullptr;
		ID3D11ShaderResourceView* weapon_bullpuprifle = nullptr;
		ID3D11ShaderResourceView* weapon_bullpuprifle_mk2 = nullptr;
		ID3D11ShaderResourceView* weapon_bullpupshotgun = nullptr;
		ID3D11ShaderResourceView* weapon_bzgas = nullptr;
		ID3D11ShaderResourceView* weapon_carbinerifle = nullptr;
		ID3D11ShaderResourceView* weapon_carbinerifle_mk2 = nullptr;
		ID3D11ShaderResourceView* weapon_ceramicpistol = nullptr;
		ID3D11ShaderResourceView* weapon_combatmg = nullptr;
		ID3D11ShaderResourceView* weapon_combatmg_mk2 = nullptr;
		ID3D11ShaderResourceView* weapon_combatpdw = nullptr;
		ID3D11ShaderResourceView* weapon_combatpistol = nullptr;
		ID3D11ShaderResourceView* weapon_combatshotgun = nullptr;
		ID3D11ShaderResourceView* weapon_compactlauncher = nullptr;
		ID3D11ShaderResourceView* weapon_compactrifle = nullptr;
		ID3D11ShaderResourceView* weapon_crowbar = nullptr;
		ID3D11ShaderResourceView* weapon_dagger = nullptr;
		ID3D11ShaderResourceView* weapon_dbshotgun = nullptr;
		ID3D11ShaderResourceView* weapon_doubleaction = nullptr;
		ID3D11ShaderResourceView* weapon_fireextinguisher = nullptr;
		ID3D11ShaderResourceView* weapon_firework = nullptr;
		ID3D11ShaderResourceView* weapon_flare = nullptr;
		ID3D11ShaderResourceView* weapon_flaregun = nullptr;
		ID3D11ShaderResourceView* weapon_flashlight = nullptr;
		ID3D11ShaderResourceView* weapon_gadgetpistol = nullptr;
		ID3D11ShaderResourceView* weapon_golfclub = nullptr;
		ID3D11ShaderResourceView* weapon_grenade = nullptr;
		ID3D11ShaderResourceView* weapon_grenadelauncher = nullptr;
		ID3D11ShaderResourceView* weapon_grenadelauncher_smoke = nullptr;
		ID3D11ShaderResourceView* weapon_gusenberg = nullptr;
		ID3D11ShaderResourceView* weapon_hammer = nullptr;
		ID3D11ShaderResourceView* weapon_hatchet = nullptr;
		ID3D11ShaderResourceView* weapon_hazardcan = nullptr;
		ID3D11ShaderResourceView* weapon_heavypistol = nullptr;
		ID3D11ShaderResourceView* weapon_heavyshotgun = nullptr;
		ID3D11ShaderResourceView* weapon_heavysniper = nullptr;
		ID3D11ShaderResourceView* weapon_heavysniper_mk2 = nullptr;
		ID3D11ShaderResourceView* weapon_hominglauncher = nullptr;
		ID3D11ShaderResourceView* weapon_knife = nullptr;
		ID3D11ShaderResourceView* weapon_knuckle = nullptr;
		ID3D11ShaderResourceView* weapon_machete = nullptr;
		ID3D11ShaderResourceView* weapon_machinepistol = nullptr;
		ID3D11ShaderResourceView* weapon_marksmanpistol = nullptr;
		ID3D11ShaderResourceView* weapon_marksmanrifle = nullptr;
		ID3D11ShaderResourceView* weapon_marksmanrifle_mk2 = nullptr;
		ID3D11ShaderResourceView* weapon_mg = nullptr;
		ID3D11ShaderResourceView* weapon_microsmg = nullptr;
		ID3D11ShaderResourceView* weapon_militaryrifle = nullptr;
		ID3D11ShaderResourceView* weapon_minigun = nullptr;
		ID3D11ShaderResourceView* weapon_minismg = nullptr;
		ID3D11ShaderResourceView* weapon_molotov = nullptr;
		ID3D11ShaderResourceView* weapon_musket = nullptr;
		ID3D11ShaderResourceView* weapon_navyrevolver = nullptr;
		ID3D11ShaderResourceView* weapon_nightstick = nullptr;
		ID3D11ShaderResourceView* weapon_petrolcan = nullptr;
		ID3D11ShaderResourceView* weapon_pipebomb = nullptr;
		ID3D11ShaderResourceView* weapon_pistol = nullptr;
		ID3D11ShaderResourceView* weapon_pistol50 = nullptr;
		ID3D11ShaderResourceView* weapon_pistol_mk2 = nullptr;
		ID3D11ShaderResourceView* weapon_poolcue = nullptr;
		ID3D11ShaderResourceView* weapon_proxmine = nullptr;
		ID3D11ShaderResourceView* weapon_pumpshotgun = nullptr;
		ID3D11ShaderResourceView* weapon_pumpshotgun_mk2 = nullptr;
		ID3D11ShaderResourceView* weapon_railgun = nullptr;
		ID3D11ShaderResourceView* weapon_raycarbine = nullptr;
		ID3D11ShaderResourceView* weapon_rayminigun = nullptr;
		ID3D11ShaderResourceView* weapon_raypistol = nullptr;
		ID3D11ShaderResourceView* weapon_revolver = nullptr;
		ID3D11ShaderResourceView* weapon_revolver_mk2 = nullptr;
		ID3D11ShaderResourceView* weapon_rpg = nullptr;
		ID3D11ShaderResourceView* weapon_sawnoffshotgun = nullptr;
		ID3D11ShaderResourceView* weapon_smg = nullptr;
		ID3D11ShaderResourceView* weapon_smg_mk2 = nullptr;
		ID3D11ShaderResourceView* weapon_smokegrenade = nullptr;
		ID3D11ShaderResourceView* weapon_sniperrifle = nullptr;
		ID3D11ShaderResourceView* weapon_snowball = nullptr;
		ID3D11ShaderResourceView* weapon_snspistol = nullptr;
		ID3D11ShaderResourceView* weapon_snspistol_mk2 = nullptr;
		ID3D11ShaderResourceView* weapon_specialcarbine = nullptr;
		ID3D11ShaderResourceView* weapon_specialcarbine_mk2 = nullptr;
		ID3D11ShaderResourceView* weapon_stickybomb = nullptr;
		ID3D11ShaderResourceView* weapon_stone_hatchet = nullptr;
		ID3D11ShaderResourceView* weapon_stungun = nullptr;
		ID3D11ShaderResourceView* weapon_switchblade = nullptr;
		ID3D11ShaderResourceView* weapon_unarmed = nullptr;
		ID3D11ShaderResourceView* weapon_vintagepistol = nullptr;
		ID3D11ShaderResourceView* weapon_wrench = nullptr;
	}FiveM_WeaponsPic;

};

inline c_globals g_Variables;

class c_menu {
public:

	enum PAGES {
		Combat,
		Visuals,
		Local,
		World,
		Exploits,
		Settings,
		Login
	};

	bool IsOpen;

	int iTabCount = 0;
	float TabAlpha = 0.f;
	int iCurrentPage = 6;
	float TabAdd = 0.f;
	bool IsLogged = true;
	char login[40] = ""; // Variable para almacenar la clave ingresada
	char pass[40] = "";
	std::size_t g_OffsetWorld = 0x0;  // Un ejemplo de variable global de tipo std::size_t
	std::size_t g_OffsetReplay = 0x0;  // Un ejemplo de variable global de tipo std::size_t
	std::size_t g_OffsetView = 0x0;  // Un ejemplo de variable global de tipo std::size_t

	bool particles = true;

	ImVec2 MenuSize{ 940, 540 };
};

inline c_menu g_MenuInfo;
