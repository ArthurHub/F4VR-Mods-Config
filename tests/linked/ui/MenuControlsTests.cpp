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
    // the stick moved, not pushed to a side
    CHECK(ui::bindingSentence({ .hand = Hand::Primary, .type = ActivationType::AxisDirection, .direction = vrcf::Direction::Any }, false) ==
          "Move the thumbstick on the primary hand");
    CHECK(ui::bindingSentence({ .hand = Hand::Primary, .type = ActivationType::AxisDirection, .direction = vrcf::Direction::UpDown }, false) ==
          "Move the thumbstick up or down on the primary hand");
    CHECK(ui::bindingSentence({ .hand = Hand::Offhand, .type = ActivationType::AxisDirection, .direction = vrcf::Direction::RightLeft }, false) ==
          "Move the thumbstick left or right on the off hand");
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

TEST_CASE("MenuControls: the Help page says a binding short, with no time, and a press of both hands as one")
{
    CHECK(ui::shortBindingSentence(OPEN_MENU, false) == "Long press both thumbsticks");
    CHECK(ui::shortBindingSentence(OPEN_MENU, true) == "Long press both thumbsticks");
    CHECK(ui::shortBindingSentence(MOVE, false) == "Long press the grip on either hand");
    CHECK(ui::shortBindingSentence(
              { .hand = Hand::Right, .type = ActivationType::Press, .button = vr::k_EButton_A, .modifier = vrcf::InputModifier{ .button = vr::k_EButton_A, .hand = Hand::Left } },
              false) == "Press the A and X buttons");
    CHECK(ui::shortBindingSentence(
              { .hand = Hand::Offhand, .type = ActivationType::AxisDirection, .axis = vrcf::Axis::Thumbstick, .direction = vrcf::Direction::Left, .threshold = 0.5f },
              false) == "Push the thumbstick left on the off hand");

    // a button held on the same hand, or another button, is no press of both
    CHECK(ui::shortBindingSentence({ .hand = Hand::Primary,
                                       .type = ActivationType::LongPress,
                                       .button = vr::k_EButton_SteamVR_Touchpad,
                                       .modifier = vrcf::InputModifier{ .button = vr::k_EButton_SteamVR_Touchpad } },
              false) == "Long press the thumbstick on the primary hand, holding the thumbstick");
    CHECK(ui::shortBindingSentence({ .hand = Hand::Primary,
                                       .type = ActivationType::Press,
                                       .button = vr::k_EButton_SteamVR_Trigger,
                                       .modifier = vrcf::InputModifier{ .button = vr::k_EButton_Grip, .hand = Hand::Offhand } },
              false) == "Press the trigger on the primary hand, holding the grip on the off hand");
}

TEST_CASE("MenuControls: the Help page's Navigation has the controls of the config, and leaves out the ones it turns off")
{
    const auto controls = ui::menuControls({ .openMenu = OPEN_MENU, .back = BACK, .move = MOVE, .recenter = RECENTER, .leftHanded = false });
    CHECK(namesOf(controls) == Names{ "Open / Close", "Point and select", "Back", "Move the menu", "Recenter", "Navigate pages", "Minimize" });
    CHECK(controls[0].text == "Long press both thumbsticks to open or close the menu. This also restores a minimized menu.");
    CHECK(controls[1].text ==
          "Only one hand controls the menu at a time, indicated by the mark at the end of its ray. "
          "Use that hand's trigger to select and thumbstick to scroll. These inputs are blocked from the game "
          "while pointing at the menu. Point away to use the trigger in-game without accidentally selecting anything.");
    CHECK(controls[2].text ==
          "Tap the grip on either hand while pointing at the menu to close a popup, cancel an edit, or go back one page. "
          "From Help, it returns to your previous page. From the front page, it closes the menu.");
    CHECK(controls[3].text == "Long press the grip on either hand while pointing at the menu to grab it. Move your hand to reposition it, then release.");
    CHECK(controls[4].text == "Long press the thumbstick on the off hand to bring the menu in front of you. It also returns automatically if you move too far away.");
    CHECK(controls[5].text ==
          "The breadcrumbs in the title bar show your current location. "
          "Select any earlier page to jump back, or MODS CONFIG FO4 to return to the front page.");
    CHECK(controls[6].text ==
          "Select \xE2\x80\x93 in the title bar to hide the menu while inspecting changes in-game. "
          "Your current page and unsaved edits are preserved. "
          "Use Restore on your offhand wrist to bring it back and refresh its values, "
          "or Exit to close the menu (with a warning if you have unsaved changes).");

    const InputBinding none{ .type = ActivationType::Disabled };
    const auto fewer = ui::menuControls({ .openMenu = none, .back = none, .move = none, .recenter = none, .leftHanded = false });
    CHECK(namesOf(fewer) == Names{ "Point and select", "Navigate pages", "Minimize" });
}
