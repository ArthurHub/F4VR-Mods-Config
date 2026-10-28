#include "MenuControls.h"

#include <format>
#include <string_view>

#include "common/CommonUtils.h"
#include "vrcf/InputBindingParser.h"

namespace
{
    // the threshold of a push that a binding has when its table names none, as the framework reads it
    constexpr float DEFAULT_THRESHOLD = 0.85f;

    /**
     * Whether a hand is the right controller: the primary hand is, unless the player is left-handed.
     */
    bool isRightController(const vrcf::Hand hand, const bool leftHanded)
    {
        switch (hand) {
        case vrcf::Hand::Left:
            return false;
        case vrcf::Hand::Right:
            return true;
        case vrcf::Hand::Offhand:
            return leftHanded;
        case vrcf::Hand::Primary:
        default:
            return !leftHanded;
        }
    }

    /**
     * A hand as the config names it.
     */
    std::string_view handWords(const vrcf::Hand hand)
    {
        switch (hand) {
        case vrcf::Hand::Offhand:
            return "the off hand";
        case vrcf::Hand::Right:
            return "the right hand";
        case vrcf::Hand::Left:
            return "the left hand";
        case vrcf::Hand::Any:
            return "either hand";
        case vrcf::Hand::Primary:
        default:
            return "the primary hand";
        }
    }

    /**
     * A button as the controller prints it: the A and B buttons of the right controller are X and Y on the left.
     */
    std::string buttonWords(const vr::EVRButtonId button, const bool rightController)
    {
        switch (button) {
        case vr::k_EButton_A:
            return rightController ? "the A button" : "the X button";
        case vr::k_EButton_ApplicationMenu:
            return rightController ? "the B button" : "the Y button";
        case vr::k_EButton_System:
            return "the system button";
        default:
            return "the " + vrcf::buttonName(button);
        }
    }

    /**
     * A button as the controller of a hand prints it, and for either hand as both print it: "the A or X button".
     */
    std::string buttonOnHand(const vr::EVRButtonId button, const vrcf::Hand hand, const bool leftHanded)
    {
        return hand == vrcf::Hand::Any ? mods_config::ui::eitherHandButtonName(button) : buttonWords(button, isRightController(hand, leftHanded));
    }

    /**
     * The verb of a sentence: how the button is worked.
     */
    std::string_view verbOf(const vrcf::ActivationType type)
    {
        switch (type) {
        case vrcf::ActivationType::Touch:
            return "Touch";
        case vrcf::ActivationType::HoldDown:
            return "Hold";
        case vrcf::ActivationType::Release:
            return "Release";
        case vrcf::ActivationType::Tap:
            return "Tap";
        case vrcf::ActivationType::LongPress:
            return "Long press";
        case vrcf::ActivationType::DoublePress:
            return "Double press";
        case vrcf::ActivationType::Press:
        default:
            return "Press";
        }
    }

    /**
     * What a binding's duration says after the button, for the types that have one: none for a duration of 0,
     * which is the type's own.
     */
    std::string durationWords(const vrcf::InputBinding& binding)
    {
        if (binding.duration <= 0.0f) {
            return {};
        }
        switch (binding.type) {
        case vrcf::ActivationType::LongPress:
            return std::format(" for {:g} s", binding.duration);
        case vrcf::ActivationType::HoldDown:
            return std::format(", from {:g} s", binding.duration);
        case vrcf::ActivationType::DoublePress:
            return std::format(", presses within {:g} s", binding.duration);
        case vrcf::ActivationType::Release:
            return std::format(", held no longer than {:g} s", binding.duration);
        default:
            return {};
        }
    }
}

namespace mods_config::ui
{
    /**
     * The controls of the menu that its Help page tells, with the buttons and the bindings of the config, as
     * the player's controllers print them. A control the config turns off is left out.
     */
    std::vector<ControlHelp> menuControls(const MenuButtons& buttons)
    {
        std::vector<ControlHelp> controls{
            { .name = "Point and press", .text = "Point at the menu with either hand, and pull the trigger on what the ray is on." },
            { .name = "Scroll", .text = "Push the thumbstick up or down on the hand that points: the list the ray is on scrolls." },
            { .name = "Step a value", .text = "Push the thumbstick left or right on the hand that points: the slider or the choice the ray is on steps." },
        };
        if (buttons.back.isEnabled()) {
            controls.push_back({ .name = "Back",
                .text =
                    std::format("{}, pointing at the menu: one step back, and from the menu's front page the menu closes.", bindingSentence(buttons.back, buttons.leftHanded)) });
        }
        if (buttons.move.isEnabled()) {
            controls.push_back({ .name = "Move the menu",
                .text = std::format("{}, pointing at the menu: it follows the hand until you let go of the button.", bindingSentence(buttons.move, buttons.leftHanded)) });
        }
        if (buttons.recenter.isEnabled()) {
            controls.push_back({ .name = "Recenter",
                .text = std::format("{}: the menu is put in front of you again, also after you moved it.", bindingSentence(buttons.recenter, buttons.leftHanded)) });
        }
        if (buttons.openMenu.isEnabled()) {
            controls.push_back(
                { .name = "Open and close the menu", .text = bindingSentence(buttons.openMenu, buttons.leftHanded) + ". While the menu is minimized it shows it again." });
        }
        controls.push_back({ .name = "Minimize",
            .text = "Minimize, on the title bar, hides the menu as you left it, with Restore and Exit on your offhand wrist. Restore shows it again, and Exit closes it." });
        return controls;
    }

    /**
     * A binding as one sentence, as the design system's InputBinding says it: the verb of its type, the button
     * as the controller prints it, the hand as the config names it, then its duration or its threshold, and the
     * button it is held with. "Long press the thumbstick on the primary hand for 1.5 s, holding the thumbstick on
     * the off hand". A threshold is said when it is not the framework's own. A push of the trigger or the grip is a
     * pull, with no side. "None" for a binding that is off.
     */
    std::string bindingSentence(const vrcf::InputBinding& binding, const bool leftHanded)
    {
        if (!binding.isEnabled()) {
            return "None";
        }
        std::string sentence;
        if (binding.type == vrcf::ActivationType::AxisDirection) {
            // the trigger and the grip are pulled, which has no side
            sentence = binding.axis == vrcf::Axis::Thumbstick ? std::format("Push the thumbstick {} on {}", vrcf::directionName(binding.direction), handWords(binding.hand))
                                                              : std::format("Pull the {} on {}", vrcf::axisName(binding.axis), handWords(binding.hand));
            if (!common::fEqual(binding.threshold, DEFAULT_THRESHOLD)) {
                sentence += std::format(", past {:.2f}", binding.threshold);
            }
        } else {
            sentence = std::format("{} {} on {}{}", verbOf(binding.type), buttonOnHand(binding.button, binding.hand, leftHanded), handWords(binding.hand), durationWords(binding));
        }
        if (binding.modifier) {
            const auto hand = binding.modifier->hand.value_or(binding.hand);
            sentence += ", holding " + buttonOnHand(binding.modifier->button, hand, leftHanded);
            if (binding.modifier->hand) {
                sentence += std::format(" on {}", handWords(*binding.modifier->hand));
            }
        }
        return sentence;
    }

    /**
     * A button of either hand, as a binding of any hand has it: the A and B buttons are named with what the left
     * controller prints, as "the A or X button".
     */
    std::string eitherHandButtonName(const vr::EVRButtonId button)
    {
        switch (button) {
        case vr::k_EButton_A:
            return "the A or X button";
        case vr::k_EButton_ApplicationMenu:
            return "the B or Y button";
        default:
            return buttonWords(button, true);
        }
    }
}
