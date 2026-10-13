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
#include "model/Store.h"

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
        void setImageDrawer(ImageDrawer drawer);
        void setStore(model::Store* store);
        void setThumbstickSideValue(float value);
        bool select(const Selection& selection);
        bool back();
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

        /**
         * Whether a press did something in the frame that was drawn last: it showed another page, opened a
         * quick launch entry, changed a value, took hold of a slider, or opened the list of a dropdown. It is
         * the frame the hand that points gets its pulse in. So is a step of a slider or a stepper by the thumbstick. A press that changes nothing, as one
         * on the row that is selected, is not told, and neither are the steps of a slider that is dragged.
         */
        bool pressActed() const
        {
            return _pressActed;
        }

    private:
        std::optional<Selection> drawTitleBar() const;
        std::optional<PointedRow> drawFrontPage();
        void drawFooter(const ImVec2& pos) const;

        // the mods the menu lists, in order of their display names
        std::vector<model::Mod> _mods;
        std::vector<QuickLaunchEntry> _quickLaunch;

        // the mod and the page that are shown: none is the menu's front page
        Selection _selection;

        // the values of the selected mod: not the panel's own, and none in a test that has no values
        model::Store* _store = nullptr;

        ModTree _modTree;
        ModPage _modPage;
        HelpLine _helpLine;

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
