#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Choices.h"
#include "HelpLine.h"
#include "model/ConfigDefinition.h"

namespace mods_config::ui
{
    /**
     * Which config specs the Developer section of the menu's front page lists: those that need attention, all of
     * them, or the one Mods Config FO4 VR's settings name.
     */
    enum class ShowConfigSpecs : std::uint8_t
    {
        NeedingAttention,
        All,
        OnlyOne,
    };

    /**
     * The Developer tools as Mods Config FO4 VR's settings have them: whether they are on, which config specs the front
     * page's section lists, the config spec it lists alone, by its modName, and whether the section is folded.
     */
    struct DeveloperView
    {
        bool enabled = false;
        ShowConfigSpecs show = ShowConfigSpecs::NeedingAttention;
        std::string configSpec;
        bool folded = false;

        bool operator==(const DeveloperView&) const = default;
    };

    /**
     * What a frame of the Developer section did: the row the ray is on, for the help line, whether a press
     * changed its filter or folded it, which the menu writes to its settings at once, and whether a press or
     * the thumbstick did something, for the pulse of the hand that points.
     */
    struct DeveloperDrawn
    {
        std::optional<PointedRow> pointed;
        bool changed = false;
        bool pressActed = false;

        // the config spec whose row was pressed, which opens its report, by its place among the mods
        std::optional<std::size_t> opened;
    };

    /**
     * The Developer section of the menu's front page, for a config spec's author, while the Developer tools are on:
     * a heading that folds, its filter, and a row for each config spec the filter lists, with its errors and its
     * warnings, which a press opens the report of. An MCM mod is never listed.
     */
    class DeveloperSection
    {
    public:
        DeveloperSection();
        DeveloperDrawn draw(const std::vector<model::Mod>& mods, DeveloperView& view, float thumbstickSideValue, bool scrollTo = false);

    private:
        // the filter's stepper, which a stepper of the page draws
        Choices _filter;
        model::Setting _filterSetting;
    };
}
