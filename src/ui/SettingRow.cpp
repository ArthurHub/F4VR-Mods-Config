#include "SettingRow.h"

#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <imgui.h>

#include "Bindings.h"
#include "Colors.h"
#include "Drawing.h"
#include "Haptics.h"
#include "Numbers.h"
#include "RowView.h"
#include "Theme.h"
#include "Transforms.h"
#include "common/CommonUtils.h"
#include "model/ChoiceOptions.h"
#include "model/Conditions.h"
#include "model/FileChoices.h"
#include "model/Haptics.h"
#include "model/SettingValues.h"
#include "model/TextStates.h"

namespace
{
    using namespace mods_config;

    /**
     * What the help line says of an overridden setting, before its warning: who overrides it, the value in effect as
     * `nameOf` names it, or as the mod's client gave it where it has no name, the others under it, and that a change
     * applies once they end. Empty for a setting with no override.
     */
    std::string overriddenText(const std::vector<model::Override>& overrides, const std::function<std::string(const model::Value&)>& nameOf)
    {
        if (overrides.empty()) {
            return {};
        }
        const auto& first = overrides.front();
        auto name = first.value ? nameOf(*first.value) : std::string();
        if (name.empty()) {
            name = first.text;
        }
        auto text = std::format("Overridden by {}{}.", first.owner, name.empty() ? std::string() : ", in effect: " + name);
        if (overrides.size() == 1) {
            return text + std::format(" A change is saved, and applies once {} stops setting it.", first.owner);
        }
        text += " Also set by ";
        for (std::size_t i = 1; i < overrides.size(); ++i) {
            text += (i > 1 ? ", " : "") + overrides[i].owner;
        }
        return text + ". A change is saved, and applies once none of them sets it.";
    }

    /**
     * A value of a setting as the help line names it, its default or its saved value: on and off for a switcher, the
     * number of a slider as its display writes it, with its unit, and the text of the option of a stepper and a
     * dropdown. The text of a text input is in quotes, so an empty one shows, and a number is as its field shows it.
     * A binding is its sentence, with the player's handedness, and a color and a transform their numbers. Empty for a
     * value it has no name for, as one that is none of the options.
     */
    std::string valueName(const model::Setting& setting, const std::vector<model::Option>& options, const model::Value& value, const bool leftHanded)
    {
        if (setting.type == model::SettingType::InputBinding) {
            return ui::bindingValueName(setting, value, leftHanded);
        }
        if (setting.type == model::SettingType::Color) {
            return ui::colorValueName(setting, value);
        }
        if (setting.type == model::SettingType::Transform) {
            return ui::transformValueName(setting, value);
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
     * The pattern a value of a haptic plays, and none for None and for a value that is no haptic.
     */
    std::optional<vrcf::HapticPattern> hapticPattern(const model::Value& value)
    {
        const auto* haptic = model::findHaptic(value);
        return haptic ? haptic->pattern : std::nullopt;
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
            view = ui::rowViewOf(setting, context.store);
            row = ui::addRow(view.enabled && !setting.readOnly && !context.textInputs.isEdited(setting));
            right = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;
            showsValue = view.stored.available || !setting.readOnly;

            // the slot is pressed a little beside it too, as a button of a stepper is. A press is its own when it began
            // there: a slider that is dragged and let go on it is not reset
            const auto& io = ImGui::GetIO();
            slotBox = controls::controlBox(right + theme::SPACE_2, pos.y, theme::CONTROL_HEIGHT);
            onSlot = (view.dirty || view.resetValue) && row.pointed && io.MousePos.x >= slotBox.Min.x - theme::SPACE_1;
            slotHeld = onSlot && io.MouseClickedPos[ImGuiMouseButton_Left].x >= slotBox.Min.x - theme::SPACE_1;
            onPlay = setting.type == model::SettingType::Haptic && row.pointed && !onSlot && ui::HapticBoxes(right, pos.y).onPlay(io.MousePos.x);
        }

        /**
         * Reads the row view again after a change set in the store.
         */
        void refresh()
        {
            view = ui::rowViewOf(setting, context.store);
        }

        /**
         * Whether the slot is the Discard of a change that can be taken back.
         */
        bool discards() const
        {
            return view.editable && view.dirty;
        }

        const model::Setting& setting;
        const ui::SettingRowContext& context;
        const std::vector<model::Option>& choiceOptions;
        ImVec2 pos;
        float width = 0.0f;

        // where the control ends on the right, before the slot
        float right = 0.0f;
        ui::Row row;

        // how the row looks, on its value as it is after a press in this frame
        ui::RowView view;

        // a read-only setting whose storage has no value shows none
        bool showsValue = false;
        ImRect slotBox;
        bool onSlot = false;
        bool slotHeld = false;

        // the ray is on the Play button of a haptic
        bool onPlay = false;
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

        // the control is a haptic that can be edited, whose Play button plays its value
        bool haptic = false;

        // the control is a color that can be edited, in its editor
        bool color = false;

        // the control is a transform that can be edited, in its editor
        bool transform = false;

        // the value of a slider, as a drag has it
        float sliderValue = 0.0f;
        ui::SliderRow sliderRow;
        std::optional<std::size_t> chosen;
        ui::ChoiceRow choiceRow;
        ui::BindingEdited bindingEdited;
        ui::ColorEdited colorEdited;
        ui::TransformEdited transformEdited;

        // a press changed a value or began an interaction
        bool pulse = false;

        // the pattern a press played
        std::optional<vrcf::HapticPattern> played;
    };

    /**
     * Discards a dirty setting's change, or sets a clean one's default, on a press that began on the slot.
     * Returns whether the press acted.
     */
    bool pressSlot(SettingRowState& state)
    {
        if (!state.slotHeld || !state.row.pressed || !(state.discards() || state.view.resets)) {
            return false;
        }
        bool acted = true;
        if (state.view.dirty) {
            state.context.store->discard(state.setting);
        } else {
            acted = state.context.store->set(state.setting, *state.view.resetValue);
        }
        state.refresh();
        return acted;
    }

    /**
     * Edits the control of a setting row with the pointer, and sets a change in the store. It runs before the row is
     * known to be shown: a slider that is dragged out of the page is still held, the list of a dropdown and the editor
     * of a binding are not a part of their row, and the edit of a text lasts while its row is scrolled out of the page.
     * A haptic plays its value as its Play button is pressed, and an option as it is picked.
     */
    ControlEdited editControl(SettingRowState& state)
    {
        const auto& setting = state.setting;
        const auto& context = state.context;
        const auto& options = setting.valueOptions;
        const auto* on = std::get_if<bool>(&state.view.stored.value);
        const auto* number = std::get_if<float>(&state.view.stored.value);
        const bool shown = state.view.editable && model::isControlValue(setting.type, state.view.stored.value);
        const bool isSwitcher = shown && setting.type == model::SettingType::Switcher;
        const bool isSlider = shown && setting.type == model::SettingType::Slider && options.max > options.min;
        const bool isHaptic = state.view.editable && setting.type == model::SettingType::Haptic;
        const bool isChoice = state.view.editable && model::isChoice(setting.type) && !state.choiceOptions.empty();
        const bool isText = shown && setting.type == model::SettingType::TextInput;
        const bool isBinding = shown && setting.type == model::SettingType::InputBinding;
        const bool isColor = shown && setting.type == model::SettingType::Color;
        const bool isTransform = shown && setting.type == model::SettingType::Transform;
        const bool rowPressed = state.row.pressed && !state.onSlot;

        ControlEdited edited{ .editable = isSwitcher || isSlider || isChoice || isText || isBinding || isColor || isTransform,
            .binding = isBinding,
            .haptic = isHaptic && isChoice,
            .color = isColor,
            .transform = isTransform,
            .sliderValue = number ? *number : options.min,
            .chosen = ui::chosenOption(state.choiceOptions, state.view.stored),
            .choiceRow = { .pointed = isChoice && state.row.pointed, .held = isChoice && state.row.held, .pressed = isChoice && rowPressed } };
        if (isSlider) {
            bool write = false;
            bool pulse = false;
            edited.sliderRow = { .pointed = state.row.pointed, .started = state.row.started, .held = state.row.held };
            const float value = context.sliders.edit(setting, state.right, state.pos.y, edited.sliderRow, edited.sliderValue, write, pulse);
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
            // the place after the options is Type your own, which starts the edit of a text in the dropdown's box
            const bool typesNew = picked == state.choiceOptions.size();
            if (picked && !typesNew && picked != edited.chosen && context.store->set(setting, state.choiceOptions[*picked].value)) {
                edited.chosen = picked;
                pulse = true;
            }
            if (options.typedOption) {
                const auto typed = context.textInputs.edit(setting, state.right, state.pos.y, typesNew, std::string(), pulse);
                const auto value = typed ? model::typedOptionValue(*typed, state.choiceOptions) : std::nullopt;
                if (value && context.store->set(setting, *value)) {
                    state.refresh();
                    edited.chosen = ui::chosenOption(state.choiceOptions, state.view.stored);
                }
            }
            // a haptic plays the option picked, also the one it is at already, and its value on Play
            if (edited.haptic && picked) {
                edited.played = hapticPattern(state.choiceOptions[*picked].value);
            } else if (edited.haptic && rowPressed && state.onPlay) {
                edited.played = hapticPattern(state.view.stored.value);
            }
            edited.pulse = edited.pulse || pulse || edited.played.has_value();
        }
        if (isText) {
            bool pulse = false;
            const auto typed = context.textInputs.edit(setting, state.right, state.pos.y, rowPressed, std::get<std::string>(state.view.stored.value), pulse);
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
        if (isColor) {
            const auto rowBox = ImRect(state.pos, ImVec2(state.pos.x + state.width, state.pos.y + theme::CONTROL_HEIGHT));
            edited.colorEdited = context.colorEditor.edit(setting, *context.store, rowBox, rowPressed);
            state.refresh();
            edited.pulse = edited.pulse || edited.colorEdited.pulse;
        }
        if (isTransform) {
            const auto rowBox = ImRect(state.pos, ImVec2(state.pos.x + state.width, state.pos.y + theme::CONTROL_HEIGHT));
            edited.transformEdited = context.transformEditor.edit(setting, *context.store, rowBox, rowPressed);
            state.refresh();
            edited.pulse = edited.pulse || edited.transformEdited.pulse;
        }
        return edited;
    }

    /**
     * What the help line says of a setting row the ray is on, on its value as it is after a press in this frame: why
     * the value is not available, what the slot does while the ray is on it, why the value is invalid, or the row's
     * help, with its number and its default, after who overrides it. On a haptic's Play button it is
     * what Play plays, and while its list is open it is the help of the option the ray is on: a haptic's and a
     * file's always, and any other's where the option has a help of its own.
     */
    ui::PointedRow pointedRow(SettingRowState& state, const ControlEdited& edited)
    {
        const auto& setting = state.setting;
        const auto& context = state.context;
        auto& stored = state.view.stored;
        const auto nameOf = [&](const model::Value& value) {
            return valueName(setting, state.choiceOptions, value, context.leftHanded);
        };
        if (edited.haptic && context.choices.isListOpen(setting)) {
            if (const auto option = context.choices.pointedOption(); option && *option < state.choiceOptions.size()) {
                const auto* haptic = model::findHaptic(state.choiceOptions[*option].value);
                return ui::PointedRow{ .text = state.choiceOptions[*option].text, .help = haptic ? model::hapticHelp(*haptic) : std::string() };
            }
        }
        // a file of a choice of files is named in full, with its folder
        if (const auto& files = setting.valueOptions.folderOptions; files && context.choices.isListOpen(setting)) {
            if (const auto option = context.choices.pointedOption(); option && *option < state.choiceOptions.size()) {
                const auto& pointed = state.choiceOptions[*option];
                const bool isNone = !files->noneText.empty() && *option == 0;
                return ui::PointedRow{ .text = pointed.text, .help = isNone ? std::string() : std::format("In {}.", model::folderText(*files)) };
            }
        }
        if (setting.valueOptions.typedOption && context.choices.isListOpen(setting) && context.choices.pointedOption() == state.choiceOptions.size()) {
            return ui::PointedRow{ .text = ui::TYPED_OPTION_TEXT, .help = ui::TYPED_OPTION_HELP };
        }
        if (context.choices.isListOpen(setting)) {
            if (const auto option = context.choices.pointedOption(); option && *option < state.choiceOptions.size() && !state.choiceOptions[*option].help.empty()) {
                const auto& pointed = state.choiceOptions[*option];
                return ui::PointedRow{ .text = pointed.text, .help = pointed.help };
            }
        }
        std::string help;
        bool invalid = false;
        const auto playing = edited.haptic && state.onPlay ? hapticPattern(stored.value) : std::nullopt;
        if (!stored.available && !stored.reason.empty()) {
            help = std::move(stored.reason);
        } else if (!state.view.limit.empty()) {
            help = state.view.limit;
        } else if (state.discards() && state.onSlot) {
            const auto saved = context.store->savedValue(setting);
            const auto savedName = saved ? nameOf(*saved) : std::string();
            const auto* discard = context.store->writesChangesAtOnce() ? "Revert" : "Discard";
            help = savedName.empty() ? std::format("{} the change.", discard) : std::format("{} the change, back to {}.", discard, savedName);
        } else if (state.view.resets && state.onSlot) {
            help = std::format("Reset to {}", nameOf(*state.view.resetValue));
        } else if (playing) {
            help = std::format("Plays {} on the hand that points.", nameOf(stored.value));
        } else if (!stored.reason.empty()) {
            help = std::move(stored.reason);
            invalid = stored.invalid;
            if (edited.binding && invalid) {
                help += state.view.resetValue ? std::format(" Change the binding, or Reset it to {}.", nameOf(*state.view.resetValue)) : std::string(" Change the binding.");
            }
            if (edited.haptic && invalid) {
                help += state.view.resetValue ? std::format(" Pick one, or Reset it to {}.", nameOf(*state.view.resetValue)) : std::string(" Pick one.");
            }
            if (edited.color && invalid) {
                help += state.view.resetValue ? std::format(" Pick a color, or Reset it to {}.", nameOf(*state.view.resetValue)) : std::string(" Pick a color.");
            }
            if (edited.transform && invalid) {
                help += state.view.resetValue ? std::format(" Change the transform, or Reset it to {}.", nameOf(*state.view.resetValue)) : std::string(" Change the transform.");
            }
        } else {
            const auto defaultText = state.view.resetValue ? std::format("(Default: {})", nameOf(*state.view.resetValue)) : std::string();
            const auto settingHelp = model::resolveClientText(setting.help, context.store);
            const auto ownHelp = setting.readOnly ? ui::readOnlyHelp({}, settingHelp) : settingHelp;
            // a binding is said in full first, as its row has no room for it
            const auto sentence = edited.binding ? nameOf(stored.value) + "." : std::string();
            help = joinHelp({ sentence, ownHelp, numberDetail(setting, stored, edited.sliderValue), defaultText });
        }
        // the tag of when it takes effect is told as it is changed
        return ui::PointedRow{ .text = model::resolveClientText(setting.text, context.store),
            .help = std::move(help),
            .warning = state.view.warning,
            .invalid = invalid,
            .applies = state.view.applies,
            .overridden = overriddenText(stored.overrides, nameOf) };
    }

    /**
     * Draws the control of a setting row in the look of its value, disabled where it cannot be edited, and the row's
     * text with its marks on the left of it.
     */
    void drawControl(const SettingRowState& state, const ControlEdited& edited)
    {
        const auto& setting = state.setting;
        const auto& context = state.context;
        const auto& stored = state.view.stored;
        const auto look = state.view.look;
        const float right = state.right;
        const float top = state.pos.y;
        float left = right;
        ImGui::BeginDisabled(!edited.editable);
        switch (setting.type) {
        case model::SettingType::Switcher: {
            const auto* on = std::get_if<bool>(&stored.value);
            left = controls::drawSwitcher(right, top, state.showsValue ? std::optional(on && *on) : std::nullopt, toneOf(look));
            break;
        }
        case model::SettingType::Slider:
            left = context.sliders.draw(setting, right, top, edited.sliderRow, state.showsValue ? std::optional(edited.sliderValue) : std::nullopt, look);
            break;
        case model::SettingType::Stepper:
        case model::SettingType::Dropdown: {
            // a dropdown whose new value is typed has the field of the text in its box, which its edit draws
            if (context.textInputs.isEdited(setting)) {
                left = context.textInputs.draw(setting, right, top, {}, look);
                break;
            }
            // a choice of files shows the name of a file its folder does not have, and that it has none
            std::string unchosen;
            if (setting.valueOptions.folderOptions) {
                const auto* text = std::get_if<std::string>(&stored.value);
                unchosen = stored.available && text ? model::fileNameOf(*text) : state.choiceOptions.empty() ? std::string("No files") : std::string();
            }
            left = context.choices.draw(setting, state.choiceOptions, right, top, edited.choiceRow, state.showsValue ? edited.chosen : std::nullopt, look, unchosen);
            break;
        }
        case model::SettingType::Haptic: {
            // a name the framework does not have shows as its file has it
            const auto* text = std::get_if<std::string>(&stored.value);
            const auto unchosen = stored.available && text ? std::string_view(*text) : std::string_view();
            left = context.choices.draw(setting, state.choiceOptions, right, top, edited.choiceRow, state.showsValue ? edited.chosen : std::nullopt, look, unchosen);
            break;
        }
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
            left = ui::drawBinding(setting, right, top, stored.available ? text : nullptr, look, editorOpen, context.leftHanded, context.imageDrawer);
            break;
        }
        case model::SettingType::Color: {
            // one whose value is not available has an empty frame
            const auto* text = std::get_if<std::string>(&stored.value);
            left = ui::drawColor(setting, right, top, stored.available ? text : nullptr, look, context.colorEditor.isOpen(setting));
            break;
        }
        case model::SettingType::Transform: {
            // one whose value is not available has an empty frame
            const auto* text = std::get_if<std::string>(&stored.value);
            left = ui::drawTransform(setting, right, top, stored.available ? text : nullptr, look, context.transformEditor.isOpen(setting));
            break;
        }
        }
        const ui::RowMarks marks{ .pending = look == ui::ValueLook::Pending, .applies = state.view.applies.value_or(model::Applies::Unknown), .mark = state.view.mark };
        ui::drawLabel(state.pos,
            left - theme::SPACE_3,
            model::resolveClientText(setting.text, context.store),
            model::resolveClientRuns(setting.textRuns, context.store),
            marks,
            context.query);
        ImGui::EndDisabled();
    }

    /**
     * Draws the slot after the control of a setting row: Discard while the setting is dirty, and Reset for one with a
     * default otherwise. A dirty setting that is disabled has its Discard disabled.
     */
    void drawSlot(const SettingRowState& state)
    {
        if (!state.view.dirty && !state.view.resetValue) {
            return;
        }
        const bool active = state.discards() || (!state.view.dirty && state.view.resets);
        ImGui::BeginDisabled(!active);
        controls::drawButtonFrame(state.slotBox, active && state.onSlot, active && state.onSlot && state.slotHeld && state.row.held);
        if (state.view.dirty) {
            controls::drawUndoIcon(state.slotBox.GetCenter());
        } else {
            controls::drawResetIcon(state.slotBox.GetCenter());
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
        if ((!state.view.enabled || setting.readOnly) && state.row.shown) {
            controls::drawRowState();
        }
        SettingRowDrawn drawn;
        drawn.pressActed = pressSlot(state);
        auto edited = editControl(state);
        drawn.pressActed = drawn.pressActed || edited.pulse;
        drawn.haptic = edited.played;
        drawn.adjust = edited.transformEdited.adjust ? &setting : nullptr;

        // on the pending value, as it is after a press in this frame: a slider, a switcher and a choice can be set in
        // this frame too
        state.refresh();

        // the ray is on the list then, and the help line keeps the row of its dropdown, and so it does while the editor of
        // a binding is open, where the ray is on none of its parts
        if (state.row.pointed || context.choices.isListOpen(setting) || context.bindingEditor.isOpen(setting) || context.colorEditor.isOpen(setting) ||
            context.transformEditor.isOpen(setting)) {
            drawn.pointed = pointedRow(state, edited);
        }
        if (edited.bindingEdited.pointed) {
            drawn.pointed = std::move(edited.bindingEdited.pointed);
        }
        if (edited.colorEdited.pointed) {
            drawn.pointed = std::move(edited.colorEdited.pointed);
        }
        if (edited.transformEdited.pointed) {
            drawn.pointed = std::move(edited.transformEdited.pointed);
        }
        if (!state.row.shown) {
            return drawn;
        }
        drawControl(state, edited);
        drawSlot(state);
        return drawn;
    }

}
