#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ModTree.h"
#include "model/Definition.h"

namespace mods_config::ui
{
    /**
     * The content of the Full panel, drawn with ImGui as the design system has it: the title bar, the tree of
     * the mods, and beside it the menu's front page. It knows nothing of the game, so a test draws it too.
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
        void draw();

        const std::vector<model::Mod>& mods() const
        {
            return _mods;
        }

    private:
        void drawTitleBar() const;
        void drawFrontPage() const;

        // the mods the menu lists, in order of their display names
        std::vector<model::Mod> _mods;
        std::vector<QuickLaunchEntry> _quickLaunch;

        ModTree _modTree;

        // the design system's sizes and colors are set on ImGui's style once
        bool _styleApplied = false;
    };
}
