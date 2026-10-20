#include "ModPage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "Choices.h"
#include "Drawing.h"
#include "Numbers.h"
#include "Sections.h"
#include "Sliders.h"
#include "Theme.h"
#include "common/CommonUtils.h"
#include "model/Conditions.h"

namespace
{
    using namespace mods_config;

    /**
     * The width of a row at the cursor: the same whether or not the page's scroll bar shows. A row of a section
     * in a section is set in, and ends where the other rows do.
     */
    float rowWidth()
    {
        const float indent = ImGui::GetCursorPosX() - ImGui::GetCursorStartPos().x;
        return ImGui::GetWindowWidth() - theme::SCROLLBAR_SIZE - theme::SPACE_2 - indent;
    }

    /**
     * The gutter of a row, before its text, which holds the mark of its value: its width, and the middle of
     * the mark from the row's left. And where a row that has no gutter, as a text and an image, starts.
     */
    struct Gutter
    {
        float width = theme::SPACE_6;
        float mark = theme::SPACE_6 * 0.5f;
        float inset = theme::SPACE_2;
    };

    /**
     * The gutter of a row by how many sections it is in. In a section in a section the line of that section
     * runs down the left of the gutter, so the gutter is wider, its mark sits right of its middle, clear of
     * the line, and the text is further in. The controls do not move. A text and an image start past the line.
     */
    Gutter gutterAt(const int depth)
    {
        if (depth < 2) {
            return {};
        }
        return { .width = theme::SPACE_8, .mark = theme::SPACE_8 * 0.5f + theme::SPACE_2, .inset = theme::SPACE_6 };
    }

    /**
     * A text row at the cursor: its lines in the muted color, in from the edges of the rows and wrapped
     * between them.
     */
    void drawTextRow(const model::Text& text, const Gutter& gutter)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const float height = ui::drawTextLines(text.lines, ImVec2(pos.x + gutter.inset, pos.y), width - gutter.inset - theme::SPACE_2, theme::color::GREEN_MUTED);
        ImGui::Dummy(ImVec2(width, height));
    }

    /**
     * What a row that was added tells.
     */
    struct Row
    {
        // false for a row that is scrolled out of the page: nothing of it is drawn
        bool shown = false;

        // the ray is on it
        bool pointed = false;

        // the trigger was pulled on it in this frame, is held since a pull on it, and was let go on it
        bool started = false;
        bool held = false;
        bool pressed = false;
    };

    /**
     * Adds a row at the cursor, as wide as the rows and as high as a control. A row that is pressed has the
     * look of a row under the ray, and a stronger one while the trigger is held. Any other is only its room.
     */
    Row addRow(const bool pressable)
    {
        const ImVec2 size(rowWidth(), theme::CONTROL_HEIGHT);
        Row row;
        if (pressable) {
            row.pressed = ImGui::InvisibleButton("row", size);
            row.started = ImGui::IsItemActivated();
            row.held = ImGui::IsItemActive();
        } else {
            ImGui::Dummy(size);
        }
        row.shown = ImGui::IsItemVisible();
        if (row.shown) {
            row.pointed = ImGui::IsItemHovered();
            if (pressable) {
                ui::drawRowState();
            }
        }
        return row;
    }

    /**
     * The text of a row at a place, after the gutter and up to a right edge. A text whose tags give it pieces
     * is drawn in their sizes and colors, and any other as its plain text. A setting that takes effect after
     * a restart has the Restart mark after its text, and a row that warns the warning mark after that. The
     * text shortens with an ellipsis before a mark does, so the marks always show.
     */
    void drawLabel(const ImVec2& pos, const Gutter& gutter, const float right, const std::string_view text, const std::span<const model::TextRun> runs, const bool warned = false,
        const bool restart = false)
    {
        const float left = pos.x + gutter.width + theme::SPACE_3;
        const float restartWidth = restart ? ui::restartMarkWidth() : 0.0f;
        float textRight = right;
        if (restart) {
            textRight -= theme::SPACE_2 + restartWidth;
        }
        if (warned) {
            textRight -= theme::SPACE_2 + ui::WARNING_MARK_WIDTH;
        }
        float width = 0.0f;
        if (runs.empty()) {
            ui::drawText(left, textRight, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, text);
            width = text.empty() ? 0.0f : (std::min)(ui::textWidth(text), textRight - left);
        } else {
            width = ui::drawTextRuns(left, textRight, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, runs);
        }
        const float middle = pos.y + theme::CONTROL_HEIGHT * 0.5f;
        float markLeft = left + width + (width > 0.0f ? theme::SPACE_2 : 0.0f);
        if (restart) {
            ui::drawRestartMark(markLeft, middle);
            markLeft += restartWidth + theme::SPACE_2;
        }
        if (warned) {
            ui::drawWarningMark(ImVec2(markLeft + ui::WARNING_MARK_WIDTH * 0.5f, middle));
        }
    }

    /**
     * The text of the first warning that holds on the values the store has now, and none where none does.
     * A warning that a value which is not available leaves open does not hold.
     */
    std::string_view warningOf(const std::vector<model::Warning>& warnings, model::Store* store)
    {
        const auto warning = std::ranges::find_if(warnings, [store](const model::Warning& each) {
            return model::surelyHolds(each.when, store);
        });
        return warning != warnings.end() ? std::string_view(warning->text) : std::string_view();
    }

    /**
     * A row at the cursor that has no control, as its text and what it is. It is laid out as a setting's row:
     * the text after the gutter, and the type where the control ends, before the room of its Reset.
     * A row with a help is disabled, with the look of a row under the ray, and none of a press, which does nothing.
     * Returns whether the ray is on the row.
     */
    bool drawRow(const std::string_view text, const std::span<const model::TextRun> runs, const std::string_view type, const bool hasHelp, const Gutter& gutter)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const auto row = addRow(false);
        if (!row.shown) {
            return false;
        }
        if (hasHelp) {
            ui::drawRowState();
        }

        ImGui::BeginDisabled(hasHelp);
        const float typeLeft = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2 - ui::textWidth(type);
        ui::drawText(typeLeft, pos.x + width, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED, type);
        drawLabel(pos, gutter, typeLeft - theme::SPACE_3, text, runs);
        ImGui::EndDisabled();
        return row.pointed;
    }

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
     * The default the store has for a setting, where its control can show it: a switcher that is on or off,
     * a number of a slider with a range, one of the options the store has for a stepper and a dropdown, and a
     * text of a text input.
     * None for a setting with no default.
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
     * A default of a setting as the help line names it: on and off for a switcher, the number of a slider
     * as its display writes it, with its unit, and the text of the option of a stepper and a dropdown. The
     * text of a text input is in quotes, so an empty one shows, and a number is as its field shows it.
     */
    std::string defaultName(const model::Setting& setting, const std::vector<model::Option>& options, const model::Value& value)
    {
        if (setting.type == model::SettingType::TextInput) {
            const auto& text = std::get<std::string>(value);
            return setting.valueType == model::StoredType::String ? std::format("\"{}\"", text) : ui::fieldText(setting, text);
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

    // what the help line says of every row that is only shown, before its own help: it cannot be changed in the menu
    constexpr std::string_view READ_ONLY = "(Read-only)";

    /**
     * What the help line says of when a setting's change takes effect, after the row's help: nothing for one
     * that takes effect as it is made, and for one whose store does not know.
     */
    std::string_view takesEffect(const std::optional<model::Applies> applies)
    {
        if (applies == model::Applies::Save) {
            return "Takes effect after Save.";
        }
        return applies == model::Applies::Restart ? "Takes effect after the game is restarted." : std::string_view();
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
     * What the rows of a page are drawn with, and what their drawing tells.
     */
    struct PageDrawing
    {
        const ui::ImageDrawer& imageDrawer;
        model::Store* store;
        ui::Sliders& sliders;
        ui::Choices& choices;
        ui::TextInputs& textInputs;

        ui::Sections& sections;
        ui::PageDrawn& drawn;
    };

    /**
     * The row of a setting at the cursor: its text after the gutter, and its control where the control column
     * ends, before the room of its Reset. The text has the room up to its own control.
     * The control shows the value the store has for the setting. A press anywhere on the row of a switcher
     * turns it on or off, and a slider is dragged by its track, which is written as the trigger is let go,
     * and stepped by the thumbstick. A stepper
     * goes to the option before and after, and a dropdown opens its list. A text input starts its edit, and
     * is written as its field is left with another text: its row is not pressed while it is edited, so a
     * press on the field is the field's. A control whose value is not
     * available is disabled, and so is a slider with no range and a stepper and a dropdown with no options.
     * A setting whose enabledWhen does not hold is disabled as a whole, with its Reset, and a press does
     * nothing in its row. The row itself has the look of a row under the ray, as it has a help.
     * A setting with a default has its Reset in the room after the control, disabled while its value is the
     * default and valid: a press on it sets the default in the store. A press in that room does nothing else
     * in the row, also while Reset is disabled, so a second press on it changes nothing.
     * The gutter has the mark of an invalid value, or else of a dirty one, which waits for a Save, and the
     * control has its value in their colors.
     * A read-only setting is drawn as its control with its value, disabled, with the look of a row under the
     * ray and none of a press, and no Reset. One whose storage has no value shows none in its control.
     * A setting that takes effect after the game is restarted has the Restart mark after its text, whether it
     * is changed or not. A setting whose warning holds has the warning mark after its text, and one that is
     * invalid has none: what is wrong comes first.
     * Returns whether the ray is on the row or on its open list, and tells the help of the row: why a value
     * is not available, what Reset gives while the ray is on it, why a value is invalid, and else the row's
     * own, after `(Read-only)` for a read-only setting, with a percentage's number, when it takes effect but
     * as it is made, and its default. And the
     * warning of a row that has the mark, and whether the help is why the value is invalid.
     */
    bool drawSetting(const model::Setting& setting, const PageDrawing& page, const Gutter& gutter, std::string& help, std::string_view& warning, bool& invalid)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const bool enabled = !setting.enabledWhen || model::holds(*setting.enabledWhen, page.store);
        const auto row = addRow(enabled && !setting.readOnly && !page.textInputs.isEdited(setting));
        if ((!enabled || setting.readOnly) && row.shown) {
            ui::drawRowState();
        }
        const float right = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;

        auto stored = page.store ? page.store->value(setting) : model::StoredValue{};
        const bool editable = stored.available && enabled && !setting.readOnly;

        // a read-only setting whose storage has no value shows none
        const bool showsValue = stored.available || !setting.readOnly;

        // Reset is pressed a little beside it too, as a button of a stepper is. A press is its own when it
        // began there: a slider that is dragged and let go on it is not reset
        const auto& io = ImGui::GetIO();
        const auto resetBox = ui::controlBox(right + theme::SPACE_2, pos.y, theme::CONTROL_HEIGHT);
        const auto defaultValue = stored.available ? defaultOf(setting, *page.store) : std::nullopt;
        const bool onReset = defaultValue && row.pointed && io.MousePos.x >= resetBox.Min.x - theme::SPACE_1;
        const bool resetHeld = onReset && io.MouseClickedPos[ImGuiMouseButton_Left].x >= resetBox.Min.x - theme::SPACE_1;
        bool resets = editable && defaultValue && (stored.invalid || !isSameValue(stored.value, *defaultValue));
        if (resets && resetHeld && row.pressed && page.store->set(setting, *defaultValue)) {
            stored = page.store->value(setting);
            resets = false;
            page.drawn.pressActed = true;
        }
        const bool rowPressed = row.pressed && !onReset;

        const auto& options = setting.valueOptions;
        const auto& choiceOptions = page.store ? page.store->options(setting) : options.options;
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
            const ui::SliderRow sliderRow{ .pointed = row.pointed, .started = row.started, .held = row.held };
            const float edited = page.sliders.edit(setting, right, pos.y, sliderRow, value, write, pulse);
            if (!write || page.store->set(setting, edited)) {
                value = edited;
            }
            page.drawn.pressActed = page.drawn.pressActed || pulse;
        }
        if (isSwitcher && rowPressed && page.store->set(setting, !*on)) {
            page.drawn.pressActed = true;
        }
        // before the row is known to be shown too: the list of a dropdown is not a part of its row
        auto chosen = ui::chosenOption(choiceOptions, stored);
        const ui::ChoiceRow choiceRow{ .pointed = isChoice && row.pointed, .held = isChoice && row.held, .pressed = isChoice && rowPressed };
        if (isChoice) {
            bool pulse = false;
            const auto picked = page.choices.edit(setting, choiceOptions, right, pos.y, choiceRow, chosen, pulse);
            if (picked && picked != chosen && page.store->set(setting, choiceOptions[*picked].value)) {
                chosen = picked;
                pulse = true;
            }
            page.drawn.pressActed = page.drawn.pressActed || pulse;
        }
        // before the row is known to be shown too: the edit lasts while the row is scrolled out of the page
        if (isText) {
            bool pulse = false;
            const auto typed = page.textInputs.edit(setting, right, pos.y, rowPressed, std::get<std::string>(stored.value), pulse);
            if (typed && page.store->set(setting, *typed)) {
                stored = page.store->value(setting);
                resets = editable && defaultValue && (stored.invalid || !isSameValue(stored.value, *defaultValue));
            }
            page.drawn.pressActed = page.drawn.pressActed || pulse;
        }
        // on the pending value, as it is after a press in this frame
        const auto warns = stored.invalid ? std::string_view() : warningOf(setting.warnings, page.store);
        const auto applies = page.store ? page.store->applies(setting) : std::nullopt;

        // the ray is on the list then, and the help line keeps the row of its dropdown
        const bool listOpen = page.choices.isListOpen(setting);
        if (row.pointed || listOpen) {
            warning = warns;
            if (!stored.available && !stored.reason.empty()) {
                help = std::move(stored.reason);
            } else if (resets && onReset) {
                help = std::format("Reset to {}", defaultName(setting, choiceOptions, *defaultValue));
            } else if (!stored.reason.empty()) {
                help = std::move(stored.reason);
                invalid = stored.invalid;
            } else {
                const auto defaultText = defaultValue ? std::format("(Default: {})", defaultName(setting, choiceOptions, *defaultValue)) : std::string();
                help = joinHelp({ setting.readOnly ? READ_ONLY : std::string_view(), setting.help, numberDetail(setting, stored, value), takesEffect(applies), defaultText });
            }
        }
        if (!row.shown) {
            return listOpen;
        }

        const bool dirty = stored.available && page.store->isDirty(setting);
        const auto look = stored.invalid ? ui::ValueLook::Invalid : dirty ? ui::ValueLook::Pending : ui::ValueLook::Saved;
        float left = right;
        ImGui::BeginDisabled(!isSwitcher && !isSlider && !isChoice && !isText);
        const ImVec2 gutterMiddle(pos.x + gutter.mark, pos.y + theme::CONTROL_HEIGHT * 0.5f);
        if (look == ui::ValueLook::Invalid) {
            ui::drawInvalidMark(gutterMiddle);
        } else if (look == ui::ValueLook::Pending) {
            ui::drawPendingMark(gutterMiddle);
        }
        switch (setting.type) {
        case model::SettingType::Switcher:
            left = ui::drawSwitcher(right, pos.y, showsValue ? std::optional(on && *on) : std::nullopt, look);
            break;
        case model::SettingType::Slider:
            left = page.sliders.draw(setting, right, pos.y, showsValue ? std::optional(value) : std::nullopt, look);
            break;
        case model::SettingType::Stepper:
        case model::SettingType::Dropdown:
            left = page.choices.draw(setting, choiceOptions, right, pos.y, choiceRow, showsValue ? chosen : std::nullopt, look);
            break;
        case model::SettingType::TextInput: {
            // one whose value is not available has an empty box
            const auto* text = std::get_if<std::string>(&stored.value);
            left = page.textInputs.draw(setting, right, pos.y, stored.available && text ? ui::fieldText(setting, *text) : std::string(), look);
            break;
        }
        }
        drawLabel(pos, gutter, left - theme::SPACE_3, setting.text, setting.textRuns, !warns.empty(), applies == model::Applies::Restart);
        ImGui::EndDisabled();
        if (defaultValue) {
            ImGui::BeginDisabled(!resets);
            ui::drawButtonFrame(resetBox, resets && onReset, resets && onReset && resetHeld && row.held);
            drawResetIcon(resetBox.GetCenter());
            ImGui::EndDisabled();
        }
        return row.pointed || listOpen;
    }

    // the button of a button's row, in the panel's pixels: as wide as the Open of the menu's front page
    constexpr float BUTTON_WIDTH = 160.0f;

    /**
     * The row of a button at the cursor: its text after the gutter, and its button where a control ends,
     * before the room of a Reset, with its own text or `Run`. The whole row is pressed, so the button is
     * only drawn, and a press runs the action in the store. A button with no action is disabled, and so is
     * one whose action the menu does not run, which the reason says, and one whose enabledWhen does not hold.
     * A button whose warning holds has the warning mark after its text.
     * Returns whether the ray is on the row, and tells the reason and the warning.
     */
    bool drawButton(const model::Button& button, const PageDrawing& page, const Gutter& gutter, std::string& reason, std::string_view& warning)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const bool enabled = !button.enabledWhen || model::holds(*button.enabledWhen, page.store);
        const auto row = addRow(enabled);
        if (!enabled && row.shown) {
            ui::drawRowState();
        }

        const auto* unknown = std::get_if<model::UnknownAction>(&button.action);
        const bool runs = page.store && enabled && !unknown && !std::holds_alternative<model::NoAction>(button.action);
        if (runs && row.pressed && page.store->run(button.action)) {
            page.drawn.pressActed = true;
        }
        if (unknown) {
            reason = unknown->reason;
        }
        if (!row.shown) {
            return false;
        }
        warning = warningOf(button.warnings, page.store);

        ImGui::BeginDisabled(!runs);
        const auto box = ui::controlBox(pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2 - BUTTON_WIDTH, pos.y, BUTTON_WIDTH);
        ui::drawOutline(box.Min, box.Max, theme::color::FRAME_BORDER);
        ui::drawTextIn(box, button.buttonText.empty() ? std::string_view("Run") : std::string_view(button.buttonText), theme::color::GREEN, 0.5f, theme::SPACE_3);
        drawLabel(pos, gutter, box.Min.x - theme::SPACE_3, button.text, button.textRuns, !warning.empty());
        ImGui::EndDisabled();
        return row.pointed;
    }

    // the least width of the box of a hotkey's key, in the panel's pixels: not a token of the design system
    constexpr float KEY_MIN_WIDTH = 96.0f;

    /**
     * The row of a hotkey at the cursor: its text after the gutter, and the key it is bound to in a box as
     * wide as its name, where a control ends, before the room of a Reset. A hotkey with no key says `None`.
     * The key is only shown, so the whole row is always disabled, as a row that cannot be edited is. It has
     * the look of a row under the ray, for its help, but none of a press, which does nothing.
     * A hotkey that is not available looks the same, and the reason tells it from the others.
     * Returns whether the ray is on the row.
     */
    bool drawHotkey(const model::Hotkey& hotkey, const PageDrawing& page, const Gutter& gutter, std::string& reason)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const auto row = addRow(false);

        auto stored = page.store ? page.store->hotkey(hotkey) : model::StoredValue{};
        reason = std::move(stored.reason);
        if (!row.shown) {
            return false;
        }
        ui::drawRowState();
        const auto* key = std::get_if<std::string>(&stored.value);
        const std::string_view name = key ? std::string_view(*key) : "None";
        const float boxWidth = (std::max)(KEY_MIN_WIDTH, std::ceil(ui::textWidth(name)) + theme::SPACE_3 * 2.0f);
        const auto box = ui::controlBox(pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2 - boxWidth, pos.y, boxWidth);

        ImGui::BeginDisabled();
        drawLabel(pos, gutter, box.Min.x - theme::SPACE_3, hotkey.text, hotkey.textRuns);
        ui::drawFrame(box);
        ui::drawTextIn(box, name, theme::color::GREEN, 0.5f);
        ImGui::EndDisabled();
        return row.pointed;
    }

    /**
     * The help of a row that is only shown: that it is read-only, before the help of the row. One that is
     * not available has the reason alone.
     */
    std::string readOnlyHelp(std::string reason, const std::string& help)
    {
        if (!reason.empty()) {
            return reason;
        }
        return help.empty() ? std::string(READ_ONLY) : std::format("{} {}", READ_ONLY, help);
    }

    void drawItems(const std::vector<model::Item>& items, const PageDrawing& page, int depth = 0);

    /**
     * The heading of a section that folds, at the cursor: a row that a press folds or opens, which the
     * sections draw. It tells the help of the section and what it hides while the ray is on it.
     * Returns whether the section is folded, after a press in this frame.
     */
    bool drawFoldingHeading(const model::Group& group, const PageDrawing& page)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const auto row = addRow(true);
        std::string help;
        const bool folded =
            page.sections.drawHeading(group, page.store, pos, width, { .shown = row.shown, .pointed = row.pointed, .held = row.held, .pressed = row.pressed }, help);
        page.drawn.pressActed = page.drawn.pressActed || row.pressed;
        if (row.pointed) {
            page.drawn.pointed = ui::PointedRow{ .text = group.text, .help = std::move(help) };
        }
        return folded;
    }

    // how many levels a section in a section is set in at the most, so a name keeps its room
    constexpr int MAX_SET_IN = 3;

    /**
     * A section at the cursor: its heading, where it is shown, and the rows of its items under it. A section in a
     * section is set in with its rows, by the step a page is set in under its mod in the tree, up to the third
     * level, and while it is open and has rows its line runs down their left, so its rows are seen to be its and
     * not the outer section's. The depth is how many sections it is in.
     * A section that folds has a heading that a press folds or opens, and its rows are drawn while it is open.
     * One whose heading is not shown keeps its fold, and has no line. The heading of any other tells its help
     * while the ray is on it.
     */
    void drawGroup(const model::Group& group, const PageDrawing& page, const int depth, const bool headingShown)
    {
        const bool setIn = depth > 0 && depth <= MAX_SET_IN;
        if (setIn) {
            ImGui::Indent();
        }
        const float lineLeft = ImGui::GetCursorScreenPos().x + theme::SPACE_2;
        bool folded = false;
        if (group.collapsible) {
            folded = headingShown ? drawFoldingHeading(group, page) : page.sections.isFolded(group);
        } else if (headingShown) {
            ui::sectionHeading(group.text, ImGui::GetCursorScreenPos().x + rowWidth());
            if (!group.help.empty() && ImGui::IsItemHovered()) {
                page.drawn.pointed = ui::PointedRow{ .text = group.text, .help = group.help };
            }
        }
        if (!folded) {
            const float rowsTop = ImGui::GetCursorScreenPos().y;
            const float lineTop = headingShown ? ImGui::GetItemRectMax().y : rowsTop;
            drawItems(group.items, page, depth + 1);
            const float rowsEnd = ImGui::GetCursorScreenPos().y;
            if (depth > 0 && headingShown && rowsEnd > rowsTop) {
                ui::drawSectionLine(lineLeft, lineTop, rowsEnd - ImGui::GetStyle().ItemSpacing.y);
            }
        }
        if (setIn) {
            ImGui::Unindent();
        }
    }

    /**
     * Draws an item as its row. A section is its heading, with the rows of its items under it while it is
     * not folded, and a spacer
     * is empty space as high as its lines of text. An image that could not be read is a row that says an
     * image is there.
     * A row with a text that the ray is on is told as the pointed one. A setting whose value is not
     * available has the reason as its help, where the store gives one, and so has a button whose action the
     * menu does not run, and a hotkey that is not available. Any other hotkey has that it is read-only
     * before its help. A control the menu does not have says so as its help. A setting and a button that
     * warn have the warning before their help.
     */
    struct ItemDrawer
    {
        const PageDrawing& page;

        // how many sections the item is in
        int depth = 0;

        void operator()(const model::Setting& setting) const
        {
            std::string reason;
            std::string_view warning;
            bool invalid = false;
            if (drawSetting(setting, page, gutterAt(depth), reason, warning, invalid)) {
                page.drawn.pointed = ui::PointedRow{ .text = setting.text, .help = std::move(reason), .warning = warning, .invalid = invalid };
            }
        }

        void operator()(const model::Group& group) const
        {
            drawGroup(group, page, depth, true);
        }

        void operator()(const model::Button& button) const
        {
            std::string reason;
            std::string_view warning;
            if (drawButton(button, page, gutterAt(depth), reason, warning)) {
                page.drawn.pointed = ui::PointedRow{ .text = button.text, .help = reason.empty() ? button.help : std::move(reason), .warning = warning };
            }
        }

        void operator()(const model::Hotkey& hotkey) const
        {
            std::string reason;
            if (drawHotkey(hotkey, page, gutterAt(depth), reason)) {
                page.drawn.pointed = ui::PointedRow{ .text = hotkey.text, .help = readOnlyHelp(std::move(reason), hotkey.help) };
            }
        }

        void operator()(const model::Text& text) const
        {
            drawTextRow(text, gutterAt(depth));
        }

        void operator()(const model::Image& image) const
        {
            if (image.pixels) {
                ui::drawImage(image, page.imageDrawer, rowWidth(), gutterAt(depth).inset);
            } else if (!image.texture.empty()) {
                ui::drawTexture(image, page.imageDrawer, rowWidth(), gutterAt(depth).inset);
            } else {
                drawRow({}, {}, "image", false, gutterAt(depth));
            }
        }

        void operator()(const model::Spacer& spacer) const
        {
            ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize() * static_cast<float>(spacer.lines)));
        }

        void operator()(const model::Placeholder& placeholder) const
        {
            const bool typed = !placeholder.type.empty();
            if (drawRow(placeholder.text, placeholder.textRuns, typed ? std::string_view(placeholder.type) : "unknown", true, gutterAt(depth))) {
                auto help = typed ? std::format("Not available: the menu has no {} control.", placeholder.type) : std::string("Not available: the row has no type of control.");
                page.drawn.pointed = ui::PointedRow{ .text = placeholder.text, .help = std::move(help) };
            }
        }
    };

    /**
     * Whether an item is shown: one with a condition is, while it holds on the values of the store. With no
     * store no value is available, so a condition on a value holds, and one that is settled without a value,
     * as a package's condition on a plugin, is asked as any other.
     */
    bool isShown(const model::Item& item, model::Store* store)
    {
        const auto& visibleWhen = std::visit(
            [](const auto& row) -> const std::optional<model::Condition>& {
                return row.visibleWhen;
            },
            item);
        return !visibleWhen || model::holds(*visibleWhen, store);
    }

    /**
     * Draws the items that are shown, each as its row. An item that is not shown has no row and no room.
     * A section that is not shown is without its heading alone: its items are shown by their own conditions.
     * The depth is how many sections the items are in.
     */
    void drawItems(const std::vector<model::Item>& items, const PageDrawing& page, const int depth)
    {
        for (const auto& item : items) {
            // a row is an item of ImGui by the name of its kind: its place in the page tells it from the others
            ImGui::PushID(&item);
            if (isShown(item, page.store)) {
                std::visit(ItemDrawer{ .page = page, .depth = depth }, item);
            } else if (const auto* group = std::get_if<model::Group>(&item)) {
                drawGroup(*group, page, depth, false);
            }
            ImGui::PopID();
        }
    }
}

namespace mods_config::ui
{
    void ModPage::setImageDrawer(ImageDrawer drawer)
    {
        _imageDrawer = std::move(drawer);
    }

    /**
     * Draws the rows of a page into the current ImGui window, which scrolls them, with the values the store
     * has for its settings. With no store no value is available. The thumbstick's value is how far the one
     * of the hand that points is pushed to a side, from -1 at the left to 1 at the right: it steps the
     * slider or the stepper whose row the ray is on.
     * Returns the row the ray is on, for the help line: none in the gap between two rows, and none on a row
     * that has no text of its own, as a text row, an image and a spacer, or on the heading of a section with
     * no help that does not fold. While the list of a dropdown is open it is the row of that dropdown. And
     * whether a press did something, or the thumbstick stepped a slider or a stepper.
     */
    PageDrawn ModPage::draw(const model::Page& page, model::Store* store, const float thumbstickSideValue)
    {
        PageDrawn drawn;
        _sliders.startFrame(thumbstickSideValue);
        _choices.startFrame(thumbstickSideValue);
        drawItems(page.items,
            PageDrawing{ .imageDrawer = _imageDrawer, .store = store, .sliders = _sliders, .choices = _choices, .textInputs = _textInputs, .sections = _sections, .drawn = drawn });
        _sliders.endFrame();
        _choices.endFrame();
        return drawn;
    }

    /**
     * Ends the drag of a slider with nothing written, for Back.
     * Returns false while no slider is dragged.
     */
    bool ModPage::cancelDrag()
    {
        return _sliders.cancelDrag();
    }

    /**
     * Closes the list of a dropdown that is open, with no option picked, for Back.
     * Returns false while none is open.
     */
    bool ModPage::closeList()
    {
        return _choices.closeList();
    }

    /**
     * Ends the edit of a text input with nothing written, for Back.
     * Returns false while none is edited.
     */
    bool ModPage::cancelEdit()
    {
        return _textInputs.cancelEdit();
    }

    /**
     * Forgets what is edited in the page, as another page is shown, and how its sections were folded: each
     * starts as its `collapsed` says each time its page is shown.
     */
    void ModPage::reset()
    {
        _sliders.reset();
        _choices.reset();
        _textInputs.reset();
        _sections.reset();
    }
}
