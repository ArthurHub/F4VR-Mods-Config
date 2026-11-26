#include "FullPanel.h"

#include "PanelTargets.h"

#include <algorithm>
#include <format>
#include <numeric>
#include <string>
#include <tuple>
#include <variant>

#include <imgui.h>

#include "Drawing.h"
#include "HelpPage.h"
#include "Theme.h"
#include "common/CommonUtils.h"
#include "model/Conditions.h"
#include "perf/Perf.h"

namespace
{
    using namespace mods_config;

    // what the help line says of a mod while no change waits for a Save and the ray is on no row
    constexpr auto NO_UNSAVED = "No unsaved changes";

    // what the help line says with the ray on no row while the page has only its rows that match, in place of a
    // status that tells nothing to do
    constexpr auto FILTERED_STATUS = "Only the settings that match are shown. Back shows the whole page.";

    // and after a Save that wrote no change that takes effect after a restart, until the next change
    constexpr auto SAVED = "Saved";

    // the same of an MCM mod, whose change is in the game as it is made: there is nothing to save, so it is kept
    constexpr auto NONE_MADE = "No changes made";
    constexpr auto KEPT = "Kept";

    // the width of a footer button, in the panel's pixels
    constexpr float BUTTON_WIDTH = 160.0f;

    /**
     * Finds the sections an item is in, from the outermost, in a list of items and the sections in it.
     * Returns whether the item is there.
     */
    bool findSections(const std::vector<model::Item>& items, const model::Item& target, std::vector<const model::Group*>& sections)
    {
        for (const auto& item : items) {
            if (&item == &target) {
                return true;
            }
            if (const auto* group = std::get_if<model::Group>(&item)) {
                sections.push_back(group);
                if (findSections(group->items, target, sections)) {
                    return true;
                }
                sections.pop_back();
            }
        }
        return false;
    }

    /**
     * The page of a mod its places name, from its front page down: the mod's front page for none.
     */
    template <typename ModOf>
    auto* pageAt(ModOf& mod, const std::vector<std::size_t>& places)
    {
        auto* page = &mod.frontPage;
        auto* pages = &mod.pages;
        for (const auto place : places) {
            page = &(*pages)[place];
            pages = &page->pages;
        }
        return page;
    }
}

namespace mods_config::ui
{
    /**
     * The mods the menu lists, from every source they come from. They are kept in order of their display names,
     * and none of them is selected.
     */
    void FullPanel::setMods(std::vector<model::Mod> mods)
    {
        // before the mods it is of are replaced
        _session.end();
        _modList.setMods(std::move(mods));
        reset();
    }

    void FullPanel::setQuickLaunch(std::vector<QuickLaunchEntry> entries)
    {
        _frontPage.setQuickLaunch(std::move(entries));
    }

    void FullPanel::setControls(std::vector<ControlHelp> controls)
    {
        _controls = std::move(controls);
    }

    /**
     * The player's handedness, which says what a button of a binding's hand is printed as: the panel does not know the
     * game, so the menu tells it.
     */
    void FullPanel::setLeftHanded(const bool leftHanded)
    {
        _modPage.setLeftHanded(leftHanded);
    }

    /**
     * The Developer tools as Mods Config FO4 VR's settings have them, and what writes a change of their filter or
     * their fold to the settings at once, which the menu does: the panel does not know the config. With the
     * tools on, the menu's front page has its Developer section. The view the panel has changes at once, so
     * the next frame shows the change before the menu writes it.
     */
    void FullPanel::setDeveloper(DeveloperView view, std::function<void(const DeveloperView&)> write)
    {
        _developerView = std::move(view);
        _writeDeveloperView = std::move(write);
    }

    /**
     * Whether Help stands out in the title bar, as the settings have it, and what writes to them that the Help page
     * was shown. The Help page shown turns it off in the panel at once, and is told once.
     */
    void FullPanel::setHighlightHelp(const bool highlight, std::function<void()> opened)
    {
        _highlightHelp = highlight;
        _helpOpened = std::move(opened);
    }

    /**
     * What does what a press on a row of a config spec's live copy in its report asks, by the config spec's folder:
     * the menu, which reads the config spec again with `replaceMod`, or tells the mod list `liveCopyNotRead`, after
     * the panel is drawn. With none, as in a test, a press on them does nothing.
     */
    void FullPanel::setLiveCopyAction(std::function<void(const std::string& folder, LiveCopyAction action)> act)
    {
        _liveCopyAction = std::move(act);
    }

    /**
     * Puts a config spec's mod that was read again in place of the one of its folder, as its live copy is read or
     * its switch is turned off. The selected mod's editing session ends, which drops its unsaved changes, and starts
     * again with the mod read now, on the page that was shown while the mod still has it, or the nearest that is
     * there, and the menu's front page for a mod that is not available now. The dialog that asked about the changes
     * it dropped is closed. The mods are kept in order of their display names, and the selection, a report, Back and
     * the dialog keep their mods. The last read of its live copy says when it was read, as `10:42:05`, and how many
     * unsaved changes it dropped. Not called while the panel is drawn. Returns how many unsaved changes it dropped,
     * and none for a mod of no folder that is listed, which is not put in.
     */
    std::size_t FullPanel::replaceMod(model::Mod mod, const std::string_view time)
    {
        const auto place = _modList.placeOf(mod.folder);
        if (!place) {
            return 0;
        }
        const bool selected = _selection.mod == *place;
        std::size_t dropped = 0;
        if (selected) {
            const auto* const store = storeOf(mods()[*place]);
            dropped = store ? store->dirtyCount() : 0;
            // the session ends before its mod is replaced, and starts again on the new one
            _session.end();
        }
        const auto placeOf = _modList.replace(std::move(mod), dropped, time);
        if (!placeOf) {
            return 0;
        }
        const auto keep = [&placeOf](std::optional<std::size_t>& kept) {
            if (kept) {
                kept = (*placeOf)[*kept];
            }
        };
        keep(_selection.mod);
        keep(_report);
        keep(_returnToReport);
        if (_leaving) {
            keep(_leaving->to.mod);
            keep(_leaving->report);
        }

        if (selected) {
            _leaving.reset();
            const auto before = _selection;
            const auto shown = existing(_selection);
            _session.show(shown.mod ? storeOf(mods()[*shown.mod]) : nullptr, shown.mod ? &_modList.mod(*shown.mod) : nullptr, [this, &shown] {
                _selection = available(shown);
                return shownPage();
            });
            // the page that stays keeps where it was scrolled to
            _pageChanged = _selection != before;
            _modPage.reset();
            _helpLine.clear();
            updateStatus();
            _modTree.showSelection();
        }
        _search.modReplaced();
        return dropped;
    }

    /**
     * What the title bar's Minimize and Close do. With none, as in a test, a press on them does nothing.
     */
    void FullPanel::setMenuActions(MenuActions actions)
    {
        _menuActions = std::move(actions);
    }

    /**
     * What draws the images of a mod's page: the menu's, with the game's renderer. With none, as in a test,
     * an image keeps its room and is not drawn.
     */
    void FullPanel::setImageDrawer(ImageDrawer drawer)
    {
        _modPage.setImageDrawer(std::move(drawer));
    }

    /**
     * The store the panel asks for the values of a selected mod of a source, and sets them in. It is not the
     * panel's own, and has to outlive it. With none, as in a test of the tree, no value is available.
     */
    void FullPanel::setStore(const model::Source source, model::Store* store)
    {
        (source == model::Source::Mcm ? _mcmStore : _configSpecStore) = store;
    }

    /**
     * The store of the values of a mod: the one of the source of its menu.
     */
    model::Store* FullPanel::storeOf(const model::Mod& mod) const
    {
        return mod.source == model::Source::Mcm ? _mcmStore : _configSpecStore;
    }

    /**
     * The store of the selected mod's values, which its editing session is in. None on the menu's front page.
     */
    model::Store* FullPanel::selectedStore() const
    {
        return _selection.mod ? storeOf(mods()[*_selection.mod]) : nullptr;
    }

    /**
     * The status of the selected mod's editing session: how many changes are invalid, which block Save, why the
     * last Save failed, until the next change, how many wait for a Save, or of an MCM mod how many are made, with
     * how many of them take effect after a restart, or the last Save, until the next change, with how many of the
     * changes it wrote take effect after a restart. Nothing with no mod, and with no store.
     */
    FullPanel::SessionStatus FullPanel::sessionStatus() const
    {
        auto* const store = _selection.mod ? storeOf(mods()[*_selection.mod]) : nullptr;
        const bool made = store && store->writesChangesAtOnce();
        if (!store) {
            return {};
        }
        if (const auto invalid = store->invalidCount(); invalid > 0) {
            return { std::format("{} invalid value{}", invalid, invalid == 1 ? "" : "s"), StatusLook::Invalid };
        }
        if (const auto failure = store->saveFailure(); !failure.empty()) {
            return { std::format("Not saved: {}.", failure), StatusLook::Invalid };
        }
        if (const auto dirty = store->dirtyCount(); dirty > 0) {
            const auto* plural = dirty == 1 ? "" : "s";
            auto status = made ? std::format("{} change{} made", dirty, plural) : std::format("{} unsaved change{}", dirty, plural);
            if (const auto restarts = store->dirtyRestartCount(); restarts > 0) {
                status += std::format(", {} take{} effect after a restart", restarts, restarts == 1 ? "s" : "");
            }
            return { std::move(status), StatusLook::Pending };
        }
        const auto saved = store->lastSave();
        if (saved && *saved > 0) {
            return { std::format("Saved. {} change{} effect after the game is restarted.", *saved, *saved == 1 ? " takes" : "s take"), StatusLook::Restart };
        }
        return { saved ? (made ? KEPT : SAVED) : (made ? NONE_MADE : NO_UNSAVED) };
    }

    /**
     * Gives the help line the status of the selected mod's editing session. While the page has only its rows that
     * match, and the session has nothing to tell but that it has no changes or saved them, it tells that Back shows the
     * whole page.
     */
    void FullPanel::updateStatus()
    {
        auto status = sessionStatus();
        if (_selection.mod && isFiltered() && status.look == StatusLook::Plain) {
            status.text = FILTERED_STATUS;
        }
        _helpLine.setStatus(status.text, status.look);
    }

    /**
     * The width of the tree of the mods, in the panel's pixels: the content of a page has the rest of the
     * panel's width. The design system's is `theme::NAV_WIDTH`, which the panel has until it is told another.
     */
    void FullPanel::setTreeWidth(const float width)
    {
        _modTree.setWidth(width);
    }

    /**
     * Shows a mod's front page or one of its pages, or the menu's front page with no mod. A selection of
     * what is not there shows the nearest that is: for a page, the deepest page on the way to it that is there,
     * or the mod's front page, and the menu's front page for a mod, and for a mod that is not available.
     * A mod with unsaved changes is not left for another or for the menu's front page at once: the dialog asks
     * first, and its Save and Discard show the selection.
     * It leaves the Help page: the selection under it is shown again, as Back shows it, with its session kept.
     * And it leaves a config spec's report. The page that is shown, selected again, asks its client again for its rows
     * where the client gives them.
     * Returns whether it did something: it shows another page than before, or new rows, or the dialog asks.
     */
    bool FullPanel::select(const Selection& selection)
    {
        const auto target = existing(selection);
        const auto shown = target.mod == _selection.mod ? available(target) : target;
        if (shown == _selection) {
            // the menu's front page, which a report is shown over, from the report
            if (_report) {
                return leaveReport(false);
            }
            // the page, shown again from the Help page, and asked again for the rows its mod's client gives
            const bool fromHelp = showHelp(false);
            return readContentAgain() || fromHelp;
        }
        if (shown.mod != _selection.mod && hasUnsavedChanges()) {
            _leaving = Leave{ .to = shown };
            return true;
        }
        return show(shown);
    }

    /**
     * The nearest a selection names that is there: for a page, the deepest page on the way to it that is there, or
     * the mod's front page, and the menu's front page for a mod, and for a mod that is not available.
     */
    Selection FullPanel::existing(const Selection& selection) const
    {
        Selection shown;
        if (selection.mod && *selection.mod < mods().size() && mods()[*selection.mod].notAvailable.empty()) {
            shown.mod = selection.mod;
            const auto* pages = &mods()[*selection.mod].pages;
            for (const auto place : selection.page) {
                if (place >= pages->size()) {
                    break;
                }
                shown.page.push_back(place);
                pages = &(*pages)[place].pages;
            }
        }
        return shown;
    }

    /**
     * The nearest visible, enabled and available page of a selection whose mod's store is open. A blocked page also blocks
     * its descendants; its nearest available parent, or the mod's front page, is shown instead.
     */
    Selection FullPanel::available(const Selection& selection) const
    {
        Selection shown{ .mod = selection.mod, .page = {} };
        if (!selection.mod) {
            return shown;
        }
        auto* store = storeOf(mods()[*selection.mod]);
        const auto* pages = &mods()[*selection.mod].pages;
        for (const auto place : selection.page) {
            if (place >= pages->size()) {
                break;
            }
            const auto& page = (*pages)[place];
            if ((page.visibleWhen && !model::holds(*page.visibleWhen, store)) || (page.enabledWhen && !model::holds(*page.enabledWhen, store)) || !page.notAvailable.empty()) {
                break;
            }
            shown.page.push_back(place);
            pages = &page.pages;
        }
        return shown;
    }

    /**
     * Shows a selection of what is there, as the session tells the stores: another mod starts its editing session,
     * and another page of the same mod is told to its store, before the page's conditions are asked of it. A report
     * that was shown is left, and so is the report Back went to.
     * Returns true: it shows another page than before.
     */
    bool FullPanel::show(const Selection& shown)
    {
        _helpShown = false;
        _report.reset();
        _returnToReport.reset();
        _session.show(shown.mod ? storeOf(mods()[*shown.mod]) : nullptr, shown.mod ? &_modList.mod(*shown.mod) : nullptr, [this, &shown] {
            _selection = available(shown);
            return shownPage();
        });
        pageShown();
        return true;
    }

    /**
     * Asks the selected mod's client again for the rows of the page that is shown, where its client gives them, as it
     * is shown again, saved, discarded or read again. Not while a row of the page is edited, dragged or confirmed, as
     * new rows would leave that pointing nowhere. The page keeps where it was scrolled to, and forgets what it kept of
     * the old rows, as a page that is shown does. Returns whether it asked.
     */
    bool FullPanel::readContentAgain()
    {
        auto* const page = shownPage();
        if (!page || !page->contentFromClient || _modPage.isEditing()) {
            return false;
        }
        if (!_session.readPageContent(_modList.mod(*_selection.mod), *page)) {
            return false;
        }
        _modPage.reset();
        _helpLine.clear();
        _search.modReplaced();
        updateStatus();
        return true;
    }

    /**
     * Writes the selected mod's changes, as the footer's Save does, and asks its client again for the rows of the page
     * that is shown where it gives them. Returns why the Save failed, as the store does.
     */
    std::expected<void, std::string> FullPanel::save()
    {
        auto* const store = selectedStore();
        if (!store) {
            return std::unexpected(std::string("no mod is selected"));
        }
        auto saved = store->save();
        if (saved) {
            readContentAgain();
        }
        updateStatus();
        return saved;
    }

    /**
     * Takes back the selected mod's changes, as the footer's Discard does, and asks its client again for the rows of the
     * page that is shown where it gives them.
     */
    void FullPanel::discard()
    {
        if (auto* const store = selectedStore()) {
            store->discard();
            readContentAgain();
            updateStatus();
        }
    }

    /**
     * Reads the selected mod's values again, as its client or its own UI changed them, and asks its client again for
     * the rows of the page that is shown where it gives them, which reads the values too.
     */
    void FullPanel::readAgain()
    {
        if (auto* const store = selectedStore(); store && !readContentAgain()) {
            store->readAgain();
        }
    }

    /**
     * Closes the menu with the menu's action, as Close of the title bar and of the wrist do. While the selected
     * mod has unsaved changes the dialog asks first, and its Save and Discard close the menu.
     * Returns whether it did something: it closed the menu, or the dialog asks. With no action, as in a test,
     * a menu with nothing to ask is not closed.
     */
    bool FullPanel::close()
    {
        if (hasUnsavedChanges()) {
            _leaving = Leave{ .to = {}, .closeMenu = true };
            return true;
        }
        if (!_menuActions.close) {
            return false;
        }
        _menuActions.close();
        return true;
    }

    /**
     * Whether the selected mod has changes that wait for a Save, which the dialog asks about before the mod is
     * left.
     */
    bool FullPanel::hasUnsavedChanges() const
    {
        const auto* store = selectedStore();
        return store && store->dirtyCount() > 0;
    }

    /**
     * Does what the player was about to do as the dialog asked, after its Save or its Discard: shows the
     * selection or a config spec's report, or closes the menu. The dialog is closed. Returns true: it did something.
     */
    bool FullPanel::leave()
    {
        const auto leave = *_leaving;
        _leaving.reset();
        if (leave.report) {
            return openReport(*leave.report);
        }
        if (!leave.closeMenu) {
            return show(leave.to);
        }
        if (_menuActions.close) {
            _menuActions.close();
        }
        return true;
    }

    /**
     * The page of the selection: the selected mod's front page, or the page its places name. None on the
     * menu's front page.
     */
    const model::Page* FullPanel::selectedPage() const
    {
        return _selection.mod ? pageAt(mods()[*_selection.mod], _selection.page) : nullptr;
    }

    /**
     * The page of the selection, as `selectedPage`, in the mod list, where a store puts the rows a mod's client gives
     * it. None on the menu's front page.
     */
    model::Page* FullPanel::shownPage()
    {
        return _selection.mod ? pageAt(_modList.mod(*_selection.mod), _selection.page) : nullptr;
    }

    /**
     * Another page is shown in the content pane, from its top: a row of the page before keeps no drag, list,
     * edit or help, and the tree shows the selection. While searching it is filtered, until Back.
     */
    void FullPanel::pageShown()
    {
        _pageChanged = true;
        _search.pageShown();
        _modPage.reset();
        _helpLine.clear();
        point(nullptr);
        updateStatus();
        _modTree.showSelection();
    }

    /**
     * Shows the Help page in place of the selection's page, or the selection's page again in its place. The
     * selection is kept, and so is its editing session. Returns whether it shows another page than before.
     */
    bool FullPanel::showHelp(const bool shown)
    {
        if (shown == _helpShown) {
            return false;
        }
        _helpShown = shown;
        if (shown && _highlightHelp) {
            _highlightHelp = false;
            if (_helpOpened) {
                _helpOpened();
            }
        }
        pageShown();
        return true;
    }

    /**
     * Shows the report of a config spec over the menu's front page, as a press on its row of the Developer section
     * does. A mod that is selected is left, and the dialog asks first while it has unsaved changes. Back from the row
     * of one of its problems shows it where it was scrolled to, and any other from its top. Returns whether it did
     * something: it shows the report, or the dialog asks.
     */
    bool FullPanel::openReport(const std::size_t mod)
    {
        if (mod >= mods().size()) {
            return false;
        }
        if (hasUnsavedChanges()) {
            _leaving = Leave{ .to = {}, .closeMenu = false, .report = mod };
            return true;
        }
        // back from the row of one of its problems, where it was scrolled to
        const bool returning = _returnToReport == mod;
        if (_selection.mod) {
            show({});
        }
        _helpShown = false;
        _report = mod;
        _returnToReport.reset();
        pageShown();
        if (returning) {
            _scrollTo = _reportScroll;
        }
        return true;
    }

    /**
     * Leaves a config spec's report for the menu's front page, scrolled to its Developer section, as Back and the
     * Developer crumb do, or from its top, as the menu's crumb does. Returns true: it shows another page.
     */
    bool FullPanel::leaveReport(const bool toDeveloper)
    {
        _report.reset();
        _helpShown = false;
        pageShown();
        _scrollToDeveloper = toDeveloper;
        return true;
    }

    /**
     * The breadcrumbs of the title bar for what is shown now. The title bar draws them and devbench presses them.
     */
    std::vector<Crumb> FullPanel::breadcrumbs() const
    {
        return breadcrumbsOf(mods(), _selection, _helpShown, _report && !_helpShown ? &mods()[*_report] : nullptr, _selection.mod && isFiltered());
    }

    /**
     * Goes where a press on a breadcrumb goes: a selection, as a press on a row of the tree does, and the dialog asks
     * first before a mod with unsaved changes is left, or the Developer section from a config spec's report.
     * Returns whether it did something.
     */
    bool FullPanel::pressCrumb(const CrumbTarget& target)
    {
        if (const auto* selection = std::get_if<Selection>(&target)) {
            return select(*selection);
        }
        return leaveReport(true);
    }

    /**
     * Asks the menu what a press on a row of the live copy of the config spec whose report is shown asks, as the
     * report's row does. Returns false while no report is shown.
     */
    bool FullPanel::pressLiveCopy(const LiveCopyAction action)
    {
        if (!_report || action == LiveCopyAction::None || !_liveCopyAction) {
            return false;
        }
        _liveCopyAction(mods()[*_report].folder, action);
        return true;
    }

    /**
     * Changes the Developer section's filter or fold, as a press on the menu's front page does, and hands the view to
     * what writes it. Returns false while the section is not shown, and for a view that is the same.
     */
    bool FullPanel::changeDeveloper(const DeveloperView& view)
    {
        if (!_developerView.enabled || view == _developerView) {
            return false;
        }
        _developerView = view;
        if (_writeDeveloperView) {
            _writeDeveloperView(_developerView);
        }
        return true;
    }

    /**
     * Shows the row of a problem of a config spec's report, as a press on the problem does: the mod at the row's
     * page, with the row scrolled into view in the focus ring, and the sections it is in opened. Back then
     * shows the report again. A row the page does not have is not focused. Returns whether it shows the page.
     */
    bool FullPanel::showProblem(const std::size_t mod, const std::size_t problem)
    {
        if (mod >= mods().size() || problem >= mods()[mod].problems.size() || !mods()[mod].problems[problem].place) {
            return false;
        }
        const auto place = *mods()[mod].problems[problem].place;
        if (!select(Selection{ .mod = mod, .page = place.page }) || _selection.mod != mod) {
            return false;
        }
        _returnToReport = mod;
        if (_selection.page != place.page) {
            return true;
        }
        std::vector<const model::Group*> sections;
        const auto* items = &selectedPage()->items;
        const model::Item* item = nullptr;
        for (std::size_t level = 0; level < place.item.size(); ++level) {
            if (place.item[level] >= items->size()) {
                return true;
            }
            item = &(*items)[place.item[level]];
            if (level + 1 < place.item.size()) {
                const auto* group = std::get_if<model::Group>(item);
                if (!group) {
                    return true;
                }
                sections.push_back(group);
                items = &group->items;
            }
        }
        if (item) {
            _modPage.focus(*item, sections);
        }
        return true;
    }

    /**
     * Goes one step back: the dialog is cancelled, with nothing saved or discarded, a button's confirmation is
     * cancelled, with nothing run, a slider that is dragged
     * has the value it had before the drag, the list of a dropdown that is open is closed, a text input that
     * is edited keeps its value, the search field that is edited keeps its query, and the editor of a binding closes its list, or gives back the binding it had as it
     * opened. With none of them, from the Help page to the page it was shown over, with the ray on the search field
     * and a query in it, the query is emptied, as Clear does, from a page filtered by the search
     * to the whole page, with the focus ring on the last row the ray was on, from a
     * config spec's report to the Developer section of the menu's front page, from the page a problem of a report
     * showed back to the report, from a page of a mod to the page it is under or to the mod's front page, and
     * from there to the menu's front page, which asks first while the mod has unsaved changes.
     * Returns false on the menu's front page, which has nothing before it: Back closes the menu there.
     */
    bool FullPanel::back()
    {
        if (_leaving) {
            _leaving.reset();
            return true;
        }
        if (controls::get().back() || _modPage.back() || showHelp(false)) {
            return true;
        }
        if (_onSearchField && !_modTree.query().empty()) {
            _modTree.clearSearch();
            return true;
        }
        if (isFiltered()) {
            showWhole();
            return true;
        }
        if (_report) {
            return leaveReport(true);
        }
        if (_returnToReport && _selection.mod) {
            return openReport(*_returnToReport);
        }
        if (!_selection.mod) {
            return false;
        }
        if (_selection.page.empty()) {
            select({});
        } else {
            auto above = _selection;
            above.page.pop_back();
            select(above);
        }
        return true;
    }

    /**
     * The item of the selected mod whose row the ray is on, which the mod's store tells its client as it changes:
     * none for no row.
     */
    void FullPanel::point(const model::Item* item)
    {
        _session.point(item);
    }

    /**
     * Adjust in world ended: the editor of the transform reads the value it left.
     */
    void FullPanel::resumeTransformEdit()
    {
        _modPage.resumeTransformEdit();
    }

    /**
     * The panel as the menu opens: on the menu's front page, with the tree at its top. A menu that was closed
     * does not keep its place. The editing session of a mod that was shown ends in its store.
     */
    void FullPanel::reset()
    {
        _session.end();
        _selection = {};
        _helpShown = false;
        _report.reset();
        _returnToReport.reset();
        _configSpecReport.reset();
        _frontPage.reset();
        _scrollToDeveloper = false;
        _scrollTo.reset();
        _leaving.reset();
        _pageChanged = true;
        _pressActed = false;
        _hapticPlayed.reset();
        _modPage.reset();
        _helpLine.clear();
        updateStatus();
        _modTree.showTop();
        _modTree.clearSearch();
        _search.reset();
    }

    /**
     * Searches the mods for the query of the search field, before the title bar and the tree are drawn, as the search
     * session has it. A query that changes scrolls the tree to its top, and filters the page again from its top. A
     * query emptied, as by Clear, scrolls the whole tree to the selected row instead, and shows the whole page, with
     * the focus ring on the last row the ray was on.
     */
    void FullPanel::updateSearch()
    {
        F4CF_PERF_FUNCTION();
        const bool filtered = isFiltered();
        const auto storeOfMod = [this](const model::Mod& mod) {
            return storeOf(mod);
        };
        if (!_search.update(_modTree.query(), _modList.changeableMods(), _selection.mod, storeOfMod)) {
            return;
        }
        // the whole tree back has the selected row in view, with its category open
        if (_search.query().empty() && _selection.mod) {
            _modTree.showSelection();
        } else {
            _modTree.showTop();
        }
        if (_search.query().empty() && filtered) {
            showWhole();
        } else if (isFiltered()) {
            _pageChanged = true;
        }
    }

    /**
     * Whether the content is filtered by the search: while the query has text, a mod's page has only its rows that
     * match, and the menu's front page says how many match, until Back shows it whole. Not the Help page and not a
     * report.
     */
    bool FullPanel::isFiltered() const
    {
        return _search.filtered() && !_helpShown && !_report;
    }

    /**
     * Shows the page that was filtered whole, with the focus ring on the last row the ray was on while it was
     * filtered, and the sections it is in opened, or from its top.
     */
    void FullPanel::showWhole()
    {
        const auto* pointed = _search.showWhole();
        std::vector<const model::Group*> sections;
        if (pointed && _selection.mod && findSections(selectedPage()->items, *pointed, sections)) {
            _modPage.focus(*pointed, sections);
        } else {
            _pageChanged = true;
        }
    }

    /**
     * What the content says in place of a page while searching, where the search shows none of its rows: that what
     * matches on the selected page is hidden by a condition, that nothing matches, or how many settings match, on
     * the pages in the tree.
     */
    void FullPanel::drawSearchText(const bool hiddenByConditions)
    {
        const auto& text = _search.text();
        const auto matches = _search.matches();
        if (hiddenByConditions) {
            drawPageText(std::format("What matches \xE2\x80\x9C{}\xE2\x80\x9D on this page is hidden by the values of other settings. Back shows the whole page.", text));
            return;
        }
        if (matches == 0) {
            drawPageText(std::format("Nothing matches \xE2\x80\x9C{}\xE2\x80\x9D. Search finds the names of mods, pages, sections and settings.", text));
        } else {
            drawPageText(std::format("{} setting{} \xE2\x80\x9C{}\xE2\x80\x9D. Pick a page in the tree to change {}.",
                matches,
                matches == 1 ? " matches" : "s match",
                text,
                matches == 1 ? "it" : "them"));
        }
    }

    /**
     * Draws the panel into the current ImGui window: the title bar, under it the tree of the mods with the
     * menu's front page, the selected mod's page or the Help page beside it, and the footer at the bottom.
     * The help line has the row of the page the ray is on, or the row of the tree with a help, or the button of
     * the title bar, until the ray is on another or leaves the page:
     * in the gap between two rows it keeps the row it has, so it does not flicker as the ray sweeps the rows.
     * With no row it has the status of the selected mod's editing session: how many changes wait for a Save,
     * or the last Save.
     * A press selects, or does what its button of the title bar does, once all of the panel is drawn, so a
     * frame is drawn with one selection.
     * It also tells whether a press did something in this frame, for the pulse of the hand that points.
     */
    void FullPanel::draw()
    {
        F4CF_PERF_FUNCTION();
        panelTargets().startFrame();
        if (!_styleApplied) {
            theme::applyStyle(ImGui::GetStyle());
            _styleApplied = true;
        }

        _pressActed = false;
        _hapticPlayed.reset();
        {
            // the states of the mod's client are asked again in each frame
            F4CF_PERF_SCOPE("startFrame");
            _session.startFrame();
        }
        const auto shown = available(_selection);
        if (shown != _selection) {
            const bool helpShown = _helpShown;
            show(shown);
            _helpShown = helpShown;
        }
        updateSearch();
        const auto crumbs = breadcrumbs();
        auto titleBar = drawTitleBar(crumbs, _highlightHelp);
        const auto crumbPressed = titleBar.pressed ? crumbs[*titleBar.pressed].target : std::nullopt;
        // the body stands a little further from the title bar than a row from a row
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_3 - ImGui::GetStyle().ItemSpacing.y);

        // the body ends as far above the footer as it starts under the title bar
        const auto bodyPos = ImGui::GetCursorScreenPos();
        const float bodyHeight = ImGui::GetContentRegionAvail().y - theme::SPACE_3 - theme::FOOTER_HEIGHT;

        const TreeSearch search{ .query = &_search.query(), .found = &_search.found(), .matches = _search.matches() };
        auto tree =
            _modTree.draw(mods(), _selection, bodyHeight, selectedStore(), _search.query().empty() ? nullptr : &search, _modList.groups().empty() ? nullptr : &_modList.groups());
        _onSearchField = tree.searchPointed;

        ImGui::SameLine(0.0f, theme::SPACE_4);
        auto content = drawContent(bodyHeight);
        point(content.pointedItem);

        auto pointed = std::move(content.pointed);
        if (!pointed) {
            pointed = std::move(tree.pointed);
        }
        if (!pointed) {
            pointed = std::move(titleBar.pointed);
        }
        if (pointed) {
            _helpLine.show(*pointed);
        } else if (!content.onPage) {
            _helpLine.clear();
        }
        updateStatus();
        drawFooter(ImVec2(bodyPos.x, bodyPos.y + bodyHeight + theme::SPACE_3));

        const bool acted = actOnPress(tree, crumbPressed, titleBar.pressedButton, content);
        _pressActed = _pressActed || acted || tree.searchActed;
        drawDialog();
    }

    /**
     * Draws the content's pane beside the tree, of the given height: the Help page, a config spec's report, the
     * selected mod's page or what the search says in place of it, or the menu's front page.
     */
    FullPanel::ContentDrawn FullPanel::drawContent(const float height)
    {
        if (_pageChanged) {
            // the pages share the pane, which would keep the place the page before was scrolled to
            ImGui::SetNextWindowScroll(ImVec2(0.0f, _scrollTo.value_or(0.0f)));
            _pageChanged = false;
        }
        ContentDrawn content;
        if (ImGui::BeginChild("Content", ImVec2(0.0f, height))) {
            const bool onPage = ImGui::IsWindowHovered();
            if (_helpShown) {
                drawHelpPage(_controls);
            } else if (_report) {
                content = drawReport();
            } else if (_selection.mod && isFiltered() && !_search.showsRows(*_selection.mod, *selectedPage())) {
                drawSearchText(_search.hiddenByConditions(*_selection.mod, mods()[*_selection.mod], *selectedPage()));
            } else if (_selection.mod) {
                content = drawSelectedPage(onPage);
            } else if (isFiltered()) {
                drawSearchText(false);
            } else {
                content = drawFrontPage();
            }
            content.onPage = onPage;
            panelTargets().addScroll(ScrollArea::Page);
        }
        ImGui::EndChild();
        return content;
    }

    /**
     * Draws the report of the config spec that is shown and does what a press on its live copy asks. A press on a
     * problem's row is shown after the panel is drawn.
     */
    FullPanel::ContentDrawn FullPanel::drawReport()
    {
        const auto& mod = mods()[*_report];
        auto drawn = _configSpecReport.draw(mod, _modList.changeableRead(mod.folder));
        ContentDrawn content{ .pointed = std::move(drawn.pointed) };
        _pressActed = drawn.pressActed;
        if (drawn.pressed) {
            content.problem = ContentDrawn::ReportProblem{ .mod = *_report, .problem = *drawn.pressed };
        }
        if (drawn.liveCopy != LiveCopyAction::None && _liveCopyAction) {
            _liveCopyAction(mod.folder, drawn.liveCopy);
        }
        // a place the pane is scrolled to is set again once the report is laid out in it, as ImGui keeps it
        // within the height of the page the pane had before
        if (_scrollTo) {
            ImGui::SetScrollY(*_scrollTo);
            _scrollTo.reset();
        }
        _reportScroll = ImGui::GetScrollY();
        return content;
    }

    /**
     * Draws the selected page of the selected mod, with what the search found on it, and does what a press on an
     * Adjust button asks. The pointed item stays in the gap between two rows while the ray is on the pane, as the
     * help line's row does.
     */
    FullPanel::ContentDrawn FullPanel::drawSelectedPage(const bool onPage)
    {
        const auto& mod = mods()[*_selection.mod];
        PageSearch pageSearch;
        if (const auto* found = _search.foundIn(*_selection.mod)) {
            pageSearch = { .query = &_search.query(), .found = &found->rows, .hidden = &found->hidden, .filtered = isFiltered() };
        }
        auto drawn = _modPage.draw(*selectedPage(), storeOf(mod), pageSearch);
        if (pageSearch.filtered) {
            _search.pointWhileFiltered(drawn.pointedItem);
        }
        ContentDrawn content{ .pointed = std::move(drawn.pointed) };
        content.pointedItem = drawn.pointedItem ? drawn.pointedItem : onPage ? _session.pointed() : nullptr;
        _pressActed = drawn.pressActed;
        _hapticPlayed = drawn.haptic;
        if (drawn.adjust && _menuActions.adjustTransform) {
            _menuActions.adjustTransform(*drawn.adjust, *storeOf(mod));
        }
        return content;
    }

    /**
     * Draws the menu's front page, and its Developer section while the developer view is on. A press that opens
     * a config spec's report is done after the panel is drawn.
     */
    FullPanel::ContentDrawn FullPanel::drawFrontPage()
    {
        auto drawn = _frontPage.draw(frontPageMods());
        ContentDrawn content{ .pointed = std::move(drawn.pointed) };
        _pressActed = drawn.pressActed;
        if (_developerView.enabled) {
            auto developer = _developer.draw(mods(), _developerView, _scrollToDeveloper);
            _scrollToDeveloper = false;
            content.report = developer.opened;
            if (developer.pointed) {
                content.pointed = std::move(developer.pointed);
            }
            if (developer.changed && _writeDeveloperView) {
                _writeDeveloperView(_developerView);
            }
            _pressActed = _pressActed || developer.pressActed;
        }
        return content;
    }

    /**
     * Does what the press of this frame asks, once all of the panel is drawn: a row of the tree, a breadcrumb, a
     * problem of a report, a report to open, or a button of the title bar, the first that was pressed. A press on
     * the row or the breadcrumb of what is shown already changes nothing, and neither does Help on the Help page.
     * Returns whether it did something.
     */
    bool FullPanel::actOnPress(const TreeDrawn& tree, const std::optional<CrumbTarget>& crumbPressed, const TitleButton pressedButton, const ContentDrawn& content)
    {
        if (tree.pressed) {
            return select(*tree.pressed);
        }
        if (crumbPressed) {
            return pressCrumb(*crumbPressed);
        }
        if (content.problem) {
            return showProblem(content.problem->mod, content.problem->problem);
        }
        if (content.report) {
            return openReport(*content.report);
        }
        return pressButton(pressedButton);
    }

    /**
     * Does what a button of the title bar does: Help shows the Help page, and Minimize and Close are the
     * menu's, and Close asks first while the selected mod has unsaved changes. Returns whether it did something.
     */
    bool FullPanel::pressButton(const TitleButton button)
    {
        switch (button) {
        case TitleButton::Help:
            return showHelp(true);
        case TitleButton::Minimize:
            if (_menuActions.minimize) {
                _menuActions.minimize();
                return true;
            }
            return false;
        case TitleButton::Close:
            return close();
        default:
            return false;
        }
    }

    /**
     * Reads each mod's notice data for the front page, in the order of the tree.
     * The names and availability reasons refer to the panel's mods.
     */
    std::vector<FrontPageMod> FullPanel::frontPageMods() const
    {
        std::vector<FrontPageMod> frontPage;
        frontPage.reserve(mods().size());
        for (const auto& mod : mods()) {
            const auto* store = storeOf(mod);
            frontPage.push_back({ .displayName = mod.displayName, .savedRestartCount = store ? store->savedRestartCount(mod) : 0, .notAvailable = mod.notAvailable });
        }
        return frontPage;
    }

    /**
     * The footer, from its top left: a line over it, the help line on its left, and Discard and Save on its
     * right, which keep their places. They are enabled while the selected mod has changes that wait for a
     * Save, also after a Save that failed, and are disabled on the menu's front page, which has no session.
     * Save is disabled while a change is invalid, which the status says. A press on Save writes the changes,
     * and one on Discard takes them back. An MCM mod's are named Keep and Revert, as its changes are written already.
     */
    void FullPanel::drawFooter(const ImVec2& pos)
    {
        F4CF_PERF_FUNCTION();
        const float width = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + theme::BORDER_FRAME), ImGui::GetColorU32(theme::color::RULE));

        auto* const store = _selection.mod ? storeOf(mods()[*_selection.mod]) : nullptr;
        const bool made = store && store->writesChangesAtOnce();
        const float top = pos.y + (theme::FOOTER_HEIGHT - theme::CONTROL_HEIGHT) * 0.5f;
        const auto save = controls::controlBox(pos.x + width - theme::SPACE_2 - BUTTON_WIDTH, top, BUTTON_WIDTH);
        const auto discard = controls::controlBox(save.Min.x - theme::SPACE_3 - BUTTON_WIDTH, top, BUTTON_WIDTH);
        ImGui::BeginDisabled(!store || store->dirtyCount() == 0);
        if (panelButton(made ? "Revert" : "Discard", discard, controls::ButtonLook::Secondary)) {
            this->discard();
            _pressActed = true;
        }
        ImGui::BeginDisabled(store && store->invalidCount() > 0);
        if (panelButton(made ? "Keep" : "Save", save, controls::ButtonLook::Primary)) {
            // the status has the reason of a Save that failed, which the store keeps
            std::ignore = this->save();
            _pressActed = true;
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        updateStatus();

        const float helpRight = discard.Min.x - theme::SPACE_3;
        _helpLine.draw(ImVec2(pos.x + theme::SPACE_4, pos.y), helpRight - pos.x - theme::SPACE_4);
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(width, theme::FOOTER_HEIGHT));
    }

    /**
     * The dialog that asks what to do with the selected mod's unsaved changes, while it asks, and what a press on
     * one of its buttons does, with `answerDialog`.
     */
    void FullPanel::drawDialog()
    {
        F4CF_PERF_FUNCTION();
        auto* const store = selectedStore();
        if (!store) {
            _leaving.reset();
        }
        std::optional<UnsavedChanges> changes;
        if (_leaving) {
            changes = UnsavedChanges{ .mod = mods()[*_selection.mod].displayName,
                .count = store->dirtyCount(),
                .invalid = store->invalidCount(),
                .next = _leaving->closeMenu ? std::string("close the menu")
                        : _leaving->report  ? std::format("open the report of {}", mods()[*_leaving->report].displayName)
                        : _leaving->to.mod  ? std::format("open {}", mods()[*_leaving->to.mod].displayName)
                                            : std::string("go to the menu's front page"),
                .made = store->writesChangesAtOnce() };
        }
        const auto answer = drawUnsavedDialog(changes ? &*changes : nullptr);
        if (answer != DialogAnswer::None) {
            _pressActed = answerDialog(answer);
        }
    }

    /**
     * Does what a button of the dialog that asks about the selected mod's unsaved changes does, as a press on it:
     * Save writes the changes and then does what the player was about to do, and a Save that fails keeps the player
     * on the mod, with the reason in the status. Discard takes the changes back and does it. Cancel closes the dialog
     * with nothing done. The dialog closes as it is drawn next. Returns whether it did something: nothing while the
     * dialog does not ask, and nothing for Save while a change is invalid, as Save is disabled then.
     */
    bool FullPanel::answerDialog(const DialogAnswer answer)
    {
        auto* const store = selectedStore();
        if (!_leaving || !store || answer == DialogAnswer::None || (answer == DialogAnswer::Save && store->invalidCount() > 0)) {
            return false;
        }
        if (answer == DialogAnswer::Cancel || (answer == DialogAnswer::Save && !store->save())) {
            // a Save that failed has its reason in the status, which the store keeps
            _leaving.reset();
        } else {
            if (answer == DialogAnswer::Discard) {
                store->discard();
            }
            leave();
        }
        updateStatus();
        return true;
    }
}
