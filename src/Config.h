#pragma once

#include <atomic>
#include <optional>

#include "Resources.h"
#include "common/CommonUtils.h"
#include "config/ConfigBase.h"
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

        // The size of the menu's panel, with its text and its rows, and where it is put in front of the player:
        // how far, how much above the eyes, and tilted back by how many degrees. The distance and the height
        // are in game units.
        float panelScale = 0.0f;
        float panelDistance = 0.0f;
        float panelHeight = 0.0f;
        float panelTilt = 0.0f;

        // How much the panel's background hides the world behind it: 0 for none of it, 255 for all.
        std::uint8_t panelBackgroundOpacity = 0;

        // Set when the values are loaded, also when the file changes while the game runs, which is on the file
        // watcher's thread. The menu clears it when it gives its panel the values again.
        std::atomic<bool> panelChanged = false;

        // Moves the menu's panel while it is held with the pointer on the panel: a button of the hand that
        // points. Nothing for a panel that is not moved.
        std::optional<vr::EVRButtonId> moveButton;

        // How smoothly the panel follows the hand that moves it: the time it takes to catch up, in seconds,
        // which steadies the hand's shake. 0 for no smoothing.
        float moveSmoothing = 0.0f;

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
