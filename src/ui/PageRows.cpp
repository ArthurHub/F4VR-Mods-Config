#include "PageRows.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <utility>
#include <variant>

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
     * Draws a row's plain or formatted label `space-3` inside the row's frame, with its marks after it, each
     * `space-2` after what is before it. The label shortens before a mark does, so the marks always show.
     * With a query, where it matches a plain label is underlined, as the tree's names are while searching.
     */
    void drawLabel(const ImVec2& pos, const float right, const std::string_view text, const std::span<const model::TextRun> runs, const RowMarks& marks,
        const model::SearchQuery* query)
    {
        const float left = pos.x + look().space3;
        float textRight = right;
        const float pendingWidth = pendingMarkWidth(marks.applies);
        if (marks.pending) {
            textRight -= look().space2 + pendingWidth;
        }
        if (marks.mark != controls::Mark::None) {
            textRight -= look().space2 + controls::MARK_WIDTH;
        }
        float width = 0.0f;
        if (runs.empty()) {
            controls::drawText(left, textRight, pos.y, look().controlHeight, colors().accent, text);
            if (query) {
                controls::drawUnderlines(left, textRight, pos.y, look().controlHeight, colors().accent, text, model::matchRanges(*query, text));
            }
            width = text.empty() ? 0.0f : (std::min)(controls::textWidth(text), textRight - left);
        } else {
            width = drawTextRuns(left, textRight, pos.y, look().controlHeight, colors().accent, runs);
        }
        const float middle = pos.y + look().controlHeight * 0.5f;
        float markLeft = left + width + (width > 0.0f ? look().space2 : 0.0f);
        if (marks.pending) {
            drawAppliesMark(marks.applies, ImVec2(markLeft + pendingWidth * 0.5f, middle));
            markLeft += pendingWidth + look().space2;
        }
        controls::drawMark(marks.mark, ImVec2(markLeft + controls::MARK_WIDTH * 0.5f, middle));
    }

    /**
     * Whether an item is shown, as the page gives it room and devbench lists it: one with a condition is, while it holds on the values of the store. With no
     * store no value is available, so a condition on a value holds, and one that is settled without a value,
     * as a config spec's condition on a plugin, is asked as any other.
     */
    bool isShown(const model::Item& item, model::StoreReader* store)
    {
        const auto& visibleWhen = std::visit(
            [](const auto& row) -> const std::optional<model::Condition>& {
                return row.visibleWhen;
            },
            item);
        return !visibleWhen || model::holds(*visibleWhen, store);
    }

    /**
     * The text of the first warning that holds on the values the store has now, and none where none does.
     * A warning that a value which is not available leaves open does not hold.
     */
    std::string_view warningOf(const std::vector<model::Warning>& warnings, model::StoreReader* store)
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
