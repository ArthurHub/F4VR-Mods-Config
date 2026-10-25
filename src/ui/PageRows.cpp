#include "PageRows.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"
#include "model/Conditions.h"

namespace
{
    // what the help line says of every row that is only shown, before its own help: it cannot be changed in the menu
    constexpr std::string_view READ_ONLY = "(Read-only)";

}

namespace mods_config::ui
{
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
                drawRowState();
            }
        }
        return row;
    }

    /**
     * Adds the row of a heading that folds at the cursor, by its id, as a section's heading of a page is: a row
     * that is pressed, with the look of a row under the ray. A press folds or opens it. The caller draws the
     * heading in it with `drawFoldingHeading`.
     */
    Row addFoldingRow(const char* id, bool& folded)
    {
        ImGui::PushID(id);
        const auto row = addRow(true);
        ImGui::PopID();
        if (row.pressed) {
            folded = !folded;
        }
        return row;
    }

    /**
     * Draws a row's plain or formatted label `space-3` inside the row's frame, with its marks after it, each
     * `space-2` after what is before it. The label shortens before a mark does, so the marks always show.
     */
    void drawLabel(const ImVec2& pos, const float right, const std::string_view text, const std::span<const model::TextRun> runs, const RowMarks& marks)
    {
        const float left = pos.x + theme::SPACE_3;
        float textRight = right;
        const float pendingWidth = pendingMarkWidth(marks.applies);
        if (marks.pending) {
            textRight -= theme::SPACE_2 + pendingWidth;
        }
        if (marks.mark != TextMark::None) {
            textRight -= theme::SPACE_2 + WARNING_MARK_WIDTH;
        }
        float width = 0.0f;
        if (runs.empty()) {
            drawText(left, textRight, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, text);
            width = text.empty() ? 0.0f : (std::min)(textWidth(text), textRight - left);
        } else {
            width = drawTextRuns(left, textRight, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, runs);
        }
        const float middle = pos.y + theme::CONTROL_HEIGHT * 0.5f;
        float markLeft = left + width + (width > 0.0f ? theme::SPACE_2 : 0.0f);
        if (marks.pending) {
            drawAppliesMark(marks.applies, ImVec2(markLeft + pendingWidth * 0.5f, middle));
            markLeft += pendingWidth + theme::SPACE_2;
        }
        drawMark(marks.mark, ImVec2(markLeft + WARNING_MARK_WIDTH * 0.5f, middle));
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

}
