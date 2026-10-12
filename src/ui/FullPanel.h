#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "ModPage.h"
#include "ModTree.h"
#include "Selection.h"
#include "model/Definition.h"

namespace mods_config::ui
{
    /**
     * The content of the Full panel, drawn with ImGui as the design system has it: the title bar with its
     * breadcrumbs, the tree of the mods, and beside it the menu's front page or a page of the selected mod.
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

    private:
        std::optional<Selection> drawTitleBar() const;
        void drawFrontPage() const;

        // the mods the menu lists, in order of their display names
        std::vector<model::Mod> _mods;
        std::vector<QuickLaunchEntry> _quickLaunch;

        // the mod and the page that are shown: none is the menu's front page
        Selection _selection;

        ModTree _modTree;
        ModPage _modPage;

        // the design system's sizes and colors are set on ImGui's style once
        bool _styleApplied = false;

        // another page is shown since the last draw, which starts at its top
        bool _pageChanged = false;
    };
}
