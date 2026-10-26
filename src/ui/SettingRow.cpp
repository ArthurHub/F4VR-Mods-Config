#include "SettingRow.h"

#include <array>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <imgui.h>

#include "Bindings.h"
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
        case model::SettingType::InputBinding:
            shown = std::holds_alternative<std::string>(*value);
            break;
        }
        return shown ? value : std::nullopt;
    }

    /**
     * A value of a setting as the help line names it, its default or its saved value: on and off for a switcher, the
     * number of a slider as its display writes it, with its unit, and the text of the option of a stepper and a
     * dropdown. The text of a text input is in quotes, so an empty one shows, and a number is as its field shows it.
     * A binding is its sentence, with the player's handedness. Empty for a value it has no name for, as one that is
     * none of the options.
     */
    std::string valueName(const model::Setting& setting, const std::vector<model::Option>& options, const model::Value& value, const bool leftHanded)
    {
        if (setting.type == model::SettingType::InputBinding) {
            return ui::bindingValueName(value, leftHanded);
        }
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

    /**
     * A setting row in the frame that is drawn: where it is, the pointer on it, and its value in the store, which a
     * press in this frame can change. The slot after its control is Discard while the setting is dirty, and Reset for
     * one with a default otherwise.
     */
    struct SettingRowState
    {
        /**
         * Adds the row to the page, and reads its value, its default and where the pointer is on it.
         */
        SettingRowState(const model::Setting& rowSetting, const ui::SettingRowContext& rowContext)
            : setting(rowSetting),
              context(rowContext),
              choiceOptions(rowContext.store ? rowContext.store->options(rowSetting) : rowSetting.valueOptions.options)
        {
            pos = ImGui::GetCursorScreenPos();
            width = ui::rowWidth();
            enabled = !setting.enabledWhen || model::holds(*setting.enabledWhen, context.store);
            row = ui::addRow(enabled && !setting.readOnly && !context.textInputs.isEdited(setting));
            right = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;
            stored = context.store ? context.store->value(setting) : model::StoredValue{};
            editable = stored.available && enabled && !setting.readOnly;
            showsValue = stored.available || !setting.readOnly;

            // the slot is pressed a little beside it too, as a button of a stepper is. A press is its own when it began
            // there: a slider that is dragged and let go on it is not reset
            const auto& io = ImGui::GetIO();
            slotBox = ui::controlBox(right + theme::SPACE_2, pos.y, theme::CONTROL_HEIGHT);
            defaultValue = stored.available ? defaultOf(setting, *context.store) : std::nullopt;
            dirty = stored.available && context.store->isDirty(setting);
            onSlot = (dirty || defaultValue) && row.pointed && io.MousePos.x >= slotBox.Min.x - theme::SPACE_1;
            slotHeld = onSlot && io.MouseClickedPos[ImGuiMouseButton_Left].x >= slotBox.Min.x - theme::SPACE_1;
            resets = resetsNow();
        }

        /**
         * Reads the value again after a change set in the store, with whether it is dirty and Reset can set it.
         */
        void refresh()
        {
            stored = context.store->value(setting);
            dirty = stored.available && context.store->isDirty(setting);
            resets = resetsNow();
        }

        /**
         * Whether Reset can set the default now: on a clean value that is not the default already.
         */
        bool resetsNow() const
        {
            return editable && !dirty && defaultValue && (stored.invalid || !isSameValue(stored.value, *defaultValue));
        }

        /**
         * Whether the slot is the Discard of a change that can be taken back.
         */
        bool discards() const
        {
            return editable && dirty;
        }

        const model::Setting& setting;
        const ui::SettingRowContext& context;
        const std::vector<model::Option>& choiceOptions;
        ImVec2 pos;
        float width = 0.0f;

        // where the control ends on the right, before the slot
        float right = 0.0f;
        ui::Row row;
        bool enabled = false;
        model::StoredValue stored;
        bool editable = false;

        // a read-only setting whose storage has no value shows none
        bool showsValue = false;
        ImRect slotBox;
        std::optional<model::Value> defaultValue;
        bool dirty = false;
        bool onSlot = false;
        bool slotHeld = false;
        bool resets = false;
    };

    /**
     * What the control of a setting row did in the frame that is drawn, which its help and its drawing read.
     */
    struct ControlEdited
    {
        // the control can be edited, so it is drawn enabled
        bool editable = false;

        // the control is a binding that can be edited, which its help says in full
        bool binding = false;

        // the value of a slider, as a drag has it
        float sliderValue = 0.0f;
        std::optional<std::size_t> chosen;
        ui::ChoiceRow choiceRow;
        ui::BindingEdited bindingEdited;

        // a press changed a value or began an interaction
        bool pulse = false;
    };

    /**
     * Discards a dirty setting's change, or sets a clean one's default, on a press that began on the slot.
     * Returns whether the press acted.
     */
    bool pressSlot(SettingRowState& state)
    {
        if (!state.slotHeld || !state.row.pressed || !(state.discards() || state.resets)) {
            return false;
        }
        bool acted = true;
        if (state.dirty) {
            state.context.store->discard(state.setting);
        } else {
            acted = state.context.store->set(state.setting, *state.defaultValue);
        }
        state.refresh();
        return acted;
    }

    /**
     * Edits the control of a setting row with the pointer, and sets a change in the store. It runs before the row is
     * known to be shown: a slider that is dragged out of the page is still held, the list of a dropdown and the editor
     * of a binding are not a part of their row, and the edit of a text lasts while its row is scrolled out of the page.
     */
    ControlEdited editControl(SettingRowState& state)
    {
        const auto& setting = state.setting;
        const auto& context = state.context;
        const auto& options = setting.valueOptions;
        const auto* on = std::get_if<bool>(&state.stored.value);
        const auto* number = std::get_if<float>(&state.stored.value);
        const bool isSwitcher = state.editable && setting.type == model::SettingType::Switcher && on;
        const bool isSlider = state.editable && setting.type == model::SettingType::Slider && number && options.max > options.min;
        const bool isChoice = state.editable && (setting.type == model::SettingType::Stepper || setting.type == model::SettingType::Dropdown) && !state.choiceOptions.empty();
        const bool isText = state.editable && setting.type == model::SettingType::TextInput && std::holds_alternative<std::string>(state.stored.value);
        const bool isBinding = state.editable && setting.type == model::SettingType::InputBinding && std::holds_alternative<std::string>(state.stored.value);
        const bool rowPressed = state.row.pressed && !state.onSlot;

        ControlEdited edited{ .editable = isSwitcher || isSlider || isChoice || isText || isBinding,
            .binding = isBinding,
            .sliderValue = number ? *number : options.min,
            .chosen = ui::chosenOption(state.choiceOptions, state.stored),
            .choiceRow = { .pointed = isChoice && state.row.pointed, .held = isChoice && state.row.held, .pressed = isChoice && rowPressed } };
        if (isSlider) {
            bool write = false;
            bool pulse = false;
            const ui::SliderRow sliderRow{ .pointed = state.row.pointed, .started = state.row.started, .held = state.row.held };
            const float value = context.sliders.edit(setting, state.right, state.pos.y, sliderRow, edited.sliderValue, write, pulse);
            if (!write || context.store->set(setting, value)) {
                edited.sliderValue = value;
            }
            edited.pulse = edited.pulse || pulse;
        }
        if (isSwitcher && rowPressed && context.store->set(setting, !*on)) {
            edited.pulse = true;
        }
        if (isChoice) {
            bool pulse = false;
            const auto picked = context.choices.edit(setting, state.choiceOptions, state.right, state.pos.y, edited.choiceRow, edited.chosen, pulse);
            if (picked && picked != edited.chosen && context.store->set(setting, state.choiceOptions[*picked].value)) {
                edited.chosen = picked;
                pulse = true;
            }
            edited.pulse = edited.pulse || pulse;
        }
        if (isText) {
            bool pulse = false;
            const auto typed = context.textInputs.edit(setting, state.right, state.pos.y, rowPressed, std::get<std::string>(state.stored.value), pulse);
            if (typed && context.store->set(setting, *typed)) {
                state.refresh();
            }
            edited.pulse = edited.pulse || pulse;
        }
        if (isBinding) {
            const auto rowBox = ImRect(state.pos, ImVec2(state.pos.x + state.width, state.pos.y + theme::CONTROL_HEIGHT));
            edited.bindingEdited = context.bindingEditor.edit(setting, *context.store, rowBox, rowPressed, context.leftHanded, context.imageDrawer);
            state.refresh();
            edited.pulse = edited.pulse || edited.bindingEdited.pulse;
        }
        return edited;
    }

    /**
     * What the help line says of a setting row the ray is on, on its value as it is after a press in this frame: why
     * the value is not available, what the slot does while the ray is on it, why the value is invalid, or the row's
     * help, with its number, the bindings it is the same press as, and its default.
     */
    ui::PointedRow pointedRow(SettingRowState& state, const ControlEdited& edited, const std::string_view warning, const std::optional<model::Applies> applies)
    {
        const auto& setting = state.setting;
        const auto& context = state.context;
        auto& stored = state.stored;
        const auto nameOf = [&](const model::Value& value) {
            return valueName(setting, state.choiceOptions, value, context.leftHanded);
        };
        std::string help;
        bool invalid = false;
        if (!stored.available && !stored.reason.empty()) {
            help = std::move(stored.reason);
        } else if (state.discards() && state.onSlot) {
            const auto saved = context.store->savedValue(setting);
            const auto savedName = saved ? nameOf(*saved) : std::string();
            help = savedName.empty() ? std::string("Discard the change.") : std::format("Discard the change, back to {}.", savedName);
        } else if (state.resets && state.onSlot) {
            help = std::format("Reset to {}", nameOf(*state.defaultValue));
        } else if (!stored.reason.empty()) {
            help = std::move(stored.reason);
            invalid = stored.invalid;
            if (edited.binding && invalid) {
                help += state.defaultValue ? std::format(" Change the binding, or Reset it to {}.", nameOf(*state.defaultValue)) : std::string(" Change the binding.");
            }
        } else {
            const auto defaultText = state.defaultValue ? std::format("(Default: {})", nameOf(*state.defaultValue)) : std::string();
            const auto ownHelp = setting.readOnly ? ui::readOnlyHelp({}, setting.help) : setting.help;
            // a binding is said in full first, as its row has no room for it, and names the bindings it is the same
            // press as
            const auto sentence = edited.binding ? nameOf(stored.value) + "." : std::string();
            const auto samePress = edited.binding ? ui::samePressHelp(setting, context.bindings, *context.store, context.leftHanded) : std::string();
            help = joinHelp({ sentence, ownHelp, numberDetail(setting, stored, edited.sliderValue), samePress, defaultText });
        }
        // the tag of when it takes effect is told as it is changed
        return ui::PointedRow{ .text = setting.text, .help = std::move(help), .warning = warning, .invalid = invalid, .applies = state.dirty ? applies : std::nullopt };
    }

    /**
     * Draws the control of a setting row in the look of its value, disabled where it cannot be edited, and the row's
     * text with its marks on the left of it.
     */
    void drawControl(const SettingRowState& state, const ControlEdited& edited, const std::string_view warning, const std::optional<model::Applies> applies)
    {
        const auto& setting = state.setting;
        const auto& context = state.context;
        const auto& stored = state.stored;
        const auto look = stored.invalid ? ui::ValueLook::Invalid : state.dirty ? ui::ValueLook::Pending : ui::ValueLook::Saved;
        const float right = state.right;
        const float top = state.pos.y;
        float left = right;
        ImGui::BeginDisabled(!edited.editable);
        switch (setting.type) {
        case model::SettingType::Switcher: {
            const auto* on = std::get_if<bool>(&stored.value);
            left = ui::drawSwitcher(right, top, state.showsValue ? std::optional(on && *on) : std::nullopt, look);
            break;
        }
        case model::SettingType::Slider:
            left = context.sliders.draw(setting, right, top, state.showsValue ? std::optional(edited.sliderValue) : std::nullopt, look);
            break;
        case model::SettingType::Stepper:
        case model::SettingType::Dropdown:
            left = context.choices.draw(setting, state.choiceOptions, right, top, edited.choiceRow, state.showsValue ? edited.chosen : std::nullopt, look);
            break;
        case model::SettingType::TextInput: {
            // one whose value is not available has an empty box
            const auto* text = std::get_if<std::string>(&stored.value);
            left = context.textInputs.draw(setting, right, top, stored.available && text ? ui::fieldText(setting, *text) : std::string(), look);
            break;
        }
        case model::SettingType::InputBinding: {
            // one whose value is not available has an empty frame
            const auto* text = std::get_if<std::string>(&stored.value);
            const bool editorOpen = context.bindingEditor.isOpen(setting);
            left = ui::drawBinding(right, top, stored.available ? text : nullptr, look, editorOpen, context.leftHanded, context.imageDrawer);
            break;
        }
        }
        // an invalid value has its mark in the place of a warning's, which it has none of
        const ui::RowMarks marks{ .pending = look == ui::ValueLook::Pending,
            .applies = applies.value_or(model::Applies::Unknown),
            .mark = look == ui::ValueLook::Invalid ? ui::TextMark::Invalid
                    : !warning.empty()             ? ui::TextMark::Warning
                                                   : ui::TextMark::None };
        ui::drawLabel(state.pos, left - theme::SPACE_3, setting.text, setting.textRuns, marks);
        ImGui::EndDisabled();
    }

    /**
     * Draws the slot after the control of a setting row: Discard while the setting is dirty, and Reset for one with a
     * default otherwise. A dirty setting that is disabled has its Discard disabled.
     */
    void drawSlot(const SettingRowState& state)
    {
        if (!state.dirty && !state.defaultValue) {
            return;
        }
        const bool active = state.discards() || (!state.dirty && state.resets);
        ImGui::BeginDisabled(!active);
        ui::drawButtonFrame(state.slotBox, active && state.onSlot, active && state.onSlot && state.slotHeld && state.row.held);
        if (state.dirty) {
            drawDiscardIcon(state.slotBox.GetCenter());
        } else {
            drawResetIcon(state.slotBox.GetCenter());
        }
        ImGui::EndDisabled();
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
        SettingRowState state(setting, context);
        if ((!state.enabled || setting.readOnly) && state.row.shown) {
            drawRowState();
        }
        SettingRowDrawn drawn;
        drawn.pressActed = pressSlot(state);
        auto edited = editControl(state);
        drawn.pressActed = drawn.pressActed || edited.pulse;

        // on the pending value, as it is after a press in this frame: a slider, a switcher and a choice can be set in
        // this frame too
        state.dirty = state.stored.available && context.store->isDirty(setting);
        const auto warning = state.stored.invalid ? std::string_view() : warningOf(setting.warnings, context.store);
        const auto applies = context.store ? context.store->applies(setting) : std::nullopt;

        // the ray is on the list then, and the help line keeps the row of its dropdown, and so it does while the editor of
        // a binding is open, where the ray is on none of its parts
        if (state.row.pointed || context.choices.isListOpen(setting) || context.bindingEditor.isOpen(setting)) {
            drawn.pointed = pointedRow(state, edited, warning, applies);
        }
        if (edited.bindingEdited.pointed) {
            drawn.pointed = std::move(edited.bindingEdited.pointed);
        }
        if (!state.row.shown) {
            return drawn;
        }
        drawControl(state, edited, warning, applies);
        drawSlot(state);
        return drawn;
    }

}
