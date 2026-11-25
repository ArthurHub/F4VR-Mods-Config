#include "ColorEditor.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string_view>

#include "Colors.h"
#include "Drawing.h"
#include "PanelTargets.h"
#include "Theme.h"
#include "model/TextStates.h"

namespace
{
    using namespace mods_config;

    // The sizes of the editor that are not tokens of the design system, in the panel's pixels. A color with presets has
    // the line of them under the picker, which makes it taller
    constexpr float EDITOR_WIDTH = 717.0f;
    constexpr float EDITOR_HEIGHT = 414.0f;
    constexpr float PRESETS_HEIGHT = theme::CONTROL_HEIGHT + theme::SPACE_3;

    // the swatches of the header, Before and Now
    constexpr float BIG_SWATCH_WIDTH = 96.0f;
    constexpr float BIG_SWATCH_HEIGHT = 40.0f;

    // ImGui's picker at this width: with the frame height of the design system its hue bar is 45 px wide, and its square
    // is 240 x 240
    constexpr float PICKER_WIDTH = 293.0f;
    constexpr float PICKER_HEIGHT = 240.0f;

    // a channel's line: its name, its track and its value box
    constexpr float CHANNELS_WIDTH = 368.0f;
    constexpr float CHANNEL_LABEL_WIDTH = 56.0f;

    // the grab of a channel, a hollow box of two rings, each this thick
    constexpr float GRAB_HEIGHT = 40.0f;
    constexpr float GRAB_RING = 2.0f;

    // the buttons of the footer
    constexpr float FOOTER_BUTTON_WIDTH = 160.0f;

    /**
     * A channel of the editor: its name, and its help.
     */
    struct Channel
    {
        std::string_view name;
        std::string_view help;
    };

    /**
     * A color of the editor's footer, which every color has: its red, green and blue, and its name.
     */
    struct Basic
    {
        std::array<int, 3> rgb;
        std::string_view name;
    };

    constexpr std::array BASICS{
        Basic{ { 255, 255, 255 }, "White" },
        Basic{ { 0, 0, 0 }, "Black" },
        Basic{ { 255, 0, 0 }, "Red" },
        Basic{ { 0, 255, 0 }, "Green" },
        Basic{ { 0, 0, 255 }, "Blue" },
    };

    constexpr std::array CHANNELS{
        Channel{ "Red", "The red of the color, 0 to 255. The thumbstick steps it by 1." },
        Channel{ "Green", "The green of the color, 0 to 255. The thumbstick steps it by 1." },
        Channel{ "Blue", "The blue of the color, 0 to 255. The thumbstick steps it by 1." },
        Channel{ "Alpha", "The opacity of the color, from clear at 0 to solid at 255. The thumbstick steps it by 1." },
    };

    /**
     * A color with one of its channels, red, green, blue or alpha, set to a byte.
     */
    render::Color withChannel(const render::Color& color, const std::size_t channel, const int value)
    {
        auto bytes = color.bytes();
        bytes[channel] = value;
        return render::Color::rgba(bytes[0], bytes[1], bytes[2], bytes[3]);
    }

    /**
     * The track of a channel: the channel from 0 at its left to 255 at its right, with the other channels as they are,
     * and for the alpha the color from clear to opaque over a checkerboard. Outlined in `frame-border`, or in `glow`
     * while it is dragged.
     */
    void drawTrack(const ImRect& track, const render::Color& color, const std::size_t channel, const bool dragged)
    {
        auto* drawList = ImGui::GetWindowDrawList();
        if (channel == 3) {
            ui::drawCheckerboard(track);
        }
        const auto left = ImGui::GetColorU32(ui::colorU32(withChannel(color, channel, 0), channel < 3));
        const auto right = ImGui::GetColorU32(ui::colorU32(withChannel(color, channel, 255), channel < 3));
        drawList->AddRectFilledMultiColor(track.Min, track.Max, left, right, right, left);
        controls::drawOutline(track.Min, track.Max, dragged ? theme::color::GLOW : theme::color::FRAME_BORDER);
    }

    /**
     * Where the grab of a channel's value is on its track, from its left to its right edge as a slider's grab is.
     */
    float grabAt(const ImRect& track, const int value)
    {
        const float least = track.Min.x + theme::GRAB_WIDTH * 0.5f;
        const float most = track.Max.x - theme::GRAB_WIDTH * 0.5f;
        return std::lerp(least, most, static_cast<float>(value) / 255.0f);
    }

    /**
     * The value of a channel at a place of its track, as its grab is there.
     */
    int valueAt(const ImRect& track, const float x)
    {
        const float least = track.Min.x + theme::GRAB_WIDTH * 0.5f;
        const float most = track.Max.x - theme::GRAB_WIDTH * 0.5f;
        return static_cast<int>(std::round(std::clamp((x - least) / (most - least), 0.0f, 1.0f) * 255.0f));
    }

    /**
     * The grab of a channel, hollow so the color shows through it: a ring in `green`, or in `glow` while it is dragged,
     * around a ring in `on-green`, so it shows on any color.
     */
    void drawGrab(const ImRect& track, const int value, const bool dragged)
    {
        auto* drawList = ImGui::GetWindowDrawList();
        const float x = grabAt(track, value);
        const float middle = track.GetCenter().y;
        const ImVec2 min(x - theme::GRAB_WIDTH * 0.5f, middle - GRAB_HEIGHT * 0.5f);
        const ImVec2 max(x + theme::GRAB_WIDTH * 0.5f, middle + GRAB_HEIGHT * 0.5f);
        const float half = GRAB_RING * 0.5f;
        drawList->AddRect(ImVec2(min.x + half, min.y + half),
            ImVec2(max.x - half, max.y - half),
            ImGui::GetColorU32(dragged ? theme::color::GLOW : theme::color::GREEN),
            theme::RADIUS_GRAB,
            0,
            GRAB_RING);
        drawList->AddRect(ImVec2(min.x + GRAB_RING + half, min.y + GRAB_RING + half),
            ImVec2(max.x - GRAB_RING - half, max.y - GRAB_RING - half),
            ImGui::GetColorU32(theme::color::ON_GREEN),
            0.0f,
            0,
            GRAB_RING);
    }

    /**
     * A text in the detail size, as a label of the editor, in a color, from its left in the middle of a line.
     */
    void drawDetail(const std::string_view text, const float left, const float right, const float top, const float height, const ImU32 color)
    {
        ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
        controls::drawText(left, right, top, height, color, text);
        ImGui::PopFont();
    }

    /**
     * The box of a swatch at its place in a line of them, from the line's left.
     */
    ImRect swatchBox(const float left, const float top, const std::size_t place)
    {
        const float x = left + static_cast<float>(place) * (theme::CONTROL_HEIGHT + theme::SPACE_2);
        return ImRect(x, top, x + theme::CONTROL_HEIGHT, top + theme::CONTROL_HEIGHT);
    }
}

namespace mods_config::ui
{
    /**
     * An editor with no color open: its channels are each a setting of 0 to 255 by 1, which the thumbstick steps as it
     * steps a slider.
     */
    ColorEditor::ColorEditor()
    {
        for (auto& stepped : _stepped) {
            stepped.valueOptions = model::ValueOptions{ .min = 0.0f, .max = 255.0f, .step = 1.0f };
        }
    }

    /**
     * The editor of a color as one frame draws it: its setting and store, where its parts are, where the ray is, and what
     * the frame tells.
     */
    struct ColorEditor::Frame
    {
        const model::Setting& setting;
        model::Store& store;
        ColorEdited& edited;

        // the setting has an alpha, which has its channel's line and the alphas of its presets
        const bool alpha;

        // the content's left and right, the top of the header, of the picker and the channels under it, and of the footer
        const float contentLeft;
        const float contentRight;
        const float headerTop;
        const float bodyTop;
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
     * Opens the editor of a color with a press on its row, and draws it while it is open, with the color the row had:
     * one that is no color starts from its default. The editor is a popup in the page, under the row, or over it where
     * the page has no room under it, or as near as it fits: never over the title bar and the footer.
     * The picker sets the red, the green and the blue at once, and a channel's slider its channel, which the trigger
     * drags and the thumbstick steps by 1. A swatch sets its color: one of the config spec's presets, in a line under
     * the picker, or one of the basics of the footer, which every color has. Each change sets the color in the store as
     * it is made, with the keys its value had and the key of the part that changed. Before gives back the color it had
     * as it opened.
     * Returns the part the ray is on, for the help line, and whether a press did something.
     */
    ColorEdited ColorEditor::edit(const model::Setting& setting, model::Store& store, const ImRect& row, const bool pressed)
    {
        ColorEdited edited;
        const auto id = ImGui::GetID("color");
        if (pressed) {
            const auto stored = store.value(setting);
            const auto* text = std::get_if<std::string>(&stored.value);
            _before = text ? *text : std::string();
            start(setting, _before);
            _open = &setting;
            _cancel = false;
            _dragged.reset();
            ImGui::OpenPopupEx(id);
            edited.pulse = true;
        }
        if (_open != &setting) {
            return edited;
        }
        if (!ImGui::IsPopupOpen(id, ImGuiPopupFlags_None)) {
            // a press beside it closed it, which keeps the color
            _open = nullptr;
            return edited;
        }
        _openSeen = true;

        const float height = EDITOR_HEIGHT + (setting.valueOptions.presets.empty() ? 0.0f : PRESETS_HEIGHT);
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
        const float headerTop = origin.y + theme::SPACE_4;
        Frame frame{ .setting = setting,
            .store = store,
            .edited = edited,
            .alpha = setting.valueOptions.alpha,
            .contentLeft = origin.x + theme::SPACE_4,
            .contentRight = origin.x + EDITOR_WIDTH - theme::SPACE_4,
            .headerTop = headerTop,
            .bodyTop = headerTop + theme::CONTROL_HEIGHT + theme::SPACE_3,
            .footerTop = origin.y + height - theme::SPACE_4 - theme::CONTROL_HEIGHT,
            .hovered = ImGui::IsWindowHovered(),
            .mouse = ImGui::GetIO().MousePos };

        const auto now = editHeader(frame);
        editPicker(frame);
        editChannels(frame);
        editPresets(frame);
        editBasics(frame);
        // Now, as the color is after the changes of this frame
        drawSwatch(now, _color, frame.alpha);
        drawFooter(frame);
        ImGui::EndPopup();
        return edited;
    }

    /**
     * The header: the setting's name, and the color as it was and as it is, a press on Before giving the first back.
     * Returns the box of Now, which is drawn once the color has the changes of the frame.
     */
    ImRect ColorEditor::editHeader(Frame& frame)
    {
        const float nowLeft = frame.contentRight - BIG_SWATCH_WIDTH;
        const float swatchTop = frame.headerTop + (theme::CONTROL_HEIGHT - BIG_SWATCH_HEIGHT) * 0.5f;
        const ImRect now(nowLeft, swatchTop, frame.contentRight, swatchTop + BIG_SWATCH_HEIGHT);
        ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
        const float nowLabelLeft = nowLeft - theme::SPACE_2 - controls::textWidth("Now");
        const float beforeRight = nowLabelLeft - theme::SPACE_4;
        const float beforeLabelLeft = beforeRight - BIG_SWATCH_WIDTH - theme::SPACE_2 - controls::textWidth("Before");
        ImGui::PopFont();
        const ImRect before(beforeRight - BIG_SWATCH_WIDTH, swatchTop, beforeRight, swatchTop + BIG_SWATCH_HEIGHT);
        controls::drawText(frame.contentLeft,
            beforeLabelLeft - theme::SPACE_3,
            frame.headerTop,
            theme::CONTROL_HEIGHT,
            theme::color::GREEN,
            model::resolveClientText(frame.setting.text, &frame.store));
        drawDetail("Before", beforeLabelLeft, before.Min.x, frame.headerTop, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED);
        drawDetail("Now", nowLabelLeft, now.Min.x, frame.headerTop, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED);
        const auto beforeColor = model::colorOf(frame.setting, _before).value_or(model::colorBase(frame.setting));
        ImGui::SetCursorScreenPos(before.Min);
        if (ImGui::InvisibleButton("before", before.GetSize())) {
            frame.store.set(frame.setting, _before);
            start(frame.setting, _before);
            frame.edited.pulse = true;
        }
        drawSwatch(before, beforeColor, frame.alpha);
        if (ImGui::IsItemHovered()) {
            controls::drawOutline(before.Min, before.Max, theme::color::GREEN);
            frame.point("Before", "The color when the editor opened. Press it to go back to it.");
        }
        return now;
    }

    /**
     * The picker of the red, the green and the blue, which the trigger drags in its square and its hue bar. Its color is
     * set to the nearest bytes, which the file writes.
     */
    void ColorEditor::editPicker(Frame& frame)
    {
        std::array color{ _color.r, _color.g, _color.b, 1.0f };
        ImGui::SetCursorScreenPos(ImVec2(frame.contentLeft, frame.bodyTop));
        ImGui::SetNextItemWidth(PICKER_WIDTH);
        const bool picked = ImGui::ColorPicker4("##picker",
            color.data(),
            ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel);
        if (ImGui::IsItemActivated()) {
            frame.edited.pulse = true;
        }
        if (picked) {
            const auto bytes = render::Color{ color[0], color[1], color[2], 1.0f }.bytes();
            _color = render::Color::rgba(bytes[0], bytes[1], bytes[2], _color.bytes()[3]);
            apply(frame.setting, frame.store, true, false);
        }
        if (frame.hovered && ImRect(frame.contentLeft, frame.bodyTop, frame.contentLeft + PICKER_WIDTH, frame.bodyTop + PICKER_HEIGHT).Contains(frame.mouse)) {
            frame.point("Color", "Drag in the square for how strong and how bright the color is, and in the bar for its hue.");
        }
    }

    /**
     * A line for each channel, beside the picker, the alpha's for a setting that has one.
     */
    void ColorEditor::editChannels(Frame& frame)
    {
        const std::size_t count = frame.alpha ? 4 : 3;
        _sliders.startFrame(_thumbstickSideValue);
        for (std::size_t channel = 0; channel < count; ++channel) {
            editChannel(frame, channel);
        }
        _sliders.endFrame();
    }

    /**
     * The line of a channel: its name, its track and its value, which the trigger drags and the thumbstick steps while the
     * ray is on the line.
     */
    void ColorEditor::editChannel(Frame& frame, const std::size_t channel)
    {
        const float channelsLeft = frame.contentRight - CHANNELS_WIDTH;
        const float lineTop = frame.bodyTop + static_cast<float>(channel) * theme::ROW_PITCH;
        const ImRect line(channelsLeft, lineTop, frame.contentRight, lineTop + theme::CONTROL_HEIGHT);
        const auto valueBox = controls::controlBox(frame.contentRight - theme::VALUE_WIDTH, lineTop, theme::VALUE_WIDTH);
        const float trackLeft = channelsLeft + CHANNEL_LABEL_WIDTH + theme::SPACE_2;
        const ImRect track(trackLeft, lineTop, valueBox.Min.x - theme::SPACE_2, lineTop + theme::CONTROL_HEIGHT);
        const int current = _color.bytes()[channel];

        ImGui::PushID(static_cast<int>(channel));
        ImGui::SetCursorScreenPos(track.Min);
        ImGui::InvisibleButton("track", track.GetSize());
        panelTargets().add(TargetKind::ColorChannel, CHANNELS[channel].name, track);
        // the window is not hovered while the track is held, and the ray stays on its line then
        const bool pointed = line.Contains(frame.mouse) && (frame.hovered || ImGui::IsItemActive());
        if (ImGui::IsItemActivated()) {
            _dragged = channel;
            frame.edited.pulse = true;
        }
        if (_dragged == channel && ImGui::IsItemActive()) {
            if (const int value = valueAt(track, frame.mouse.x); value != current) {
                _color = withChannel(_color, channel, value);
                apply(frame.setting, frame.store, channel < 3, channel == 3);
            }
        } else if (_dragged == channel) {
            _dragged.reset();
        }
        // the thumbstick steps the channel the ray is on, as it steps a setting's slider
        bool write = false;
        bool pulse = false;
        const float stepped = _sliders.edit(_stepped[channel], track.Max.x, lineTop, SliderRow{ .pointed = pointed && !_dragged }, static_cast<float>(current), write, pulse);
        if (write) {
            _color = withChannel(_color, channel, static_cast<int>(stepped));
            apply(frame.setting, frame.store, channel < 3, channel == 3);
            frame.edited.pulse = true;
        }
        ImGui::PopID();

        if (pointed) {
            ImGui::GetWindowDrawList()->AddRectFilled(line.Min, line.Max, ImGui::GetColorU32(theme::color::HOVER), theme::RADIUS_FRAME);
            controls::drawOutline(line.Min, line.Max, theme::color::FRAME_BORDER);
            frame.point(CHANNELS[channel].name, std::string(CHANNELS[channel].help));
        }
        drawDetail(CHANNELS[channel].name, channelsLeft, trackLeft, lineTop, theme::CONTROL_HEIGHT, theme::color::GREEN);
        const int byte = _color.bytes()[channel];
        drawTrack(track, _color, channel, _dragged == channel);
        drawGrab(track, byte, _dragged == channel);
        controls::drawFrame(valueBox);
        controls::drawTextIn(valueBox, std::format("{}", byte), theme::color::GREEN, 1.0f, theme::SPACE_3);
    }

    /**
     * The config spec's presets, in a line under the picker.
     */
    void ColorEditor::editPresets(Frame& frame)
    {
        const auto& presets = frame.setting.valueOptions.presets;
        const float presetsTop = frame.bodyTop + PICKER_HEIGHT + theme::SPACE_3;
        ImGui::PushID("presets");
        for (std::size_t i = 0; i < presets.size(); ++i) {
            editSwatch(frame, static_cast<int>(i), swatchBox(frame.contentLeft, presetsTop, i), presets[i].rgb, presets[i].alpha, presets[i].text);
        }
        ImGui::PopID();
    }

    /**
     * The rule over the footer, and the basics at its left, which every color has.
     */
    void ColorEditor::editBasics(Frame& frame)
    {
        const float ruleY = frame.footerTop - theme::SPACE_3 - theme::BORDER_FRAME * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(frame.contentLeft, ruleY), ImVec2(frame.contentRight, ruleY), ImGui::GetColorU32(theme::color::RULE), theme::BORDER_FRAME);
        ImGui::PushID("basics");
        for (std::size_t i = 0; i < BASICS.size(); ++i) {
            editSwatch(frame, static_cast<int>(i), swatchBox(frame.contentLeft, frame.footerTop, i), BASICS[i].rgb, std::nullopt, BASICS[i].name);
        }
        ImGui::PopID();
    }

    /**
     * A swatch a press on which sets its color: its red, green and blue, and its alpha where it has one and the setting
     * has an alpha. One with no alpha keeps the color's, and is drawn opaque.
     */
    void ColorEditor::editSwatch(Frame& frame, const int place, const ImRect& box, const std::array<int, 3>& rgb, const std::optional<int> presetAlpha, const std::string_view name)
    {
        const bool hasAlpha = frame.alpha && presetAlpha;
        const auto color = render::Color::rgba(rgb[0], rgb[1], rgb[2], hasAlpha ? *presetAlpha : _color.bytes()[3]);
        ImGui::PushID(place);
        ImGui::SetCursorScreenPos(box.Min);
        if (ImGui::InvisibleButton("preset", box.GetSize())) {
            _color = color;
            apply(frame.setting, frame.store, true, hasAlpha);
            frame.edited.pulse = true;
        }
        const bool over = ImGui::IsItemHovered();
        ImGui::PopID();
        drawSwatch(box, color, hasAlpha);
        if (over) {
            controls::drawOutline(box.Min, box.Max, theme::color::GREEN);
            frame.point(name.empty() ? "Preset" : name, std::format("{}. Press it to set the color.", model::colorNumbers(color, hasAlpha)));
        }
    }

    /**
     * Cancel and Done, at the right of the footer: Cancel gives back the color the editor opened with, and Done keeps it.
     * Both close the editor.
     */
    void ColorEditor::drawFooter(Frame& frame)
    {
        const auto done = controls::controlBox(frame.contentRight - FOOTER_BUTTON_WIDTH, frame.footerTop, FOOTER_BUTTON_WIDTH);
        const auto cancelBox = controls::controlBox(done.Min.x - theme::SPACE_2 - FOOTER_BUTTON_WIDTH, frame.footerTop, FOOTER_BUTTON_WIDTH);
        if (panelButton("Cancel", cancelBox, controls::ButtonLook::Quiet)) {
            frame.store.set(frame.setting, _before);
            ImGui::CloseCurrentPopup();
            _open = nullptr;
            frame.edited.pulse = true;
        }
        if (frame.hovered && cancelBox.Contains(frame.mouse)) {
            frame.point("Cancel", "Give back the color it had as the editor opened.");
        }
        if (panelButton("Done", done, controls::ButtonLook::Primary)) {
            ImGui::CloseCurrentPopup();
            _open = nullptr;
            frame.edited.pulse = true;
        }
        if (frame.hovered && done.Contains(frame.mouse)) {
            frame.point("Done", "Keep the color.");
        }
    }

    /**
     * Starts editing a color from the text of a value: its keys, and its color as the framework reads it over the
     * setting's default. One that is no color starts from its default.
     */
    void ColorEditor::start(const model::Setting& setting, const std::string& text)
    {
        _table = model::colorOrDefault(setting, text);
        _color = model::tableColor(_table, model::colorBase(setting));
    }

    /**
     * Sets the color as its channels are now in the store, in its setting's form, the TOML text of its table or an INI
     * line: with the keys it had, and the key of the part that changed, its red, green and blue or its alpha. A color
     * that is RGB only writes no alpha, and keeps one its value has.
     */
    void ColorEditor::apply(const model::Setting& setting, model::Store& store, const bool rgb, const bool alpha)
    {
        const auto bytes = _color.bytes();
        if (rgb) {
            _table.rgb = std::array{ bytes[0], bytes[1], bytes[2] };
        }
        if (alpha && setting.valueOptions.alpha) {
            _table.alpha = bytes[3];
        }
        store.set(setting, model::colorText(_table, setting.tableForm));
    }

    /**
     * Starts a frame of the page, before its rows. The thumbstick's value is how far the one of the hand that points is
     * pushed to a side, from -1 at the left to 1 at the right, which steps the channel the ray is on.
     */
    void ColorEditor::startFrame(const float thumbstickSideValue)
    {
        _thumbstickSideValue = thumbstickSideValue;
    }

    bool ColorEditor::isOpen(const model::Setting& setting) const
    {
        return _open == &setting;
    }

    /**
     * Ends a frame of the page, after its rows: a color whose editor was not drawn in it, as one whose row is not shown
     * any more, has none open.
     */
    void ColorEditor::endFrame()
    {
        if (!_openSeen) {
            _open = nullptr;
        }
        _openSeen = false;
    }

    /**
     * Back in the editor: it gives back the color the row had as the editor opened, and closes the editor, as the next
     * frame draws it. Returns false while no editor is open.
     */
    bool ColorEditor::cancel()
    {
        if (!_open || _cancel) {
            return false;
        }
        _cancel = true;
        return true;
    }

    /**
     * Forgets the editor that is open, as another page is shown: it is of a row of the page before.
     */
    void ColorEditor::reset()
    {
        _open = nullptr;
        _openSeen = false;
        _cancel = false;
        _dragged.reset();
    }
}
