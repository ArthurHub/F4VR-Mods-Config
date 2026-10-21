#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "HelpLine.h"
#include "model/ConfigDefinition.h"

namespace mods_config::ui
{
    /**
     * What a press on a row of a report's Live copy asks of its config spec, which the menu does: read it from its
     * live copy, or from its installed file again, read the copy again now, copy the installed file over the copy
     * and read it, or open the copy or its folder on the desktop.
     */
    enum class LiveCopyAction : std::uint8_t
    {
        None,
        TurnOn,
        TurnOff,
        Reload,
        Overwrite,
        Open,
        Show,
    };

    /**
     * The last read of a config spec's live copy, by Reload, by a save of the copy or by its switch, which is the
     * result of Reload: what it did, as `Reloaded at 10:42:05.`, and whether the copy could not be read. Reload has
     * the info mark of a read until the ray leaves its row after being on it. And whether the installed spec.json
     * was newer than the copy as it was read, which Overwrite warns of.
     */
    struct LiveCopyRead
    {
        std::string result;
        bool failed = false;
        bool seen = false;
        bool stale = false;

        // the ray is on Reload's row with the result there, which is seen as the ray leaves it
        bool seeing = false;
    };

    /**
     * What a frame of a config spec's report did: the row the ray is on, for the help line, the problem whose row
     * a press asks to show, by its place among the config spec's problems, what a press on a row of its live copy
     * asks, and whether a press did something.
     */
    struct ReportDrawn
    {
        std::optional<PointedRow> pointed;
        std::optional<std::size_t> pressed;
        LiveCopyAction liveCopy = LiveCopyAction::None;
        bool pressActed = false;
    };

    /**
     * A config spec's report, for its author, as the design system's FullPanelConfigSpecReport has it: where its
     * spec.json is read from, its live copy, and its problems, its errors and then its warnings, each under a
     * heading that folds. A problem on a row that the menu shows has an arrow, and a press on it asks to show the row.
     * The headings, and the sub-section of the live copy, which starts folded, stay as the author left them until the
     * menu is closed.
     */
    class ConfigSpecReport
    {
    public:
        ReportDrawn draw(const model::Mod& mod, LiveCopyRead* read);
        void reset();

    private:
        // whether the headings of the errors and of the warnings are folded, and the sub-section of the live copy,
        // which starts folded
        bool _errorsFolded = false;
        bool _warningsFolded = false;
        bool _liveSpecFolded = true;
    };
}
