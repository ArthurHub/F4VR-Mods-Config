#pragma once

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
#include "ModPage.h"
#include "ModTree.h"
#include "Selection.h"
#include "TitleBar.h"
#include "UnsavedDialog.h"
#include "model/ConfigDefinition.h"
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
         * What the title bar's Minimize and Close do, which the menu does: the panel does not know the game.
         */
        struct MenuActions
        {
            std::function<void()> minimize;
            std::function<void()> close;
        };

        void setMods(std::vector<model::Mod> mods);
        void setQuickLaunch(std::vector<QuickLaunchEntry> entries);
        void setControls(std::vector<ControlHelp> controls);
        void setLeftHanded(bool leftHanded);
        void setMenuActions(MenuActions actions);
        void setDeveloper(DeveloperView view, std::function<void(const DeveloperView&)> write);
        void setLiveCopyAction(std::function<void(const std::string& folder, LiveCopyAction action)> act);
        std::size_t replaceMod(model::Mod mod, std::string_view time);
        void liveCopyNotRead(std::string_view folder, std::string_view liveCopy, std::string_view time, std::string_view reason);
        void setLiveCopyStale(std::string_view folder, bool stale);
        void setImageDrawer(ImageDrawer drawer);
        void setStore(model::Source source, model::Store* store);
        void setThumbstickSideValue(float value);
        void setTreeWidth(float width);
        bool select(const Selection& selection);
        bool close();
        bool showHelp(bool shown);
        bool back();
        void reset();
        void draw();
        model::Store* selectedStore() const;
        const model::Page* selectedPage() const;

        const std::vector<model::Mod>& mods() const
        {
            return _mods;
        }

        const Selection& selection() const
        {
            return _selection;
        }

        const HelpLine& helpLine() const
        {
            return _helpLine;
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
        bool answerDialog(DialogAnswer answer);

        // the last read of the live copy of the config spec of a folder, and none while it was not read
        const LiveCopyRead* liveCopyRead(const std::string_view folder) const
        {
            const auto read = _liveCopyReads.find(folder);
            return read != _liveCopyReads.end() ? &read->second : nullptr;
        }

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

        model::Store* storeOf(const model::Mod& mod) const;
        Selection existing(const Selection& selection) const;
        Selection available(const Selection& selection) const;
        bool hasUnsavedChanges() const;
        bool show(const Selection& shown);
        bool leave();
        bool leaveReport(bool toDeveloper);
        void updateStatus();
        void pageShown();
        bool pressButton(TitleButton button);
        std::vector<FrontPageMod> frontPageMods() const;
        void drawFooter(const ImVec2& pos);
        void drawDialog();

        // the mods the menu lists, in order of their display names
        std::vector<model::Mod> _mods;

        // the controls of the menu that the Help page tells, and what Minimize and Close do
        std::vector<ControlHelp> _controls;
        MenuActions _menuActions;

        // the mod and the page that are shown: none is the menu's front page
        Selection _selection;

        // the Help page is shown in place of the selection's page, which Back shows again
        bool _helpShown = false;

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

        // the Developer tools as the settings have them, the front page's section of them, and what writes a change
        DeveloperView _developerView;
        DeveloperSection _developer;
        std::function<void(const DeveloperView&)> _writeDeveloperView;

        // what does what a press on a row of a report's live copy asks, which the menu does, and the last read of each
        // config spec's live copy, by its folder
        std::function<void(const std::string&, LiveCopyAction)> _liveCopyAction;
        std::map<std::string, LiveCopyRead, std::less<>> _liveCopyReads;

        // the design system's sizes and colors are set on ImGui's style once
        bool _styleApplied = false;

        // another page is shown since the last draw, which starts at its top
        bool _pageChanged = false;

        // a press did something in the last draw
        bool _pressActed = false;

        // how far the thumbstick of the hand that points is pushed to a side, as the menu tells it
        float _thumbstickSideValue = 0.0f;
    };
}
