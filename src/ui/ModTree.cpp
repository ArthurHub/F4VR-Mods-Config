#include "ModTree.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"
#include "model/Conditions.h"

namespace
{
    using namespace mods_config;

    // The tree's sizes that are not tokens of the design system, in the panel's pixels.
    // Its header:
    constexpr float HEADER_HEIGHT = 40.0f;
    // the room it keeps on the right of its rows, for its scroll bar with space-1 on each side:
    constexpr float RIGHT_ROOM = theme::SPACE_1 + theme::SCROLLBAR_SIZE + theme::SPACE_1;
    // the column the arrow of a row is in the middle of, which every row keeps:
    constexpr float ARROW_COLUMN = 16.0f;
    // and the gap from it to the name, 2 px less than space-2:
    constexpr float ARROW_GAP = theme::SPACE_2 - 2.0f;

    // the deepest level a row is set in to: a row further in is set in as one of it, so its name keeps its room
    constexpr int MAX_INDENT_LEVEL = 3;

    // the tag of an MCM mod's row, and what the help line says of it, as its change is made at once
    constexpr auto MCM_TAG = "MCM";
    constexpr auto MCM_HELP = "An MCM menu: a change is made in the game as you make it. Keep accepts it, and Revert sets it back.";
    // a tag's font size, a little over half the detail size, the room between its text and its outline, and its
    // gap from the row's right edge
    constexpr float TAG_FONT_SIZE = 10.0f;
    constexpr float TAG_PADDING = 1.0f;
    constexpr float TAG_GAP = theme::SPACE_2;

    /**
     * The arrow before the name of a row: none for a page with no pages, down for a row whose pages are the
     * rows under it, and right for any other mod or page.
     */
    enum class Arrow : std::uint8_t
    {
        None,
        Closed,
        Open,
    };

    /**
     * How far a row of a level is set in, from a mod at 0: a step for each level, up to the deepest.
     */
    float indentOf(const int level)
    {
        return theme::SPACE_4 * static_cast<float>((std::min)(level, MAX_INDENT_LEVEL));
    }

    /**
     * The arrow of a row, in the middle of its column, which starts at `left`. It points down at a row whose
     * pages are the rows under it, and right at any other. It is faded in a disabled row, as the row's text is.
     */
    void drawArrow(const float left, const float rowTop, const bool open, const ImU32 color)
    {
        const ImVec2 box(left + (ARROW_COLUMN - ui::FOLD_ARROW_SIZE) * 0.5f, rowTop + (theme::CONTROL_HEIGHT - ui::FOLD_ARROW_SIZE) * 0.5f);
        ui::drawFoldArrow(box, open, color);
    }

    /**
     * A row's tag, ending at `right` in the middle of the row: its text in green in an outline close around it,
     * in the color of a section's rule, or both in the color of a text on a fill. Returns its left edge.
     */
    float drawRowTag(const float right, const float rowTop, const std::string_view tag, const bool onFill)
    {
        ImGui::PushFont(nullptr, TAG_FONT_SIZE);
        const float inside = TAG_PADDING + theme::BORDER_FRAME;
        const float height = TAG_FONT_SIZE + inside * 2.0f;
        const float width = std::ceil(ui::textWidth(tag) + inside * 2.0f);
        const ImVec2 min(std::floor(right - width), std::floor(rowTop + (theme::CONTROL_HEIGHT - height) * 0.5f));
        const ImVec2 max(min.x + width, min.y + height);
        ui::drawOutline(min, max, onFill ? theme::color::ON_GREEN : theme::color::RULE);
        ui::drawText(min.x + inside, max.x, min.y, height, onFill ? theme::color::ON_GREEN : theme::color::GREEN, tag);
        ImGui::PopFont();
        return min.x;
    }

    /**
     * The name of a row from its left, and its tag at the row's right where it has one, as an MCM mod's: the tag
     * is inside the row, so the row's look under the ray has it, and the name ends in an ellipsis before it, so
     * the tags of the rows line up.
     */
    void drawName(const float left, const ImVec2& min, const ImVec2& max, const ImU32 color, const std::string_view name, const std::string_view tag, const bool onFill)
    {
        float right = max.x - theme::SPACE_3;
        if (!tag.empty()) {
            right = drawRowTag(max.x - TAG_GAP, min.y, tag, onFill) - theme::SPACE_2;
        }
        ui::drawText(left + ARROW_COLUMN + ARROW_GAP, right, min.y, theme::CONTROL_HEIGHT, color, name);
    }

    /**
     * A row of the tree at the cursor, in the width of every row: a mod's at level 0, and a page's a level
     * further in than the mod or the page it is under. Its name is set in by its level, after the column of its
     * arrow, which every row keeps, so the names of a level line up whether or not they have one, and has its
     * tag after it, where it has one. The selected row is a solid bar, whose tip is inside the row, and any
     * other has the look of a row under the ray. With `show`, a selected row that is not whole in
     * the shown rows is scrolled to. Returns whether the row was pressed, and tells whether the ray is on it.
     */
    bool treeRow(const std::string_view name, const std::string_view tag, const int level, const Arrow arrow, const bool selected, const bool show, const float width,
        bool& pointed)
    {
        const auto min = ImGui::GetCursorScreenPos();
        const auto max = ImVec2(min.x + width, min.y + theme::CONTROL_HEIGHT);
        const bool pressed = ImGui::InvisibleButton("row", ImVec2(width, theme::CONTROL_HEIGHT));
        pointed = ImGui::IsItemHovered();

        const float shownTop = ImGui::GetWindowPos().y;
        if (selected && show && (min.y < shownTop || max.y > shownTop + ImGui::GetWindowHeight())) {
            ImGui::SetScrollHereY();
        }
        if (!ImGui::IsItemVisible()) {
            return pressed;
        }

        if (selected) {
            ui::drawSelectedBar(min, max);
        } else {
            ui::drawRowState();
        }
        const auto color = selected ? theme::color::ON_GREEN : theme::color::GREEN;
        const float left = min.x + theme::SPACE_2 + indentOf(level);
        if (arrow != Arrow::None) {
            drawArrow(left, min.y, arrow == Arrow::Open, color);
        }
        drawName(left, min, max, color, name, tag, selected);
        return pressed;
    }

    /**
     * The row of a mod or page that is not available, at the cursor, in the width of every row, with its tag
     * where it has one. It is disabled: it has the look of a row under the ray, so the reason in the help line is
     * seen to be its, and none of a press, which does nothing. Returns whether the ray is on the row.
     */
    bool disabledRow(const std::string_view name, const std::string_view tag, const int level, const Arrow arrow, const float width)
    {
        const auto min = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, theme::CONTROL_HEIGHT));
        if (!ImGui::IsItemVisible()) {
            return false;
        }
        const bool pointed = ImGui::IsItemHovered();
        ui::drawRowState();

        ImGui::BeginDisabled();
        const float left = min.x + theme::SPACE_2 + indentOf(level);
        if (arrow != Arrow::None) {
            drawArrow(left, min.y, false, theme::color::GREEN);
        }
        drawName(left, min, ImVec2(min.x + width, min.y + theme::CONTROL_HEIGHT), theme::color::GREEN, name, tag, false);
        ImGui::EndDisabled();
        return pointed;
    }
}

namespace mods_config::ui
{
    /**
     * Draws the tree at the cursor of the current ImGui window, in the height it is given. The header stays,
     * and the rows scroll under it.
     * The selected mod's own row is its front page, and it is open when it has pages: they are the rows under it,
     * and so are the pages of a page of it that is open.
     * A mod that is not available is a disabled row, in its place, which says why while the ray is on it. An
     * MCM mod's row has the MCM tag, as its change is made at once, which its help says.
     * Returns what a pressed row selects, and the row the ray is on where it has something to say: the reason
     * of a mod that is not available, the help of an MCM mod, or the help of a mod or a page, which a config spec
     * gives them.
     */
    TreeDrawn ModTree::draw(const std::vector<model::Mod>& mods, const Selection& selection, const float height, model::Store* store)
    {
        TreeDrawn drawn;
        // a row is the same width whether or not the scroll bar shows
        const float rowWidth = _width - theme::SPACE_2 - RIGHT_ROOM;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::color::PLATE);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(theme::SPACE_2, theme::SPACE_2));
        const bool shown = ImGui::BeginChild("Mods", ImVec2(_width, height), ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // the rows of the tree are closer to each other than the rows of a page
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(theme::SPACE_3, theme::SPACE_1));
        if (shown) {
            const auto pos = ImGui::GetCursorScreenPos();
            const auto count = std::to_string(mods.size());
            ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
            const float countLeft = pos.x + rowWidth - theme::SPACE_2 - textWidth(count);
            drawText(pos.x + theme::SPACE_2, countLeft, pos.y, HEADER_HEIGHT, theme::color::GREEN_MUTED, "Mods");
            drawText(countLeft, pos.x + rowWidth, pos.y, HEADER_HEIGHT, theme::color::GREEN_MUTED, count);
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(rowWidth, HEADER_HEIGHT));

            if (_showTop) {
                ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
                _showTop = false;
            }
            // wider than the rows, by the room of the tip and of the scroll bar
            if (ImGui::BeginChild("ModRows", ImVec2(rowWidth + RIGHT_ROOM - theme::SPACE_1, 0.0f))) {
                for (std::size_t modIndex = 0; modIndex < mods.size(); ++modIndex) {
                    const auto& mod = mods[modIndex];
                    const bool selected = selection.mod == modIndex;
                    const bool open = selected && !mod.pages.empty();
                    const bool mcm = mod.source == model::Source::Mcm;
                    const std::string_view tag = mcm ? MCM_TAG : "";
                    ImGui::PushID(static_cast<int>(modIndex));
                    if (!mod.notAvailable.empty()) {
                        if (disabledRow(mod.displayName, tag, 0, Arrow::Closed, rowWidth)) {
                            drawn.pointed = PointedRow{ .text = mod.displayName, .help = std::format("Not available: {}.", mod.notAvailable) };
                        }
                        ImGui::PopID();
                        continue;
                    }
                    bool pointed = false;
                    if (treeRow(mod.displayName, tag, 0, open ? Arrow::Open : Arrow::Closed, selected && selection.page.empty(), _showSelection, rowWidth, pointed)) {
                        drawn.pressed = Selection{ .mod = modIndex, .page = {} };
                    }
                    if (pointed && (mcm || !mod.help.empty())) {
                        drawn.pointed = PointedRow{ .text = mod.displayName, .help = mcm ? MCM_HELP : mod.help };
                    }
                    if (open) {
                        std::vector<std::size_t> path;
                        drawPages(mod.pages, modIndex, selection, path, rowWidth, drawn, store);
                    }
                    ImGui::PopID();
                }
                _showSelection = false;
            }
            ImGui::EndChild();
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
        return drawn;
    }

    /**
     * The rows of the pages of the selected mod, or of a page of it, under its row: `path` is the place of that
     * page, and is empty for the mod. A page with pages is open while it or one of its pages is selected, and its
     * pages are the rows under it, a level further in. Tells a pressed row and a pointed one with a help.
     */
    void ModTree::drawPages(const std::vector<model::Page>& pages, const std::size_t modIndex, const Selection& selection, std::vector<std::size_t>& path, const float rowWidth,
        TreeDrawn& drawn, model::Store* store) const
    {
        for (std::size_t pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
            const auto& page = pages[pageIndex];
            if (page.visibleWhen && !model::holds(*page.visibleWhen, store)) {
                continue;
            }
            path.push_back(pageIndex);
            const bool enabled = page.notAvailable.empty() && (!page.enabledWhen || model::holds(*page.enabledWhen, store));
            const bool open = !page.pages.empty() && std::ranges::starts_with(selection.page, path);
            const auto arrow = page.pages.empty() ? Arrow::None : open ? Arrow::Open : Arrow::Closed;
            ImGui::PushID(static_cast<int>(pageIndex));
            bool pointed = false;
            if (!enabled) {
                pointed = disabledRow(page.name, {}, static_cast<int>(path.size()), arrow, rowWidth);
            } else if (treeRow(page.name, {}, static_cast<int>(path.size()), arrow, selection.page == path, _showSelection, rowWidth, pointed)) {
                drawn.pressed = Selection{ .mod = modIndex, .page = path };
            }
            if (pointed && (!enabled || !page.help.empty())) {
                const auto disabled = page.notAvailable.empty() ? std::string("This page is disabled.") : std::format("Not available: {}.", page.notAvailable);
                drawn.pointed = PointedRow{ .text = page.name, .help = enabled ? page.help : disabled + (page.help.empty() ? std::string() : " " + page.help) };
            }
            if (open && enabled) {
                drawPages(page.pages, modIndex, selection, path, rowWidth, drawn, store);
            }
            ImGui::PopID();
            path.pop_back();
        }
    }

    /**
     * Has the selected row scrolled into view, when it is not whole in the shown rows: a row that is selected
     * from a breadcrumb, or that moved when the mod above it closed.
     */
    void ModTree::showSelection()
    {
        _showSelection = true;
    }

    /**
     * The width of the tree's plate, in the panel's pixels, which the content beside it gives up. A row has the
     * plate's width less the room for its tip and the scroll bar.
     */
    void ModTree::setWidth(const float width)
    {
        _width = width;
    }

    /**
     * Has the rows scrolled back to their top, as they are when the menu opens.
     */
    void ModTree::showTop()
    {
        _showTop = true;
    }
}
