#pragma once

#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ConfigSpecReport.h"
#include "Developer.h"
#include "FrontPage.h"
#include "HelpLine.h"
#include "MenuControls.h"
#include "ModList.h"
#include "ModPage.h"
#include "ModTree.h"
#include "SearchSession.h"
#include "Selection.h"
#include "StoreSession.h"
#include "TitleBar.h"
#include "UnsavedDialog.h"
#include "model/ConfigDefinition.h"
#include "model/Search.h"
#include "model/Store.h"

namespace mods_config::ui
{
    /**
     * The content of the Full panel, drawn with ImGui as the design system has it: the title bar with its
     * breadcrumbs and its Help, Minimize and Close, the tree of the mods, beside it the menu's front page, a
     * page of the selected mod or the Help page, and the footer with the help line, and Discard and Save.
     * Before a mod with unsaved changes is left, or the menu is closed from it, a dialog asks to Save, Discard
     * or Cancel. It knows nothing of the game, so a test draws it too.
     */
    class FullPanel
    {
    public:
        using QuickLaunchEntry = mods_config::ui::QuickLaunchEntry;

        /**
         * What the help line says of the selected mod's editing session while the ray is on no row, and its look.
         */
        struct SessionStatus
        {
            std::string text;
            StatusLook look = StatusLook::Plain;
        };

        /**
         * What the title bar's Minimize and Close do, which the menu does: the panel does not know the game.
         */
        struct MenuActions
        {
            std::function<void()> minimize;
            std::function<void()> close;

            // tunes a transform of the selected mod in the world, in its store
            std::function<void(const model::Setting& setting, model::Store& store)> adjustTransform;
        };

        void setMods(std::vector<model::Mod> mods);
        void setQuickLaunch(std::vector<QuickLaunchEntry> entries);
        void setControls(std::vector<ControlHelp> controls);
        void setLeftHanded(bool leftHanded);
        void setMenuActions(MenuActions actions);
        void setDeveloper(DeveloperView view, std::function<void(const DeveloperView&)> write);
        void setHighlightHelp(bool highlight, std::function<void()> opened);
        void setLiveCopyAction(std::function<void(const std::string& folder, LiveCopyAction action)> act);
        std::size_t replaceMod(model::Mod mod, std::string_view time);
        void setImageDrawer(ImageDrawer drawer);
        void setStore(model::Source source, model::Store* store);
        void setThumbstickSideValue(float value);
        void setTreeWidth(float width);
        bool select(const Selection& selection);
        std::vector<Crumb> breadcrumbs() const;
        bool pressCrumb(const CrumbTarget& target);
        bool close();
        bool showHelp(bool shown);
        bool back();
        void resumeTransformEdit();
        void point(const model::Item* item);
        void reset();
        void draw();
        std::expected<void, std::string> save();
        void discard();
        void readAgain();
        model::Store* selectedStore() const;
        SessionStatus sessionStatus() const;
        std::vector<FrontPageMod> frontPageMods() const;
        const model::Page* selectedPage() const;

        const std::vector<model::Mod>& mods() const
        {
            return _modList.mods();
        }

        // the mods with their categories and live copy reads, which the menu sets the categories and the reads of
        ModList& modList()
        {
            return _modList;
        }

        const ModList& modList() const
        {
            return _modList;
        }

        const Selection& selection() const
        {
            return _selection;
        }

        // the entries of the quick launch section of the menu's front page, as the menu gave them
        const std::vector<QuickLaunchEntry>& quickLaunch() const
        {
            return _frontPage.quickLaunch();
        }

        const HelpLine& helpLine() const
        {
            return _helpLine;
        }

        // the search of the mods for the search field's query, as the panel last drew it
        const SearchSession& search() const
        {
            return _search;
        }

        // gives the search field a query, as the keyboard types it, which the panel searches as it draws next
        void setSearchQuery(std::string query)
        {
            _modTree.setSearchQuery(std::move(query));
        }

        bool isFiltered() const;

        std::optional<ModPage::OpenEditor> openEditor() const
        {
            return _modPage.openEditor();
        }

        bool isSearchEdited() const
        {
            return _modTree.isSearchEdited();
        }

        // whether a section that folds, of the page that is shown, is folded now
        bool isFolded(const model::Group& group) const
        {
            return _modPage.isFolded(group);
        }

        // folds a section that folds, of the page that is shown, or opens it, as a press on its heading does
        void setFolded(const model::Group& group, const bool folded)
        {
            _modPage.setFolded(group, folded);
        }

        const DeveloperView& developerView() const
        {
            return _developerView;
        }

        // the config spec whose report is shown over the menu's front page, by its place among the mods, and none
        std::optional<std::size_t> report() const
        {
            return _report;
        }

        bool openReport(std::size_t mod);
        bool showProblem(std::size_t mod, std::size_t problem);
        bool pressLiveCopy(LiveCopyAction action);
        bool changeDeveloper(const DeveloperView& view);
        bool answerDialog(DialogAnswer answer);

        // whether the Help page is shown, over the selection, which it keeps
        bool isHelpShown() const
        {
            return _helpShown;
        }

        // whether the dialog asks what to do with the selected mod's unsaved changes, before the mod is left or
        // the menu is closed
        bool isAsking() const
        {
            return _leaving.has_value();
        }

        /**
         * Whether a press did something in the frame that was drawn last: it showed another page, opened a
         * quick launch entry, changed a value, took hold of a slider, opened the list of a dropdown, saved or
         * discarded the changes, or minimized or closed the menu. It is the frame the hand that points gets its
         * pulse in. So is a step of a slider or a stepper by the thumbstick. A press that changes nothing, as one
         * on the row that is selected, is not told, and neither are the steps of a slider that is dragged.
         */
        bool pressActed() const
        {
            return _pressActed;
        }

        /**
         * The pattern a press on a haptic played in the frame that was drawn last: its Play button, or an option
         * picked in its list. The hand that points plays it in place of the pulse, as it is the press's feedback.
         */
        std::optional<vrcf::HapticPattern> hapticPlayed() const
        {
            return _hapticPlayed;
        }

    private:
        /**
         * What the player was about to do as the dialog asks what to do with the unsaved changes: show
         * another selection, or close the menu.
         */
        struct Leave
        {
            Selection to;
            bool closeMenu = false;

            // the config spec whose report is shown over the menu's front page, which it goes to
            std::optional<std::size_t> report;
        };

        /**
         * What the drawing of the content tells, which the panel acts on once all of it is drawn, as it does a press
         * on a row of the tree.
         */
        struct ContentDrawn
        {
            // the row the ray is on, for the help line, and the item of the mod's page it is on, which stays in the
            // gap between two rows
            std::optional<PointedRow> pointed;
            const model::Item* pointedItem = nullptr;

            // the ray is on the content's pane
            bool onPage = false;

            // a problem of a report whose row a press asks to show, by the mod's place and the problem's
            struct ReportProblem
            {
                std::size_t mod = 0;
                std::size_t problem = 0;
            };

            std::optional<ReportProblem> problem;

            // the config spec whose report a press on the Developer section opens, by its place among the mods
            std::optional<std::size_t> report;
        };

        model::Store* storeOf(const model::Mod& mod) const;
        Selection existing(const Selection& selection) const;
        Selection available(const Selection& selection) const;
        bool hasUnsavedChanges() const;
        bool show(const Selection& shown);
        model::Page* shownPage();
        bool readContentAgain();
        bool leave();
        bool leaveReport(bool toDeveloper);
        void updateStatus();
        void updateSearch();
        void showWhole();
        void drawSearchText(bool hiddenByConditions);
        ContentDrawn drawContent(float height);
        ContentDrawn drawReport();
        ContentDrawn drawSelectedPage(bool onPage);
        ContentDrawn drawFrontPage();
        bool actOnPress(const TreeDrawn& tree, const std::optional<CrumbTarget>& crumbPressed, TitleButton pressedButton, const ContentDrawn& content);
        void pageShown();
        bool pressButton(TitleButton button);
        void drawFooter(const ImVec2& pos);
        void drawDialog();

        // the mods the menu lists, in order of their display names, with their categories and live copy reads
        ModList _modList;

        // the controls of the menu that the Help page tells, and what Minimize and Close do
        std::vector<ControlHelp> _controls;
        MenuActions _menuActions;

        // the mod and the page that are shown: none is the menu's front page
        Selection _selection;

        // the Help page is shown in place of the selection's page, which Back shows again
        bool _helpShown = false;

        // Help stands out until the Help page is first shown, which is told
        bool _highlightHelp = false;
        std::function<void()> _helpOpened;

        // the config spec whose report is shown over the menu's front page, by its place among the mods, and none.
        // And the one Back shows again, from the page a press on one of its problems showed
        std::optional<std::size_t> _report;
        std::optional<std::size_t> _returnToReport;
        FrontPage _frontPage;
        ConfigSpecReport _configSpecReport;

        // the front page is scrolled to its Developer section as it is drawn next, as it is left a report for
        bool _scrollToDeveloper = false;

        // where the report was scrolled to as it was drawn last, and where the page that is shown next is scrolled
        // to: Back from the row of a problem shows its report where it was. None is its top
        float _reportScroll = 0.0f;
        std::optional<float> _scrollTo;

        // what the dialog asks before, while it is shown
        std::optional<Leave> _leaving;

        // the values of a selected mod, by the source of its menu: not the panel's own, and none in a test that
        // has no values
        model::Store* _mcmStore = nullptr;
        model::Store* _configSpecStore = nullptr;

        ModTree _modTree;
        ModPage _modPage;
        HelpLine _helpLine;

        // the selected mod's editing session, as its store is told of it, with the row the ray is on
        StoreSession _session;

        // the search of the mods for the search field's query, and whether the page has only its rows that match
        SearchSession _search;

        // the ray is on the search field or its Clear, as the tree was drawn last: Back there empties the query
        bool _onSearchField = false;

        // the Developer tools as the settings have them, the front page's section of them, and what writes a change
        DeveloperView _developerView;
        DeveloperSection _developer;
        std::function<void(const DeveloperView&)> _writeDeveloperView;

        // what does what a press on a row of a report's live copy asks, which the menu does
        std::function<void(const std::string&, LiveCopyAction)> _liveCopyAction;

        // the design system's sizes and colors are set on ImGui's style once
        bool _styleApplied = false;

        // another page is shown since the last draw, which starts at its top
        bool _pageChanged = false;

        // a press did something in the last draw
        bool _pressActed = false;

        // the pattern a press on a haptic played in the frame that was drawn last
        std::optional<vrcf::HapticPattern> _hapticPlayed;

        // how far the thumbstick of the hand that points is pushed to a side, as the menu tells it
        float _thumbstickSideValue = 0.0f;
    };
}
