#include "ModTree.h"

#include <cstdint>
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
    // a row: the same width whether or not the scroll bar shows
    constexpr float ROW_WIDTH = theme::NAV_WIDTH - theme::SPACE_2 - RIGHT_ROOM;
    // and the arrow of a mod's row, with the column it is in the middle of:
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
     */
    void drawArrow(const ImVec2& rowMin, const bool open, const ImU32 color)
    {
        const ImVec2 box(rowMin.x + theme::SPACE_2 + (ARROW_COLUMN - ARROW_SIZE) * 0.5f, rowMin.y + (theme::CONTROL_HEIGHT - ARROW_SIZE) * 0.5f);
        const auto at = [&box](const float x, const float y) {
            return ImVec2(box.x + x, box.y + y);
        };
        if (open) {
            ImGui::GetWindowDrawList()->AddTriangleFilled(at(2.0f, 4.0f), at(12.0f, 4.0f), at(7.0f, 11.0f), color);
        } else {
            ImGui::GetWindowDrawList()->AddTriangleFilled(at(4.0f, 2.0f), at(11.0f, 7.0f), at(4.0f, 12.0f), color);
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
     * A row of the tree, a mod's or a page's under its mod, at the cursor. The selected row is a solid bar,
     * and any other has the look of a row under the ray. With `show`, a selected row that is not whole in the
     * shown rows is scrolled to. Returns whether the row was pressed.
     */
    bool treeRow(const std::string_view name, const RowKind kind, const bool selected, const bool show)
    {
        const auto min = ImGui::GetCursorScreenPos();
        const auto max = ImVec2(min.x + ROW_WIDTH, min.y + theme::CONTROL_HEIGHT);
        const bool pressed = ImGui::InvisibleButton("row", ImVec2(ROW_WIDTH, theme::CONTROL_HEIGHT));

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
}

namespace mods_config::ui
{
    /**
     * Draws the tree at the cursor of the current ImGui window, in the height it is given. The header stays,
     * and the rows scroll under it.
     * The selected mod's own row is its front page, and it is open when it has pages: they are the rows under it.
     * Returns what a pressed row selects.
     */
    std::optional<Selection> ModTree::draw(const std::vector<model::Mod>& mods, const Selection& selection, const float height)
    {
        std::optional<Selection> pressed;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::color::PLATE);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(theme::SPACE_2, theme::SPACE_2));
        const bool shown = ImGui::BeginChild("Mods", ImVec2(theme::NAV_WIDTH, height), ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // the rows of the tree are closer to each other than the rows of a page
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(theme::SPACE_3, theme::SPACE_1));
        if (shown) {
            const auto pos = ImGui::GetCursorScreenPos();
            const auto count = std::to_string(mods.size());
            ImGui::SetWindowFontScale(theme::FONT_SCALE_DETAIL);
            const float countLeft = pos.x + ROW_WIDTH - theme::SPACE_2 - textWidth(count);
            drawText(pos.x + theme::SPACE_2, countLeft, pos.y, HEADER_HEIGHT, theme::color::GREEN_MUTED, "Mods");
            drawText(countLeft, pos.x + ROW_WIDTH, pos.y, HEADER_HEIGHT, theme::color::GREEN_MUTED, count);
            ImGui::SetWindowFontScale(theme::FONT_SCALE_BODY);
            ImGui::Dummy(ImVec2(ROW_WIDTH, HEADER_HEIGHT));

            if (_showTop) {
                ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
                _showTop = false;
            }
            // wider than the rows, by the room of the tip and of the scroll bar
            if (ImGui::BeginChild("ModRows", ImVec2(ROW_WIDTH + RIGHT_ROOM - theme::SPACE_1, 0.0f))) {
                for (std::size_t modIndex = 0; modIndex < mods.size(); ++modIndex) {
                    const auto& mod = mods[modIndex];
                    const bool selected = selection.mod == modIndex;
                    const bool open = selected && !mod.pages.empty();
                    ImGui::PushID(static_cast<int>(modIndex));
                    if (treeRow(mod.displayName, open ? RowKind::OpenMod : RowKind::ClosedMod, selected && !selection.page, _showSelection)) {
                        pressed = Selection{ .mod = modIndex, .page = std::nullopt };
                    }
                    for (std::size_t pageIndex = 0; open && pageIndex < mod.pages.size(); ++pageIndex) {
                        ImGui::PushID(static_cast<int>(pageIndex));
                        if (treeRow(mod.pages[pageIndex].name, RowKind::Page, selection.page == pageIndex, _showSelection)) {
                            pressed = Selection{ .mod = modIndex, .page = pageIndex };
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
        return pressed;
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
     * Has the rows scrolled back to their top, as they are when the menu opens.
     */
    void ModTree::showTop()
    {
        _showTop = true;
    }
}
