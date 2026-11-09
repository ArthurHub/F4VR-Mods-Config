#include "ConfigSpecReport.h"

#include <array>
#include <cmath>
#include <format>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "Drawing.h"
#include "PageRows.h"
#include "Theme.h"
#include "perf/Perf.h"

namespace
{
    using namespace mods_config;

    // what stands between where a problem is on its page and in spec.json: U+00B7, as its bytes in UTF-8
    constexpr auto WHERE_PATH_SEPARATOR = " \xC2\xB7 ";

    /**
     * A count of a thing: `1 error`, `2 errors`.
     */
    std::string countOf(const std::size_t count, const std::string_view thing)
    {
        return std::format("{} {}{}", count, thing, count == 1 ? "" : "s");
    }

    /**
     * What the help line says of a problem: its message, then where it is, on its page and in spec.json, and
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

    // the rows of the live copy, and what the help line says of them
    constexpr auto LIVE_COPY = "Live copy";
    constexpr auto READ_LIVE_COPY = "Read the live copy";
    constexpr auto READ_LIVE_COPY_HELP =
        "Reads this config spec from a copy of its spec.json in My Games, made where there is none, and again each time the copy is saved. Off reads its installed "
        "spec.json again, and keeps the copy. A read drops the mod's unsaved changes.";
    constexpr auto LIVE_SPEC = "Live spec.json";
    constexpr auto RELOAD = "Reload";
    constexpr auto RELOAD_HELP = "Reads the live copy again now.";
    constexpr auto OVERWRITE = "Re-copy the installed spec.json";
    constexpr auto OVERWRITE_HELP = "Copies the installed spec.json over the live copy, which loses the copy's edits, and reads it.";
    constexpr auto STALE = "The installed spec.json is newer than the live copy.";
    constexpr auto EDIT = "Edit on desktop";
    constexpr auto EDIT_HELP = "Opens the live copy in the desktop's editor for .json.";
    constexpr auto FOLDER = "Reveal in file explorer";
    constexpr auto FOLDER_HELP = "Opens the live copy's folder on the desktop, with the file selected.";

    // what the help line says of a row of the live copy while the switch is off
    constexpr auto NEEDS_LIVE_COPY = " Read the live copy first.";

    // what the help line says of the sub-section of the copy, and what its heading says while it is folded and the copy
    // is stale
    constexpr auto LIVE_SPEC_HELP = "Reload, overwrite, edit and reveal the live copy.";
    constexpr auto STALE_FOLDED = "Installed is newer";

    // the button of a row of the live copy, in the panel's pixels: as wide as a button's of a page
    constexpr float BUTTON_WIDTH = 160.0f;

    /**
     * A heading that folds at the cursor, as wide as the rows there, so its rule ends where theirs do: a press folds
     * or opens it. Folded, it ends with the counts of what it hides. A mark is before its text, in its color.
     * Returns whether the ray is on it, and tells whether a press did something.
     */
    bool foldingHeading(const char* id, const std::string& text, bool& folded, bool& pressActed, const std::span<const ui::MarkedCount> hidden = {},
        const ui::TextMark mark = ui::TextMark::None)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ui::rowWidth();
        const auto row = ui::addFoldingRow(id, folded);
        pressActed = pressActed || row.pressed;
        ui::drawFoldingHeading(text, pos, width, folded, folded ? hidden : std::span<const ui::MarkedCount>(), mark);
        return row.pointed;
    }

    /**
     * A row of the live copy: its text, the label of its button, and none for the switch, whether a press acts
     * on it, and the mark after its text.
     */
    struct LiveCopyRow
    {
        const char* text = nullptr;
        const char* button = nullptr;
        bool enabled = true;
        ui::TextMark mark = ui::TextMark::None;
    };

    /**
     * Draws a row of the live copy at the cursor, laid out as a setting's row: its text with its mark, and its
     * button, which is only drawn, or the switch, which shows whether the copy is read, where a control ends.
     * The whole row is pressed. One that is disabled has the look of a row under the ray, for its help.
     * Returns whether a press acts on it, and tells whether the ray is on it.
     */
    bool drawLiveCopyRow(const LiveCopyRow& row, const bool on, bool& pointed)
    {
        ImGui::PushID(row.text);
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ui::rowWidth();
        const auto added = ui::addRow(row.enabled);
        if (!row.enabled && added.shown) {
            ui::drawRowState();
        }
        pointed = added.pointed;
        const float right = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;
        ImGui::BeginDisabled(!row.enabled);
        float left = right;
        if (row.button) {
            const auto box = ui::controlBox(right - BUTTON_WIDTH, pos.y, BUTTON_WIDTH);
            ui::drawOutline(box.Min, box.Max, theme::color::FRAME_BORDER);
            ui::drawTextIn(box, row.button, theme::color::GREEN, 0.5f, theme::SPACE_3);
            left = box.Min.x;
        } else {
            left = ui::drawSwitcher(right, pos.y, on, ui::ValueLook::Saved);
        }
        ui::drawLabel(pos, left - theme::SPACE_3, row.text, {}, { .mark = row.mark });
        ImGui::EndDisabled();
        ImGui::PopID();
        return added.pressed && row.enabled;
    }

    /**
     * The section of a config spec's live copy at the cursor, with its heading: the switch that reads the copy,
     * and the sub-section of the copy, a level further in, whose heading folds, with its line while it is open. Its
     * rows are disabled while the switch is off: Reload, Overwrite, Open and Show. Reload has the result of the last
     * read of the copy: its info mark until the ray leaves the row after being on it, or the invalid mark of a copy
     * that could not be read, and the result in the help line. Overwrite has the warning mark while the installed
     * spec.json is newer than the copy, which the heading says while it is folded. Tells what a press asks.
     */
    void drawLiveCopy(const model::Mod& mod, ui::LiveCopyRead* read, bool& folded, ui::ReportDrawn& drawn)
    {
        const bool live = !mod.liveCopy.empty();
        ui::sectionHeading(LIVE_COPY, ImGui::GetCursorScreenPos().x + ui::rowWidth());
        ImGui::Indent(theme::SPACE_3);
        const auto help = [live](const char* text) {
            return std::string(text) + (live ? "" : NEEDS_LIVE_COPY);
        };

        bool pointed = false;
        if (drawLiveCopyRow({ .text = READ_LIVE_COPY }, live, pointed)) {
            drawn.liveCopy = live ? ui::LiveCopyAction::TurnOff : ui::LiveCopyAction::TurnOn;
        }
        if (pointed) {
            drawn.pointed = ui::PointedRow{ .text = READ_LIVE_COPY, .help = READ_LIVE_COPY_HELP, .warning = {}, .invalid = false };
        }

        // the sub-section of the copy, whose heading starts where the rows do, and whose rows are a level further in
        const bool stale = live && read && read->stale;
        const float headingLeft = ImGui::GetCursorScreenPos().x;
        const std::array staleCount{ ui::MarkedCount{ .mark = ui::CountMark::Warning, .text = STALE_FOLDED, .color = theme::color::AMBER } };
        if (foldingHeading("liveSpec", LIVE_SPEC, folded, drawn.pressActed, stale ? std::span<const ui::MarkedCount>(staleCount) : std::span<const ui::MarkedCount>())) {
            drawn.pointed = ui::PointedRow{ .text = LIVE_SPEC, .help = LIVE_SPEC_HELP, .warning = folded && stale ? STALE : "", .invalid = false };
        }
        if (folded) {
            drawn.pressActed = drawn.pressActed || drawn.liveCopy != ui::LiveCopyAction::None;
            ImGui::Unindent(theme::SPACE_3);
            return;
        }
        const float lineTop = ImGui::GetItemRectMax().y;
        ImGui::Indent(theme::SPACE_3);

        const bool hasResult = read && !read->result.empty();
        const bool failed = hasResult && read->failed;
        const auto mark = failed ? ui::TextMark::Invalid : hasResult && !read->seen ? ui::TextMark::Info : ui::TextMark::None;
        if (drawLiveCopyRow({ .text = RELOAD, .button = RELOAD, .enabled = live, .mark = mark }, live, pointed)) {
            drawn.liveCopy = ui::LiveCopyAction::Reload;
        }
        if (pointed) {
            drawn.pointed = ui::PointedRow{ .text = RELOAD,
                .help = failed ? read->result : help(RELOAD_HELP),
                .warning = {},
                .invalid = failed,
                .result = hasResult && !failed ? std::string_view(read->result) : std::string_view() };
        }
        if (hasResult) {
            read->seen = read->seen || (read->seeing && !pointed);
            read->seeing = pointed;
        }

        if (drawLiveCopyRow({ .text = OVERWRITE, .button = "Overwrite", .enabled = live, .mark = stale ? ui::TextMark::Warning : ui::TextMark::None }, live, pointed)) {
            drawn.liveCopy = ui::LiveCopyAction::Overwrite;
        }
        if (pointed) {
            drawn.pointed = ui::PointedRow{ .text = OVERWRITE, .help = help(OVERWRITE_HELP), .warning = stale ? STALE : "", .invalid = false };
        }

        if (drawLiveCopyRow({ .text = EDIT, .button = "Open", .enabled = live }, live, pointed)) {
            drawn.liveCopy = ui::LiveCopyAction::Open;
        }
        if (pointed) {
            drawn.pointed = ui::PointedRow{ .text = EDIT, .help = help(EDIT_HELP), .warning = {}, .invalid = false };
        }
        if (drawLiveCopyRow({ .text = FOLDER, .button = "Open", .enabled = live }, live, pointed)) {
            drawn.liveCopy = ui::LiveCopyAction::Show;
        }
        if (pointed) {
            drawn.pointed = ui::PointedRow{ .text = FOLDER, .help = help(FOLDER_HELP), .warning = {}, .invalid = false };
        }
        // the sub-section's line, from under its heading to its last row, short of the rows
        const float rowsEnd = ImGui::GetCursorScreenPos().y;
        ui::drawSectionLine(headingLeft + theme::SPACE_1, lineTop, rowsEnd - ImGui::GetStyle().ItemSpacing.y, headingLeft + theme::SPACE_3 - theme::SPACE_1);
        ImGui::Unindent(theme::SPACE_3);

        drawn.pressActed = drawn.pressActed || drawn.liveCopy != ui::LiveCopyAction::None;
        ImGui::Unindent(theme::SPACE_3);
    }

    /**
     * Whether a problem is a spec.json that cannot be read, installed or a live copy: an error of the file as a
     * whole, with no place in it. Its help line is in red, as a value that is invalid is.
     */
    bool isUnreadable(const model::Problem& problem)
    {
        return problem.severity == model::Severity::Error && problem.where.empty() && problem.path.empty() && !problem.place;
    }
}

namespace mods_config::ui
{
    /**
     * Draws the report of a config spec's mod into the content pane, from its top: a text of where its spec.json
     * is read from, its installed file or its live copy, and `No problems` for a config spec with none. Then the
     * section of its live copy, for a config spec of a folder, with the last read of the copy. Then its errors and its
     * warnings, each under its heading, which says how many there are, in red after the invalid mark or in amber after
     * the warning mark, and is left out with none. A problem is a row
     * set in a level under its heading, of the name of its row and its message, and an arrow where the room of a
     * Reset is for one on a row the menu shows: a row of a mod that is available. A press on such a row tells its
     * problem. Only the problem of a spec.json that cannot be read has its help line in red after the invalid mark.
     */
    ReportDrawn ConfigSpecReport::draw(const model::Mod& mod, LiveCopyRead* read)
    {
        F4CF_PERF_FUNCTION();
        ReportDrawn drawn;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_1);
        std::size_t errors = 0;
        for (const auto& problem : mod.problems) {
            errors += problem.severity == model::Severity::Error ? 1 : 0;
        }
        const auto warnings = mod.problems.size() - errors;
        const auto* noProblems = mod.problems.empty() ? "\nNo problems." : "";
        if (!mod.liveCopy.empty()) {
            drawPageText(std::format("Read from the live copy, {}. It is read again each time it is saved.{}", mod.liveCopy, noProblems));
        } else {
            const auto file = mod.folder.empty() ? std::string("its spec.json") : std::format(R"(Data\ModsConfigFO4\Config\{}\spec.json)", mod.folder);
            drawPageText(std::format("Read from {}.{}", file, noProblems));
        }
        if (!mod.folder.empty()) {
            drawLiveCopy(mod, read, _liveSpecFolded, drawn);
        }

        const bool available = mod.notAvailable.empty();
        for (const auto severity : { model::Severity::Error, model::Severity::Warning }) {
            const bool isError = severity == model::Severity::Error;
            const auto count = isError ? errors : warnings;
            if (count == 0) {
                continue;
            }
            auto& folded = isError ? _errorsFolded : _warningsFolded;
            const auto heading = countOf(count, isError ? "error" : "warning");
            if (foldingHeading(isError ? "errors" : "warnings", heading, folded, drawn.pressActed, {}, isError ? TextMark::Invalid : TextMark::Warning)) {
                drawn.pointed = PointedRow{ .text = isError ? "Errors" : "Warnings",
                    .help = isError ? "What the menu left out of the config spec: all of it, or a part." : "What the menu read that is likely not what is meant.",
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
                const float width = rowWidth();
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
                    drawn.pointed = PointedRow{ .text = problem.row, .help = problemHelp(problem, goes), .warning = {}, .invalid = isUnreadable(problem) };
                }
                ImGui::PopID();
            }
            ImGui::Unindent(theme::SPACE_3);
        }
        return drawn;
    }

    /**
     * Opens the headings of the problems again, and folds the sub-section of the live copy, as the menu is closed.
     */
    void ConfigSpecReport::reset()
    {
        _errorsFolded = false;
        _warningsFolded = false;
        _liveSpecFolded = true;
    }
}
