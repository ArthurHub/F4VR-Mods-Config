#pragma once

#include <atomic>
#include <optional>

#include "Resources.h"
#include "common/CommonUtils.h"
#include "config/ConfigBase.h"
#include "imgui/ImGuiPointer.h"
#include "ui/Developer.h"
#include "vrcf/VRControllersHaptic.h"
#include "vrcf/VRControllersManager.h"

namespace mods_config
{
    static const auto BASE_MOD_PATH = BASE_PATH + "\\" + std::string(Version::PROJECT);
    static const auto CONFIG_PATH = BASE_MOD_PATH + "\\" + std::string(Version::PROJECT) + ".toml";

    class Config : public ConfigBase
    {
    public:
        explicit Config()
            : ConfigBase(Version::PROJECT, CONFIG_PATH, IDR_CONFIG)
        {}

        // Opens and closes the config menu
        vrcf::InputBinding openMenuBinding;

        // Back: a short press of this button on the hand that points at the menu's panel. Nothing for no Back.
        std::optional<vr::EVRButtonId> backButton;

        // The size of the menu's panel, with its text and its rows, and where it is put in front of the player:
        // how far, how much above the eyes, and tilted back by how many degrees. The distance and the elevation
        // are in game units.
        float panelScale = 0.0f;
        float panelDistance = 0.0f;
        float panelElevation = 0.0f;
        float panelTilt = 0.0f;

        // The room on the panel for its rows, in the panel's own units, which the scale makes larger: a larger
        // panel shows more of a page, with its text at the same size.
        float panelWidth = 0.0f;
        float panelHeight = 0.0f;

        // The width of the tree of the mods on the left of the panel, in the panel's own units: the page beside
        // it has the rest of the panel's width.
        float treeWidth = 0.0f;

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

        // The pulse the hand that points gets as its press does something in the menu. Nothing for no pulse.
        std::optional<vrcf::HapticPattern> pressHaptic;

        // The Developer tools, for a package's author: whether they are on, which packages the menu's front page
        // lists, the one it lists alone, and whether its section is folded
        ui::DeveloperView developer;

        void saveDeveloper(const ui::DeveloperView& view);

    protected:
        virtual void loadConfigInternal(const toml::table& config) override;
    };

    // Global singleton for easy access
    inline Config g_config;
}
