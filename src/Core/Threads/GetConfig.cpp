#pragma once
#include <windows.h>
#include <string>
#include "../Core/Features/auth.hpp"
#include "../Core/config.hpp"
#include "includes/includes.hpp"

inline static void refreshconfig() {
    if (!Core::g_Config.General->AutoConfig)
        return;
    std::string cfg;
    std::string raw_response;
    if (auth::LoadCloudConfig(cfg, &raw_response))
    {
        Core::g_Config.LoadCfg("cloud", cfg);
    }
}
