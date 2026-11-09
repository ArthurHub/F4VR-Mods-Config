#include "HelpPage.h"

#include <array>
#include <string>
#include <string_view>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"
#include "perf/Perf.h"

namespace
{
    using namespace mods_config;

    // the column of a line's mark, as wide as the largest mark, and the space after it
    constexpr float MARK_COLUMN = 20.0f;

    // the part of their size the Reset and the Discard icon are drawn at in the column, from their box of 24
    constexpr float ICON_SCALE = MARK_COLUMN / 24.0f;

    /**
     * The mark a line of the page starts with, as its row draws it.
     */
    enum class LineMark : std::uint8_t
    {
        None,
        Live,
        Save,
        Restart,
        NoMark,
        Discard,
        Reset,
        Invalid,
        Warning,
        Info,
    };

    /**
     * A line of the page: its name in its color, its text, and the mark it starts with.
     */
    struct PageLine
    {
        std::string_view name;
        std::string_view text;
        LineMark mark = LineMark::None;
        ImU32 nameColor = theme::color::GREEN;
    };

    /**
     * Draws a line's mark in the middle of its column.
     */
    void drawLineMark(const LineMark mark, const ImVec2& center)
    {
        switch (mark) {
        case LineMark::Live:
            ui::drawAppliesMark(model::Applies::Live, center);
            break;
        case LineMark::Save:
            ui::drawAppliesMark(model::Applies::Save, center);
            break;
        case LineMark::Restart:
            ui::drawAppliesMark(model::Applies::Restart, center);
            break;
        case LineMark::NoMark:
            ui::drawAppliesMark(model::Applies::Unknown, center);
            break;
        case LineMark::Discard:
            ui::drawDiscardIcon(center, ICON_SCALE);
            break;
        case LineMark::Reset:
            ui::drawResetIcon(center, ICON_SCALE);
            break;
        case LineMark::Invalid:
            ui::drawInvalidMark(center);
            break;
        case LineMark::Warning:
            ui::drawWarningMark(center);
            break;
        case LineMark::Info:
            ui::drawInfoMark(center);
            break;
        default:
            break;
        }
    }

    /**
     * A line of the page as the help line has a row's help, `Name: text`, wrapped to the page's width with as many lines
     * as it needs. A line with a mark has it first, in a column of its own, `space-2` before the name.
     */
    void drawLine(const PageLine& line)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float markRoom = line.mark != LineMark::None ? MARK_COLUMN + theme::SPACE_2 : 0.0f;
        const float left = pos.x + ui::HEADING_TEXT_INSET;
        const float width = ImGui::GetContentRegionAvail().x - ui::HEADING_TEXT_INSET - theme::SPACE_2 - markRoom;
        const auto label = std::string(line.name) + ": ";
        const std::array pieces{ ui::TextPiece{ .text = label, .color = line.nameColor }, ui::TextPiece{ .text = line.text, .color = theme::color::GREEN_MUTED } };
        const int lines = ui::countTextLines(pieces, width);
        if (line.mark != LineMark::None) {
            drawLineMark(line.mark, ImVec2(left + MARK_COLUMN * 0.5f, pos.y + ImGui::GetFontSize() * 0.5f));
        }
        ui::drawTextPieces(pieces, ImVec2(left + markRoom, pos.y), width, lines);
        ImGui::Dummy(ImVec2(width, static_cast<float>(lines) * ImGui::GetFontSize()));
    }

    /**
     * The heading of a section of the page, which does not fold: `space-2` under the line before it, and a row of a
     * control's height with its text in the middle, right above its first line, as the design system's heading is. The page's lines are as tall as
     * their text, so it does not take that room from the rows around it as a heading on a mod's page does.
     */
    void drawHeading(const std::string_view heading)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_2);
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding, ImVec2(ImGui::GetStyle().SeparatorTextPadding.x, (theme::CONTROL_HEIGHT - ImGui::GetFontSize()) * 0.5f));
        ui::sectionHeading(heading, ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x);
        ImGui::PopStyleVar();
        // the first line starts under the heading's row, with no spacing of its own
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
    }

    /**
     * A section of the page: its heading and its lines.
     */
    void drawSection(const std::string_view heading, const auto& lines)
    {
        drawHeading(heading);
        for (const auto& line : lines) {
            drawLine(line);
        }
    }

    const std::array CHANGES{
        PageLine{ .name = "Save",
            .text = "Changes wait until you select Save, which writes them, or Discard, which drops them. An MCM mod applies a change at once instead: see MCM below. "
                    "Each mod's changes are separate, and you're asked first when leaving a mod or closing the menu with unsaved changes." },
        PageLine{ .name = "Undo one change",
            .text = "A changed value turns amber. Select the undo arrow after it to undo just that change. Discard, in the footer, undoes all of them.",
            .mark = LineMark::Discard },
        PageLine{ .name = "Reset", .text = "Sets the mod's default value, like any other change. The help line shows the default.", .mark = LineMark::Reset },
        PageLine{ .name = "Buttons", .text = "A button that runs a mod's script asks you to save first, since the script reads the saved values. An MCM mod's buttons don't ask." },
        PageLine{ .name = "When a change takes effect", .text = "A mod can show it with a mark after the setting's name:" },
        PageLine{ .name = "Live",
            .text = "The mod applies the change as you make it, so you see it in the game before saving. It is still unsaved: Save keeps it, and Discard, or "
                    "leaving without saving, restores the old value in the game too.",
            .mark = LineMark::Live },
        PageLine{ .name = "Save", .text = "The mod applies the change when you save.", .mark = LineMark::Save },
        PageLine{ .name = "Restart", .text = "The change applies after restarting the game. The front page reminds you until then.", .mark = LineMark::Restart },
        PageLine{ .name = "No mark", .text = "The mod doesn't say. MCM mods never do.", .mark = LineMark::NoMark },
    };

    const std::array MARKS{
        PageLine{ .name = "Amber value", .text = "Changed, but not saved or kept yet.", .nameColor = theme::color::AMBER },
        PageLine{ .name = "Red value", .text = "Invalid. The help line explains why.", .mark = LineMark::Invalid, .nameColor = theme::color::RED },
        PageLine{ .name = "Warning", .text = "Something to know about the current value. The help line explains it.", .mark = LineMark::Warning },
        PageLine{ .name = "Result", .text = "A button returned a result. Point at its row to read it.", .mark = LineMark::Info },
        PageLine{ .name = "Greyed row", .text = "Can't be changed. Point at it and the help line explains why.", .nameColor = theme::color::DISABLED },
    };

    const std::array MCM{
        PageLine{ .name = "MCM tag", .text = "Shown after a mod's name in the list when its menu comes from MCM. Its changes work as described below." },
        PageLine{ .name = "Keep and Revert",
            .text = "Changes apply as you make them, as in MCM's own menu, so you see them in the game and a button's script reads them. Keep accepts a change, and "
                    "some mods only apply it then. Revert undoes it and runs the mod's script for it again." },
    };
}

namespace mods_config::ui
{
    /**
     * Draws the Help page as the design system's FullPanelHelp has it: its introduction, then four sections that do not
     * fold, Navigation with the controls of the player's config the menu gives, Changes, Marks, and what differs from
     * MCM's menu. Each line is `Name: text`, its name green or in the color it tells, after its mark where it has one.
     */
    void drawHelpPage(const std::vector<ControlHelp>& controls)
    {
        F4CF_PERF_FUNCTION();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_1);
        drawPageText(
            "The menu doesn't pause the game, so you can see changes in the world "
            "as you make them. The controls below reflect your current button configuration.");
        drawHeading("Navigation");
        for (const auto& control : controls) {
            drawLine({ .name = control.name, .text = control.text });
        }
        drawSection("Changes", CHANGES);
        drawSection("Marks", MARKS);
        drawSection("MCM", MCM);
    }
}
