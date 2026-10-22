#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "HelpLine.h"

namespace mods_config::ui
{
    // What a press on a quick launch entry did: whether it failed, and the message of its result, which can be none.
    struct QuickLaunchResult
    {
        bool failed = false;
        std::string message;
    };

    // A quick launch entry that opens a mod's own config UI.
    struct QuickLaunchEntry
    {
        std::string name;

        // what the help line says while the ray is on the entry
        std::string help;
        std::function<QuickLaunchResult()> open;

        // the text on its button: empty for Open
        std::string buttonText;

        // why it does not open: its row is disabled, with the reason as its help. Empty for one that opens
        std::string notAvailable;
    };

    // A mod's notice data, read from the model and its store by the panel.
    struct FrontPageMod
    {
        std::string_view displayName;
        std::size_t savedRestartCount = 0;
        std::string_view notAvailable;
    };

    // What a frame of the front page did: the row the ray is on, and whether a press did something.
    struct FrontPageDrawn
    {
        std::optional<PointedRow> pointed;
        bool pressActed = false;
    };

    /**
     * The menu's front page: its introduction, the quick launch entries and the notices of the mods, each
     * under a heading that folds. A press on an entry opens it. A section stays as the player left it until
     * the menu is closed.
     */
    class FrontPage
    {
    public:
        void setQuickLaunch(std::vector<QuickLaunchEntry> entries);
        FrontPageDrawn draw(const std::vector<FrontPageMod>& mods);
        void reset();

    private:
        std::vector<QuickLaunchEntry> _quickLaunch;

        // the entry that was pressed last, by its name, and what its press did, which its help line tells
        std::string _resultOf;
        QuickLaunchResult _result;

        // whether the headings of the quick launch entries and of the notices are folded
        bool _quickLaunchFolded = false;
        bool _noticesFolded = false;
    };
}
