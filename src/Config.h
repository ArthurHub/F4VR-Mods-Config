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

        // Puts the menu's panel in front of the player again
        vrcf::InputBinding recenterBinding;

        // The panel is also put in front of the player again when they are further than this from it, in game
        // units. 0 for never. It is read when the menu opens.
        float recenterDistance = 0.0f;

        // The pointer of the menu: where the ray is on the hand, how the ray and the mark on the menu look,
        // and how fast the thumbstick scrolls
        imgui::PointerStyle pointerStyle;

    protected:
        virtual void loadIniConfigInternal(const CSimpleIniA& ini) override;
    };

    // Global singleton for easy access
    inline Config g_config;
}
