#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "HelpLine.h"
#include "ModPage.h"
#include "ModTree.h"
#include "Selection.h"
#include "model/Definition.h"

namespace mods_config::ui
{
    /**
     * The content of the Full panel, drawn with ImGui as the design system has it: the title bar with its
     * breadcrumbs, the tree of the mods, beside it the menu's front page or a page of the selected mod, and
     * the footer with the help line.
     * It knows nothing of the game, so a test draws it too.
     */
    class FullPanel
    {
    public:
        /**
         * An entry of the front page's quick launch section, which opens a mod's own config UI.
         */
        struct QuickLaunchEntry
        {
            std::string name;

            // what the help line says of the entry: what the mod's own UI is for
            std::string help;
            std::function<void()> open;
        };

        void setMods(std::vector<model::Mod> mods);
        void setQuickLaunch(std::vector<QuickLaunchEntry> entries);
        void select(const Selection& selection);
        void reset();
        void draw();

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

    private:
        std::optional<Selection> drawTitleBar() const;
        std::optional<PointedRow> drawFrontPage() const;
        void drawFooter(const ImVec2& pos) const;

        // the mods the menu lists, in order of their display names
        std::vector<model::Mod> _mods;
        std::vector<QuickLaunchEntry> _quickLaunch;

        // the mod and the page that are shown: none is the menu's front page
        Selection _selection;

        ModTree _modTree;
        ModPage _modPage;
        HelpLine _helpLine;

        // the design system's sizes and colors are set on ImGui's style once
        bool _styleApplied = false;

        // another page is shown since the last draw, which starts at its top
        bool _pageChanged = false;
    };
}
