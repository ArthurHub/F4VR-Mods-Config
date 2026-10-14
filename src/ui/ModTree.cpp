#include "ModTree.h"

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
    // the arrow of a mod's row, with the column it is in the middle of:
    constexpr float ARROW_SIZE = 14.0f;
    constexpr float ARROW_COLUMN = 16.0f;

    enum class RowKind : std::uint8_t
    {
        // a mod with no rows under it: one that is not selected, or that has no pages
        ClosedMod,
        // the selected mod, with its pages as the rows under it
        OpenMod,
        Page,
    };

    /**
     * The arrow of a mod's row: it points down at the mod whose pages are the rows under it, and right at
     * any other. A mod with no pages has nothing to open, so its arrow stays when it is selected.
     * It is faded in a disabled row, as the row's text is.
     */
    void drawArrow(const ImVec2& rowMin, const bool open, const ImU32 color)
    {
        const ImVec2 box(rowMin.x + theme::SPACE_2 + (ARROW_COLUMN - ARROW_SIZE) * 0.5f, rowMin.y + (theme::CONTROL_HEIGHT - ARROW_SIZE) * 0.5f);
        const auto at = [&box](const float x, const float y) {
            return ImVec2(box.x + x, box.y + y);
        };
        if (open) {
            ImGui::GetWindowDrawList()->AddTriangleFilled(at(2.0f, 4.0f), at(12.0f, 4.0f), at(7.0f, 11.0f), ImGui::GetColorU32(color));
        } else {
            ImGui::GetWindowDrawList()->AddTriangleFilled(at(4.0f, 2.0f), at(11.0f, 7.0f), at(4.0f, 12.0f), ImGui::GetColorU32(color));
        }
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
     * A row of the tree, a mod's or a page's under its mod, at the cursor, in the width of every row. The selected
     * row is a solid bar, and any other has the look of a row under the ray. With `show`, a selected row that is
     * not whole in the shown rows is scrolled to. Returns whether the row was pressed.
     */
    bool treeRow(const std::string_view name, const RowKind kind, const bool selected, const bool show, const float width)
    {
        const auto min = ImGui::GetCursorScreenPos();
        const auto max = ImVec2(min.x + width, min.y + theme::CONTROL_HEIGHT);
        const bool pressed = ImGui::InvisibleButton("row", ImVec2(width, theme::CONTROL_HEIGHT));

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
        float nameLeft = min.x + theme::SPACE_2 + theme::SPACE_8;
        if (kind != RowKind::Page) {
            drawArrow(min, kind == RowKind::OpenMod, color);
            nameLeft = min.x + theme::SPACE_2 + ARROW_COLUMN + theme::SPACE_2;
        }
        ui::drawText(nameLeft, max.x - theme::SPACE_3, min.y, theme::CONTROL_HEIGHT, color, name);
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
        drawArrow(min, false, theme::color::GREEN);
        ui::drawText(min.x + theme::SPACE_2 + ARROW_COLUMN + theme::SPACE_2, min.x + width - theme::SPACE_3, min.y, theme::CONTROL_HEIGHT, theme::color::GREEN, name);
        ImGui::EndDisabled();
        return pointed;
    }
}

namespace mods_config::ui
{
    /**
     * Draws the tree at the cursor of the current ImGui window, in the height it is given. The header stays,
     * and the rows scroll under it.
     * The selected mod's own row is its front page, and it is open when it has pages: they are the rows under it.
     * A mod that is not available is a disabled row, in its place, which says why while the ray is on it.
     * Returns what a pressed row selects, and the row of a mod that is not available the ray is on.
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
            ImGui::SetWindowFontScale(theme::FONT_SCALE_DETAIL);
            const float countLeft = pos.x + rowWidth - theme::SPACE_2 - textWidth(count);
            drawText(pos.x + theme::SPACE_2, countLeft, pos.y, HEADER_HEIGHT, theme::color::GREEN_MUTED, "Mods");
            drawText(countLeft, pos.x + rowWidth, pos.y, HEADER_HEIGHT, theme::color::GREEN_MUTED, count);
            ImGui::SetWindowFontScale(theme::FONT_SCALE_BODY);
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
                    if (treeRow(mod.displayName, open ? RowKind::OpenMod : RowKind::ClosedMod, selected && !selection.page, _showSelection, rowWidth)) {
                        drawn.pressed = Selection{ .mod = modIndex, .page = std::nullopt };
                    }
                    for (std::size_t pageIndex = 0; open && pageIndex < mod.pages.size(); ++pageIndex) {
                        ImGui::PushID(static_cast<int>(pageIndex));
                        if (treeRow(mod.pages[pageIndex].name, RowKind::Page, selection.page == pageIndex, _showSelection, rowWidth)) {
                            drawn.pressed = Selection{ .mod = modIndex, .page = pageIndex };
                        }
                        ImGui::PopID();
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
