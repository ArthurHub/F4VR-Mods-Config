#include "FullPanel.h"

#include <algorithm>
#include <format>
#include <string>
#include <tuple>

#include <imgui.h>

#include "Drawing.h"
#include "HelpPage.h"
#include "Theme.h"
#include "common/CommonUtils.h"

namespace
{
    using namespace mods_config;

    // what the help line says of a mod whose changes are not kept for a Save, while the ray is on no row
    constexpr auto SAVED_AS_MADE = "Changes are saved as they are made";

    // and of a mod whose changes wait for a Save, while none does
    constexpr auto NO_UNSAVED = "No unsaved changes";

    // and after a Save that wrote no change that takes effect after a restart, until the next change
    constexpr auto SAVED = "Saved";

    // the width of a footer button, in the panel's pixels
    constexpr float BUTTON_WIDTH = 160.0f;

}

namespace mods_config::ui
{
    /**
     * The mods the menu lists, from every source they come from. They are kept in order of their display names,
     * and none of them is selected.
     */
    void FullPanel::setMods(std::vector<model::Mod> mods)
    {
        std::ranges::sort(mods, common::CaseInsensitiveComparator(), &model::Mod::displayName);
        _mods = std::move(mods);
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
     * The Developer tools as Mods Config VR's settings have them, and what writes a change of their filter or
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
        (source == model::Source::Mcm ? _mcmStore : _packageStore) = store;
    }

    /**
     * The store of the values of a mod: the one of the source of its menu.
     */
    model::Store* FullPanel::storeOf(const model::Mod& mod) const
    {
        return mod.source == model::Source::Mcm ? _mcmStore : _packageStore;
    }

    /**
     * The store of the selected mod's values, which its editing session is in. None on the menu's front page.
     */
    model::Store* FullPanel::selectedStore() const
    {
        return _selection.mod ? storeOf(_mods[*_selection.mod]) : nullptr;
    }

    /**
     * Gives the help line the status of the selected mod's editing session: that its changes are saved as
     * they are made, how many changes are invalid, which block Save, why the last Save failed, until the next
     * change, how many wait for a Save, with how many of them take effect after a restart, or the last Save,
     * until the next change, with how many of the changes it wrote take effect after a restart. Nothing with
     * no mod, and with no store.
     */
    void FullPanel::updateStatus()
    {
        auto* const store = _selection.mod ? storeOf(_mods[*_selection.mod]) : nullptr;
        if (!store) {
            _helpLine.setStatus({});
        } else if (store->savesAtOnce()) {
            _helpLine.setStatus(SAVED_AS_MADE);
        } else if (const auto invalid = store->invalidCount(); invalid > 0) {
            _helpLine.setStatus(std::format("{} invalid value{}", invalid, invalid == 1 ? "" : "s"), StatusLook::Invalid);
        } else if (const auto failure = store->saveFailure(); !failure.empty()) {
            _helpLine.setStatus(std::format("Not saved: {}.", failure), StatusLook::Invalid);
        } else if (const auto dirty = store->dirtyCount(); dirty > 0) {
            auto status = std::format("{} unsaved change{}", dirty, dirty == 1 ? "" : "s");
            if (const auto restarts = store->dirtyRestartCount(); restarts > 0) {
                status += std::format(", {} take{} effect after a restart", restarts, restarts == 1 ? "s" : "");
            }
            _helpLine.setStatus(status, StatusLook::Pending);
        } else if (const auto saved = store->lastSave(); saved && *saved > 0) {
            _helpLine.setStatus(std::format("Saved. {} change{} effect after the game is restarted.", *saved, *saved == 1 ? " takes" : "s take"), StatusLook::Restart);
        } else {
            _helpLine.setStatus(saved ? SAVED : NO_UNSAVED);
        }
    }

    /**
     * How far the thumbstick of the hand that points is pushed to a side, from -1 at the left to 1 at the
     * right, for the frames that are drawn next: it steps the slider or the stepper the ray is on. The panel does not know
     * the game, so the menu tells it.
     */
    void FullPanel::setThumbstickSideValue(const float value)
    {
        _thumbstickSideValue = value;
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
     * And it leaves a package's report.
     * Returns whether it did something: it shows another page than before, or the dialog asks.
     */
    bool FullPanel::select(const Selection& selection)
    {
        Selection shown;
        if (selection.mod && *selection.mod < _mods.size() && _mods[*selection.mod].notAvailable.empty()) {
            shown.mod = selection.mod;
            const auto* pages = &_mods[*selection.mod].pages;
            for (const auto place : selection.page) {
                if (place >= pages->size()) {
                    break;
                }
                shown.page.push_back(place);
                pages = &(*pages)[place].pages;
            }
        }
        if (shown == _selection) {
            // the menu's front page, which a report is shown over, from the report
            return _report ? leaveReport(false) : showHelp(false);
        }
        if (shown.mod != _selection.mod && hasUnsavedChanges()) {
            _leaving = Leave{ .to = shown };
            return true;
        }
        return show(shown);
    }

    /**
     * Shows a selection of what is there. Another mod than before starts its editing session in its store,
     * which ends the session it had. A mod that is left for no mod, or for a mod of another store, ends its
     * session in its store. Another page of the same mod is told to its store, which reads the values it keeps
     * in files again. A report that was shown is left, and so is the report Back went to.
     * Returns true: it shows another page than before.
     */
    bool FullPanel::show(const Selection& shown)
    {
        _helpShown = false;
        _report.reset();
        _returnToReport.reset();
        auto* const storeBefore = _selection.mod ? storeOf(_mods[*_selection.mod]) : nullptr;
        auto* const store = shown.mod ? storeOf(_mods[*shown.mod]) : nullptr;
        if (shown.mod != _selection.mod) {
            if (storeBefore && storeBefore != store) {
                storeBefore->close();
            }
            if (store) {
                store->open(_mods[*shown.mod]);
            }
        } else if (store) {
            store->pageShown();
        }
        _selection = shown;
        pageShown();
        return true;
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
     * left. An MCM mod has none: its changes are saved as they are made.
     */
    bool FullPanel::hasUnsavedChanges() const
    {
        const auto* store = selectedStore();
        return store && store->dirtyCount() > 0;
    }

    /**
     * Does what the player was about to do as the dialog asked, after its Save or its Discard: shows the
     * selection or a package's report, or closes the menu. The dialog is closed. Returns true: it did something.
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
        if (!_selection.mod) {
            return nullptr;
        }
        const auto& mod = _mods[*_selection.mod];
        const auto* page = &mod.frontPage;
        const auto* pages = &mod.pages;
        for (const auto place : _selection.page) {
            page = &(*pages)[place];
            pages = &page->pages;
        }
        return page;
    }

    /**
     * Another page is shown in the content pane, from its top: a row of the page before keeps no drag, list,
     * edit or help, and the tree shows the selection.
     */
    void FullPanel::pageShown()
    {
        _pageChanged = true;
        _modPage.reset();
        _helpLine.clear();
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
        pageShown();
        return true;
    }

    /**
     * Shows the report of a package over the menu's front page, as a press on its row of the Developer section
     * does. A mod that is selected is left, and the dialog asks first while it has unsaved changes. Returns
     * whether it did something: it shows the report, or the dialog asks.
     */
    bool FullPanel::openReport(const std::size_t mod)
    {
        if (mod >= _mods.size()) {
            return false;
        }
        if (hasUnsavedChanges()) {
            _leaving = Leave{ .to = {}, .closeMenu = false, .report = mod };
            return true;
        }
        if (_selection.mod) {
            show({});
        }
        _helpShown = false;
        _report = mod;
        _returnToReport.reset();
        pageShown();
        return true;
    }

    /**
     * Leaves a package's report for the menu's front page, scrolled to its Developer section, as Back and the
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
     * Shows the row of a problem of a package's report, as a press on the problem does: the mod at the row's
     * page, with the row scrolled into view in the focus ring, and the sections it is in opened. Back then
     * shows the report again. A row the page does not have is not focused. Returns whether it shows the page.
     */
    bool FullPanel::showProblem(const std::size_t mod, const std::size_t problem)
    {
        if (mod >= _mods.size() || problem >= _mods[mod].problems.size() || !_mods[mod].problems[problem].place) {
            return false;
        }
        const auto place = *_mods[mod].problems[problem].place;
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
     * Goes one step back: the dialog is cancelled, with nothing saved or discarded, a slider that is dragged
     * has the value it had before the drag, the list of a dropdown that is open is closed, and a text input that
     * is edited keeps its value. With none of them, from the Help page to the page it was shown over, from a
     * package's report to the Developer section of the menu's front page, from the page a problem of a report
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
        if (_modPage.cancelDrag() || _modPage.closeList() || _modPage.cancelEdit() || showHelp(false)) {
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
     * The panel as the menu opens: on the menu's front page, with the tree at its top. A menu that was closed
     * does not keep its place. The editing session of a mod that was shown ends in its store.
     */
    void FullPanel::reset()
    {
        if (auto* const store = _selection.mod ? storeOf(_mods[*_selection.mod]) : nullptr) {
            store->close();
        }
        _selection = {};
        _helpShown = false;
        _report.reset();
        _returnToReport.reset();
        _packageReport.reset();
        _frontPage.reset();
        _scrollToDeveloper = false;
        _leaving.reset();
        _pageChanged = true;
        _pressActed = false;
        _thumbstickSideValue = 0.0f;
        _modPage.reset();
        _helpLine.clear();
        updateStatus();
        _modTree.showTop();
    }

    /**
     * Draws the panel into the current ImGui window: the title bar, under it the tree of the mods with the
     * menu's front page, the selected mod's page or the Help page beside it, and the footer at the bottom.
     * The help line has the row of the page the ray is on, or the row of the tree with a help, or the button of
     * the title bar, until the ray is on another or leaves the page:
     * in the gap between two rows it keeps the row it has, so it does not flicker as the ray sweeps the rows.
     * With no row it has the status of the selected mod's editing session: that its changes are saved as they
     * are made, or how many wait for a Save.
     * A press selects, or does what its button of the title bar does, once all of the panel is drawn, so a
     * frame is drawn with one selection.
     * It also tells whether a press did something in this frame, for the pulse of the hand that points.
     */
    void FullPanel::draw()
    {
        if (!_styleApplied) {
            theme::applyStyle(ImGui::GetStyle());
            _styleApplied = true;
        }

        _pressActed = false;
        auto titleBar = drawTitleBar(_mods, _selection, _helpShown, _report && !_helpShown ? &_mods[*_report] : nullptr);
        auto pressed = titleBar.pressed;
        // the body stands a little further from the title bar than a row from a row
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_3 - ImGui::GetStyle().ItemSpacing.y);

        // the body ends as far above the footer as it starts under the title bar
        const auto bodyPos = ImGui::GetCursorScreenPos();
        const float bodyHeight = ImGui::GetContentRegionAvail().y - theme::SPACE_3 - theme::FOOTER_HEIGHT;

        auto tree = _modTree.draw(_mods, _selection, bodyHeight);
        if (tree.pressed) {
            pressed = tree.pressed;
        }

        ImGui::SameLine(0.0f, theme::SPACE_4);
        if (_pageChanged) {
            // the pages share the pane, which would keep the place the page before was scrolled to
            ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
            _pageChanged = false;
        }
        std::optional<PointedRow> pointed;
        bool onPage = false;

        // a problem of a report whose row a press asks to show, and a package whose report a press opens: after
        // the panel is drawn, as a press on a row of the tree is
        struct ReportProblem
        {
            std::size_t mod = 0;
            std::size_t problem = 0;
        };

        std::optional<ReportProblem> problem;
        std::optional<std::size_t> report;
        if (ImGui::BeginChild("Content", ImVec2(0.0f, bodyHeight))) {
            onPage = ImGui::IsWindowHovered();
            if (_helpShown) {
                drawHelpPage(_controls);
            } else if (_report) {
                auto drawn = _packageReport.draw(_mods[*_report]);
                pointed = std::move(drawn.pointed);
                _pressActed = drawn.pressActed;
                if (drawn.pressed) {
                    problem = ReportProblem{ .mod = *_report, .problem = *drawn.pressed };
                }
            } else if (_selection.mod) {
                auto drawn = _modPage.draw(*selectedPage(), storeOf(_mods[*_selection.mod]), _thumbstickSideValue);
                pointed = std::move(drawn.pointed);
                _pressActed = drawn.pressActed;
            } else {
                auto drawn = _frontPage.draw(frontPageMods());
                pointed = std::move(drawn.pointed);
                _pressActed = drawn.pressActed;
                if (_developerView.enabled) {
                    auto developer = _developer.draw(_mods, _developerView, _thumbstickSideValue, _scrollToDeveloper);
                    _scrollToDeveloper = false;
                    report = developer.opened;
                    if (developer.pointed) {
                        pointed = std::move(developer.pointed);
                    }
                    if (developer.changed && _writeDeveloperView) {
                        _writeDeveloperView(_developerView);
                    }
                    _pressActed = _pressActed || developer.pressActed;
                }
            }
        }
        ImGui::EndChild();

        if (!pointed) {
            pointed = std::move(tree.pointed);
        }
        if (!pointed) {
            pointed = std::move(titleBar.pointed);
        }
        if (pointed) {
            _helpLine.show(*pointed);
        } else if (!onPage) {
            _helpLine.clear();
        }
        updateStatus();
        drawFooter(ImVec2(bodyPos.x, bodyPos.y + bodyHeight + theme::SPACE_3));

        // a press on the row or the breadcrumb of what is shown already changes nothing, and neither does Help
        // on the Help page
        bool acted = false;
        if (pressed) {
            acted = select(*pressed);
        } else if (titleBar.developerPressed) {
            acted = leaveReport(true);
        } else if (problem) {
            acted = showProblem(problem->mod, problem->problem);
        } else if (report) {
            acted = openReport(*report);
        } else {
            acted = pressButton(titleBar.pressedButton);
        }
        _pressActed = _pressActed || acted;
        drawDialog();
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
        std::vector<FrontPageMod> mods;
        mods.reserve(_mods.size());
        for (const auto& mod : _mods) {
            const auto* store = storeOf(mod);
            mods.push_back({ .displayName = mod.displayName, .savedRestartCount = store ? store->savedRestartCount(mod) : 0, .notAvailable = mod.notAvailable });
        }
        return mods;
    }

    /**
     * The footer, from its top left: a line over it, the help line on its left, and Discard and Save on its
     * right, which keep their places. They are enabled while the selected mod has changes that wait for a
     * Save, also after a Save that failed, and are disabled on the menu's front page, which has no session.
     * Save is disabled while a change is invalid, which the status says. A press on Save writes the changes,
     * and one on Discard takes them back. An MCM mod saves its changes as they are made, so it has no Discard
     * and Save, and its help line has all of the footer's width.
     */
    void FullPanel::drawFooter(const ImVec2& pos)
    {
        const float width = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + theme::BORDER_FRAME), ImGui::GetColorU32(theme::color::RULE));

        float helpRight = pos.x + width - theme::SPACE_2;
        auto* const store = _selection.mod ? storeOf(_mods[*_selection.mod]) : nullptr;
        if (!store || !store->savesAtOnce()) {
            const float top = pos.y + (theme::FOOTER_HEIGHT - theme::CONTROL_HEIGHT) * 0.5f;
            const auto save = controlBox(helpRight - BUTTON_WIDTH, top, BUTTON_WIDTH);
            const auto discard = controlBox(save.Min.x - theme::SPACE_3 - BUTTON_WIDTH, top, BUTTON_WIDTH);
            ImGui::BeginDisabled(!store || store->dirtyCount() == 0);
            if (button("Discard", discard, ButtonLook::Secondary)) {
                store->discard();
                _pressActed = true;
            }
            ImGui::BeginDisabled(store && store->invalidCount() > 0);
            if (button("Save", save, ButtonLook::Primary)) {
                // the status has the reason of a Save that failed, which the store keeps
                std::ignore = store->save();
                _pressActed = true;
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            updateStatus();
            helpRight = discard.Min.x - theme::SPACE_3;
        }

        _helpLine.draw(ImVec2(pos.x + theme::SPACE_4, pos.y), helpRight - pos.x - theme::SPACE_4);
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(width, theme::FOOTER_HEIGHT));
    }

    /**
     * The dialog that asks what to do with the selected mod's unsaved changes, while it asks, and what its
     * answer does. Save writes the changes and then does what the player was about to do, and a Save that
     * fails keeps the player on the mod, with the reason in the status. Discard takes the changes back and does
     * it. Cancel closes the dialog with nothing done, as Back does.
     */
    void FullPanel::drawDialog()
    {
        auto* const store = selectedStore();
        if (!store) {
            _leaving.reset();
        }
        std::optional<UnsavedChanges> changes;
        if (_leaving) {
            changes = UnsavedChanges{ .mod = _mods[*_selection.mod].displayName,
                .count = store->dirtyCount(),
                .invalid = store->invalidCount(),
                .next = _leaving->closeMenu ? std::string("close the menu")
                        : _leaving->report  ? std::format("open the report of {}", _mods[*_leaving->report].displayName)
                        : _leaving->to.mod  ? std::format("open {}", _mods[*_leaving->to.mod].displayName)
                                            : std::string("go to the menu's front page") };
        }
        const auto answer = drawUnsavedDialog(changes ? &*changes : nullptr);
        if (answer == DialogAnswer::None) {
            return;
        }
        _pressActed = true;
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
    }
}
