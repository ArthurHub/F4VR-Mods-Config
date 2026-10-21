#include "Developer.h"

#include <array>
#include <cmath>
#include <format>
#include <string_view>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"
#include "common/CommonUtils.h"

namespace
{
    using namespace mods_config;

    // the texts of the section's heading and of its filter, and what the help line says of them
    constexpr auto HEADING = "Developer";
    constexpr auto HEADING_HELP = "The problems of each package, for its author: what the menu left out of it, and what it read that is likely not meant.";
    constexpr auto FILTER = "Show";
    constexpr auto FILTER_HELP =
        "Which packages this section lists: those that need attention, with an error or a warning or not loaded, all of them, or only the Package of Mods Config VR's "
        "settings.";

    // the options of the filter, as Mods Config VR's settings have them: the third is the name of its Package
    constexpr std::array FILTER_OPTIONS{ "Needing attention", "All" };

    /**
     * What a package's row tells: how many errors and warnings it has, and whether it is not loaded, as one that
     * cannot be read is, or not available, as one whose required plugins are not loaded is.
     */
    struct PackageState
    {
        std::size_t errors = 0;
        std::size_t warnings = 0;
        bool notLoaded = false;
        bool notAvailable = false;
    };

    PackageState stateOf(const model::Mod& mod)
    {
        PackageState state;
        for (const auto& problem : mod.problems) {
            (problem.severity == model::Severity::Error ? state.errors : state.warnings) += 1;
        }
        state.notLoaded = !mod.notAvailable.empty() && state.errors > 0;
        state.notAvailable = !mod.notAvailable.empty() && !state.notLoaded;
        return state;
    }

    /**
     * A count of a thing: `1 error`, `2 errors`.
     */
    std::string countOf(const std::size_t count, const std::string_view thing)
    {
        return std::format("{} {}{}", count, thing, count == 1 ? "" : "s");
    }

    /**
     * The places of the packages the filter lists, among the mods, in their order, and how many packages there
     * are. A package needs attention while it has an error or a warning, or is not loaded.
     */
    std::vector<std::size_t> listedPackages(const std::vector<model::Mod>& mods, const ui::DeveloperView& view, std::size_t& packages)
    {
        std::vector<std::size_t> listed;
        packages = 0;
        const auto package = common::str_tolower(view.package);
        for (std::size_t place = 0; place < mods.size(); ++place) {
            const auto& mod = mods[place];
            if (mod.source != model::Source::Package) {
                continue;
            }
            ++packages;
            const auto state = stateOf(mod);
            const bool shown = view.show == ui::ShowPackages::All ||
                               (view.show == ui::ShowPackages::NeedingAttention && (state.errors > 0 || state.warnings > 0 || state.notLoaded)) ||
                               (view.show == ui::ShowPackages::OnlyOne && common::str_tolower(mod.name) == package);
            if (shown) {
                listed.push_back(place);
            }
        }
        return listed;
    }

    /**
     * The mod the settings name as their Package. None for a name of no package.
     */
    const model::Mod* namedPackage(const std::vector<model::Mod>& mods, const std::string& name)
    {
        const auto lower = common::str_tolower(name);
        for (const auto& mod : mods) {
            if (mod.source == model::Source::Package && !name.empty() && common::str_tolower(mod.name) == lower) {
                return &mod;
            }
        }
        return nullptr;
    }

    /**
     * The counts of errors and warnings, each with its mark, of those there are.
     */
    std::vector<ui::MarkedCount> problemCounts(const std::size_t errors, const std::size_t warnings)
    {
        std::vector<ui::MarkedCount> counts;
        if (errors > 0) {
            counts.push_back({ .mark = ui::CountMark::Invalid, .text = countOf(errors, "error"), .color = theme::color::RED });
        }
        if (warnings > 0) {
            counts.push_back({ .mark = ui::CountMark::Warning, .text = countOf(warnings, "warning"), .color = theme::color::AMBER });
        }
        return counts;
    }

    /**
     * What the help line says of a package's row: why it is not loaded, as an error is told, why it is not
     * available, or its errors and its warnings.
     */
    ui::PointedRow packageHelp(const model::Mod& mod, const PackageState& state)
    {
        if (state.notLoaded) {
            const auto error = std::ranges::find(mod.problems, model::Severity::Error, &model::Problem::severity);
            return { .text = mod.displayName, .help = error->message + " Opens its report.", .warning = {}, .invalid = true };
        }
        if (state.notAvailable) {
            return { .text = mod.displayName, .help = std::format("Not available: {}. Opens its report.", mod.notAvailable), .warning = {}, .invalid = false };
        }
        std::string help;
        if (state.errors > 0) {
            help = countOf(state.errors, "error") + ": parts of it are left out.";
        }
        if (state.warnings > 0) {
            help += (help.empty() ? "" : " ") + countOf(state.warnings, "warning") + ": read, and likely not what is meant.";
        }
        return { .text = mod.displayName, .help = (help.empty() ? "No problems." : help) + " Opens its report.", .warning = {}, .invalid = false };
    }
}

namespace mods_config::ui
{
    DeveloperSection::DeveloperSection()
    {
        _filterSetting.type = model::SettingType::Stepper;
    }

    /**
     * Draws the section at the cursor, as the design system's Developer has it.
     * Its heading folds, and folded it says the errors and the warnings of the packages it lists. A press on it
     * folds or opens it, which changes the view.
     * Its first row is the filter: `Show`, with how many of the packages it lists, and a stepper of Needing
     * attention, All and the name of the settings' Package, for Only one, which a press or the thumbstick
     * steps, as any stepper. A Package that names no package has no option.
     * Then a row for each package the filter lists, in the order of the mods: its name, and its errors and its
     * warnings, `No problems`, `Not loaded` in red after the invalid mark for one that cannot be read, or `Not
     * available`, and an arrow in the room of a Reset: a press on it tells its package, whose report it opens.
     * Scrolled to, the page is scrolled to put the section's heading at its top.
     */
    DeveloperDrawn DeveloperSection::draw(const std::vector<model::Mod>& mods, DeveloperView& view, const float thumbstickSideValue, const bool scrollTo)
    {
        DeveloperDrawn drawn;
        _filter.startFrame(thumbstickSideValue);

        const auto headingPos = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        if (ImGui::InvisibleButton("developer", ImVec2(width, theme::CONTROL_HEIGHT))) {
            view.folded = !view.folded;
            drawn.changed = true;
            drawn.pressActed = true;
        }
        const bool onHeading = ImGui::IsItemHovered();
        if (scrollTo) {
            ImGui::SetScrollHereY(0.0f);
        }
        std::size_t packages = 0;
        auto listed = listedPackages(mods, view, packages);
        std::size_t errors = 0;
        std::size_t warnings = 0;
        for (const auto place : listed) {
            const auto state = stateOf(mods[place]);
            errors += state.errors;
            warnings += state.warnings;
        }
        const auto hidden = view.folded ? problemCounts(errors, warnings) : std::vector<MarkedCount>{};
        drawFoldingHeading(HEADING, headingPos, width, view.folded, onHeading || ImGui::IsItemActive(), hidden);
        if (onHeading) {
            auto help = std::string(HEADING_HELP);
            if (!hidden.empty()) {
                help += " Folded: " + hidden.front().text + (hidden.size() > 1 ? " and " + hidden.back().text : "") + ".";
            }
            drawn.pointed = PointedRow{ .text = HEADING, .help = std::move(help), .warning = {}, .invalid = false };
        }
        if (view.folded) {
            _filter.endFrame();
            return drawn;
        }

        // the filter
        const float textLeft = std::floor(headingPos.x + theme::SPACE_6 + theme::SPACE_3);
        const float right = headingPos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;
        std::vector<model::Option> options;
        for (const auto* text : FILTER_OPTIONS) {
            options.push_back({ .value = static_cast<int>(options.size()), .text = text });
        }
        if (const auto* package = namedPackage(mods, view.package)) {
            options.push_back({ .value = static_cast<int>(options.size()), .text = package->displayName });
        }
        const auto shown = static_cast<std::size_t>(view.show);
        auto chosen = shown < options.size() ? std::optional(shown) : std::nullopt;

        const auto filterPos = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton("filter", ImVec2(width, theme::CONTROL_HEIGHT));
        const ChoiceRow row{ .pointed = ImGui::IsItemHovered(), .held = ImGui::IsItemActive(), .pressed = pressed };
        drawRowState();
        bool pulse = false;
        if (const auto picked = _filter.edit(_filterSetting, options, right, filterPos.y, row, chosen, pulse); picked && picked != chosen) {
            chosen = picked;
            view.show = static_cast<ShowPackages>(*picked);
            listed = listedPackages(mods, view, packages);
            drawn.changed = true;
            pulse = true;
        }
        drawn.pressActed = drawn.pressActed || pulse;
        const float stepperLeft = _filter.draw(_filterSetting, options, right, filterPos.y, row, chosen, ValueLook::Saved);
        drawText(textLeft, stepperLeft - theme::SPACE_3, filterPos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, FILTER);
        drawTag(textLeft + textWidth(FILTER) + theme::SPACE_2, filterPos.y + theme::CONTROL_HEIGHT * 0.5f, std::format("{} of {}", listed.size(), packages));
        if (row.pointed) {
            drawn.pointed = PointedRow{ .text = FILTER, .help = FILTER_HELP, .warning = {}, .invalid = false };
        }
        _filter.endFrame();

        // a row for each package
        for (const auto place : listed) {
            const auto& mod = mods[place];
            const auto state = stateOf(mod);
            ImGui::PushID(static_cast<int>(place));
            const auto pos = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("package", ImVec2(width, theme::CONTROL_HEIGHT))) {
                drawn.opened = place;
                drawn.pressActed = true;
            }
            const bool onRow = ImGui::IsItemHovered();
            drawRowState();

            const float slotLeft = right + theme::SPACE_2;
            drawFoldArrow(ImVec2(std::floor(slotLeft + (theme::CONTROL_HEIGHT - FOLD_ARROW_SIZE) * 0.5f), pos.y + (theme::CONTROL_HEIGHT - FOLD_ARROW_SIZE) * 0.5f),
                false,
                theme::color::GREEN);
            float left = right;
            if (state.notLoaded) {
                const std::array counts{ MarkedCount{ .mark = CountMark::Invalid, .text = "Not loaded", .color = theme::color::RED } };
                left = drawMarkedCounts(right, pos.y, counts);
            } else if (state.errors > 0 || state.warnings > 0) {
                left = drawMarkedCounts(right, pos.y, problemCounts(state.errors, state.warnings));
            } else {
                const auto* text = state.notAvailable ? "Not available" : "No problems";
                left = std::floor(right - textWidth(text));
                drawText(left, right, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED, text);
            }
            drawText(textLeft, left - theme::SPACE_3, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, mod.displayName);
            if (onRow) {
                drawn.pointed = packageHelp(mod, state);
            }
            ImGui::PopID();
        }
        return drawn;
    }
}
