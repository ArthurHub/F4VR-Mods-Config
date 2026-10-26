#include "SettingRow.h"

#include <array>
#include <format>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <imgui.h>

#include "Drawing.h"
#include "Numbers.h"
#include "Theme.h"
#include "common/CommonUtils.h"
#include "model/Conditions.h"

namespace
{
    using namespace mods_config;

    /**
     * The Reset icon around a point: most of a circle, open at its left, with an arrowhead at its upper end.
     */
    void drawResetIcon(const ImVec2& center)
    {
        // as the design system draws it, in a box of 24: the circle from its lower left end around its
        // right to its upper left end, in radians
        constexpr float HALF_BOX = 12.0f;
        constexpr float RADIUS = 7.5f;
        constexpr float ARC_START = 2.744f;
        constexpr float ARC_END = -2.396f;
        const ImVec2 origin(center.x - HALF_BOX, center.y - HALF_BOX);
        const auto at = [&origin](const float x, const float y) {
            return ImVec2(origin.x + x, origin.y + y);
        };
        const auto color = ImGui::GetColorU32(theme::color::GREEN);
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->PathArcTo(at(12.1f, 12.1f), RADIUS, ARC_START, ARC_END);
        drawList->PathLineTo(at(4.0f, 10.0f));
        drawList->PathStroke(color, ui::ICON_THICKNESS);
        const std::array head{ at(4.0f, 5.0f), at(4.0f, 10.0f), at(9.0f, 10.0f) };
        drawList->AddPolyline(head.data(), static_cast<int>(head.size()), color, ui::ICON_THICKNESS);
    }

    /**
     * The Discard icon around a point, an undo: an arrowhead to the left, whose tail turns back down in a half circle,
     * in amber as the pending change it takes back.
     */
    void drawDiscardIcon(const ImVec2& center)
    {
        // as the design system draws it, in a box of 24
        constexpr float HALF_BOX = 12.0f;
        const ImVec2 origin(center.x - HALF_BOX, center.y - HALF_BOX);
        const auto at = [&origin](const float x, const float y) {
            return ImVec2(origin.x + x, origin.y + y);
        };
        const auto color = ImGui::GetColorU32(theme::color::AMBER);
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->PathLineTo(at(4.0f, 9.0f));
        drawList->PathArcTo(at(14.0f, 14.0f), 5.0f, -IM_PI / 2.0f, IM_PI / 2.0f);
        drawList->PathLineTo(at(8.0f, 19.0f));
        drawList->PathStroke(color, ui::ICON_THICKNESS);
        const std::array head{ at(9.0f, 4.0f), at(4.0f, 9.0f), at(9.0f, 14.0f) };
        drawList->AddPolyline(head.data(), static_cast<int>(head.size()), color, ui::ICON_THICKNESS);
    }

    /**
     * Whether two values of a setting are the same one. Two numbers are, when they are as near as a number
     * and the text of it in a file.
     */
    bool isSameValue(const model::Value& value, const model::Value& other)
    {
        const auto* number = std::get_if<float>(&value);
        const auto* otherNumber = std::get_if<float>(&other);
        return number && otherNumber ? common::fEqual(*number, *otherNumber) : value == other;
    }

    /**
     * Returns the store's default when the setting's control can display it.
     * Unsupported values, unmatched choices and sliders without a range have no Reset.
     */
    std::optional<model::Value> defaultOf(const model::Setting& setting, model::Store& store)
    {
        auto value = store.defaultValue(setting);
        if (!value) {
            return std::nullopt;
        }
        bool shown = false;
        switch (setting.type) {
        case model::SettingType::Switcher:
            shown = std::holds_alternative<bool>(*value);
            break;
        case model::SettingType::Slider:
            shown = std::holds_alternative<float>(*value) && setting.valueOptions.max > setting.valueOptions.min;
            break;
        case model::SettingType::Stepper:
        case model::SettingType::Dropdown:
            shown = ui::chosenOption(store.options(setting), { .value = *value, .available = true, .reason = {} }).has_value();
            break;
        case model::SettingType::TextInput:
            shown = std::holds_alternative<std::string>(*value);
            break;
        }
        return shown ? value : std::nullopt;
    }

    /**
     * A value of a setting as the help line names it, its default or its saved value: on and off for a switcher, the
     * number of a slider as its display writes it, with its unit, and the text of the option of a stepper and a
     * dropdown. The text of a text input is in quotes, so an empty one shows, and a number is as its field shows it.
     * Empty for a value it has no name for, as one that is none of the options.
     */
    std::string valueName(const model::Setting& setting, const std::vector<model::Option>& options, const model::Value& value)
    {
        if (setting.type == model::SettingType::TextInput) {
            const auto* text = std::get_if<std::string>(&value);
            if (!text) {
                return {};
            }
            return setting.valueType == model::StoredType::String ? std::format("\"{}\"", *text) : ui::fieldText(setting, *text);
        }
        if (const auto* on = std::get_if<bool>(&value)) {
            return *on ? "on" : "off";
        }
        if (const auto* number = std::get_if<float>(&value)) {
            return ui::numberText(setting, *number);
        }
        const auto option = ui::chosenOption(options, { .value = value, .available = true, .reason = {} });
        return option ? options[*option].text : std::string();
    }

    /**
     * The parts of a help that are not empty, with a space between each two.
     */
    std::string joinHelp(const std::initializer_list<std::string_view> parts)
    {
        std::string help;
        for (const auto part : parts) {
            if (!part.empty()) {
                help += help.empty() ? "" : " ";
                help += part;
            }
        }
        return help;
    }

    /**
     * What the help line says of the number of a slider and of a text input after the row's help: a
     * percentage with the number it is of, and a number whose unit the box of a slider has no room for, with
     * its unit. Empty for any other value, and for one that is not available.
     */
    std::string numberDetail(const model::Setting& setting, const model::StoredValue& stored, const float sliderValue)
    {
        if (!stored.available) {
            return {};
        }
        std::optional<float> number;
        const auto* text = std::get_if<std::string>(&stored.value);
        if (setting.type == model::SettingType::Slider && std::holds_alternative<float>(stored.value)) {
            number = sliderValue;
        } else if (setting.type == model::SettingType::TextInput && setting.valueType != model::StoredType::String && text) {
            number = ui::fieldNumber(*text);
        }
        if (!number) {
            return {};
        }
        if (ui::isPercent(setting)) {
            return ui::percentDetail(setting, *number) + ".";
        }
        if (setting.type == model::SettingType::Slider && !ui::valueBoxText(setting, *number).withUnit) {
            return ui::numberText(setting, *number) + ".";
        }
        return {};
    }

}

namespace mods_config::ui
{
    /**
     * Draws and edits a setting row, including Reset, validation marks and help.
     * Editing continues for clipped rows so drags, text edits and dropdowns remain active.
     * Returns hover help and whether an input changed a value or began an interaction.
     */
    SettingRowDrawn drawSettingRow(const model::Setting& setting, const SettingRowContext& context)
    {
        SettingRowDrawn drawn;
        std::string help;
        std::string_view warning;
        bool invalid = false;
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const bool enabled = !setting.enabledWhen || model::holds(*setting.enabledWhen, context.store);
        const auto row = addRow(enabled && !setting.readOnly && !context.textInputs.isEdited(setting));
        if ((!enabled || setting.readOnly) && row.shown) {
            drawRowState();
        }
        const float right = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;

        auto stored = context.store ? context.store->value(setting) : model::StoredValue{};
        const bool editable = stored.available && enabled && !setting.readOnly;

        // a read-only setting whose storage has no value shows none
        const bool showsValue = stored.available || !setting.readOnly;

        // the slot after the control is Discard while the setting is dirty, and Reset for one with a default
        // otherwise. It is pressed a little beside it too, as a button of a stepper is. A press is its own when it
        // began there: a slider that is dragged and let go on it is not reset
        const auto& io = ImGui::GetIO();
        const auto slotBox = controlBox(right + theme::SPACE_2, pos.y, theme::CONTROL_HEIGHT);
        const auto defaultValue = stored.available ? defaultOf(setting, *context.store) : std::nullopt;
        bool dirty = stored.available && context.store->isDirty(setting);
        const bool onSlot = (dirty || defaultValue) && row.pointed && io.MousePos.x >= slotBox.Min.x - theme::SPACE_1;
        const bool slotHeld = onSlot && io.MouseClickedPos[ImGuiMouseButton_Left].x >= slotBox.Min.x - theme::SPACE_1;
        const auto resetsNow = [&] {
            return editable && !dirty && defaultValue && (stored.invalid || !isSameValue(stored.value, *defaultValue));
        };
        bool resets = resetsNow();
        if (slotHeld && row.pressed && ((editable && dirty) || resets)) {
            if (dirty) {
                context.store->discard(setting);
                drawn.pressActed = true;
            } else {
                drawn.pressActed = context.store->set(setting, *defaultValue);
            }
            stored = context.store->value(setting);
            dirty = context.store->isDirty(setting);
            resets = resetsNow();
        }
        const bool rowPressed = row.pressed && !onSlot;

        const auto& options = setting.valueOptions;
        const auto& choiceOptions = context.store ? context.store->options(setting) : options.options;
        const auto* on = std::get_if<bool>(&stored.value);
        const auto* number = std::get_if<float>(&stored.value);
        const bool isSwitcher = editable && setting.type == model::SettingType::Switcher && on;
        const bool isSlider = editable && setting.type == model::SettingType::Slider && number && options.max > options.min;
        const bool isChoice = editable && (setting.type == model::SettingType::Stepper || setting.type == model::SettingType::Dropdown) && !choiceOptions.empty();
        const bool isText = editable && setting.type == model::SettingType::TextInput && std::holds_alternative<std::string>(stored.value);

        // before the row is known to be shown: a slider that is dragged out of the page is still held
        float value = number ? *number : options.min;
        if (isSlider) {
            bool write = false;
            bool pulse = false;
            const SliderRow sliderRow{ .pointed = row.pointed, .started = row.started, .held = row.held };
            const float edited = context.sliders.edit(setting, right, pos.y, sliderRow, value, write, pulse);
            if (!write || context.store->set(setting, edited)) {
                value = edited;
            }
            drawn.pressActed = drawn.pressActed || pulse;
        }
        if (isSwitcher && rowPressed && context.store->set(setting, !*on)) {
            drawn.pressActed = true;
        }
        // before the row is known to be shown too: the list of a dropdown is not a part of its row
        auto chosen = chosenOption(choiceOptions, stored);
        const ChoiceRow choiceRow{ .pointed = isChoice && row.pointed, .held = isChoice && row.held, .pressed = isChoice && rowPressed };
        if (isChoice) {
            bool pulse = false;
            const auto picked = context.choices.edit(setting, choiceOptions, right, pos.y, choiceRow, chosen, pulse);
            if (picked && picked != chosen && context.store->set(setting, choiceOptions[*picked].value)) {
                chosen = picked;
                pulse = true;
            }
            drawn.pressActed = drawn.pressActed || pulse;
        }
        // before the row is known to be shown too: the edit lasts while the row is scrolled out of the page
        if (isText) {
            bool pulse = false;
            const auto typed = context.textInputs.edit(setting, right, pos.y, rowPressed, std::get<std::string>(stored.value), pulse);
            if (typed && context.store->set(setting, *typed)) {
                stored = context.store->value(setting);
                dirty = context.store->isDirty(setting);
                resets = resetsNow();
            }
            drawn.pressActed = drawn.pressActed || pulse;
        }
        // on the pending value, as it is after a press in this frame
        const auto warns = stored.invalid ? std::string_view() : warningOf(setting.warnings, context.store);
        const auto applies = context.store ? context.store->applies(setting) : std::nullopt;

        // a slider, a switcher and a choice can be set in this frame too
        dirty = stored.available && context.store->isDirty(setting);
        const bool discards = editable && dirty;

        // the ray is on the list then, and the help line keeps the row of its dropdown
        const bool listOpen = context.choices.isListOpen(setting);
        if (row.pointed || listOpen) {
            warning = warns;
            if (!stored.available && !stored.reason.empty()) {
                help = std::move(stored.reason);
            } else if (discards && onSlot) {
                const auto saved = context.store->savedValue(setting);
                const auto savedName = saved ? valueName(setting, choiceOptions, *saved) : std::string();
                help = savedName.empty() ? std::string("Discard the change.") : std::format("Discard the change, back to {}.", savedName);
            } else if (resets && onSlot) {
                help = std::format("Reset to {}", valueName(setting, choiceOptions, *defaultValue));
            } else if (!stored.reason.empty()) {
                help = std::move(stored.reason);
                invalid = stored.invalid;
            } else {
                const auto defaultText = defaultValue ? std::format("(Default: {})", valueName(setting, choiceOptions, *defaultValue)) : std::string();
                const auto ownHelp = setting.readOnly ? readOnlyHelp({}, setting.help) : setting.help;
                help = joinHelp({ ownHelp, numberDetail(setting, stored, value), defaultText });
            }
            // the tag of when it takes effect is told as it is changed
            drawn.pointed = PointedRow{ .text = setting.text, .help = std::move(help), .warning = warning, .invalid = invalid, .applies = dirty ? applies : std::nullopt };
        }
        if (!row.shown) {
            return drawn;
        }

        const auto look = stored.invalid ? ValueLook::Invalid : dirty ? ValueLook::Pending : ValueLook::Saved;
        float left = right;
        ImGui::BeginDisabled(!isSwitcher && !isSlider && !isChoice && !isText);
        switch (setting.type) {
        case model::SettingType::Switcher:
            left = drawSwitcher(right, pos.y, showsValue ? std::optional(on && *on) : std::nullopt, look);
            break;
        case model::SettingType::Slider:
            left = context.sliders.draw(setting, right, pos.y, showsValue ? std::optional(value) : std::nullopt, look);
            break;
        case model::SettingType::Stepper:
        case model::SettingType::Dropdown:
            left = context.choices.draw(setting, choiceOptions, right, pos.y, choiceRow, showsValue ? chosen : std::nullopt, look);
            break;
        case model::SettingType::TextInput: {
            // one whose value is not available has an empty box
            const auto* text = std::get_if<std::string>(&stored.value);
            left = context.textInputs.draw(setting, right, pos.y, stored.available && text ? fieldText(setting, *text) : std::string(), look);
            break;
        }
        }
        // an invalid value has its mark in the place of a warning's, which it has none of
        const RowMarks marks{ .pending = look == ValueLook::Pending,
            .applies = applies.value_or(model::Applies::Unknown),
            .mark = look == ValueLook::Invalid ? TextMark::Invalid
                    : !warns.empty()           ? TextMark::Warning
                                               : TextMark::None };
        drawLabel(pos, left - theme::SPACE_3, setting.text, setting.textRuns, marks);
        ImGui::EndDisabled();
        // a dirty setting that is disabled has its Discard disabled
        if (dirty || defaultValue) {
            const bool active = discards || (!dirty && resets);
            ImGui::BeginDisabled(!active);
            drawButtonFrame(slotBox, active && onSlot, active && onSlot && slotHeld && row.held);
            if (dirty) {
                drawDiscardIcon(slotBox.GetCenter());
            } else {
                drawResetIcon(slotBox.GetCenter());
            }
            ImGui::EndDisabled();
        }
        return drawn;
    }

}
