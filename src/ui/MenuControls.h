#pragma once

#include <optional>
#include <string>
#include <vector>

#include "vrcf/VRControllersManager.h"

namespace mods_config::ui
{
    /**
     * A line of the Help page's Navigation: what a control of the menu does, and how it is worked with the player's
     * config.
     */
    struct ControlHelp
    {
        std::string name;
        std::string text;
    };

    /**
     * The buttons and the bindings of the config that work the menu, and the player's handedness, which says
     * what a button of a hand is printed as. The panel does not know the config or the game, so the menu
     * tells it.
     */
    struct MenuButtons
    {
        vrcf::InputBinding openMenu;
        vrcf::InputBinding back;
        vrcf::InputBinding move;
        vrcf::InputBinding recenter;
        bool leftHanded = false;
    };

    std::vector<ControlHelp> menuControls(const MenuButtons& buttons);
    std::string bindingSentence(const vrcf::InputBinding& binding, bool leftHanded);
    std::string shortBindingSentence(const vrcf::InputBinding& binding, bool leftHanded);
    std::string eitherHandButtonName(vr::EVRButtonId button);
}
