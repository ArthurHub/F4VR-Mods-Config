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

    using ui::ARROW_SIZE;
    using ui::ControlArrow;
    using ui::DROPDOWN_WIDTH;

    // The sizes of a stepper and a dropdown that are not tokens of the design system, in the panel's pixels.
    // The options of a list are this far from each other, and from the edge of the list
    constexpr float LIST_GAP = theme::SPACE_1;
    constexpr float LIST_PADDING = theme::SPACE_2;
    // the options a list shows at most: one with more scrolls
    constexpr std::size_t LIST_MAX_OPTIONS = 5;
    // The thumbstick steps a stepper from this far off its center, of 1 for a full push, as it steps a slider
    constexpr float STEP_PUSH = 0.6f;

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
    void drawStepButton(const ImRect& box, const ControlArrow arrow, const bool enabled, const bool pointed, const bool held)
    {
        ImGui::BeginDisabled(!enabled);
        ui::drawButtonFrame(box, enabled && pointed, enabled && held);
        ui::drawControlArrow(box.GetCenter(), arrow);
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

}

namespace mods_config::ui
{
    /**
     * The place of the option a choice is at, among its options: the one whose value is the stored one, and
     * none where no option has it. A choice whose value is not available is at its first option.
     */
    std::optional<std::size_t> chosenOption(const std::vector<model::Option>& options, const model::StoredValue& stored)
    {
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
    std::optional<std::size_t> Choices::edit(const model::Setting& setting, const std::vector<model::Option>& options, const float right, const float top, const ChoiceRow& row,
        const std::optional<std::size_t> chosen, bool& pulse)
    {
        if (setting.type == model::SettingType::Dropdown) {
            return editList(setting, options, right, top, row, chosen, pulse);
        }
        const StepperBoxes boxes(right, top);
        const auto count = options.size();
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
    std::optional<std::size_t> Choices::editList(const model::Setting& setting, const std::vector<model::Option>& options, const float right, const float top, const ChoiceRow& row,
        const std::optional<std::size_t> chosen, bool& pulse)
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
     * A stepper or a dropdown that ends at the right, with the text of the option it is at in the color of
     * its value's look, and none where it is at no option. Returns its left edge.
     * A stepper is the option in a box, between a button to the option before it and one to the option
     * after. A dropdown is the option at the left of its box, and an arrow at the right. The box has its
     * outline in the full color while its list is open.
     */
    float Choices::draw(const model::Setting& setting, const std::vector<model::Option>& options, const float right, const float top, const ChoiceRow& row,
        const std::optional<std::size_t> chosen, const ValueLook look) const
    {
        const auto option = chosen ? std::string_view(options[*chosen].text) : std::string_view();

        if (setting.type == model::SettingType::Dropdown) {
            const auto box = dropdownBox(right, top);
            const float arrowLeft = box.Max.x - theme::SPACE_4 - ARROW_SIZE;
            drawFrame(box, look);
            if (isListOpen(setting)) {
                drawOutline(box.Min, box.Max, theme::color::GREEN);
            }
            drawText(box.Min.x + theme::SPACE_4, arrowLeft - theme::SPACE_2, top, theme::CONTROL_HEIGHT, valueColor(look), option);
            ui::drawControlArrow(ImVec2(arrowLeft + ARROW_SIZE * 0.5f, box.GetCenter().y), ControlArrow::Down);
            return box.Min.x;
        }

        const StepperBoxes boxes(right, top);
        const float x = ImGui::GetIO().MousePos.x;
        const bool onPrevious = boxes.onPrevious(x);
        const bool onNext = boxes.onNext(x);
        const auto after = chosen ? *chosen + 1 : 0;
        drawStepButton(boxes.previous, ControlArrow::Left, chosen && *chosen > 0, row.pointed && onPrevious, row.held && onPrevious);
        drawFrame(boxes.option, look);
        drawTextIn(boxes.option, option, valueColor(look), 0.5f, theme::SPACE_3);
        drawStepButton(boxes.next, ControlArrow::Right, after < options.size(), row.pointed && onNext, row.held && onNext);
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
