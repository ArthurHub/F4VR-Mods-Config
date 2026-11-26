#include "ModTree.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include "Drawing.h"
#include "Targets.h"
#include "Theme.h"
#include "imgui/controls/Panes.h"
#include "imgui/controls/Tree.h"
#include "model/Conditions.h"
#include "model/UnsavedPages.h"
#include "perf/Perf.h"

namespace
{
    using namespace mods_config;

    // The tree's sizes that are not tokens of the design system, in the panel's pixels.
    // Its header:
    constexpr float HEADER_HEIGHT = 40.0f;
    // the tag of an MCM mod's row, and what the help line says of it, as its change is made at once
    constexpr auto MCM_TAG = "MCM";
    constexpr auto MCM_HELP = "An MCM menu: a change is made in the game as you make it. Keep accepts it, and Revert sets it back.";
    // how many of the first mods of a category that a press opened are scrolled to
    constexpr std::size_t CATEGORY_SHOWN_MODS = 3;
    // how many of the first pages of a mod that a press selected are scrolled to
    constexpr std::size_t MOD_SHOWN_PAGES = 4;

    /**
     * What the help line says of a row whose page has unsaved changes: its help, then how many, as
     * `2 unsaved changes.`
     */
    std::string withUnsaved(const std::string& help, const std::size_t unsaved)
    {
        if (unsaved == 0) {
            return help;
        }
        return std::format("{}{}{} unsaved change{}.", help, help.empty() ? "" : " ", unsaved, unsaved == 1 ? "" : "s");
    }

    /**
     * Adds the row just drawn, of a mod or of a page of it, to the panel's targets: by the mod's name, and the names of
     * the pages on the way to the page, by their places in `path`.
     */
    void addTreeTarget(const model::Mod& mod, const std::span<const std::size_t> path = {})
    {
        auto& targets = controls::targets();
        if (!targets.isRecording()) {
            return;
        }
        std::vector<std::string> names;
        const auto* pages = &mod.pages;
        for (const auto place : path) {
            names.push_back((*pages)[place].name);
            pages = &(*pages)[place].pages;
        }
        targets.add(ui::target::TREE, mod.name, ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), std::move(names));
    }

    /**
     * What the help line says of a page's row: its help, and why it is disabled before it for one that is.
     */
    std::string pageHelp(const model::Page& page, const bool enabled)
    {
        if (enabled) {
            return page.help;
        }
        const auto disabled = page.notAvailable.empty() ? std::string("This page is disabled.") : std::format("Not available: {}.", page.notAvailable);
        return disabled + (page.help.empty() ? std::string() : " " + page.help);
    }
}

namespace mods_config::ui
{
    /**
     * Draws the tree at the cursor of the current ImGui window, in the height it is given. The header, the search
     * field, stays, and the rows scroll under it.
     * The selected mod's own row is its front page, and it is open when it has pages: they are the rows under it,
     * and so are the pages of a page of it that is open.
     * A mod that is not available is a disabled row, in its place, which says why while the ray is on it. An
     * MCM mod's row has the MCM tag, as its change is made at once, which its help says.
     * With a search, the rows are the mods and pages it found (`drawFound`), and else with categories, the tree is
     * grouped by them (`drawCategories`).
     * Returns what a pressed row selects, and the row the ray is on where it has something to say: the reason
     * of a mod that is not available, the help of an MCM mod, or the help of a mod or a page, which a config spec
     * gives them.
     */
    TreeDrawn ModTree::draw(const std::vector<model::Mod>& mods, const Selection& selection, const float height, model::Store* store, const TreeSearch* search,
        const std::vector<model::CategoryGroup>* categories)
    {
        F4CF_PERF_FUNCTION();
        TreeDrawn drawn;
        // the pages are counted only while the session has a change, which most frames it has not
        _unsavedStore = store && store->dirtyCount() > 0 ? store : nullptr;
        // a row is the same width whether or not the scroll bar shows
        const float rowWidth = controls::navRowWidth(_width);

        if (controls::beginNavPane("Mods", _width, height)) {
            const auto pos = ImGui::GetCursorScreenPos();
            auto field = _search.draw(pos, rowWidth, mods.size(), search ? std::optional(search->matches) : std::nullopt);
            drawn.searchActed = field.pressActed;
            drawn.searchPointed = field.pointed.has_value();
            drawn.pointed = std::move(field.pointed);
            ImGui::SetCursorScreenPos(pos);
            ImGui::Dummy(ImVec2(rowWidth, HEADER_HEIGHT));

            if (controls::beginNavRows("ModRows", _width, std::exchange(_showTop, false))) {
                if (search) {
                    drawFound(mods, selection, *search, rowWidth, drawn);
                } else if (categories) {
                    drawCategories(mods, *categories, selection, rowWidth, drawn, store);
                } else {
                    for (std::size_t modIndex = 0; modIndex < mods.size(); ++modIndex) {
                        drawMod(mods, modIndex, 0, selection, rowWidth, drawn, store);
                    }
                }
                _showSelection = false;
            }
            controls::endNavRows();
        }
        controls::endNavPane();
        return drawn;
    }

    /**
     * The rows of a grouped tree: a row for each category, and under one that is open the rows of its mods, a level
     * further in. A press opens and closes a category, and several can be open. The category of the selected mod is
     * opened as the mod is selected, and as its row is to be scrolled to, so the row is there. A category that a
     * press opened has the selected row scrolled to, where it has it, and else the rows of its first mods, where they
     * are under the shown rows.
     */
    void ModTree::drawCategories(const std::vector<model::Mod>& mods, const std::vector<model::CategoryGroup>& categories, const Selection& selection, const float rowWidth,
        TreeDrawn& drawn, model::Store* store)
    {
        for (const auto& category : categories) {
            const auto key = model::categoryKey(category.name);
            if (_showSelection && selection.mod && std::ranges::find(category.mods, *selection.mod) != category.mods.end()) {
                _openCategories.insert(key);
            }
            bool open = _openCategories.contains(key);
            const bool hasSelected = selection.mod && std::ranges::find(category.mods, *selection.mod) != category.mods.end();
            // closed, it tells the unsaved changes of the selected mod it hides
            const std::size_t unsaved = !open && hasSelected && _unsavedStore ? model::unsavedChanges(mods[*selection.mod], *_unsavedStore) : 0;
            const auto& name = category.name.empty() ? std::string(model::NO_CATEGORY) : category.name;
            // not a number, which a mod's place is
            ImGui::PushID(key.c_str());
            const auto row = controls::treeGroupRow(name, category.mods.size(), open, unsaved > 0, rowWidth);
            controls::targets().add(target::CATEGORY, name, ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
            if (row.pointed && unsaved > 0) {
                drawn.pointed = PointedRow{ .text = name, .help = withUnsaved({}, unsaved) };
            }
            if (row.pressed) {
                open = !open;
                if (open) {
                    _openCategories.insert(key);
                    // the selected row, which can be far under the category's with the pages of its mod open
                    if (hasSelected) {
                        _showSelection = true;
                    } else {
                        _openedCategory = key;
                    }
                } else {
                    _openCategories.erase(key);
                }
            }
            for (std::size_t place = 0; open && place < category.mods.size(); ++place) {
                const float rowBottom = drawMod(mods, category.mods[place], 1, selection, rowWidth, drawn, store);
                if (_openedCategory == key && place + 1 == (std::min)(category.mods.size(), CATEGORY_SHOWN_MODS)) {
                    if (rowBottom > ImGui::GetWindowPos().y + ImGui::GetWindowHeight()) {
                        ImGui::SetScrollFromPosY(rowBottom + theme::SPACE_1 - ImGui::GetWindowPos().y, 1.0f);
                    }
                    _openedCategory.reset();
                }
            }
            ImGui::PopID();
        }
    }

    /**
     * The row of a mod, at a level of the tree, and the rows of its pages under it while it is selected. A mod that
     * is not available is a disabled row. Tells a pressed row and a pointed one with something to say. Returns the
     * bottom of the mod's own row, on the screen.
     */
    float ModTree::drawMod(const std::vector<model::Mod>& mods, const std::size_t modIndex, const int level, const Selection& selection, const float rowWidth, TreeDrawn& drawn,
        model::Store* store)
    {
        const auto& mod = mods[modIndex];
        const bool selected = selection.mod == modIndex;
        const bool open = selected && !mod.pages.empty();
        const bool mcm = mod.source == model::Source::Mcm;
        const std::string_view tag = mcm ? MCM_TAG : "";
        ImGui::PushID(static_cast<int>(modIndex));
        if (!mod.notAvailable.empty()) {
            if (controls::treeRow({ .name = mod.displayName, .tag = tag, .level = level, .arrow = controls::TreeArrow::Closed, .disabled = true }, rowWidth).pointed) {
                drawn.pointed = PointedRow{ .text = mod.displayName, .help = std::format("Not available: {}.", mod.notAvailable) };
            }
            addTreeTarget(mod);
            ImGui::PopID();
            return ImGui::GetItemRectMax().y;
        }
        // the mod's row is its front page, and its pages are the rows under it while it is selected
        const std::size_t unsaved = selected && _unsavedStore ? model::unsavedChanges(mod.frontPage, *_unsavedStore, false) : 0;
        const auto row = controls::treeRow({ .name = mod.displayName,
                                               .tag = tag,
                                               .level = level,
                                               .arrow = open ? controls::TreeArrow::Open : controls::TreeArrow::Closed,
                                               .selected = selected && selection.page.empty(),
                                               .show = _showSelection,
                                               .dot = unsaved > 0 },
            rowWidth);
        if (row.pressed) {
            drawn.pressed = Selection{ .mod = modIndex, .page = {} };
            if (!selected && !mod.pages.empty()) {
                _openedMod = modIndex;
            }
        }
        addTreeTarget(mod);
        const float rowBottom = ImGui::GetItemRectMax().y;
        if (row.pointed && (mcm || !mod.help.empty() || unsaved > 0)) {
            drawn.pointed = PointedRow{ .text = mod.displayName, .help = withUnsaved(mcm ? MCM_HELP : mod.help, unsaved) };
        }
        if (open) {
            std::vector<std::size_t> path;
            drawPages(mod, mod.pages, modIndex, level + 1, selection, path, rowWidth, drawn, store);
            if (_openedMod == modIndex) {
                // the room of the first pages, under the mod's row, scrolled to where they are under the shown rows
                const float shownCount = static_cast<float>((std::min)(mod.pages.size(), MOD_SHOWN_PAGES));
                const float pagesBottom = rowBottom + shownCount * (theme::SPACE_1 + theme::CONTROL_HEIGHT);
                if (pagesBottom > ImGui::GetWindowPos().y + ImGui::GetWindowHeight()) {
                    ImGui::SetScrollFromPosY(pagesBottom + theme::SPACE_1 - ImGui::GetWindowPos().y, 1.0f);
                }
                _openedMod.reset();
            }
        }
        ImGui::PopID();
        return rowBottom;
    }

    /**
     * The rows of the pages of the selected mod, or of a page of it, under its row, at a level of the tree: `path` is
     * the place of that page, and is empty for the mod. A page with pages is open while it or one of its pages is
     * selected, and its pages are the rows under it, a level further in. Tells a pressed row and a pointed one with
     * a help.
     */
    void ModTree::drawPages(const model::Mod& mod, const std::vector<model::Page>& pages, const std::size_t modIndex, const int level, const Selection& selection,
        std::vector<std::size_t>& path, const float rowWidth, TreeDrawn& drawn, model::Store* store) const
    {
        for (std::size_t pageIndex = 0; pageIndex < pages.size(); ++pageIndex) {
            const auto& page = pages[pageIndex];
            if (page.visibleWhen && !model::holds(*page.visibleWhen, store)) {
                continue;
            }
            path.push_back(pageIndex);
            const bool enabled = page.notAvailable.empty() && (!page.enabledWhen || model::holds(*page.enabledWhen, store));
            const bool open = !page.pages.empty() && std::ranges::starts_with(selection.page, path);
            const auto arrow = page.pages.empty() ? controls::TreeArrow::None : open ? controls::TreeArrow::Open : controls::TreeArrow::Closed;
            // a page whose pages are not the rows under it tells their changes too
            const std::size_t unsaved = _unsavedStore ? model::unsavedChanges(page, *_unsavedStore, !(open && enabled)) : 0;
            ImGui::PushID(static_cast<int>(pageIndex));
            const auto row = controls::treeRow(
                { .name = page.name, .level = level, .arrow = arrow, .selected = selection.page == path, .show = _showSelection, .disabled = !enabled, .dot = unsaved > 0 },
                rowWidth);
            if (row.pressed) {
                drawn.pressed = Selection{ .mod = modIndex, .page = path };
            }
            addTreeTarget(mod, path);
            if (row.pointed && (!enabled || !page.help.empty() || unsaved > 0)) {
                drawn.pointed = PointedRow{ .text = page.name, .help = withUnsaved(pageHelp(page, enabled), unsaved) };
            }
            if (open && enabled) {
                drawPages(mod, page.pages, modIndex, level + 1, selection, path, rowWidth, drawn, store);
            }
            ImGui::PopID();
            path.pop_back();
        }
    }

    /**
     * The rows of the mods a search found, flat, in their order, with where the query matches each name underlined
     * and how many of its rows it shows: a mod's row has the count of its front page, and every page it found is
     * open, with the pages under it it found. A mod found by its name alone is closed, with the count of all its
     * rows, until it is selected. With none, the tree says that no mod matches.
     */
    void ModTree::drawFound(const std::vector<model::Mod>& mods, const Selection& selection, const TreeSearch& search, const float rowWidth, TreeDrawn& drawn) const
    {
        bool any = false;
        for (std::size_t modIndex = 0; modIndex < mods.size() && modIndex < search.found->size(); ++modIndex) {
            const auto& found = (*search.found)[modIndex];
            if (!found) {
                continue;
            }
            any = true;
            const auto& mod = mods[modIndex];
            const bool selected = selection.mod == modIndex;
            const bool mcm = mod.source == model::Source::Mcm;
            const std::string_view tag = mcm ? MCM_TAG : "";
            const bool closed = found->byName && !selected;
            const bool open = !closed && !found->pages.empty();
            const auto ranges = model::matchRanges(*search.query, mod.displayName);
            const std::size_t count = closed ? found->total : found->frontCount;
            ImGui::PushID(static_cast<int>(modIndex));
            if (!mod.notAvailable.empty()) {
                if (controls::treeRow({ .name = mod.displayName, .tag = tag, .arrow = controls::TreeArrow::Closed, .disabled = true, .found = ranges, .count = count }, rowWidth)
                        .pointed) {
                    drawn.pointed = PointedRow{ .text = mod.displayName, .help = std::format("Not available: {}.", mod.notAvailable) };
                }
                addTreeTarget(mod);
                ImGui::PopID();
                continue;
            }
            const auto row = controls::treeRow({ .name = mod.displayName,
                                                   .tag = tag,
                                                   .arrow = open ? controls::TreeArrow::Open : controls::TreeArrow::Closed,
                                                   .selected = selected && selection.page.empty(),
                                                   .show = _showSelection,
                                                   .found = ranges,
                                                   .count = count },
                rowWidth);
            if (row.pressed) {
                drawn.pressed = Selection{ .mod = modIndex, .page = {} };
            }
            addTreeTarget(mod);
            if (row.pointed && (mcm || !mod.help.empty())) {
                drawn.pointed = PointedRow{ .text = mod.displayName, .help = mcm ? MCM_HELP : mod.help };
            }
            if (open) {
                std::vector<std::size_t> path;
                drawFoundPages(mod, mod.pages, found->pages, modIndex, selection, *search.query, path, rowWidth, drawn);
            }
            ImGui::PopID();
        }
        if (!any) {
            const auto pos = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(rowWidth, theme::CONTROL_HEIGHT));
            controls::drawText(pos.x + theme::SPACE_2, pos.x + rowWidth, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED, "No mod matches");
        }
    }

    /**
     * The rows of the pages a search found of a list of pages, under the row of the mod or the page they are under:
     * `path` is the place of that page, and is empty for the mod. Each is open, a level further in, with the pages it
     * found under it. A page with pages and none found under it has a closed arrow. The pages of several mods are
     * shown, so a page is selected by its mod and its place.
     */
    void ModTree::drawFoundPages(const model::Mod& mod, const std::vector<model::Page>& pages, const std::vector<model::PageFound>& found, const std::size_t modIndex,
        const Selection& selection, const model::SearchQuery& query, std::vector<std::size_t>& path, const float rowWidth, TreeDrawn& drawn) const
    {
        for (const auto& pageFound : found) {
            const auto& page = pages[pageFound.place];
            path.push_back(pageFound.place);
            const auto arrow = !pageFound.pages.empty() ? controls::TreeArrow::Open : page.pages.empty() ? controls::TreeArrow::None : controls::TreeArrow::Closed;
            const auto ranges = model::matchRanges(query, page.name);
            ImGui::PushID(static_cast<int>(pageFound.place));
            const auto row = controls::treeRow({ .name = page.name,
                                                   .level = static_cast<int>(path.size()),
                                                   .arrow = arrow,
                                                   .selected = selection.mod == modIndex && selection.page == path,
                                                   .show = _showSelection,
                                                   .disabled = !pageFound.enabled,
                                                   .found = ranges,
                                                   .count = pageFound.count },
                rowWidth);
            if (row.pressed) {
                drawn.pressed = Selection{ .mod = modIndex, .page = path };
            }
            addTreeTarget(mod, path);
            if (row.pointed && (!pageFound.enabled || !page.help.empty())) {
                drawn.pointed = PointedRow{ .text = page.name, .help = pageHelp(page, pageFound.enabled) };
            }
            drawFoundPages(mod, page.pages, pageFound.pages, modIndex, selection, query, path, rowWidth, drawn);
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
