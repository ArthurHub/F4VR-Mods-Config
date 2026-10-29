#include "ColorEditor.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string_view>

#include "Colors.h"
#include "Drawing.h"
#include "Theme.h"

namespace
{
    using namespace mods_config;

    // The sizes of the editor that are not tokens of the design system, in the panel's pixels.
    constexpr float EDITOR_WIDTH = 717.0f;
    constexpr float EDITOR_HEIGHT = 414.0f;

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
        ui::drawOutline(track.Min, track.Max, dragged ? theme::color::GLOW : theme::color::FRAME_BORDER);
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
        ui::drawText(left, right, top, height, color, text);
        ImGui::PopFont();
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
     * Opens the editor of a color with a press on its row, and draws it while it is open, with the color the row had:
     * one that is no color starts from its default. The editor is a popup in the page, under the row, or over it where
     * the page has no room under it, or as near as it fits: never over the title bar and the footer.
     * The picker sets the red, the green and the blue at once, and a channel's slider its channel, which the trigger
     * drags and the thumbstick steps by 1. Each change sets the color in the store as it is made, with the keys its value
     * had and the key of the part that changed. Before gives back the color it had as it opened.
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

        const bool alpha = setting.valueOptions.alpha;
        const auto& page = ImGui::GetCurrentWindow()->InnerRect;
        const float left = std::clamp(std::floor(row.GetCenter().x - EDITOR_WIDTH * 0.5f), page.Min.x, (std::max)(page.Min.x, page.Max.x - EDITOR_WIDTH));
        float top = (std::max)(page.Min.y, page.Max.y - EDITOR_HEIGHT);
        if (row.Max.y + EDITOR_HEIGHT <= page.Max.y) {
            top = row.Max.y;
        } else if (row.Min.y - EDITOR_HEIGHT >= page.Min.y) {
            top = row.Min.y - EDITOR_HEIGHT;
        }
        ImGui::SetNextWindowPos(ImVec2(left, top));
        ImGui::SetNextWindowSize(ImVec2(EDITOR_WIDTH, EDITOR_HEIGHT));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(theme::SPACE_4, theme::SPACE_4));
        ImGui::PushStyleColor(ImGuiCol_Border, theme::color::GREEN);
        const bool shown = ImGui::BeginPopupEx(id,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        if (!shown) {
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
        const float contentLeft = origin.x + theme::SPACE_4;
        const float contentRight = origin.x + EDITOR_WIDTH - theme::SPACE_4;
        const float headerTop = origin.y + theme::SPACE_4;
        const float bodyTop = headerTop + theme::CONTROL_HEIGHT + theme::SPACE_3;
        const bool hovered = ImGui::IsWindowHovered();
        const auto& mouse = ImGui::GetIO().MousePos;
        const auto point = [&](const std::string_view name, std::string help) {
            edited.pointed = PointedRow{ .text = name, .help = std::move(help) };
        };

        // the header: the setting's name, and the color as it was and as it is, a press on the first giving it back
        const float nowLeft = contentRight - BIG_SWATCH_WIDTH;
        const float swatchTop = headerTop + (theme::CONTROL_HEIGHT - BIG_SWATCH_HEIGHT) * 0.5f;
        const ImRect now(nowLeft, swatchTop, contentRight, swatchTop + BIG_SWATCH_HEIGHT);
        ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
        const float nowLabelLeft = nowLeft - theme::SPACE_2 - textWidth("Now");
        const float beforeRight = nowLabelLeft - theme::SPACE_4;
        const float beforeLabelLeft = beforeRight - BIG_SWATCH_WIDTH - theme::SPACE_2 - textWidth("Before");
        ImGui::PopFont();
        const ImRect before(beforeRight - BIG_SWATCH_WIDTH, swatchTop, beforeRight, swatchTop + BIG_SWATCH_HEIGHT);
        drawText(contentLeft, beforeLabelLeft - theme::SPACE_3, headerTop, theme::CONTROL_HEIGHT, theme::color::GREEN, setting.text);
        drawDetail("Before", beforeLabelLeft, before.Min.x, headerTop, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED);
        drawDetail("Now", nowLabelLeft, now.Min.x, headerTop, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED);
        const auto beforeColor = model::colorOf(setting, _before).value_or(model::colorBase(setting));
        ImGui::SetCursorScreenPos(before.Min);
        if (ImGui::InvisibleButton("before", before.GetSize())) {
            store.set(setting, _before);
            start(setting, _before);
            edited.pulse = true;
        }
        drawSwatch(before, beforeColor, alpha);
        if (ImGui::IsItemHovered()) {
            drawOutline(before.Min, before.Max, theme::color::GREEN);
            point("Before", "The color when the editor opened. Press it to go back to it.");
        }

        // the picker of the red, the green and the blue, which the trigger drags in its square and its hue bar
        std::array color{ _color.r, _color.g, _color.b, 1.0f };
        ImGui::SetCursorScreenPos(ImVec2(contentLeft, bodyTop));
        ImGui::SetNextItemWidth(PICKER_WIDTH);
        const bool picked = ImGui::ColorPicker4("##picker",
            color.data(),
            ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoInputs |
                ImGuiColorEditFlags_NoLabel);
        if (ImGui::IsItemActivated()) {
            edited.pulse = true;
        }
        if (picked) {
            // the picker's color to the nearest bytes, which the file writes
            const auto bytes = render::Color{ color[0], color[1], color[2], 1.0f }.bytes();
            _color = render::Color::rgba(bytes[0], bytes[1], bytes[2], _color.bytes()[3]);
            apply(setting, store, true, false);
        }
        if (hovered && ImRect(contentLeft, bodyTop, contentLeft + PICKER_WIDTH, bodyTop + PICKER_HEIGHT).Contains(mouse)) {
            point("Color", "Drag in the square for how strong and how bright the color is, and in the bar for its hue.");
        }

        // a line for each channel: its name, its track and its value, which the trigger drags and the thumbstick steps
        const float channelsLeft = contentRight - CHANNELS_WIDTH;
        const std::size_t count = alpha ? 4 : 3;
        _sliders.startFrame(_thumbstickSideValue);
        for (std::size_t i = 0; i < count; ++i) {
            const float lineTop = bodyTop + static_cast<float>(i) * theme::ROW_PITCH;
            const ImRect line(channelsLeft, lineTop, contentRight, lineTop + theme::CONTROL_HEIGHT);
            const auto valueBox = controlBox(contentRight - theme::VALUE_WIDTH, lineTop, theme::VALUE_WIDTH);
            const float trackLeft = channelsLeft + CHANNEL_LABEL_WIDTH + theme::SPACE_2;
            const ImRect track(trackLeft, lineTop, valueBox.Min.x - theme::SPACE_2, lineTop + theme::CONTROL_HEIGHT);
            const bool pointed = hovered && line.Contains(mouse);
            const int current = _color.bytes()[i];

            ImGui::PushID(static_cast<int>(i));
            ImGui::SetCursorScreenPos(track.Min);
            ImGui::InvisibleButton("track", track.GetSize());
            if (ImGui::IsItemActivated()) {
                _dragged = i;
                edited.pulse = true;
            }
            if (_dragged == i && ImGui::IsItemActive()) {
                if (const int value = valueAt(track, mouse.x); value != current) {
                    _color = withChannel(_color, i, value);
                    apply(setting, store, i < 3, i == 3);
                }
            } else if (_dragged == i) {
                _dragged.reset();
            }
            // the thumbstick steps the channel the ray is on, as it steps a setting's slider
            bool write = false;
            bool pulse = false;
            const float stepped = _sliders.edit(_stepped[i], track.Max.x, lineTop, SliderRow{ .pointed = pointed && !_dragged }, static_cast<float>(current), write, pulse);
            if (write) {
                _color = withChannel(_color, i, static_cast<int>(stepped));
                apply(setting, store, i < 3, i == 3);
                edited.pulse = true;
            }
            ImGui::PopID();

            if (pointed) {
                ImGui::GetWindowDrawList()->AddRectFilled(line.Min, line.Max, ImGui::GetColorU32(theme::color::HOVER), theme::RADIUS_FRAME);
                drawOutline(line.Min, line.Max, theme::color::FRAME_BORDER);
                point(CHANNELS[i].name, std::string(CHANNELS[i].help));
            }
            drawDetail(CHANNELS[i].name, channelsLeft, trackLeft, lineTop, theme::CONTROL_HEIGHT, theme::color::GREEN);
            const int byte = _color.bytes()[i];
            drawTrack(track, _color, i, _dragged == i);
            drawGrab(track, byte, _dragged == i);
            drawFrame(valueBox);
            drawTextIn(valueBox, std::format("{}", byte), theme::color::GREEN, 1.0f, theme::SPACE_3);
        }
        _sliders.endFrame();

        // Now, as the color is after the changes of this frame
        drawSwatch(now, _color, alpha);

        // the footer: Cancel and Done, under a rule
        const float footerTop = origin.y + EDITOR_HEIGHT - theme::SPACE_4 - theme::CONTROL_HEIGHT;
        const float ruleY = footerTop - theme::SPACE_3 - theme::BORDER_FRAME * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(contentLeft, ruleY), ImVec2(contentRight, ruleY), ImGui::GetColorU32(theme::color::RULE), theme::BORDER_FRAME);
        const auto done = controlBox(contentRight - FOOTER_BUTTON_WIDTH, footerTop, FOOTER_BUTTON_WIDTH);
        const auto cancelBox = controlBox(done.Min.x - theme::SPACE_2 - FOOTER_BUTTON_WIDTH, footerTop, FOOTER_BUTTON_WIDTH);
        if (button("Cancel", cancelBox, ButtonLook::Quiet)) {
            store.set(setting, _before);
            ImGui::CloseCurrentPopup();
            _open = nullptr;
            edited.pulse = true;
        }
        if (hovered && cancelBox.Contains(mouse)) {
            point("Cancel", "Give back the color it had as the editor opened.");
        }
        if (button("Done", done, ButtonLook::Primary)) {
            ImGui::CloseCurrentPopup();
            _open = nullptr;
            edited.pulse = true;
        }
        if (hovered && done.Contains(mouse)) {
            point("Done", "Keep the color.");
        }
        ImGui::EndPopup();
        return edited;
    }

    /**
     * Starts editing a color from the text of a value: its keys, and its color as the framework reads it over the
     * setting's default. One that is no color starts from its default.
     */
    void ColorEditor::start(const model::Setting& setting, const std::string& text)
    {
        auto table = model::parseColorTable(text);
        if (!table && setting.defaultValue) {
            table = model::parseColorTable(std::get<std::string>(*setting.defaultValue));
        }
        _table = table ? *table : model::ColorTable{};
        _color = model::tableColor(_table, model::colorBase(setting));
    }

    /**
     * Sets the color as its channels are now in the store, as the TOML text of its table: with the keys it had, and the
     * key of the part that changed, its red, green and blue or its alpha. A color that is RGB only writes no alpha, and
     * keeps one its value has.
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
        store.set(setting, model::colorText(_table));
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
