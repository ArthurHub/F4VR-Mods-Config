#include "Choices.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "Drawing.h"
#include "Theme.h"

namespace
{
    using namespace mods_config;

    // The sizes of a stepper and a dropdown that are not tokens of the design system, in the panel's pixels.
    // The box an arrow is drawn in:
    constexpr float ARROW_SIZE = 14.0f;
    // the box of a dropdown, which fills the room of a control in its row: the control column without the room of Reset
    constexpr float DROPDOWN_WIDTH = theme::CONTROL_WIDTH - theme::CONTROL_HEIGHT - theme::SPACE_2;
    // the room of the mark of the chosen option, before the text of every option of a list
    constexpr float CHECK_SIZE = 20.0f;
    constexpr float CHECK_THICKNESS = 2.5f;
    // the options of a list are this far from each other, and from the edge of the list
    constexpr float LIST_GAP = theme::SPACE_1;
    constexpr float LIST_PADDING = theme::SPACE_2;
    // the options a list shows at most: one with more scrolls
    constexpr std::size_t LIST_MAX_OPTIONS = 5;
    // The thumbstick steps a stepper from this far off its center, of 1 for a full push, as it steps a slider
    constexpr float STEP_PUSH = 0.6f;

    enum class Arrow : std::uint8_t
    {
        Left,
        Right,
        Down,
    };

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
     * The mark of the chosen option of a list, in a box of its size from a corner.
     */
    void drawCheck(const ImVec2& min, const ImU32 color)
    {
        // as the design system draws it, in a box of 24
        constexpr float SCALE = CHECK_SIZE / 24.0f;
        const std::array points{ ImVec2(min.x + 4.0f * SCALE, min.y + 12.5f * SCALE),
            ImVec2(min.x + 9.5f * SCALE, min.y + 18.0f * SCALE),
            ImVec2(min.x + 20.0f * SCALE, min.y + 6.5f * SCALE) };
        ImGui::GetWindowDrawList()->AddPolyline(points.data(), static_cast<int>(points.size()), ImGui::GetColorU32(color), ImDrawFlags_None, CHECK_THICKNESS);
    }

    /**
     * Where the parts of a stepper that ends at the right are: the box of its option, between the button to
     * the option before it and the one to the option after.
     */
    struct StepperBoxes
    {
        ImRect next;
        ImRect option;
        ImRect previous;

        StepperBoxes(const float right, const float top)
            : next(ui::controlBox(right - theme::CONTROL_HEIGHT, top, theme::CONTROL_HEIGHT)),
              option(ui::controlBox(next.Min.x - theme::SPACE_2 - ui::TRACK_WIDTH, top, ui::TRACK_WIDTH)),
              previous(ui::controlBox(option.Min.x - theme::SPACE_2 - theme::CONTROL_HEIGHT, top, theme::CONTROL_HEIGHT))
        {}

        /**
         * Whether a place of the row is on a button. A button is pressed a little beside it too, and in
         * the gap between it and the box of the option.
         */
        bool onPrevious(const float x) const
        {
            return x >= previous.Min.x - theme::SPACE_1 && x < option.Min.x;
        }

        bool onNext(const float x) const
        {
            return x >= option.Max.x && x < next.Max.x + theme::SPACE_1;
        }
    };

    /**
     * A button of a stepper, with its arrow. One that goes nowhere is disabled. Any other has the look of
     * a control under the ray while the ray is on it, and a stronger one while the trigger is held on it.
     */
    void drawStepButton(const ImRect& box, const Arrow arrow, const bool enabled, const bool pointed, const bool held)
    {
        ImGui::BeginDisabled(!enabled);
        ui::drawButtonFrame(box, enabled && pointed, enabled && held);
        drawArrow(box.GetCenter(), arrow);
        ImGui::EndDisabled();
    }

    ImRect dropdownBox(const float right, const float top)
    {
        return ui::controlBox(right - DROPDOWN_WIDTH, top, DROPDOWN_WIDTH);
    }

    /**
     * Where the list of a dropdown is, by the box of the dropdown and the number of its options: as wide as
     * the box, and as high as its options, up to the most it shows. It is under the box, or over it where
     * the page has no room under it. Where the page has room for neither, it is at the bottom of the page,
     * over the box. The list is so never over the title bar and the footer.
     */
    ImRect listBox(const ImRect& box, const std::size_t options)
    {
        const auto& page = ImGui::GetCurrentWindow()->InnerRect;
        const auto shown = static_cast<float>((std::min)(options, LIST_MAX_OPTIONS));
        const float height = (std::min)(shown * (theme::CONTROL_HEIGHT + LIST_GAP) - LIST_GAP + LIST_PADDING * 2.0f, page.GetHeight());
        float top = page.Max.y - height;
        if (box.Max.y + height <= page.Max.y) {
            top = box.Max.y;
        } else if (box.Min.y - height >= page.Min.y) {
            top = box.Min.y - height;
        }
        return { box.Min.x, top, box.Max.x, top + height };
    }

    /**
     * An option of the list that is open, at the cursor: the mark of the chosen one, and its text. The
     * chosen option is a solid fill, and any other has the look of a row under the ray.
     * Returns whether it was pressed.
     */
    bool listOption(const std::string_view text, const bool chosen, const float width)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const ImVec2 max(pos.x + width, pos.y + theme::CONTROL_HEIGHT);
        const bool pressed = ImGui::InvisibleButton("option", ImVec2(width, theme::CONTROL_HEIGHT));
        auto color = theme::color::GREEN;
        if (chosen) {
            ImGui::GetWindowDrawList()->AddRectFilled(pos, max, ImGui::GetColorU32(theme::color::GREEN), theme::RADIUS_FRAME);
            drawCheck(ImVec2(pos.x + theme::SPACE_3, pos.y + (theme::CONTROL_HEIGHT - CHECK_SIZE) * 0.5f), theme::color::ON_GREEN);
            color = theme::color::ON_GREEN;
        } else {
            ui::drawRowState();
        }
        ui::drawText(pos.x + theme::SPACE_3 + CHECK_SIZE + theme::SPACE_3, max.x - theme::SPACE_4, pos.y, theme::CONTROL_HEIGHT, color, text);
        return pressed;
    }
}

namespace mods_config::ui
{
    /**
     * The place of the option a choice is at, among its options: the one whose value is the stored one, and
     * none where no option has it. A choice whose value is not available is at its first option.
     */
    std::optional<std::size_t> chosenOption(const model::Setting& setting, const model::StoredValue& stored)
    {
        const auto& options = setting.valueOptions.options;
        if (!stored.available) {
            return options.empty() ? std::nullopt : std::optional<std::size_t>(0);
        }
        const auto option = std::ranges::find(options, stored.value, &model::Option::value);
        return option != options.end() ? std::optional(static_cast<std::size_t>(option - options.begin())) : std::nullopt;
    }

    /**
     * Starts a frame of the page, before its rows. The thumbstick's value is how far the one of the hand
     * that points is pushed to a side, from -1 at the left to 1 at the right: a push is one step, in the
     * frame it begins, and a push that is held steps no more.
     * A list that Back closed, or that is of the page before, is closed here: ImGui closes a list inside
     * a frame.
     */
    void Choices::startFrame(const float thumbstickSideValue)
    {
        const int side = thumbstickSideValue >= STEP_PUSH ? 1 : thumbstickSideValue <= -STEP_PUSH ? -1 : 0;
        _step = side != _pushSide ? side : 0;
        _pushSide = side;
        _openSeen = false;
        if (_close) {
            ImGui::ClosePopupsExceptModals();
            _close = false;
        }
    }

    /**
     * The option the player picks in this frame for a stepper or a dropdown that ends at the right, by its
     * place among the options, and none where they pick none. The setting has an option at least.
     * A press on a button of a stepper goes to the option before or after, and stops at the first and the
     * last. A press anywhere else on its row goes to the option after, and from the last to the first, as
     * in MCM's menu. A push of the thumbstick to a side, while the ray is on the row, goes to the option
     * before or after as the buttons do.
     * A press on the row of a dropdown opens its list, which tells that the hand gets its pulse.
     */
    std::optional<std::size_t> Choices::edit(const model::Setting& setting, const float right, const float top, const ChoiceRow& row, const std::optional<std::size_t> chosen,
        bool& pulse)
    {
        if (setting.type == model::SettingType::Dropdown) {
            return editList(setting, right, top, row, chosen, pulse);
        }
        const StepperBoxes boxes(right, top);
        const auto count = setting.valueOptions.options.size();
        const auto after = chosen ? *chosen + 1 : 0;
        const float x = ImGui::GetIO().MousePos.x;
        const bool pushed = row.pointed && _step != 0;
        if (!row.pressed && !pushed) {
            return std::nullopt;
        }
        if (row.pressed ? boxes.onPrevious(x) : _step < 0) {
            return chosen && *chosen > 0 ? std::optional(*chosen - 1) : std::nullopt;
        }
        if (row.pressed ? boxes.onNext(x) : _step > 0) {
            return after < count ? std::optional(after) : std::nullopt;
        }
        return after % count;
    }

    /**
     * The list of a dropdown: a press on the row opens it, at the box of the dropdown. One with many
     * options scrolls, from the chosen option as it opens. A press on an option picks it and closes the
     * list, and a press beside the list closes it and does nothing else: ImGui gives the page under an open
     * list no press.
     * Returns the option that was picked, and none.
     */
    std::optional<std::size_t> Choices::editList(const model::Setting& setting, const float right, const float top, const ChoiceRow& row, const std::optional<std::size_t> chosen,
        bool& pulse)
    {
        const auto id = ImGui::GetID("list");
        if (row.pressed) {
            ImGui::OpenPopupEx(id);
            pulse = true;
        }
        if (!ImGui::IsPopupOpen(id, ImGuiPopupFlags_None)) {
            return std::nullopt;
        }
        _open = &setting;
        _openSeen = true;

        const auto& options = setting.valueOptions.options;
        const auto list = listBox(dropdownBox(right, top), options.size());
        ImGui::SetNextWindowPos(list.Min);
        ImGui::SetNextWindowSize(list.GetSize());
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(LIST_PADDING, LIST_PADDING));
        ImGui::PushStyleColor(ImGuiCol_Border, theme::color::GREEN);
        const bool shown = ImGui::BeginPopupEx(id, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        if (!shown) {
            return std::nullopt;
        }

        std::optional<std::size_t> picked;
        // the options end a little before a scroll bar
        const float width = ImGui::GetContentRegionAvail().x - (ImGui::GetCurrentWindow()->ScrollbarY ? LIST_PADDING : 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, LIST_GAP));
        for (std::size_t i = 0; i < options.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            if (listOption(options[i].text, chosen == i, width)) {
                picked = i;
                ImGui::CloseCurrentPopup();
            }
            if (chosen == i && ImGui::IsWindowAppearing()) {
                ImGui::SetScrollHereY();
            }
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        ImGui::EndPopup();
        return picked;
    }

    /**
     * A stepper or a dropdown that ends at the right, with the text of the option it is at, and none where
     * it is at no option. Returns its left edge.
     * A stepper is the option in a box, between a button to the option before it and one to the option
     * after. A dropdown is the option at the left of its box, and an arrow at the right. The box has its
     * outline in the full color while its list is open.
     */
    float Choices::draw(const model::Setting& setting, const float right, const float top, const ChoiceRow& row, const std::optional<std::size_t> chosen) const
    {
        const auto& options = setting.valueOptions.options;
        const auto option = chosen ? std::string_view(options[*chosen].text) : std::string_view();

        if (setting.type == model::SettingType::Dropdown) {
            const auto box = dropdownBox(right, top);
            const float arrowLeft = box.Max.x - theme::SPACE_4 - ARROW_SIZE;
            drawFrame(box);
            if (isListOpen(setting)) {
                drawOutline(box.Min, box.Max, theme::color::GREEN);
            }
            drawText(box.Min.x + theme::SPACE_4, arrowLeft - theme::SPACE_2, top, theme::CONTROL_HEIGHT, theme::color::GREEN, option);
            drawArrow(ImVec2(arrowLeft + ARROW_SIZE * 0.5f, box.GetCenter().y), Arrow::Down);
            return box.Min.x;
        }

        const StepperBoxes boxes(right, top);
        const float x = ImGui::GetIO().MousePos.x;
        const bool onPrevious = boxes.onPrevious(x);
        const bool onNext = boxes.onNext(x);
        const auto after = chosen ? *chosen + 1 : 0;
        drawStepButton(boxes.previous, Arrow::Left, chosen && *chosen > 0, row.pointed && onPrevious, row.held && onPrevious);
        drawFrame(boxes.option);
        drawTextIn(boxes.option, option, theme::color::GREEN, 0.5f, theme::SPACE_3);
        drawStepButton(boxes.next, Arrow::Right, after < options.size(), row.pointed && onNext, row.held && onNext);
        return boxes.previous.Min.x;
    }

    /**
     * Ends a frame of the page, after its rows: a dropdown whose list was not open in it has none.
     */
    void Choices::endFrame()
    {
        if (!_openSeen) {
            _open = nullptr;
        }
    }

    bool Choices::isListOpen(const model::Setting& setting) const
    {
        return _open == &setting;
    }

    /**
     * Closes the list that is open, with no option picked, for Back. Returns false while none is open.
     */
    bool Choices::closeList()
    {
        if (!_open || _close) {
            return false;
        }
        _close = true;
        return true;
    }

    /**
     * Forgets the dropdown whose list is open, as another page is shown: it is a row of the page before.
     * Its list is closed. A push of the thumbstick that is held is still not a new one.
     */
    void Choices::reset()
    {
        _open = nullptr;
        _openSeen = false;
        _close = true;
        _step = 0;
    }
}
