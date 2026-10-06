#pragma once

#include "ConfigBase.h"
#include "Resources.h"
#include "common/CommonUtils.h"
#include "imgui/ImGuiPointer.h"
#include "vrcf/VRControllersManager.h"

namespace mods_config
{
    static const auto BASE_MOD_PATH = BASE_PATH + "\\" + std::string(Version::PROJECT);
    static const auto INI_PATH = BASE_MOD_PATH + "\\" + std::string(Version::PROJECT) + ".ini";

    class Config : public ConfigBase
    {
    public:
        explicit Config()
            : ConfigBase(Version::PROJECT, INI_PATH, IDR_CONFIG_INI)
        {}

        // Opens and closes the config menu
        vrcf::InputBinding openMenuBinding;

        // The pointer of the menu: where the ray is on the hand, and how the ray and the mark on the menu look
        imgui::PointerStyle pointerStyle;

    protected:
        virtual void loadIniConfigInternal(const CSimpleIniA& ini) override;
    };

    // Global singleton for easy access
    inline Config g_config;
}
