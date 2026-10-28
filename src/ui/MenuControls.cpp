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

    /**
     * Whether a binding is a press of one button on both controllers, the button with the same button of the other
     * hand held: "Long press both thumbsticks". Either hand is on neither.
     */
    bool isBothHandsChord(const vrcf::InputBinding& binding, const bool leftHanded)
    {
        const auto& modifier = binding.modifier;
        return modifier && modifier->hand && modifier->button == binding.button && binding.hand != vrcf::Hand::Any && *modifier->hand != vrcf::Hand::Any &&
               isRightController(binding.hand, leftHanded) != isRightController(*modifier->hand, leftHanded);
    }

    /**
     * A button of both controllers, as their prints name it together: "both thumbsticks", "the A and X buttons".
     */
    std::string bothHandsButtonWords(const vr::EVRButtonId button)
    {
        switch (button) {
        case vr::k_EButton_A:
            return "the A and X buttons";
        case vr::k_EButton_ApplicationMenu:
            return "the B and Y buttons";
        case vr::k_EButton_System:
            return "both system buttons";
        default:
            return "both " + vrcf::buttonName(button) + "s";
        }
    }

    /**
     * A binding as one sentence: the verb of its type, the button as the controller prints it, the hand as the config
     * names it, then its duration or its threshold when `withTimes`, and the button it is held with. Without the times, a
     * press of one button on both controllers is said as one press of both. A push of the trigger or the grip is a
     * pull, with no side. "None" for a binding that is off.
     */
    std::string sentenceOf(const vrcf::InputBinding& binding, const bool leftHanded, const bool withTimes)
    {
        if (!binding.isEnabled()) {
            return "None";
        }
        if (!withTimes && binding.type != vrcf::ActivationType::AxisDirection && isBothHandsChord(binding, leftHanded)) {
            return std::format("{} {}", verbOf(binding.type), bothHandsButtonWords(binding.button));
        }
        std::string sentence;
        if (binding.type == vrcf::ActivationType::AxisDirection) {
            // the trigger and the grip are pulled, which has no side
            sentence = binding.axis == vrcf::Axis::Thumbstick ? std::format("Push the thumbstick {} on {}", vrcf::directionName(binding.direction), handWords(binding.hand))
                                                              : std::format("Pull the {} on {}", vrcf::axisName(binding.axis), handWords(binding.hand));
            if (withTimes && !common::fEqual(binding.threshold, DEFAULT_THRESHOLD)) {
                sentence += std::format(", past {:.2f}", binding.threshold);
            }
        } else {
            sentence = std::format("{} {} on {}{}",
                verbOf(binding.type),
                buttonOnHand(binding.button, binding.hand, leftHanded),
                handWords(binding.hand),
                withTimes ? durationWords(binding) : std::string());
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
}

namespace mods_config::ui
{
    /**
     * The Navigation section of the Help page, as the design system's FullPanelHelp has it: how the menu is opened,
     * pointed at, moved and minimized, with the buttons and the bindings of the config in their short sentence. A
     * control the config turns off is left out.
     */
    std::vector<ControlHelp> menuControls(const MenuButtons& buttons)
    {
        std::vector<ControlHelp> controls;
        if (buttons.openMenu.isEnabled()) {
            controls.push_back(
                { .name = "Open and close", .text = shortBindingSentence(buttons.openMenu, buttons.leftHanded) + ". While the menu is minimized it shows it again." });
        }
        controls.push_back(
            { .name = "Pointing at the menu", .text = "That hand's trigger and thumbstick work the menu, and the game does not get them: you do not fire or turn by accident." });
        if (buttons.back.isEnabled()) {
            controls.push_back({ .name = "Back", .text = shortBindingSentence(buttons.back, buttons.leftHanded) + " while pointing at the menu." });
        }
        if (buttons.move.isEnabled()) {
            controls.push_back(
                { .name = "Move the menu", .text = shortBindingSentence(buttons.move, buttons.leftHanded) + " while pointing at the menu: it follows the hand until you let go." });
        }
        if (buttons.recenter.isEnabled()) {
            controls.push_back({ .name = "Recenter",
                .text = shortBindingSentence(buttons.recenter, buttons.leftHanded) +
                        ": the menu is put in front of you again. It also comes back by itself when you walk away from it." });
        }
        controls.push_back({ .name = "Minimize",
            .text = "On the title bar: the menu is hidden as you left it, unsaved changes too. Your offhand wrist then has Restore, which shows it again with its "
                    "values read again, and Exit, which closes it." });
        return controls;
    }

    /**
     * A binding as one sentence, as the design system's InputBinding says it, with its duration or its threshold:
     * "Long press the thumbstick on the primary hand for 1.5 s, holding the thumbstick on the off hand". A threshold
     * is said when it is not the framework's own.
     */
    std::string bindingSentence(const vrcf::InputBinding& binding, const bool leftHanded)
    {
        return sentenceOf(binding, leftHanded, true);
    }

    /**
     * A binding as the Help page says it, shorter than its sentence: with no time or threshold, and a press of one
     * button on both controllers as one press of both, "Long press both thumbsticks".
     */
    std::string shortBindingSentence(const vrcf::InputBinding& binding, const bool leftHanded)
    {
        return sentenceOf(binding, leftHanded, false);
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
