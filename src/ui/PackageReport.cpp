#include "PackageReport.h"

#include <cmath>
#include <format>
#include <string>
#include <string_view>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"

namespace
{
    using namespace mods_config;

    // what stands between where a problem is on its page and in config.json: U+00B7, as its bytes in UTF-8
    constexpr auto WHERE_PATH_SEPARATOR = " \xC2\xB7 ";

    /**
     * A count of a thing: `1 error`, `2 errors`.
     */
    std::string countOf(const std::size_t count, const std::string_view thing)
    {
        return std::format("{} {}{}", count, thing, count == 1 ? "" : "s");
    }

    /**
     * What the help line says of a problem: its message, then where it is, on its page and in config.json, and
     * that a press shows its row, for one that has a row the menu shows.
     */
    std::string problemHelp(const model::Problem& problem, const bool goes)
    {
        std::string where = problem.where;
        if (!problem.path.empty()) {
            where += (where.empty() ? "" : WHERE_PATH_SEPARATOR) + problem.path;
        }
        auto help = problem.message;
        if (!where.empty()) {
            help += " " + where + ".";
        }
        if (goes) {
            help += " Goes to the row.";
        }
        return help;
    }

    /**
     * A heading that folds at the cursor, as wide as the page: a press folds or opens it. Returns whether the
     * ray is on it, and tells whether a press did something.
     */
    bool foldingHeading(const char* id, const std::string& text, bool& folded, bool& pressActed)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        if (ImGui::InvisibleButton(id, ImVec2(width, theme::CONTROL_HEIGHT))) {
            folded = !folded;
            pressActed = true;
        }
        const bool pointed = ImGui::IsItemHovered();
        ui::drawFoldingHeading(text, pos, width, folded, pointed || ImGui::IsItemActive(), {});
        return pointed;
    }
}

namespace mods_config::ui
{
    /**
     * Draws the report of a package's mod into the content pane, from its top: a text of where its config.json
     * is read from, and `No problems` for a package with none. Then its errors and its warnings, each under its
     * heading, which says how many there are and is left out with none. A problem is a row set in a level
     * under its heading, of the name of its row and its message, and an arrow where the room of a Reset is for one on a row the menu
     * shows: a row of a mod that is available. A press on such a row tells its problem.
     * Only the problem of a package that cannot be read has its help line in red after the invalid mark.
     */
    ReportDrawn PackageReport::draw(const model::Mod& mod)
    {
        ReportDrawn drawn;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_1);
        const auto file = mod.folder.empty() ? std::string("its config.json") : std::format(R"(Data\ModsConfigVR\Config\{}\config.json)", mod.folder);
        std::size_t errors = 0;
        for (const auto& problem : mod.problems) {
            errors += problem.severity == model::Severity::Error ? 1 : 0;
        }
        const auto warnings = mod.problems.size() - errors;
        drawPageText(std::format("Read from {}.{}", file, mod.problems.empty() ? "\nNo problems." : ""));

        const bool available = mod.notAvailable.empty();
        const bool notLoaded = !available && errors > 0;
        for (const auto severity : { model::Severity::Error, model::Severity::Warning }) {
            const bool isError = severity == model::Severity::Error;
            const auto count = isError ? errors : warnings;
            if (count == 0) {
                continue;
            }
            auto& folded = isError ? _errorsFolded : _warningsFolded;
            const auto heading = countOf(count, isError ? "error" : "warning");
            if (foldingHeading(isError ? "errors" : "warnings", heading, folded, drawn.pressActed)) {
                drawn.pointed = PointedRow{ .text = isError ? "Errors" : "Warnings",
                    .help = isError ? "What the menu left out of the package: all of it, or a part." : "What the menu read that is likely not what is meant.",
                    .warning = {},
                    .invalid = false };
            }
            if (folded) {
                continue;
            }
            ImGui::Indent(theme::SPACE_3);
            for (std::size_t place = 0; place < mod.problems.size(); ++place) {
                const auto& problem = mod.problems[place];
                if (problem.severity != severity) {
                    continue;
                }
                ImGui::PushID(static_cast<int>(place));
                const auto pos = ImGui::GetCursorScreenPos();
                const float width = ImGui::GetContentRegionAvail().x;
                const bool pressed = ImGui::InvisibleButton("problem", ImVec2(width, theme::CONTROL_HEIGHT));
                const bool pointed = ImGui::IsItemHovered();
                drawRowState();
                const bool goes = available && problem.place.has_value();
                const float right = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;
                if (goes) {
                    const float slotLeft = right + theme::SPACE_2;
                    drawFoldArrow(ImVec2(std::floor(slotLeft + (theme::CONTROL_HEIGHT - FOLD_ARROW_SIZE) * 0.5f), pos.y + (theme::CONTROL_HEIGHT - FOLD_ARROW_SIZE) * 0.5f),
                        false,
                        theme::color::GREEN);
                    if (pressed) {
                        drawn.pressed = place;
                        drawn.pressActed = true;
                    }
                }
                // the message says "it" of its row, so the row's name is before it
                drawText(pos.x + theme::SPACE_3, right, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, problem.row + ": " + problem.message);
                if (pointed) {
                    drawn.pointed =
                        PointedRow{ .text = problem.row, .help = problemHelp(problem, goes), .warning = {}, .invalid = notLoaded && problem.severity == model::Severity::Error };
                }
                ImGui::PopID();
            }
            ImGui::Unindent(theme::SPACE_3);
        }
        return drawn;
    }

    /**
     * Opens both headings again, as the menu is closed.
     */
    void PackageReport::reset()
    {
        _errorsFolded = false;
        _warningsFolded = false;
    }
}
