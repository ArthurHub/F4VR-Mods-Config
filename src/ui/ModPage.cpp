#include "ModPage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "Drawing.h"
#include "Theme.h"

namespace
{
    using namespace mods_config;

    /**
     * The width of a row: the same whether or not the page's scroll bar shows.
     */
    float rowWidth()
    {
        return ImGui::GetWindowWidth() - theme::SCROLLBAR_SIZE - theme::SPACE_2;
    }

    /**
     * A text row at the cursor: its lines in the muted color, in from the edges of the rows and wrapped
     * between them.
     */
    void drawTextRow(const model::Text& text)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const float height = ui::drawTextLines(text.lines, ImVec2(pos.x + theme::SPACE_2, pos.y), width - theme::SPACE_2 * 2.0f, theme::color::GREEN_MUTED);
        ImGui::Dummy(ImVec2(width, height));
    }

    /**
     * A row at the cursor, as its text and what it is. It is laid out as a setting's row: the text after the
     * gutter, and the type where the control ends, before the room of its Reset.
     */
    void drawRow(const std::string_view text, const std::string_view type)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        ImGui::Dummy(ImVec2(width, theme::CONTROL_HEIGHT));
        if (!ImGui::IsItemVisible()) {
            return;
        }

        const float typeLeft = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2 - ui::textWidth(type);
        ui::drawText(typeLeft, pos.x + width, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED, type);
        ui::drawText(pos.x + theme::SPACE_6 + theme::SPACE_3, typeLeft - theme::SPACE_3, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, text);
    }

    // The sizes of the controls that are not tokens of the design system, in the panel's pixels.
    // A cell of a switcher, which has two:
    constexpr float TOGGLE_CELL_WIDTH = 88.0f;
    // the track of a slider, and the box of a stepper's option:
    constexpr float TRACK_WIDTH = 280.0f;
    constexpr float SLIDER_FILL_HEIGHT = 8.0f;
    constexpr float GRAB_HEIGHT = 40.0f;
    // the box an arrow is drawn in:
    constexpr float ARROW_SIZE = 14.0f;
    // and the room of a control in its row, which the widest fill: the control column without the room of Reset
    constexpr float CONTROL_ROOM = theme::CONTROL_WIDTH - theme::CONTROL_HEIGHT - theme::SPACE_2;

    enum class Arrow : std::uint8_t
    {
        Left,
        Right,
        Down,
    };

    /**
     * The box of a control that is as high as a row, from its left edge.
     */
    ImRect controlBox(const float left, const float top, const float width)
    {
        return { left, top, left + width, top + theme::CONTROL_HEIGHT };
    }

    /**
     * The frame of a control: the fill of a track, with an outline.
     */
    void drawFrame(const ImRect& box)
    {
        ImGui::GetWindowDrawList()->AddRectFilled(box.Min, box.Max, ImGui::GetColorU32(theme::color::TRACK), theme::RADIUS_FRAME);
        ui::drawOutline(box.Min, box.Max, theme::color::FRAME_BORDER);
    }

    /**
     * A filled arrow around a point, as the buttons of a stepper and a dropdown have.
     */
    void drawArrow(const ImVec2& center, const Arrow arrow)
    {
        // the corners of each arrow in its box, as the design system draws them
        static constexpr std::array<std::array<ImVec2, 3>, 3> CORNERS{ {
            { ImVec2(10.0f, 2.0f), ImVec2(3.0f, 7.0f), ImVec2(10.0f, 12.0f) },
            { ImVec2(4.0f, 2.0f), ImVec2(11.0f, 7.0f), ImVec2(4.0f, 12.0f) },
            { ImVec2(2.0f, 4.0f), ImVec2(12.0f, 4.0f), ImVec2(7.0f, 11.0f) },
        } };
        const auto& corners = CORNERS[static_cast<std::size_t>(arrow)];
        const ImVec2 origin(center.x - ARROW_SIZE * 0.5f, center.y - ARROW_SIZE * 0.5f);
        const auto at = [&origin](const ImVec2& corner) {
            return ImVec2(origin.x + corner.x, origin.y + corner.y);
        };
        ImGui::GetWindowDrawList()->AddTriangleFilled(at(corners[0]), at(corners[1]), at(corners[2]), ImGui::GetColorU32(theme::color::GREEN));
    }

    /**
     * One line of text in a box, in from its sides by a padding. The align sets it in that room: 0 at the
     * left, 0.5 in the middle and 1 at the right. A text that is wider than the room ends in an ellipsis.
     */
    void drawTextIn(const ImRect& box, const std::string_view text, const ImU32 color, const float align, const float padding = 0.0f)
    {
        const float room = box.GetWidth() - padding * 2.0f;
        // on a whole pixel to the left, so a text that just fits its room gets no ellipsis
        const float left = std::floor(box.Min.x + padding + (std::max)(room - ui::textWidth(text), 0.0f) * align);
        ui::drawText(left, box.Max.x - padding, box.Min.y, box.GetHeight(), color, text);
    }

    /**
     * A switcher that ends at the right: an Off and an On cell, with the one of its value marked. On is a
     * solid fill, and Off a ring, so a solid fill always says on. Returns its left edge.
     */
    float drawSwitcher(const float right, const float top, const bool on)
    {
        const auto box = controlBox(right - TOGGLE_CELL_WIDTH * 2.0f, top, TOGGLE_CELL_WIDTH * 2.0f);
        const auto offCell = controlBox(box.Min.x, top, TOGGLE_CELL_WIDTH);
        const auto onCell = controlBox(offCell.Max.x, top, TOGGLE_CELL_WIDTH);
        drawFrame(box);
        if (on) {
            ImGui::GetWindowDrawList()->AddRectFilled(onCell.Min, onCell.Max, ImGui::GetColorU32(theme::color::GREEN), theme::RADIUS_FRAME, ImDrawFlags_RoundCornersRight);
        } else {
            ImGui::GetWindowDrawList()->AddRectFilled(offCell.Min, offCell.Max, ImGui::GetColorU32(theme::color::PRESSED), theme::RADIUS_FRAME, ImDrawFlags_RoundCornersLeft);
            ui::drawOutline(offCell.Min, offCell.Max, theme::color::GREEN);
        }
        drawTextIn(offCell, "Off", on ? theme::color::GREEN_MUTED : theme::color::GREEN, 0.5f);
        drawTextIn(onCell, "On", on ? theme::color::ON_GREEN : theme::color::GREEN_MUTED, 0.5f);
        return box.Min.x;
    }

    /**
     * A value of a slider as its text, with as many decimals as the slider's step has, and three at most.
     */
    std::string sliderText(const float value, const float step)
    {
        int decimals = 0;
        for (float scaled = std::abs(step); decimals < 3 && std::abs(scaled - std::round(scaled)) > 0.001f; scaled *= 10.0f) {
            ++decimals;
        }
        return std::format("{:.{}f}", value, decimals);
    }

    /**
     * A slider that ends at the right: its track, with a fill from the least to the grab, and the box of its
     * value. The grab is whole inside the track at both ends. Returns its left edge.
     */
    float drawSlider(const float right, const float top, const model::ValueOptions& options, const float value)
    {
        const auto valueBox = controlBox(right - theme::VALUE_WIDTH, top, theme::VALUE_WIDTH);
        const auto track = controlBox(valueBox.Min.x - theme::SPACE_2 - TRACK_WIDTH, top, TRACK_WIDTH);
        drawFrame(track);

        const float inset = theme::BORDER_FRAME + theme::GRAB_WIDTH * 0.5f;
        const float fraction = options.max > options.min ? std::clamp((value - options.min) / (options.max - options.min), 0.0f, 1.0f) : 0.0f;
        const float grab = std::lerp(track.Min.x + inset, track.Max.x - inset, fraction);
        const float middle = track.GetCenter().y;
        const auto green = ImGui::GetColorU32(theme::color::GREEN);
        auto* drawList = ImGui::GetWindowDrawList();
        if (fraction > 0.0f) {
            drawList->AddRectFilled(ImVec2(track.Min.x + inset, middle - SLIDER_FILL_HEIGHT * 0.5f), ImVec2(grab, middle + SLIDER_FILL_HEIGHT * 0.5f), green, theme::RADIUS_GRAB);
        }
        drawList->AddRectFilled(ImVec2(grab - theme::GRAB_WIDTH * 0.5f, middle - GRAB_HEIGHT * 0.5f),
            ImVec2(grab + theme::GRAB_WIDTH * 0.5f, middle + GRAB_HEIGHT * 0.5f),
            green,
            theme::RADIUS_GRAB);

        drawFrame(valueBox);
        drawTextIn(valueBox, sliderText(value, options.step), theme::color::GREEN, 1.0f, theme::SPACE_3);
        return track.Min.x;
    }

    /**
     * A stepper that ends at the right: the option in a box, between a button to the option before it and
     * one to the option after. Returns its left edge.
     */
    float drawStepper(const float right, const float top, const std::string_view option)
    {
        const auto next = controlBox(right - theme::CONTROL_HEIGHT, top, theme::CONTROL_HEIGHT);
        const auto box = controlBox(next.Min.x - theme::SPACE_2 - TRACK_WIDTH, top, TRACK_WIDTH);
        const auto previous = controlBox(box.Min.x - theme::SPACE_2 - theme::CONTROL_HEIGHT, top, theme::CONTROL_HEIGHT);

        ui::drawOutline(previous.Min, previous.Max, theme::color::FRAME_BORDER);
        drawArrow(previous.GetCenter(), Arrow::Left);
        drawFrame(box);
        drawTextIn(box, option, theme::color::GREEN, 0.5f, theme::SPACE_3);
        ui::drawOutline(next.Min, next.Max, theme::color::FRAME_BORDER);
        drawArrow(next.GetCenter(), Arrow::Right);
        return previous.Min.x;
    }

    /**
     * A dropdown that ends at the right, closed: the option at the left of its box, and an arrow at the
     * right. Returns its left edge.
     */
    float drawDropdown(const float right, const float top, const std::string_view option)
    {
        const auto box = controlBox(right - CONTROL_ROOM, top, CONTROL_ROOM);
        const float arrowLeft = box.Max.x - theme::SPACE_4 - ARROW_SIZE;
        drawFrame(box);
        ui::drawText(box.Min.x + theme::SPACE_4, arrowLeft - theme::SPACE_2, top, theme::CONTROL_HEIGHT, theme::color::GREEN, option);
        drawArrow(ImVec2(arrowLeft + ARROW_SIZE * 0.5f, box.GetCenter().y), Arrow::Down);
        return box.Min.x;
    }

    /**
     * The row of a setting at the cursor: its text after the gutter, and its control where the control column
     * ends, before the room of its Reset. The text has the room up to its own control.
     * The menu has no values yet, so the control shows what a setting is with none: a switcher is off, a
     * slider is at its least, and a choice is its first option. And the row is disabled, as it changes nothing.
     * A text input is its text and its type until the commit that draws it.
     */
    void drawSetting(const model::Setting& setting)
    {
        if (setting.type == model::SettingType::TextInput) {
            drawRow(setting.text, "textinput");
            return;
        }
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        ImGui::Dummy(ImVec2(width, theme::CONTROL_HEIGHT));
        if (!ImGui::IsItemVisible()) {
            return;
        }

        const auto& options = setting.valueOptions;
        const std::string_view firstOption = options.options.empty() ? std::string_view() : std::string_view(options.options.front().text);
        const float right = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;
        float left = right;

        ImGui::BeginDisabled();
        switch (setting.type) {
        case model::SettingType::Switcher:
            left = drawSwitcher(right, pos.y, false);
            break;
        case model::SettingType::Slider:
            left = drawSlider(right, pos.y, options, options.min);
            break;
        case model::SettingType::Stepper:
            left = drawStepper(right, pos.y, firstOption);
            break;
        case model::SettingType::Dropdown:
            left = drawDropdown(right, pos.y, firstOption);
            break;
        case model::SettingType::TextInput:
            break;
        }
        ui::drawText(pos.x + theme::SPACE_6 + theme::SPACE_3, left - theme::SPACE_3, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, setting.text);
        ImGui::EndDisabled();
    }

    void drawItems(const std::vector<model::Item>& items);

    /**
     * Draws an item as its row. A section is its heading, with the rows of its items under it, and a spacer
     * is empty space as high as its lines of text.
     */
    struct ItemDrawer
    {
        void operator()(const model::Setting& setting) const
        {
            drawSetting(setting);
        }

        void operator()(const model::Group& group) const
        {
            ui::sectionHeading(group.text);
            drawItems(group.items);
        }

        void operator()(const model::Button& button) const
        {
            drawRow(button.text, "button");
        }

        void operator()(const model::Text& text) const
        {
            drawTextRow(text);
        }

        void operator()(const model::Image&) const
        {
            drawRow({}, "image");
        }

        void operator()(const model::Spacer& spacer) const
        {
            ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize() * static_cast<float>(spacer.lines)));
        }

        void operator()(const model::Placeholder& placeholder) const
        {
            drawRow(placeholder.text, placeholder.type.empty() ? "unknown" : std::string_view(placeholder.type));
        }
    };

    void drawItems(const std::vector<model::Item>& items)
    {
        for (const auto& item : items) {
            std::visit(ItemDrawer{}, item);
        }
    }
}

namespace mods_config::ui
{
    /**
     * Draws the rows of a page into the current ImGui window, which scrolls them.
     */
    void ModPage::draw(const model::Page& page) const
    {
        drawItems(page.items);
    }
}
