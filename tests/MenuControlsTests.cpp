#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "ui/MenuControls.h"

using namespace mods_config;
using vrcf::ActivationType;
using vrcf::Hand;
using vrcf::InputBinding;

namespace
{
    using Names = std::vector<std::string>;

    // the bindings of the shipped config
    const InputBinding OPEN_MENU{
        .hand = Hand::Primary,
        .type = ActivationType::LongPress,
        .button = vr::k_EButton_SteamVR_Touchpad,
        .modifier = vrcf::InputModifier{ .button = vr::k_EButton_SteamVR_Touchpad, .hand = Hand::Offhand },
        .duration = 1.5f,
    };
    const InputBinding RECENTER{ .hand = Hand::Offhand, .type = ActivationType::LongPress, .button = vr::k_EButton_SteamVR_Touchpad };
    const InputBinding BACK{ .hand = Hand::Any, .type = ActivationType::Tap, .button = vr::k_EButton_Grip };
    const InputBinding MOVE{ .hand = Hand::Any, .type = ActivationType::LongPress, .button = vr::k_EButton_Grip, .duration = 0.3f };

    Names namesOf(const std::vector<ui::ControlHelp>& controls)
    {
        Names names;
        for (const auto& control : controls) {
            names.push_back(control.name);
        }
        return names;
    }
}

TEST_CASE("MenuControls: a binding is one sentence, as the design system says it")
{
    CHECK(ui::bindingSentence(OPEN_MENU, false) == "Long press the thumbstick on the primary hand for 1.5 s, holding the thumbstick on the off hand");
    CHECK(ui::bindingSentence({ .hand = Hand::Offhand, .type = ActivationType::Tap, .button = vr::k_EButton_SteamVR_Trigger }, false) == "Tap the trigger on the off hand");
    CHECK(ui::bindingSentence({ .hand = Hand::Primary, .type = ActivationType::Press, .button = vr::k_EButton_SteamVR_Trigger, .modifier = vrcf::InputModifier{} }, false) ==
          "Press the trigger on the primary hand, holding the grip");
    CHECK(
        ui::bindingSentence({ .hand = Hand::Offhand, .type = ActivationType::AxisDirection, .axis = vrcf::Axis::Thumbstick, .direction = vrcf::Direction::Left, .threshold = 0.5f },
            false) == "Push the thumbstick left on the off hand, past 0.50");
    CHECK(ui::bindingSentence({ .type = ActivationType::Disabled }, false) == "None");
    CHECK(ui::bindingSentence({ .hand = Hand::Right, .type = ActivationType::AxisDirection, .axis = vrcf::Axis::Trigger, .direction = vrcf::Direction::Right, .threshold = 0.7f },
              false) == "Pull the trigger on the right hand, past 0.70");
}

TEST_CASE("MenuControls: a button is named as the controller of its hand prints it")
{
    const InputBinding offhandA{ .hand = Hand::Offhand, .type = ActivationType::LongPress, .button = vr::k_EButton_A };
    CHECK(ui::bindingSentence(offhandA, false) == "Long press the X button on the off hand");
    // a left-handed player's off hand is the right controller
    CHECK(ui::bindingSentence(offhandA, true) == "Long press the A button on the off hand");
    CHECK(ui::bindingSentence({ .hand = Hand::Left, .type = ActivationType::Press, .button = vr::k_EButton_ApplicationMenu }, true) == "Press the Y button on the left hand");

    // a binding of any hand is on either hand, whose letters are named for both controllers
    CHECK(ui::bindingSentence({ .hand = Hand::Any, .type = ActivationType::Tap, .button = vr::k_EButton_A }, true) == "Tap the A or X button on either hand");
    CHECK(ui::bindingSentence({ .hand = Hand::Primary,
                                  .type = ActivationType::Press,
                                  .button = vr::k_EButton_SteamVR_Trigger,
                                  .modifier = vrcf::InputModifier{ .button = vr::k_EButton_ApplicationMenu, .hand = Hand::Any } },
              false) == "Press the trigger on the primary hand, holding the B or Y button on either hand");
    CHECK(ui::eitherHandButtonName(vr::k_EButton_Grip) == "the grip");
    CHECK(ui::eitherHandButtonName(vr::k_EButton_A) == "the A or X button");
    CHECK(ui::eitherHandButtonName(vr::k_EButton_ApplicationMenu) == "the B or Y button");
}

TEST_CASE("MenuControls: the Help page has the controls of the config, and leaves out the ones it turns off")
{
    const auto controls = ui::menuControls({ .openMenu = OPEN_MENU, .back = BACK, .move = MOVE, .recenter = RECENTER, .leftHanded = false });
    CHECK(namesOf(controls) == Names{ "Point and press", "Scroll", "Step a value", "Back", "Move the menu", "Recenter", "Open and close the menu", "Minimize" });
    CHECK(controls[3].text == "Tap the grip on either hand, pointing at the menu: one step back, and from the menu's front page the menu closes.");
    CHECK(controls[4].text == "Long press the grip on either hand for 0.3 s, pointing at the menu: it follows the hand until you let go of the button.");
    CHECK(controls[5].text == "Long press the thumbstick on the off hand: the menu is put in front of you again, also after you moved it.");
    CHECK(controls[6].text == "Long press the thumbstick on the primary hand for 1.5 s, holding the thumbstick on the off hand.");

    const InputBinding none{ .type = ActivationType::Disabled };
    const auto fewer = ui::menuControls({ .openMenu = OPEN_MENU, .back = none, .move = none, .recenter = none, .leftHanded = false });
    CHECK(namesOf(fewer) == Names{ "Point and press", "Scroll", "Step a value", "Open and close the menu", "Minimize" });
}
