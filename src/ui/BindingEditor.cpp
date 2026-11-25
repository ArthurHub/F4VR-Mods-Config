#include "BindingEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <span>
#include <string_view>
#include <vector>

#include "Bindings.h"
#include "Choices.h"
#include "Drawing.h"
#include "MenuControls.h"
#include "Theme.h"
#include "common/CommonUtils.h"
#include "model/InputBindings.h"
#include "model/TextStates.h"

namespace
{
    using namespace mods_config;
    using Part = ui::BindingEditor::Part;

    // The sizes of the editor that are not tokens of the design system, in the panel's pixels: the editor, with the line
    // of Direction and without it, the label and the control of a part, the strip of the binding with its icons, the
    // track of the slider of its time, and the buttons of its footer
    constexpr float EDITOR_WIDTH = 717.0f;
    constexpr float EDITOR_HEIGHT = 458.0f;
    constexpr float EDITOR_HEIGHT_NO_DIRECTION = EDITOR_HEIGHT - theme::ROW_PITCH;
    constexpr float LABEL_WIDTH = 96.0f;
    constexpr float PART_WIDTH = 226.0f;
    constexpr float STRIP_HEIGHT = 64.0f;
    constexpr float STRIP_ICON_SCALE = 0.7f;
    constexpr float SLIDER_TRACK_WIDTH = 114.0f;
    constexpr float CANCEL_WIDTH = 120.0f;
    constexpr float DONE_WIDTH = 160.0f;

    // the line of a part: its label and its control
    constexpr float PART_LINE_WIDTH = LABEL_WIDTH + theme::SPACE_2 + PART_WIDTH;

    // the fill and the grab of the slider, as a setting's slider has them
    constexpr float FILL_HEIGHT = 8.0f;
    constexpr float GRAB_HEIGHT = 40.0f;

    // a time runs to 3 s in steps of 0.05 s, and a threshold from 0.10 to 1.00 in steps of 0.05
    constexpr float MOST_DURATION = 3.0f;
    constexpr float LEAST_THRESHOLD = 0.1f;
    constexpr float MOST_THRESHOLD = 1.0f;
    constexpr float SLIDER_STEP = 0.05f;

    /**
     * A choice of a part of a binding: what it sets, its text, and what the help line says of it.
     */
    template <typename T>
    struct Choice
    {
        T value;
        std::string_view text;
        std::string_view help;
    };

    constexpr std::array HANDS{
        Choice<vrcf::Hand>{ vrcf::Hand::Primary, "Primary hand", "The hand you hold your weapon in: the right one, or the left one when you play left-handed." },
        Choice<vrcf::Hand>{ vrcf::Hand::Offhand, "Off hand", "The other hand: the left one, or the right one when you play left-handed." },
        Choice<vrcf::Hand>{ vrcf::Hand::Right, "Right hand", "The right controller, also when you play left-handed." },
        Choice<vrcf::Hand>{ vrcf::Hand::Left, "Left hand", "The left controller, also when you play left-handed." },
        Choice<vrcf::Hand>{ vrcf::Hand::Any, "Any hand", "Either controller: the binding fires on the hand that works it." },
    };

    constexpr std::array TYPES{
        Choice<vrcf::ActivationType>{ vrcf::ActivationType::Tap, "Tap", "Press the button and let go of it quickly, within 0.3 s." },
        Choice<vrcf::ActivationType>{ vrcf::ActivationType::Press, "Press", "The moment the button goes down." },
        Choice<vrcf::ActivationType>{ vrcf::ActivationType::Release,
            "Release",
            "The moment the button is let go, after a hold no longer than Duration, of any time unless it says." },
        Choice<vrcf::ActivationType>{ vrcf::ActivationType::LongPress, "Long press", "Hold the button past a time, 0.6 s unless Duration says." },
        Choice<vrcf::ActivationType>{ vrcf::ActivationType::DoublePress, "Double press", "Two presses within a time, 0.4 s unless Duration says." },
        Choice<vrcf::ActivationType>{ vrcf::ActivationType::HoldDown, "Hold", "Every moment the button is held, from a time Duration says, or at once." },
        Choice<vrcf::ActivationType>{ vrcf::ActivationType::Touch, "Touch", "A touch of the button, which need not be pressed." },
        Choice<vrcf::ActivationType>{ vrcf::ActivationType::AxisDirection, "Push", "Push the thumbstick to a side, or pull the trigger or the grip, past Threshold." },
    };

    /**
     * A button of a binding, with what the right controller and the left one print on it.
     */
    struct ButtonChoice
    {
        vr::EVRButtonId button;
        std::string_view right;
        std::string_view left;
        std::string_view help;
    };

    constexpr std::array BUTTONS{
        ButtonChoice{ vr::k_EButton_SteamVR_Trigger, "Trigger", "Trigger", "The trigger, under the index finger." },
        ButtonChoice{ vr::k_EButton_Grip, "Grip", "Grip", "The grip, under the middle finger." },
        ButtonChoice{ vr::k_EButton_A, "A", "X", "The lower face button: A on the right controller, and X on the left one." },
        ButtonChoice{ vr::k_EButton_ApplicationMenu, "B", "Y", "The upper face button: B on the right controller, and Y on the left one." },
        ButtonChoice{ vr::k_EButton_SteamVR_Touchpad, "Thumbstick", "Thumbstick", "The thumbstick, pressed in." },
        ButtonChoice{ vr::k_EButton_System, "System", "System", "The system button, which SteamVR also uses." },
    };

    // the buttons that can be held while a binding is pressed: all but the system button, which SteamVR takes
    constexpr std::size_t HOLDING_BUTTONS = 5;

    /**
     * The axis a push of a button moves: the thumbstick's, the trigger's and the grip's. None for a button with no axis.
     */
    std::optional<vrcf::Axis> axisOf(const vr::EVRButtonId button)
    {
        switch (button) {
        case vr::k_EButton_SteamVR_Touchpad:
            return vrcf::Axis::Thumbstick;
        case vr::k_EButton_SteamVR_Trigger:
            return vrcf::Axis::Trigger;
        case vr::k_EButton_Grip:
            return vrcf::Axis::Grip;
        default:
            return std::nullopt;
        }
    }

    /**
     * The button of an axis, as the Button of a push shows it.
     */
    vr::EVRButtonId buttonOf(const vrcf::Axis axis)
    {
        switch (axis) {
        case vrcf::Axis::Trigger:
            return vr::k_EButton_SteamVR_Trigger;
        case vrcf::Axis::Grip:
            return vr::k_EButton_Grip;
        default:
            return vr::k_EButton_SteamVR_Touchpad;
        }
    }

    /**
     * Whether a button can be worked a way: a push is of a button with an axis, and a touch of any but the system
     * button.
     */
    bool hasType(const vr::EVRButtonId button, const vrcf::ActivationType type)
    {
        if (type == vrcf::ActivationType::AxisDirection) {
            return axisOf(button).has_value();
        }
        return type != vrcf::ActivationType::Touch || button != vr::k_EButton_System;
    }

    constexpr std::array DIRECTIONS{
        Choice<vrcf::Direction>{ vrcf::Direction::Up, "Up", "" },
        Choice<vrcf::Direction>{ vrcf::Direction::Down, "Down", "" },
        Choice<vrcf::Direction>{ vrcf::Direction::Left, "Left", "" },
        Choice<vrcf::Direction>{ vrcf::Direction::Right, "Right", "" },
    };

    // the stick moved rather than pushed to a side, which a mod that reads the stick itself binds: shown, not stepped to
    constexpr std::array MOVED_DIRECTIONS{
        Choice<vrcf::Direction>{ vrcf::Direction::Any, "Any", "" },
        Choice<vrcf::Direction>{ vrcf::Direction::UpDown, "Up/Down", "" },
        Choice<vrcf::Direction>{ vrcf::Direction::RightLeft, "Left/Right", "" },
    };

    /**
     * The place of a value among choices, and none for a value none of them has.
     */
    template <typename T, std::size_t N>
    std::optional<std::size_t> placeOf(const std::array<Choice<T>, N>& choices, const T value)
    {
        const auto found = std::ranges::find(choices, value, &Choice<T>::value);
        return found != choices.end() ? std::optional(static_cast<std::size_t>(found - choices.begin())) : std::nullopt;
    }

    /**
     * The texts and the helps of choices, as a list of a part has them.
     */
    template <typename T, std::size_t N>
    std::pair<std::vector<std::string>, std::vector<std::string>> listOf(const std::array<Choice<T>, N>& choices)
    {
        std::pair<std::vector<std::string>, std::vector<std::string>> list;
        for (const auto& choice : choices) {
            list.first.emplace_back(choice.text);
            list.second.emplace_back(choice.help);
        }
        return list;
    }

    /**
     * Whether a type of a binding has a time: how long a long press and a hold are held, the most time between the
     * presses of a double press, and the longest hold a release counts.
     */
    bool hasDuration(const vrcf::ActivationType type)
    {
        return type == vrcf::ActivationType::LongPress || type == vrcf::ActivationType::DoublePress || type == vrcf::ActivationType::HoldDown ||
               type == vrcf::ActivationType::Release;
    }

    /**
     * The time of a type that a binding with no `duration` has, as the framework uses it: 0 for a hold, which fires at
     * once, and for a release, which counts a hold of any time.
     */
    float typeDuration(const vrcf::ActivationType type)
    {
        switch (type) {
        case vrcf::ActivationType::LongPress:
            return 0.6f;
        case vrcf::ActivationType::DoublePress:
            return 0.4f;
        default:
            return 0.0f;
        }
    }

    /**
     * What the help line says of the time of a type.
     */
    std::string_view durationHelp(const vrcf::ActivationType type)
    {
        switch (type) {
        case vrcf::ActivationType::LongPress:
            return "How long the button is held before the binding fires.";
        case vrcf::ActivationType::DoublePress:
            return "The most time between the two presses.";
        case vrcf::ActivationType::HoldDown:
            return "How long the button is held before the binding fires, and then every moment while it is held.";
        case vrcf::ActivationType::Release:
            return "The longest the button can be held for its release to count.";
        default:
            return "A tap, a press and a touch have no time.";
        }
    }

    /**
     * The slider of Duration, or of Threshold for a push: its range, the value of the binding's type, which a binding
     * that sets none has, and the binding's value.
     */
    struct TimeSlider
    {
        float typeValue = 0.0f;
        float least = 0.0f;
        float most = 0.0f;
        float current = 0.0f;
    };

    /**
     * The slider of the time of a binding, or of its threshold for a push. A type whose own time is 0 slides from 0, as
     * a release counts a hold of any time.
     */
    TimeSlider timeSliderOf(const vrcf::InputBinding& binding, const bool push)
    {
        const float typeValue = push ? vrcf::InputBinding{}.threshold : typeDuration(binding.type);
        return TimeSlider{ .typeValue = typeValue,
            .least = push               ? LEAST_THRESHOLD
                     : typeValue > 0.0f ? SLIDER_STEP * 2.0f
                                        : 0.0f,
            .most = push ? MOST_THRESHOLD : MOST_DURATION,
            .current = push                      ? binding.threshold
                       : binding.duration > 0.0f ? binding.duration
                                                 : typeValue };
    }

    /**
     * The text of the slider's value: a threshold, or a time in seconds, which is `Any` for a release that counts a hold
     * of any time.
     */
    std::string timeText(const vrcf::ActivationType type, const bool push, const float value)
    {
        if (push) {
            return std::format("{:.2f}", value);
        }
        return type == vrcf::ActivationType::Release && value <= 0.0f ? std::string("Any") : std::format("{:.2f} s", value);
    }

    /**
     * What the help line says of the line of the time or the threshold, which tells a value that is the type's own.
     */
    std::string timeHelp(const vrcf::ActivationType type, const bool off, const bool push, const bool isDefault)
    {
        std::string help = off    ? std::string("A binding that is off has no time.")
                           : push ? std::string("How far it is pushed or pulled before the binding fires, to 1.00 all the way.")
                                  : std::string(durationHelp(type));
        if (isDefault) {
            help += " Default.";
        }
        return help;
    }

    /**
     * Draws the fill and the grab of the slider's track at a fraction of the way between the grab's least and most
     * centres. A grab that is dragged glows.
     */
    void drawTimeGrab(const ImRect& track, const float grabLeast, const float grabMost, const float fraction, const bool dragged)
    {
        const float grab = std::lerp(grabLeast, grabMost, fraction);
        const float middle = track.GetCenter().y;
        auto* drawList = ImGui::GetWindowDrawList();
        const auto color = ImGui::GetColorU32(theme::color::GREEN);
        if (grab > grabLeast) {
            drawList->AddRectFilled(ImVec2(grabLeast, middle - FILL_HEIGHT * 0.5f), ImVec2(grab, middle + FILL_HEIGHT * 0.5f), color, theme::RADIUS_GRAB);
        }
        drawList->AddRectFilled(ImVec2(grab - theme::GRAB_WIDTH * 0.5f, middle - GRAB_HEIGHT * 0.5f),
            ImVec2(grab + theme::GRAB_WIDTH * 0.5f, middle + GRAB_HEIGHT * 0.5f),
            dragged ? ImGui::GetColorU32(theme::color::GLOW) : color,
            theme::RADIUS_GRAB);
    }

    /**
     * A hand as the config names it, in a sentence: `the off hand`.
     */
    std::string_view handWords(const vrcf::Hand hand)
    {
        switch (hand) {
        case vrcf::Hand::Offhand:
            return "off hand";
        case vrcf::Hand::Right:
            return "right hand";
        case vrcf::Hand::Left:
            return "left hand";
        case vrcf::Hand::Any:
            return "any hand";
        default:
            return "primary hand";
        }
    }

    /**
     * The other hand of a binding, in the binding's own terms: the off hand of the primary hand, and the left hand of
     * the right one. Any hand has no other, and its list offers a button held on either hand.
     */
    vrcf::Hand otherHand(const vrcf::Hand hand)
    {
        switch (hand) {
        case vrcf::Hand::Any:
            return vrcf::Hand::Any;
        case vrcf::Hand::Primary:
            return vrcf::Hand::Offhand;
        case vrcf::Hand::Offhand:
            return vrcf::Hand::Primary;
        case vrcf::Hand::Right:
            return vrcf::Hand::Left;
        default:
            return vrcf::Hand::Right;
        }
    }

    /**
     * A button as the controller of a hand prints it, as a choice of a list has it: `X` for the A button of a left
     * controller.
     */
    std::string_view buttonText(const vr::EVRButtonId button, const vrcf::Hand hand, const bool leftHanded)
    {
        const auto found = std::ranges::find(BUTTONS, button, &ButtonChoice::button);
        if (found == BUTTONS.end()) {
            return "Button";
        }
        return ui::onRightController(hand, leftHanded) ? found->right : found->left;
    }

    /**
     * A button as the controller of a hand prints it, in a sentence: `the X button`, `the thumbstick`.
     */
    std::string buttonWords(const vr::EVRButtonId button, const vrcf::Hand hand, const bool leftHanded)
    {
        const auto text = buttonText(button, hand, leftHanded);
        if (button == vr::k_EButton_A || button == vr::k_EButton_ApplicationMenu || button == vr::k_EButton_System) {
            return std::format("the {} button", button == vr::k_EButton_System ? std::string("system") : std::string(text));
        }
        return "the " + common::str_tolower(std::string(text));
    }

    /**
     * A choice of the button that is held while the binding is pressed: none, or a button of the same hand or of the
     * other.
     */
    struct Holding
    {
        std::optional<vrcf::InputModifier> modifier;
        std::string text;
        std::string help;
    };

    /**
     * The buttons that can be held while a binding is pressed, as its list has them: nothing, then each button of the
     * same hand and of the other, which is named in the binding's own terms.
     */
    std::vector<Holding> holdingsOf(const vrcf::InputBinding& binding, const bool leftHanded)
    {
        std::vector<Holding> holdings{ { .modifier = std::nullopt, .text = "Nothing", .help = "No button needs to be held." } };
        const auto other = otherHand(binding.hand);
        for (const bool same : { true, false }) {
            const auto hand = same ? binding.hand : other;
            for (std::size_t place = 0; place < HOLDING_BUTTONS; ++place) {
                const auto button = BUTTONS[place].button;
                const auto text = buttonText(button, hand, leftHanded);
                holdings.push_back({ .modifier = vrcf::InputModifier{ .button = button, .hand = same ? std::nullopt : std::optional(other) },
                    .text = std::format("{}, {}", text, same ? "same hand" : handWords(other)),
                    .help = std::format("{} is held on the {} while the binding is pressed.", text, same ? "same hand" : handWords(other)) });
            }
        }
        return holdings;
    }

    /**
     * The place of a binding's modifier among the holdings of its list: one on the binding's own hand is of the same
     * hand. None for one of another hand than the other.
     */
    std::optional<std::size_t> holdingOf(const vrcf::InputBinding& binding, const std::vector<Holding>& holdings)
    {
        if (!binding.modifier) {
            return 0;
        }
        const auto hand = binding.modifier->hand.value_or(binding.hand);
        for (std::size_t place = 1; place < holdings.size(); ++place) {
            const auto& modifier = *holdings[place].modifier;
            if (modifier.button == binding.modifier->button && modifier.hand.value_or(binding.hand) == hand) {
                return place;
            }
        }
        return std::nullopt;
    }

    // the most lines of a label: Suppress from the game takes three
    constexpr int LABEL_MAX_LINES = 3;

    /**
     * The label of a part, in the detail size and the muted color, in its room before the part's control, on more lines
     * where it needs them.
     */
    void drawLabel(const std::string_view text, const ImVec2& at)
    {
        ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
        const controls::TextPiece piece{ .text = text, .color = ImGui::GetColorU32(theme::color::GREEN_MUTED) };
        const int lines = (std::min)(controls::countTextLines(std::span(&piece, 1), LABEL_WIDTH), LABEL_MAX_LINES);
        const float height = static_cast<float>(lines) * ImGui::GetFontSize();
        controls::drawTextPieces(std::span(&piece, 1), ImVec2(at.x, at.y + std::floor((theme::CONTROL_HEIGHT - height) * 0.5f)), LABEL_WIDTH, LABEL_MAX_LINES);
        ImGui::PopFont();
    }

    /**
     * The box of a part's control, of a width, after the label of the part at a place.
     */
    ImRect partControl(const ImVec2& at, const float width)
    {
        return controls::controlBox(at.x + LABEL_WIDTH + theme::SPACE_2, at.y, width);
    }

    /**
     * The control of a part that is a dropdown, in its box: the text of its choice and the down arrow, with the outline in
     * the full color while the ray is on it or its list is open. Returns whether it was pressed.
     */
    bool dropdownPart(const ImRect& box, const std::string_view text, const bool open)
    {
        ImGui::SetCursorScreenPos(box.Min);
        const bool pressed = ImGui::InvisibleButton("box", box.GetSize());
        const bool pointed = ImGui::IsItemHovered();
        controls::drawFrame(box);
        if (pointed || open) {
            controls::drawOutline(box.Min, box.Max, theme::color::GREEN);
        }
        const float arrowLeft = box.Max.x - theme::SPACE_4 - controls::ARROW_SIZE;
        controls::drawText(box.Min.x + theme::SPACE_4, arrowLeft - theme::SPACE_2, box.Min.y, theme::CONTROL_HEIGHT, theme::color::GREEN, text);
        controls::drawControlArrow(ImVec2(arrowLeft + controls::ARROW_SIZE * 0.5f, box.GetCenter().y), controls::Arrow::Down);
        return pressed;
    }

    /**
     * A button of the stepper of a part, with its arrow. One that goes nowhere is disabled. Returns whether it was
     * pressed.
     */
    bool stepButton(const char* id, const ImRect& box, const controls::Arrow arrow, const bool enabled)
    {
        ImGui::BeginDisabled(!enabled);
        ImGui::SetCursorScreenPos(box.Min);
        const bool pressed = ImGui::InvisibleButton(id, box.GetSize());
        controls::drawButtonFrame(box, enabled && ImGui::IsItemHovered(), enabled && ImGui::IsItemActive());
        controls::drawControlArrow(box.GetCenter(), arrow);
        ImGui::EndDisabled();
        return pressed;
    }

    /**
     * A binding with its button, worked as its type: the axis of a push is its button's, and a pull of the trigger or the
     * grip is its axis pushed right, as the framework reads the pull of an axis on its x. A type changes its time to the
     * type's own. The button can be worked as the type.
     */
    vrcf::InputBinding withButton(vrcf::InputBinding binding, const vr::EVRButtonId button, const vrcf::ActivationType type)
    {
        if (binding.type != type) {
            binding.duration = 0.0f;
        }
        binding.type = type;
        binding.button = button;
        if (const auto axis = axisOf(button); axis && type == vrcf::ActivationType::AxisDirection) {
            binding.axis = *axis;
            if (*axis != vrcf::Axis::Thumbstick) {
                binding.direction = vrcf::Direction::Right;
            }
        }
        return binding;
    }

    /**
     * Whether a binding can be worked a way with a button: the button has it, and the setting takes it.
     */
    bool fits(const vr::EVRButtonId button, const vrcf::ActivationType type, const model::ValueOptions& options)
    {
        return hasType(button, type) && model::takesType(options, type);
    }

    /**
     * The way a button is worked when a binding comes to it with a way that does not fit it: a tap where the setting
     * takes one, as every button has it, or the first way of the list that fits. A tap for a button none fits.
     */
    vrcf::ActivationType fittingType(const vr::EVRButtonId button, const model::ValueOptions& options)
    {
        if (fits(button, vrcf::ActivationType::Tap, options)) {
            return vrcf::ActivationType::Tap;
        }
        const auto first = std::ranges::find_if(TYPES, [&](const auto& type) {
            return fits(button, type.value, options);
        });
        return first != TYPES.end() ? first->value : vrcf::ActivationType::Tap;
    }
}

namespace mods_config::ui
{
    /**
     * The editor of a binding as one frame draws it: its setting and store, where its parts are, where the ray is, and
     * what the frame tells. Whether the binding is off, a push, and a push of the thumbstick is as the frame starts.
     */
    struct BindingEditor::Frame
    {
        const model::Setting& setting;
        model::Store& store;
        const bool leftHanded;
        const ImageDrawer& drawer;
        BindingEdited& edited;

        // the binding is off, is a push, and is a push of the thumbstick, which has the line of Direction
        const bool off;
        const bool push;
        const bool sided;

        // the content's left and right, the top of the header, the strip of the binding under it, and the parts under that
        const float contentLeft;
        const float contentRight;
        const float headerTop;
        const ImRect strip;
        const float partsTop;

        // the ray is on the editor, and not on a list over it, and where it is
        const bool hovered;
        const ImVec2 mouse;

        // a part changed the binding, which is set in the store once every part is drawn
        bool changed = false;

        /**
         * Tells the help line of the part or the option the ray is on: its name and its help.
         */
        void point(const std::string_view name, std::string help)
        {
            edited.pointed = PointedRow{ .text = std::string(name), .help = std::move(help) };
        }

        /**
         * Where a part is: in the left or the right column, on a line of it.
         */
        ImVec2 partAt(const bool second, const int line) const
        {
            return ImVec2(second ? contentRight - PART_LINE_WIDTH : contentLeft, partsTop + static_cast<float>(line) * (theme::CONTROL_HEIGHT + theme::SPACE_2));
        }

        /**
         * Whether the ray is on the line of a part, its label or its control.
         */
        bool pointedAt(const ImVec2& at) const
        {
            return hovered && ImRect(at, ImVec2(at.x + PART_LINE_WIDTH, at.y + theme::CONTROL_HEIGHT)).Contains(mouse);
        }
    };

    /**
     * Opens the editor of a binding with a press on its row, and draws it while it is open, with the parts of the
     * binding the row had: one that is no binding starts from its default. The editor is a popup in the page, under the
     * row, or over it where the page has no room under it, or as near as it fits: never over the title bar and the
     * footer.
     * The button is picked before how it is worked, which offers the ways the button has, and None, of the hand, turns the
     * binding off. A part that the binding's type does not have is disabled and keeps its place, and its help says why,
     * but for Direction, which is there for a push of the thumbstick alone: the editor has its line then, and is shorter
     * without it. A dropdown opens its list over the editor, and
     * an option of it the ray is on tells its help. Each change sets the binding in the store as it is made, a time or a
     * threshold as its slider is let go, and each step of the thumbstick on it.
     * Returns the part or the option the ray is on, for the help line, and whether a press did something.
     */
    BindingEdited BindingEditor::edit(const model::Setting& setting, model::Store& store, const ImRect& row, const bool pressed, const bool leftHanded, const ImageDrawer& drawer)
    {
        BindingEdited edited;
        const auto id = ImGui::GetID("binding");
        if (pressed) {
            open(setting, store);
            ImGui::OpenPopupEx(id);
            edited.pulse = true;
        }
        if (_open != &setting) {
            return edited;
        }
        if (!ImGui::IsPopupOpen(id, ImGuiPopupFlags_None)) {
            // a press beside it closed it, which keeps the binding
            _open = nullptr;
            return edited;
        }
        _openSeen = true;

        // the line of Direction is there for a push of the thumbstick alone, and the editor is as high as its lines: a
        // change of the binding in this frame shows it in the next
        const bool sided = _binding.type == vrcf::ActivationType::AxisDirection && _binding.axis == vrcf::Axis::Thumbstick;
        const float height = sided ? EDITOR_HEIGHT : EDITOR_HEIGHT_NO_DIRECTION;
        if (!beginRowEditor(id, row, EDITOR_WIDTH, height)) {
            return edited;
        }
        if (backClosed(setting, store)) {
            return edited;
        }

        Frame frame = frameOf(setting, store, leftHanded, drawer, edited, sided);
        editParts(frame);
        if (frame.changed) {
            apply(setting, store);
            edited.pulse = true;
        }
        drawHeader(frame);
        drawFooter(frame);
        ImGui::EndPopup();
        return edited;
    }

    /**
     * Opens the editor of a binding with the parts its row has in the store, or its default for one that is no binding,
     * and keeps the value to give back. The editor starts with no list open and no slider dragged.
     */
    void BindingEditor::open(const model::Setting& setting, model::Store& store)
    {
        const auto stored = store.value(setting);
        const auto* text = std::get_if<std::string>(&stored.value);
        _before = text ? *text : std::string();
        _binding = model::bindingOrDefault(setting, _before);
        _typeOn = _binding.isEnabled() ? _binding.type : vrcf::ActivationType::Tap;
        _open = &setting;
        _list = Part::None;
        _closeList = false;
        _cancel = false;
        _dragged.reset();
    }

    /**
     * Does what Back asked in the frame before, inside the editor's popup: gives back the binding the row had as the
     * editor opened and closes the editor, or closes the list over it. Returns true when it closed the editor, and its
     * popup is ended then.
     */
    bool BindingEditor::backClosed(const model::Setting& setting, model::Store& store)
    {
        if (_cancel) {
            store.set(setting, _before);
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            _open = nullptr;
            _cancel = false;
            return true;
        }
        if (_closeList) {
            // the lists over the editor, and not the editor
            ImGui::ClosePopupToLevel(ImGui::GetCurrentContext()->BeginPopupStack.Size, true);
            _list = Part::None;
            _closeList = false;
        }
        return false;
    }

    /**
     * The frame of the editor whose popup is begun: where its header, strip and parts are in the popup, where the ray
     * is, and the binding as the frame starts.
     */
    BindingEditor::Frame BindingEditor::frameOf(const model::Setting& setting, model::Store& store, const bool leftHanded, const ImageDrawer& drawer, BindingEdited& edited,
        const bool sided) const
    {
        const auto origin = ImGui::GetWindowPos();
        const float headerTop = origin.y + theme::SPACE_4;
        const float stripTop = headerTop + theme::CONTROL_HEIGHT + theme::SPACE_3;
        const ImRect strip(origin.x + theme::SPACE_4, stripTop, origin.x + EDITOR_WIDTH - theme::SPACE_4, stripTop + STRIP_HEIGHT);
        return Frame{ .setting = setting,
            .store = store,
            .leftHanded = leftHanded,
            .drawer = drawer,
            .edited = edited,
            .off = !_binding.isEnabled(),
            .push = _binding.type == vrcf::ActivationType::AxisDirection,
            .sided = sided,
            .contentLeft = strip.Min.x,
            .contentRight = strip.Max.x,
            .headerTop = headerTop,
            .strip = strip,
            .partsTop = strip.Max.y + theme::SPACE_3,
            .hovered = ImGui::IsWindowHovered(),
            .mouse = ImGui::GetIO().MousePos,
            .changed = false };
    }

    /**
     * Draws the parts of the binding, in the order a change of one sets the next: the hand, then the button, which a
     * push moves the axis of, then how it is worked, and the parts a binding of that way has. Direction is there for a
     * push of the thumbstick alone, and Suppress for a setting that offers it.
     */
    void BindingEditor::editParts(Frame& frame)
    {
        editHand(frame);
        // the button as the hand left it, which a push moves the axis of
        const auto bound = frame.push ? buttonOf(_binding.axis) : _binding.button;
        editButton(frame, bound);
        editHow(frame, bound);
        if (frame.sided) {
            editDirection(frame);
        }
        editTime(frame);
        editHolding(frame);
        if (frame.setting.valueOptions.suppress) {
            editSuppress(frame);
        }
    }

    /**
     * A part that is a dropdown: its label and its control at a place, and its list over the editor while it is open, of
     * options with their texts and helps. The help line has the option the ray is on, or else the part with its help.
     * Returns the option that was picked.
     */
    std::optional<std::size_t> BindingEditor::dropdown(Frame& frame, const Part part, const std::string_view name, const ImVec2& at, const std::string_view text,
        const bool enabled, const std::vector<std::string>& texts, const std::vector<std::string>& helps, const std::optional<std::size_t> chosen, std::string help)
    {
        ImGui::PushID(static_cast<int>(part));
        const auto box = partControl(at, PART_WIDTH);
        const auto listId = ImGui::GetID("list");
        ImGui::BeginDisabled(!enabled);
        drawLabel(name, at);
        if (dropdownPart(box, text, _list == part)) {
            ImGui::OpenPopupEx(listId);
            _list = part;
            frame.edited.pulse = true;
        }
        ImGui::EndDisabled();
        std::vector<std::string_view> views(texts.begin(), texts.end());
        const auto drawn = drawList(listId, box, views, chosen);
        if (_list == part && !ImGui::IsPopupOpen(listId, ImGuiPopupFlags_None)) {
            _list = Part::None;
        }
        ImGui::PopID();
        if (drawn.pointed) {
            frame.point(texts[*drawn.pointed], helps[*drawn.pointed]);
        } else if (frame.pointedAt(at)) {
            frame.point(name, std::move(help));
        }
        return drawn.picked;
    }

    /**
     * The hand, or None for a binding that is off, which comes first: a hand picked after None turns the binding on again,
     * worked as it was before where its button fits that, or else in a way that fits.
     * The lists offer the parts of the binding its setting takes alone, and a part the file has that the setting does not
     * take is shown as it is, which makes the value invalid.
     */
    void BindingEditor::editHand(Frame& frame)
    {
        const auto& taken = frame.setting.valueOptions;
        std::vector<vrcf::Hand> hands;
        std::vector<std::string> texts{ "None" };
        std::vector<std::string> helps{ "The binding is off." };
        std::optional<std::size_t> chosen = frame.off ? std::optional<std::size_t>(0) : std::nullopt;
        for (const auto& hand : HANDS) {
            if (!model::takesHand(taken, hand.value)) {
                continue;
            }
            if (!frame.off && hand.value == _binding.hand) {
                chosen = texts.size();
            }
            hands.push_back(hand.value);
            texts.emplace_back(hand.text);
            helps.emplace_back(hand.help);
        }
        const auto own = placeOf(HANDS, _binding.hand);
        const auto text = chosen ? std::string_view(texts[*chosen]) : own ? HANDS[*own].text : std::string_view();
        const auto help = std::string("The hand the binding is pressed on, or None for a binding that is off.");
        if (const auto picked = dropdown(frame, Part::Hand, "Hand", frame.partAt(false, 0), text, true, texts, helps, chosen, help)) {
            const auto bound = model::boundButton(_binding);
            if (*picked == 0) {
                if (!frame.off) {
                    _typeOn = _binding.type;
                }
                _binding = withButton(_binding, bound, vrcf::ActivationType::Disabled);
            } else {
                _binding.hand = hands[*picked - 1];
                if (frame.off) {
                    _binding = withButton(_binding, bound, fits(bound, _typeOn, taken) ? _typeOn : fittingType(bound, taken));
                }
            }
            frame.changed = true;
        }
    }

    /**
     * The button the binding has, which a push moves the axis of: one with no way the setting takes is not offered, and
     * one the binding's way does not fit is worked in a way that does.
     */
    void BindingEditor::editButton(Frame& frame, const vr::EVRButtonId bound)
    {
        const auto& taken = frame.setting.valueOptions;
        std::vector<vr::EVRButtonId> buttons;
        std::vector<std::string> texts;
        std::vector<std::string> helps;
        std::optional<std::size_t> chosen;
        for (const auto& choice : BUTTONS) {
            const bool worked = std::ranges::any_of(TYPES, [&](const auto& type) {
                return fits(choice.button, type.value, taken);
            });
            if (!model::takesButton(taken, choice.button) || !worked) {
                continue;
            }
            if (choice.button == bound) {
                chosen = buttons.size();
            }
            buttons.push_back(choice.button);
            texts.emplace_back(buttonText(choice.button, _binding.hand, frame.leftHanded));
            helps.emplace_back(choice.help);
        }
        const auto help = frame.off ? std::string("A binding that is off has no button.") : std::string("The button that is pressed, as the controller of its hand prints it.");
        const auto text = chosen ? std::string_view(texts[*chosen]) : buttonText(bound, _binding.hand, frame.leftHanded);
        if (const auto picked = dropdown(frame, Part::Button, "Button", frame.partAt(false, 1), text, !frame.off, texts, helps, chosen, help)) {
            const auto next = buttons[*picked];
            _binding = withButton(_binding, next, fits(next, _binding.type, taken) || frame.off ? _binding.type : fittingType(next, taken));
            frame.changed = true;
        }
    }

    /**
     * How the button is worked: the ways it has that the setting takes, of which a push of the trigger or the grip is a
     * pull.
     */
    void BindingEditor::editHow(Frame& frame, const vr::EVRButtonId bound)
    {
        const auto& taken = frame.setting.valueOptions;
        const bool pulled = axisOf(bound).value_or(vrcf::Axis::Thumbstick) != vrcf::Axis::Thumbstick;
        std::vector<vrcf::ActivationType> types;
        std::vector<std::string> texts;
        std::vector<std::string> helps;
        std::optional<std::size_t> chosen;
        for (const auto& type : TYPES) {
            if (!fits(bound, type.value, taken)) {
                continue;
            }
            if (type.value == _binding.type) {
                chosen = types.size();
            }
            types.push_back(type.value);
            const bool pull = pulled && type.value == vrcf::ActivationType::AxisDirection;
            texts.emplace_back(pull ? "Pull" : type.text);
            helps.emplace_back(pull ? "Pull it past Threshold, as an axis, and not as a press of the button." : type.help);
        }
        const auto own = placeOf(TYPES, _binding.type);
        const auto text = chosen ? std::string_view(texts[*chosen]) : own && !frame.off ? TYPES[*own].text : std::string_view();
        const auto help = frame.off ? std::string("A binding that is off is not worked.") : std::string("How the button is worked: the ways it has.");
        if (const auto picked = dropdown(frame, Part::How, "How", frame.partAt(false, 2), text, !frame.off, texts, helps, chosen, help)) {
            _binding = withButton(_binding, bound, types[*picked]);
            frame.changed = true;
        }
    }

    /**
     * The side of a push of the thumbstick, a stepper of the four sides. A stick moved, rather than pushed to a side, is
     * shown as it is, with no side to step to.
     */
    void BindingEditor::editDirection(Frame& frame)
    {
        const auto at = frame.partAt(false, 3);
        const auto box = partControl(at, PART_WIDTH);
        const auto previous = controls::controlBox(box.Min.x, at.y, theme::CONTROL_HEIGHT);
        const auto next = controls::controlBox(box.Max.x - theme::CONTROL_HEIGHT, at.y, theme::CONTROL_HEIGHT);
        const ImRect side(previous.Max.x + theme::SPACE_2, at.y, next.Min.x - theme::SPACE_2, at.y + theme::CONTROL_HEIGHT);
        const auto place = placeOf(DIRECTIONS, _binding.direction);
        const auto moved = placeOf(MOVED_DIRECTIONS, _binding.direction);
        ImGui::PushID("direction");
        drawLabel("Direction", at);
        if (stepButton("previous", previous, controls::Arrow::Left, place && *place > 0)) {
            _binding.direction = DIRECTIONS[*place - 1].value;
            frame.changed = true;
        }
        controls::drawFrame(side);
        controls::drawTextIn(side, moved ? MOVED_DIRECTIONS[*moved].text : DIRECTIONS[place.value_or(0)].text, theme::color::GREEN, 0.5f);
        if (stepButton("next", next, controls::Arrow::Right, place && *place + 1 < DIRECTIONS.size())) {
            _binding.direction = DIRECTIONS[*place + 1].value;
            frame.changed = true;
        }
        ImGui::PopID();
        if (frame.pointedAt(at)) {
            frame.point("Direction", moved ? "The thumbstick is moved, not pushed to a side: the mod reads how far itself." : "The side the thumbstick is pushed to.");
        }
    }

    /**
     * The time of a type that has one, or the threshold of a push: a slider whose value is set as the trigger lets it go,
     * and at each step of the thumbstick while the ray is on its line. The type's own time writes no key.
     */
    void BindingEditor::editTime(Frame& frame)
    {
        const auto at = frame.partAt(true, 0);
        const bool push = frame.push;
        const bool enabled = hasDuration(_binding.type) || push;
        const auto [typeValue, least, most, current] = timeSliderOf(_binding, push);
        const auto track = partControl(at, SLIDER_TRACK_WIDTH);
        const auto valueBox = controls::controlBox(track.Max.x + theme::SPACE_2, at.y, PART_WIDTH - SLIDER_TRACK_WIDTH - theme::SPACE_2);
        const float grabLeast = track.Min.x + theme::BORDER_FRAME + theme::GRAB_WIDTH * 0.5f;
        const float grabMost = track.Max.x - theme::BORDER_FRAME - theme::GRAB_WIDTH * 0.5f;
        const auto name = push ? std::string_view("Threshold") : std::string_view("Duration");

        ImGui::PushID("slider");
        ImGui::BeginDisabled(!enabled);
        drawLabel(name, at);
        ImGui::SetCursorScreenPos(track.Min);
        ImGui::InvisibleButton("track", track.GetSize());
        if (enabled && ImGui::IsItemActive()) {
            const float fraction = std::clamp((frame.mouse.x - grabLeast) / (grabMost - grabLeast), 0.0f, 1.0f);
            _dragged = std::clamp(least + std::round((most - least) * fraction / SLIDER_STEP) * SLIDER_STEP, least, most);
        }
        const auto setValue = [&](const float value) {
            if (push) {
                _binding.threshold = value;
            } else {
                _binding.duration = common::fEqual(value, typeValue) ? 0.0f : value;
            }
            frame.changed = true;
            frame.edited.pulse = true;
        };
        if (ImGui::IsItemDeactivated() && _dragged) {
            setValue(*_dragged);
            _dragged.reset();
        }
        const auto stepped = stepTime(least, most, current, ImVec2(track.Max.x, at.y), enabled && !_dragged && frame.pointedAt(at));
        if (stepped) {
            setValue(*stepped);
        }
        const float shownValue = _dragged.value_or(stepped.value_or(current));
        controls::drawFrame(track);
        controls::drawFrame(valueBox);
        if (enabled) {
            drawTimeGrab(track, grabLeast, grabMost, std::clamp((shownValue - least) / (most - least), 0.0f, 1.0f), _dragged.has_value());
            controls::drawTextIn(valueBox, timeText(_binding.type, push, shownValue), theme::color::GREEN, 1.0f, theme::SPACE_3);
        }
        ImGui::EndDisabled();
        ImGui::PopID();
        if (frame.pointedAt(at)) {
            frame.point(name, timeHelp(_binding.type, frame.off, push, enabled && common::fEqual(shownValue, typeValue)));
        }
    }

    /**
     * The step of the thumbstick on the time's slider while the ray is on its line, as it steps a setting's slider of
     * the same range, whose end is at a place. Returns the value it stepped to, and none while it steps nothing.
     */
    std::optional<float> BindingEditor::stepTime(const float least, const float most, const float current, const ImVec2& end, const bool pointed)
    {
        _stepped.valueOptions = model::ValueOptions{ .min = least, .max = most, .step = SLIDER_STEP };
        const auto stepped = controls::sliderEdit(sliderId(_stepped), end.x, end.y, controls::SliderRow{ .pointed = pointed }, current, sliderRange(_stepped));
        return stepped.write ? std::optional(stepped.value) : std::nullopt;
    }

    /**
     * The button held while the binding is pressed: nothing, or a button of the same hand or of the other. A modifier of
     * a hand that is neither is shown as it is.
     */
    void BindingEditor::editHolding(Frame& frame)
    {
        const auto holdings = holdingsOf(_binding, frame.leftHanded);
        std::vector<std::string> texts;
        std::vector<std::string> helps;
        for (const auto& holding : holdings) {
            texts.push_back(holding.text);
            helps.push_back(holding.help);
        }
        const auto chosen = holdingOf(_binding, holdings);
        std::string text;
        if (chosen) {
            text = texts[*chosen];
        } else {
            text = std::format("{}, {}", buttonText(_binding.modifier->button, *_binding.modifier->hand, frame.leftHanded), handWords(*_binding.modifier->hand));
        }
        std::string help = "A binding that is off has no button to hold.";
        if (!frame.off) {
            help = "A button held down while the binding is pressed, on the same hand or the other. Now ";
            if (_binding.modifier) {
                const auto hand = _binding.modifier->hand.value_or(_binding.hand);
                help += std::format("{} of the {}.", buttonWords(_binding.modifier->button, hand, frame.leftHanded), _binding.modifier->hand ? handWords(hand) : "same hand");
            } else {
                help += "nothing.";
            }
        }
        if (const auto picked = dropdown(frame, Part::Holding, "While holding", frame.partAt(true, 1), text, !frame.off, texts, helps, chosen, std::move(help))) {
            _binding.modifier = holdings[*picked].modifier;
            frame.changed = true;
        }
    }

    /**
     * Whether the button is hidden from the game, a switch, for a binding whose setting offers it.
     */
    void BindingEditor::editSuppress(Frame& frame)
    {
        const auto at = frame.partAt(true, 2);
        ImGui::PushID("suppress");
        ImGui::BeginDisabled(frame.off);
        drawLabel("Suppress from the game", at);
        const float toggleLeft = at.x + LABEL_WIDTH + theme::SPACE_2;
        const float toggleRight = toggleLeft + controls::SWITCHER_CELL_WIDTH * 2.0f;
        controls::drawSwitcher(toggleRight, at.y, _binding.suppress, controls::Tone::Normal);
        ImGui::SetCursorScreenPos(ImVec2(toggleLeft, at.y));
        if (ImGui::InvisibleButton("toggle", ImVec2(toggleRight - toggleLeft, theme::CONTROL_HEIGHT))) {
            const bool on = frame.mouse.x >= (toggleLeft + toggleRight) * 0.5f;
            if (on != _binding.suppress) {
                _binding.suppress = on;
                frame.changed = true;
            }
            frame.edited.pulse = true;
        }
        ImGui::EndDisabled();
        ImGui::PopID();
        if (frame.pointedAt(at)) {
            frame.point("Suppress from the game",
                frame.off ? "A binding that is off has nothing to hide." : "Hides the button from the game while the binding is in use, as in its activation sphere's zone.");
        }
    }

    /**
     * The header: the setting's name, and under it the strip of the binding as the parts left it in this frame, its icons
     * and its sentence, in the color of its value's state.
     */
    void BindingEditor::drawHeader(const Frame& frame) const
    {
        const auto stored = frame.store.value(frame.setting);
        const auto color = valueColor(stored.invalid ? ValueLook::Invalid : frame.store.isDirty(frame.setting) ? ValueLook::Pending : ValueLook::Saved);
        controls::drawText(frame.contentLeft,
            frame.contentRight,
            frame.headerTop,
            theme::CONTROL_HEIGHT,
            theme::color::GREEN,
            model::resolveClientText(frame.setting.text, &frame.store));
        const auto& strip = frame.strip;
        ImGui::GetWindowDrawList()->AddRectFilled(strip.Min, strip.Max, ImGui::GetColorU32(theme::color::PLATE), theme::RADIUS_FRAME);
        const float iconsLeft = strip.Min.x + theme::SPACE_3;
        const float iconsRight = drawBindingIcons(_binding, iconsLeft, strip.GetCenter().y, STRIP_ICON_SCALE, ImGui::GetColorU32(color), frame.leftHanded, frame.drawer);
        const float sentenceLeft = iconsRight > iconsLeft ? iconsRight + theme::SPACE_3 : strip.Min.x + theme::SPACE_4;
        const auto sentence = bindingSentence(_binding, frame.leftHanded);
        const controls::TextPiece piece{ .text = sentence, .color = ImGui::GetColorU32(color) };
        const float sentenceWidth = strip.Max.x - theme::SPACE_4 - sentenceLeft;
        const int lines = (std::min)(controls::countTextLines(std::span(&piece, 1), sentenceWidth), 2);
        const float sentenceTop = strip.GetCenter().y - static_cast<float>(lines) * ImGui::GetFontSize() * 0.5f;
        controls::drawTextPieces(std::span(&piece, 1), ImVec2(sentenceLeft, std::floor(sentenceTop)), sentenceWidth, 2);
    }

    /**
     * The footer under a rule, after the parts' lines: Cancel, which gives back the binding the editor opened with, and
     * Done, which keeps it. Both close the editor.
     */
    void BindingEditor::drawFooter(Frame& frame)
    {
        const float partLines = frame.sided ? 4.0f : 3.0f;
        const float ruleY = frame.partsTop + partLines * theme::CONTROL_HEIGHT + (partLines - 1.0f) * theme::SPACE_2 + theme::SPACE_3;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(frame.contentLeft, ruleY), ImVec2(frame.contentRight, ruleY), ImGui::GetColorU32(theme::color::RULE), theme::BORDER_FRAME);
        const float footerTop = ruleY + theme::BORDER_FRAME + theme::SPACE_3;
        const auto done = controls::controlBox(frame.contentRight - DONE_WIDTH, footerTop, DONE_WIDTH);
        const auto cancelBox = controls::controlBox(done.Min.x - theme::SPACE_3 - CANCEL_WIDTH, footerTop, CANCEL_WIDTH);
        if (panelButton("Cancel", cancelBox, controls::ButtonLook::Quiet)) {
            frame.store.set(frame.setting, _before);
            ImGui::CloseCurrentPopup();
            _open = nullptr;
            frame.edited.pulse = true;
        }
        if (frame.hovered && cancelBox.Contains(frame.mouse)) {
            frame.point("Cancel", "Give back the binding it had as the editor opened.");
        }
        if (panelButton("Done", done, controls::ButtonLook::Primary)) {
            ImGui::CloseCurrentPopup();
            _open = nullptr;
            frame.edited.pulse = true;
        }
        if (frame.hovered && done.Contains(frame.mouse)) {
            frame.point("Done", "Keep the binding.");
        }
    }

    /**
     * Sets the binding as its parts are now in the store, in its setting's form: the TOML text of its table, which the
     * framework writes, or an INI line. A binding that is off is `{ type = "none" }` or `none`, and a `suppress` a setting
     * does not offer stays as it was.
     */
    void BindingEditor::apply(const model::Setting& setting, model::Store& store) const
    {
        store.set(setting, model::bindingText(_binding, setting.tableForm));
    }

    bool BindingEditor::isOpen(const model::Setting& setting) const
    {
        return _open == &setting;
    }

    /**
     * Ends a frame of the page, after its rows: a binding whose editor was not drawn in it, as one whose row is not
     * shown any more, has none open.
     */
    void BindingEditor::endFrame()
    {
        if (!_openSeen) {
            _open = nullptr;
        }
        _openSeen = false;
    }

    /**
     * Back in the editor: it closes the list that is open, or else gives back the binding the row had as the editor
     * opened, and closes the editor, as the next frame draws it. Returns false while no editor is open.
     */
    bool BindingEditor::cancel()
    {
        if (!_open || _cancel) {
            return false;
        }
        if (_list != Part::None) {
            _closeList = true;
        } else {
            _cancel = true;
        }
        return true;
    }

    /**
     * Forgets the editor that is open, as another page is shown: it is of a row of the page before.
     */
    void BindingEditor::reset()
    {
        _open = nullptr;
        _openSeen = false;
        _list = Part::None;
        _closeList = false;
        _cancel = false;
        _dragged.reset();
    }
}
