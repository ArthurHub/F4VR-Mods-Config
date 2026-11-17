#include "TransformEditor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <ranges>
#include <string_view>

#include "ButtonIcons.h"
#include "Drawing.h"
#include "Numbers.h"
#include "Theme.h"
#include "common/CommonUtils.h"
#include "model/TextStates.h"

namespace
{
    using namespace mods_config;

    // The sizes of the editor that are not tokens of the design system, in the panel's pixels.
    constexpr float EDITOR_WIDTH = 560.0f;
    constexpr float LABEL_WIDTH = 96.0f;

    // the button of the footer that tunes the transform in the world, and its help
    constexpr std::string_view ADJUST_TEXT = "Adjust in world";
    constexpr std::string_view ADJUST_HELP = "Tune it with the controllers while you look at it, from your wrist.";

    /**
     * An axis of the editor: its name, and what it does to the transform, around the name of what the transform places.
     */
    struct Axis
    {
        std::string_view name;
        std::string_view verb;
        std::string_view how;
    };

    constexpr std::array<Axis, model::TRANSFORM_AXES> AXES{
        Axis{ "X", "Moves", " along X" },
        Axis{ "Y", "Moves", " along Y" },
        Axis{ "Z", "Moves", " along Z" },
        Axis{ "Heading", "Turns", " by its heading" },
        Axis{ "Roll", "Turns", " by its roll" },
        Axis{ "Attitude", "Turns", " by its attitude" },
        Axis{ "Scale", "Scales", "" },
    };

    /**
     * The unit of an axis's numbers: a degree for a rotation.
     */
    std::string_view axisUnit(const std::size_t axis)
    {
        return axis >= model::TRANSFORM_ROTATION && axis < model::TRANSFORM_SCALE ? "°" : "";
    }

    /**
     * A setting's name in a sentence, after `the`: its first letter in lower case, unless the second is a capital too, as
     * `FOV offset`.
     */
    std::string sentenceName(std::string name)
    {
        if (name.size() > 1 && std::isupper(static_cast<unsigned char>(name[0])) && !std::isupper(static_cast<unsigned char>(name[1]))) {
            name[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[0])));
        }
        return name;
    }

    /**
     * The table of a setting's default, and none for a setting with no default.
     */
    std::optional<model::TransformTable> defaultTable(const model::Setting& setting)
    {
        const auto* text = setting.defaultValue ? std::get_if<std::string>(&*setting.defaultValue) : nullptr;
        const auto table = text ? model::parseTransformTable(*text, setting.tableForm) : std::unexpected(std::string());
        return table ? std::optional(*table) : std::nullopt;
    }

    /**
     * The table of the value a setting has saved, and none where it has no transform saved, as one the file cannot be
     * read as.
     */
    std::optional<model::TransformTable> savedTable(const model::Setting& setting, model::Store& store)
    {
        const auto saved = store.savedValue(setting);
        const auto* text = saved ? std::get_if<std::string>(&*saved) : nullptr;
        const auto table = text ? model::parseTransformTable(*text, setting.tableForm) : std::unexpected(std::string());
        return table ? std::optional(*table) : std::nullopt;
    }

    /**
     * What the help line says of an axis: what it moves, its range and its step, and its default where it has one.
     */
    std::string axisHelp(const model::Setting& setting, const std::size_t axis, model::Store& store)
    {
        const auto& range = setting.valueOptions.transformAxes[axis];
        const auto unit = axisUnit(axis);
        const auto table = defaultTable(setting);
        const auto defaultText = table ? std::format(" Default {}{}.", model::axisValue(*table, axis), unit) : std::string();
        return std::format("{} the {}{}, {}{} to {}{}, by {}{} a step.{}",
            AXES[axis].verb,
            sentenceName(model::resolveClientText(setting.text, &store)),
            AXES[axis].how,
            range.min,
            unit,
            range.max,
            unit,
            range.step,
            unit,
            defaultText);
    }

    /**
     * The button of Adjust in world in a box, in the `important` style, the accent outline and text, with the hand icon
     * before its text, a ring while it is pressed, and disabled as a button of a row is. Returns whether it is pressed.
     */
    bool drawAdjustButton(const ImRect& box, const bool enabled)
    {
        ImGui::BeginDisabled(!enabled);
        ImGui::SetCursorScreenPos(box.Min);
        const bool pressed = ImGui::InvisibleButton("adjust", box.GetSize());
        ui::drawOutline(box.Min, box.Max, theme::color::ACCENT);
        if (ImGui::IsItemActive()) {
            ui::drawButtonRing(box, theme::color::ACCENT);
        }
        const float iconLeft = box.Min.x + theme::SPACE_4;
        ui::drawButtonIcon(model::ButtonIcon::Hand, ImVec2(iconLeft + ui::BUTTON_ICON_SIZE * 0.5f, box.GetCenter().y), theme::color::ACCENT);
        ui::drawText(iconLeft + ui::BUTTON_ICON_SIZE + theme::SPACE_2, box.Max.x, box.Min.y, box.GetHeight(), theme::color::ACCENT, ADJUST_TEXT);
        ImGui::EndDisabled();
        return pressed;
    }

    /**
     * How high the editor is with a number of lines: its header, the lines, and its footer under a rule.
     */
    float editorHeight(const std::size_t lines)
    {
        const auto count = static_cast<float>(lines);
        return theme::SPACE_4 + theme::CONTROL_HEIGHT + theme::SPACE_3 + count * theme::CONTROL_HEIGHT + (count - 1.0f) * theme::SPACE_1 + theme::SPACE_3 + theme::BORDER_FRAME +
               theme::SPACE_3 + theme::CONTROL_HEIGHT + theme::SPACE_4;
    }
}

namespace mods_config::ui
{
    /**
     * The editor of a transform as one frame draws it: its setting and store, where its parts are, where the ray is, and
     * what the frame tells.
     */
    struct TransformEditor::Frame
    {
        const model::Setting& setting;
        model::Store& store;
        TransformEdited& edited;

        // the content's left and right, the top of the header, and of the footer
        const float contentLeft;
        const float contentRight;
        const float headerTop;
        const float footerTop;

        // the ray is on the editor, and where it is
        const bool hovered;
        const ImVec2 mouse;

        /**
         * Tells the help line of the part the ray is on: its name and its help.
         */
        void point(const std::string_view name, std::string help)
        {
            edited.pointed = PointedRow{ .text = std::string(name), .help = std::move(help) };
        }
    };

    /**
     * Opens the editor of a transform with a press on its row, and draws it while it is open, with the transform the row
     * had: one that is no transform starts from its default. The editor is a popup in the page, under the row, or over it
     * where the page has no room under it, or as near as it fits: never over the title bar and the footer.
     * A line for each axis of each part that is on has the slider of the setting rows, in the range its config spec gives.
     * A value outside the range keeps its place until a step or a drag brings it in, and a value keeps the decimals it has
     * past its step. Each change sets the transform in the store as it is made, also during a drag, with the keys its
     * value had and the key of the part that changed.
     * Returns the part the ray is on, for the help line, and whether a press did something.
     */
    TransformEdited TransformEditor::edit(const model::Setting& setting, model::Store& store, const ImRect& row, const bool pressed)
    {
        TransformEdited edited;
        const auto id = ImGui::GetID("transform");
        if (pressed) {
            const auto stored = store.value(setting);
            const auto* text = std::get_if<std::string>(&stored.value);
            _before = text ? *text : std::string();
            start(setting, _before);
            _open = &setting;
            _cancel = false;
            _resume = false;
            _sliders.reset();
            ImGui::OpenPopupEx(id);
            edited.pulse = true;
        }
        if (_open != &setting) {
            return edited;
        }
        if (_resume) {
            // the panel was not drawn while it was tuned, and its value is in the store. Cancel and Back still give back
            // the value it had as the editor opened
            _resume = false;
            const auto stored = store.value(setting);
            const auto* text = std::get_if<std::string>(&stored.value);
            start(setting, text ? *text : _before);
            _sliders.reset();
        }
        if (!ImGui::IsPopupOpen(id, ImGuiPopupFlags_None)) {
            // a press beside it closed it, which keeps the transform
            _open = nullptr;
            return edited;
        }
        _openSeen = true;

        const auto& axes = setting.valueOptions.transformAxes;
        auto lines = std::views::iota(std::size_t{ 0 }, model::TRANSFORM_AXES) | std::views::filter([&axes](const std::size_t axis) {
            return axes[axis].on;
        });
        const float height = editorHeight(static_cast<std::size_t>(std::ranges::distance(lines)));
        if (!beginRowEditor(id, row, EDITOR_WIDTH, height)) {
            return edited;
        }
        if (_cancel) {
            store.set(setting, _before);
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            _open = nullptr;
            _cancel = false;
            return edited;
        }

        const auto origin = ImGui::GetWindowPos();
        Frame frame{ .setting = setting,
            .store = store,
            .edited = edited,
            .contentLeft = origin.x + theme::SPACE_4,
            .contentRight = origin.x + EDITOR_WIDTH - theme::SPACE_4,
            .headerTop = origin.y + theme::SPACE_4,
            .footerTop = origin.y + height - theme::SPACE_4 - theme::CONTROL_HEIGHT,
            .hovered = ImGui::IsWindowHovered(),
            .mouse = ImGui::GetIO().MousePos };

        // the header: the setting's name
        drawText(frame.contentLeft, frame.contentRight, frame.headerTop, theme::CONTROL_HEIGHT, theme::color::GREEN, model::resolveClientText(setting.text, &store));

        // a line for each axis of the parts that are on
        const auto saved = savedTable(setting, store);
        float lineTop = frame.headerTop + theme::CONTROL_HEIGHT + theme::SPACE_3;
        _sliders.startFrame(_thumbstickSideValue);
        for (const std::size_t axis : lines) {
            editAxis(frame, axis, lineTop, saved);
            lineTop += theme::CONTROL_HEIGHT + theme::SPACE_1;
        }
        _sliders.endFrame();

        drawFooter(frame);
        ImGui::EndPopup();
        return edited;
    }

    /**
     * The line of an axis: its name and its slider, which sets the axis in the store at each change. Its value is
     * pending while it differs from the saved transform's, which is none where the file has no transform.
     */
    void TransformEditor::editAxis(Frame& frame, const std::size_t axis, const float lineTop, const std::optional<model::TransformTable>& saved)
    {
        const ImRect line(frame.contentLeft, lineTop, frame.contentRight, lineTop + theme::CONTROL_HEIGHT);

        ImGui::PushID(static_cast<int>(axis));
        ImGui::SetCursorScreenPos(line.Min);
        ImGui::InvisibleButton("line", line.GetSize());
        // of the item, as a row's is: the window is not hovered while the line is held, and another line is not
        const bool pointed = ImGui::IsItemHovered();
        const SliderRow sliderRow{ .pointed = pointed, .started = ImGui::IsItemActivated(), .held = ImGui::IsItemActive() };
        ImGui::PopID();

        const float value = model::axisValue(_table, axis);
        bool write = false;
        const float right = frame.contentRight - theme::SPACE_2;
        const float changed = _sliders.edit(_axes[axis], right, lineTop, sliderRow, value, write, frame.edited.pulse);
        if (write || !common::fEqual(changed, value)) {
            model::setAxis(_table, axis, changed);
            frame.store.set(frame.setting, model::transformText(_table, frame.setting.tableForm));
        }

        if (pointed) {
            ImGui::GetWindowDrawList()->AddRectFilled(line.Min, line.Max, ImGui::GetColorU32(theme::color::HOVER), theme::RADIUS_FRAME);
            drawOutline(line.Min, line.Max, theme::color::FRAME_BORDER);
            frame.point(AXES[axis].name, axisHelp(frame.setting, axis, frame.store));
        }
        const float shownValue = model::axisValue(_table, axis);
        const bool differs = frame.store.isDirty(frame.setting) && (!saved || common::fNotEqual(model::axisValue(*saved, axis), shownValue));
        _axes[axis].display.decimals = (std::max)(stepDecimals(frame.setting.valueOptions.transformAxes[axis].step), stepDecimals(shownValue));
        const float labelLeft = frame.contentLeft + theme::SPACE_2;
        drawText(labelLeft, labelLeft + LABEL_WIDTH, lineTop, theme::CONTROL_HEIGHT, theme::color::GREEN, AXES[axis].name);
        _sliders.draw(_axes[axis], right, lineTop, sliderRow, shownValue, differs ? ValueLook::Pending : ValueLook::Saved);
    }

    /**
     * The footer under a rule: Adjust in world, which the menu does and a setting that cannot be previewed has disabled,
     * and Cancel and Done. Cancel gives back the value the editor opened with, and Done keeps it: both close the editor.
     */
    void TransformEditor::drawFooter(Frame& frame)
    {
        const float ruleY = frame.footerTop - theme::SPACE_3 - theme::BORDER_FRAME * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(frame.contentLeft, ruleY), ImVec2(frame.contentRight, ruleY), ImGui::GetColorU32(theme::color::RULE), theme::BORDER_FRAME);
        const auto notPreviewed = frame.store.notPreviewed(frame.setting);
        const auto adjustBox = controlBox(frame.contentLeft, frame.footerTop, textWidth(ADJUST_TEXT) + BUTTON_ICON_SIZE + theme::SPACE_2 + theme::SPACE_4 * 2.0f);
        if (drawAdjustButton(adjustBox, notPreviewed.empty())) {
            frame.edited.adjust = true;
            frame.edited.pulse = true;
        }
        if (frame.hovered && adjustBox.Contains(frame.mouse)) {
            frame.point(ADJUST_TEXT, notPreviewed.empty() ? std::string(ADJUST_HELP) : notPreviewed);
        }
        const float doneWidth = textWidth("Done") + theme::SPACE_4 * 2.0f;
        const float cancelWidth = textWidth("Cancel") + theme::SPACE_4 * 2.0f;
        const auto done = controlBox(frame.contentRight - doneWidth, frame.footerTop, doneWidth);
        const auto cancelBox = controlBox(done.Min.x - theme::SPACE_2 - cancelWidth, frame.footerTop, cancelWidth);
        if (button("Cancel", cancelBox, ButtonLook::Quiet)) {
            frame.store.set(frame.setting, _before);
            ImGui::CloseCurrentPopup();
            _open = nullptr;
            frame.edited.pulse = true;
        }
        if (frame.hovered && cancelBox.Contains(frame.mouse)) {
            frame.point("Cancel", "Give back the value it had as the editor opened.");
        }
        if (button("Done", done, ButtonLook::Primary)) {
            ImGui::CloseCurrentPopup();
            _open = nullptr;
            frame.edited.pulse = true;
        }
        if (frame.hovered && done.Contains(frame.mouse)) {
            frame.point("Done", "Keep the value.");
        }
    }

    /**
     * Starts editing a transform from the text of a value, with its keys: one that is no transform starts from its default.
     * Each axis is given the setting a slider needs of it.
     */
    void TransformEditor::start(const model::Setting& setting, const std::string& text)
    {
        _table = model::transformOrDefault(setting, text);
        for (std::size_t axis = 0; axis < model::TRANSFORM_AXES; ++axis) {
            const auto& range = setting.valueOptions.transformAxes[axis];
            _axes[axis].valueOptions = model::ValueOptions{ .min = range.min, .max = range.max, .step = range.step };
            _axes[axis].display.unit = axisUnit(axis);
        }
    }

    /**
     * Starts a frame of the page, before its rows. The thumbstick's value is how far the one of the hand that points is
     * pushed to a side, from -1 at the left to 1 at the right, which steps the slider the ray is on.
     */
    void TransformEditor::startFrame(const float thumbstickSideValue)
    {
        _thumbstickSideValue = thumbstickSideValue;
    }

    bool TransformEditor::isOpen(const model::Setting& setting) const
    {
        return _open == &setting;
    }

    /**
     * Ends a frame of the page, after its rows: a transform whose editor was not drawn in it, as one whose row is not
     * shown any more, has none open.
     */
    void TransformEditor::endFrame()
    {
        if (!_openSeen) {
            _open = nullptr;
        }
        _openSeen = false;
    }

    /**
     * Back in the editor: it gives back the value the row had as the editor opened, and closes the editor, as the next
     * frame draws it. Returns false while no editor is open.
     */
    bool TransformEditor::cancel()
    {
        if (!_open || _cancel) {
            return false;
        }
        _cancel = true;
        return true;
    }

    /**
     * Adjust in world ended, and the editor is shown again: it reads the value the tuning left as it is drawn next.
     */
    void TransformEditor::resume()
    {
        _resume = _open != nullptr;
    }

    /**
     * Forgets the editor that is open, as another page is shown: it is of a row of the page before.
     */
    void TransformEditor::reset()
    {
        _open = nullptr;
        _openSeen = false;
        _cancel = false;
        _resume = false;
        _sliders.reset();
    }
}
