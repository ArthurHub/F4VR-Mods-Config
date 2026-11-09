#include "Developer.h"

#include <array>
#include <cmath>
#include <format>
#include <string_view>

#include <imgui.h>

#include "Drawing.h"
#include "PageRows.h"
#include "Theme.h"
#include "common/CommonUtils.h"
#include "perf/Perf.h"

namespace
{
    using namespace mods_config;

    // the texts of the section's heading and of its filter, and what the help line says of them
    constexpr auto HEADING = "Developer";
    constexpr auto HEADING_HELP = "The problems of each config spec, for its author: what the menu left out of it, and what it read that is likely not meant.";
    constexpr auto FILTER = "Show";
    constexpr auto FILTER_HELP =
        "Which config specs this section lists: those that need attention, with an error or a warning or not loaded, all of them, or only the Config spec of Mods Config FO4 VR's "
        "settings.";

    // the tag of a config spec's row while it is read from its live copy
    constexpr auto LIVE_COPY_TAG = "Live copy";

    // the options of the filter, as Mods Config FO4 VR's settings have them: the third is the name of its Config spec
    constexpr std::array FILTER_OPTIONS{ "Needing attention", "All" };

    /**
     * What a config spec's row tells: how many errors and warnings it has, and whether it is not loaded, as one that
     * cannot be read is, or not available, as one whose required plugins are not loaded is.
     */
    struct ConfigSpecState
    {
        std::size_t errors = 0;
        std::size_t warnings = 0;
        bool notLoaded = false;
        bool notAvailable = false;
    };

    ConfigSpecState stateOf(const model::Mod& mod)
    {
        ConfigSpecState state;
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
     * The places of the config specs the filter lists, among the mods, in their order, and how many config specs there
     * are. A config spec needs attention while it has an error or a warning, is not loaded, or is read from its live
     * copy.
     */
    std::vector<std::size_t> listedConfigSpecs(const std::vector<model::Mod>& mods, const ui::DeveloperView& view, std::size_t& configSpecs)
    {
        std::vector<std::size_t> listed;
        configSpecs = 0;
        const auto configSpec = common::str_tolower(view.configSpec);
        for (std::size_t place = 0; place < mods.size(); ++place) {
            const auto& mod = mods[place];
            if (mod.source != model::Source::ConfigSpec) {
                continue;
            }
            ++configSpecs;
            const auto state = stateOf(mod);
            const bool shown = view.show == ui::ShowConfigSpecs::All ||
                               (view.show == ui::ShowConfigSpecs::NeedingAttention && (state.errors > 0 || state.warnings > 0 || state.notLoaded || !mod.liveCopy.empty())) ||
                               (view.show == ui::ShowConfigSpecs::OnlyOne && common::str_tolower(mod.name) == configSpec);
            if (shown) {
                listed.push_back(place);
            }
        }
        return listed;
    }

    /**
     * The mod the settings name as their Config spec. None for a name of no config spec.
     */
    const model::Mod* namedConfigSpec(const std::vector<model::Mod>& mods, const std::string& name)
    {
        const auto lower = common::str_tolower(name);
        for (const auto& mod : mods) {
            if (mod.source == model::Source::ConfigSpec && !name.empty() && common::str_tolower(mod.name) == lower) {
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
     * What the help line says of a config spec's row: that it is read from its live copy, then why it is not loaded,
     * as an error is told, why it is not available, or its errors and its warnings.
     */
    ui::PointedRow configSpecHelp(const model::Mod& mod, const ConfigSpecState& state)
    {
        const std::string live = mod.liveCopy.empty() ? "" : "Read from its live copy. ";
        if (state.notLoaded) {
            const auto error = std::ranges::find(mod.problems, model::Severity::Error, &model::Problem::severity);
            return { .text = mod.displayName, .help = live + error->message + " Opens its report.", .warning = {}, .invalid = true };
        }
        if (state.notAvailable) {
            return { .text = mod.displayName, .help = std::format("{}Not available: {}. Opens its report.", live, mod.notAvailable), .warning = {}, .invalid = false };
        }
        std::string help;
        if (state.errors > 0) {
            help = countOf(state.errors, "error") + ": parts of it are left out.";
        }
        if (state.warnings > 0) {
            help += (help.empty() ? "" : " ") + countOf(state.warnings, "warning") + ": read, and likely not what is meant.";
        }
        return { .text = mod.displayName, .help = live + (help.empty() ? "No problems." : help) + " Opens its report.", .warning = {}, .invalid = false };
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
     * Its heading folds, and folded it says the errors and the warnings of the config specs it lists. A press on it
     * folds or opens it, which changes the view.
     * Its first row is the filter: `Show`, with how many of the config specs it lists, and a stepper of Needing
     * attention, All and the name of the settings' Config spec, for Only one, which a press or the thumbstick
     * steps, as any stepper. A Config spec that names no config spec has no option.
     * Then a row for each config spec the filter lists, in the order of the mods: its name, the `Live copy` tag while
     * it is read from its live copy, and its errors and its warnings, `No problems`, `Not loaded` in red after the invalid mark for one that cannot be read, or `Not
     * available`, and an arrow in the room of a Reset: a press on it tells its config spec, whose report it opens.
     * Scrolled to, the page is scrolled to put the section's heading at its top.
     */
    DeveloperDrawn DeveloperSection::draw(const std::vector<model::Mod>& mods, DeveloperView& view, const float thumbstickSideValue, const bool scrollTo)
    {
        F4CF_PERF_FUNCTION();
        DeveloperDrawn drawn;
        _filter.startFrame(thumbstickSideValue);

        const auto headingPos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const auto headingRow = addFoldingRow("developer", view.folded);
        drawn.changed = headingRow.pressed;
        drawn.pressActed = headingRow.pressed;
        const bool onHeading = headingRow.pointed;
        if (scrollTo) {
            ImGui::SetScrollHereY(0.0f);
        }
        std::size_t configSpecs = 0;
        auto listed = listedConfigSpecs(mods, view, configSpecs);
        std::size_t errors = 0;
        std::size_t warnings = 0;
        for (const auto place : listed) {
            const auto state = stateOf(mods[place]);
            errors += state.errors;
            warnings += state.warnings;
        }
        const auto hidden = view.folded ? problemCounts(errors, warnings) : std::vector<MarkedCount>{};
        drawFoldingHeading(HEADING, headingPos, width, view.folded, hidden);
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

        // the rows are set in a level under the heading, and their names are a level inside their frames
        ImGui::Indent(theme::SPACE_3);
        const float rowWidth = width - theme::SPACE_3;
        const float textLeft = std::floor(headingPos.x + theme::SPACE_3 + theme::SPACE_3);

        // the filter
        const float right = headingPos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;
        std::vector<model::Option> options;
        for (const auto* text : FILTER_OPTIONS) {
            options.push_back({ .value = static_cast<int>(options.size()), .text = text });
        }
        if (const auto* configSpec = namedConfigSpec(mods, view.configSpec)) {
            options.push_back({ .value = static_cast<int>(options.size()), .text = configSpec->displayName });
        }
        const auto shown = static_cast<std::size_t>(view.show);
        auto chosen = shown < options.size() ? std::optional(shown) : std::nullopt;

        const auto filterPos = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton("filter", ImVec2(rowWidth, theme::CONTROL_HEIGHT));
        const ChoiceRow row{ .pointed = ImGui::IsItemHovered(), .held = ImGui::IsItemActive(), .pressed = pressed };
        drawRowState();
        bool pulse = false;
        if (const auto picked = _filter.edit(_filterSetting, options, right, filterPos.y, row, chosen, pulse); picked && picked != chosen) {
            chosen = picked;
            view.show = static_cast<ShowConfigSpecs>(*picked);
            listed = listedConfigSpecs(mods, view, configSpecs);
            drawn.changed = true;
            pulse = true;
        }
        drawn.pressActed = drawn.pressActed || pulse;
        const float stepperLeft = _filter.draw(_filterSetting, options, right, filterPos.y, row, chosen, ValueLook::Saved);
        drawText(textLeft, stepperLeft - theme::SPACE_3, filterPos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, FILTER);
        drawTag(textLeft + textWidth(FILTER) + theme::SPACE_2, filterPos.y + theme::CONTROL_HEIGHT * 0.5f, std::format("{} of {}", listed.size(), configSpecs));
        if (row.pointed) {
            drawn.pointed = PointedRow{ .text = FILTER, .help = FILTER_HELP, .warning = {}, .invalid = false };
        }
        _filter.endFrame();

        // a row for each config spec
        for (const auto place : listed) {
            const auto& mod = mods[place];
            const auto state = stateOf(mod);
            ImGui::PushID(static_cast<int>(place));
            const auto pos = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("configSpec", ImVec2(rowWidth, theme::CONTROL_HEIGHT))) {
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
            if (!mod.liveCopy.empty()) {
                left -= tagWidth(LIVE_COPY_TAG) + theme::SPACE_3;
                drawTag(left, pos.y + theme::CONTROL_HEIGHT * 0.5f, LIVE_COPY_TAG);
            }
            drawText(textLeft, left - theme::SPACE_3, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, mod.displayName);
            if (onRow) {
                drawn.pointed = configSpecHelp(mod, state);
            }
            ImGui::PopID();
        }
        ImGui::Unindent(theme::SPACE_3);
        return drawn;
    }
}
