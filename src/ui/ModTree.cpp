#include "ModTree.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"

namespace
{
    using namespace mods_config;

    // The tree's sizes that are not tokens of the design system, in the panel's pixels.
    // Its header:
    constexpr float HEADER_HEIGHT = 40.0f;
    // the tip its selected row ends in, which points at the content:
    constexpr float TIP_DEPTH = 20.0f;
    // the room it keeps on its right, for that tip and for its scroll bar:
    constexpr float RIGHT_ROOM = TIP_DEPTH + theme::SPACE_1 + theme::SCROLLBAR_SIZE + theme::SPACE_1;
    // the column the arrow of a row is in the middle of, which every row keeps:
    constexpr float ARROW_COLUMN = 16.0f;
    // and the gap from it to the name, 2 px less than space-2:
    constexpr float ARROW_GAP = theme::SPACE_2 - 2.0f;

    // the deepest level a row is set in to: a row further in is set in as one of it, so its name keeps its room
    constexpr int MAX_INDENT_LEVEL = 3;

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
     * The bar of the selected row: a solid fill that ends in a tip, in the room the tree keeps beside its rows.
     * It is one shape, so no seam shows between the bar and its tip.
     */
    void drawSelectedBar(const ImVec2& min, const ImVec2& max)
    {
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->PathArcToFast(ImVec2(min.x + theme::RADIUS_FRAME, min.y + theme::RADIUS_FRAME), theme::RADIUS_FRAME, 6, 9);
        drawList->PathLineTo(ImVec2(max.x, min.y));
        drawList->PathLineTo(ImVec2(max.x + TIP_DEPTH, (min.y + max.y) * 0.5f));
        drawList->PathLineTo(max);
        drawList->PathArcToFast(ImVec2(min.x + theme::RADIUS_FRAME, max.y - theme::RADIUS_FRAME), theme::RADIUS_FRAME, 3, 6);
        drawList->PathFillConvex(theme::color::GREEN);
    }

    /**
     * A row of the tree at the cursor, in the width of every row: a mod's at level 0, and a page's a level
     * further in than the mod or the page it is under. Its name is set in by its level, after the column of its
     * arrow, which every row keeps, so the names of a level line up whether or not they have one. The selected
     * row is a solid bar, and any other has the look of a row under the ray. With `show`, a selected row that is
     * not whole in the shown rows is scrolled to. Returns whether the row was pressed, and tells whether the ray
     * is on it.
     */
    bool treeRow(const std::string_view name, const int level, const Arrow arrow, const bool selected, const bool show, const float width, bool& pointed)
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
            drawSelectedBar(min, max);
        } else {
            ui::drawRowState();
        }
        const auto color = selected ? theme::color::ON_GREEN : theme::color::GREEN;
        const float left = min.x + theme::SPACE_2 + indentOf(level);
        if (arrow != Arrow::None) {
            drawArrow(left, min.y, arrow == Arrow::Open, color);
        }
        ui::drawText(left + ARROW_COLUMN + ARROW_GAP, max.x - theme::SPACE_3, min.y, theme::CONTROL_HEIGHT, color, name);
        return pressed;
    }

    /**
     * The row of a mod that is not available, at the cursor, in the width of every row. It is disabled: it
     * has the look of a row under the ray, so the reason in the help line is seen to be its, and none of a
     * press, which does nothing. Returns whether the ray is on the row.
     */
    bool notAvailableRow(const std::string_view name, const float width)
    {
        const auto min = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, theme::CONTROL_HEIGHT));
        if (!ImGui::IsItemVisible()) {
            return false;
        }
        const bool pointed = ImGui::IsItemHovered();
        ui::drawRowState();

        ImGui::BeginDisabled();
        drawArrow(min.x + theme::SPACE_2, min.y, false, theme::color::GREEN);
        ui::drawText(min.x + theme::SPACE_2 + ARROW_COLUMN + ARROW_GAP, min.x + width - theme::SPACE_3, min.y, theme::CONTROL_HEIGHT, theme::color::GREEN, name);
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
     * A mod that is not available is a disabled row, in its place, which says why while the ray is on it.
     * Returns what a pressed row selects, and the row the ray is on where it has something to say: the reason
     * of a mod that is not available, or the help of a mod or a page, which a package gives them.
     */
    TreeDrawn ModTree::draw(const std::vector<model::Mod>& mods, const Selection& selection, const float height)
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
                    ImGui::PushID(static_cast<int>(modIndex));
                    if (!mod.notAvailable.empty()) {
                        if (notAvailableRow(mod.displayName, rowWidth)) {
                            drawn.pointed = PointedRow{ .text = mod.displayName, .help = std::format("Not available: {}.", mod.notAvailable) };
                        }
                        ImGui::PopID();
                        continue;
                    }
                    bool pointed = false;
                    if (treeRow(mod.displayName, 0, open ? Arrow::Open : Arrow::Closed, selected && selection.page.empty(), _showSelection, rowWidth, pointed)) {
                        drawn.pressed = Selection{ .mod = modIndex, .page = {} };
                    }
                    if (pointed && !mod.help.empty()) {
                        drawn.pointed = PointedRow{ .text = mod.displayName, .help = mod.help };
                    }
                    if (open) {
                        std::vector<std::size_t> path;
                        drawPages(mod.pages, modIndex, selection, path, rowWidth, drawn);
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
        TreeDrawn& drawn) const
    {
        for (std::size_t pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
            const auto& page = pages[pageIndex];
            path.push_back(pageIndex);
            const bool open = !page.pages.empty() && std::ranges::starts_with(selection.page, path);
            const auto arrow = page.pages.empty() ? Arrow::None : open ? Arrow::Open : Arrow::Closed;
            ImGui::PushID(static_cast<int>(pageIndex));
            bool pointed = false;
            if (treeRow(page.name, static_cast<int>(path.size()), arrow, selection.page == path, _showSelection, rowWidth, pointed)) {
                drawn.pressed = Selection{ .mod = modIndex, .page = path };
            }
            if (pointed && !page.help.empty()) {
                drawn.pointed = PointedRow{ .text = page.name, .help = page.help };
            }
            if (open) {
                drawPages(page.pages, modIndex, selection, path, rowWidth, drawn);
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
