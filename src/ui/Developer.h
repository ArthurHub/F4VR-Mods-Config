#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Choices.h"
#include "HelpLine.h"
#include "model/Definition.h"

namespace mods_config::ui
{
    /**
     * Which packages the Developer section of the menu's front page lists: those that need attention, all of
     * them, or the one Mods Config VR's settings name.
     */
    enum class ShowPackages : std::uint8_t
    {
        NeedingAttention,
        All,
        OnlyOne,
    };

    /**
     * The Developer tools as Mods Config VR's settings have them: whether they are on, which packages the front
     * page's section lists, the package it lists alone, by its modName, and whether the section is folded.
     */
    struct DeveloperView
    {
        bool enabled = false;
        ShowPackages show = ShowPackages::NeedingAttention;
        std::string package;
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
    };

    /**
     * The Developer section of the menu's front page, for a package's author, while the Developer tools are on:
     * a heading that folds, its filter, and a row for each package the filter lists, with its errors and its
     * warnings. An MCM mod is never listed.
     */
    class DeveloperSection
    {
    public:
        DeveloperSection();
        DeveloperDrawn draw(const std::vector<model::Mod>& mods, DeveloperView& view, float thumbstickSideValue);

    private:
        // the filter's stepper, which a stepper of the page draws
        Choices _filter;
        model::Setting _filterSetting;
    };
}
