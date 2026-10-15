#include "HelpLine.h"

#include <algorithm>
#include <array>

#include "Drawing.h"
#include "Theme.h"
#include "common/CommonUtils.h"

namespace
{
    // the lines the footer has room for: in the body size, and in the detail size for a help that needs more
    constexpr int BODY_LINES = 2;
    constexpr int DETAIL_LINES = 3;
}

namespace mods_config::ui
{
    /**
     * Shows a row's text and its help. A row with no help is its text alone, which a row that ends in an
     * ellipsis has in full here. A mod sets a row in by spaces before its text: they are the page's.
     */
    void HelpLine::show(const PointedRow& row)
    {
        auto name = common::trim(std::string(row.text));
        if (!name.empty() && !row.help.empty()) {
            name += ": ";
        }
        _name = std::move(name);
        _help = row.help;
    }

    void HelpLine::clear()
    {
        _name.clear();
        _help.clear();
    }

    /**
     * What the line says while the ray is on no row: the status of the mod's editing session, and nothing
     * where there is none, as on the menu's front page.
     */
    void HelpLine::setStatus(const std::string_view status)
    {
        _status = status;
    }

    /**
     * Draws the line from a place at the top of the footer, in the middle of the footer's height.
     * It is two lines of body text at most. A help that needs a third is drawn in the detail size, where
     * three lines fit, and a longer one ends in an ellipsis. With no row it is the status, in the muted color.
     */
    void HelpLine::draw(const ImVec2& pos, const float width) const
    {
        const bool hasRow = !_name.empty() || !_help.empty();
        const std::array pieces{ TextPiece{ .text = _name, .color = theme::color::GREEN }, TextPiece{ .text = hasRow ? _help : _status, .color = theme::color::GREEN_MUTED } };
        int lines = countTextLines(pieces, width);
        if (lines == 0) {
            return;
        }
        const bool detail = lines > BODY_LINES;
        if (detail) {
            ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
            lines = (std::min)(countTextLines(pieces, width), DETAIL_LINES);
        }
        const float height = static_cast<float>(lines) * ImGui::GetFontSize();
        drawTextPieces(pieces, ImVec2(pos.x, pos.y + (theme::FOOTER_HEIGHT - height) * 0.5f), width, lines);
        if (detail) {
            ImGui::PopFont();
        }
    }

    /**
     * The text of the line's row, as it is shown before it is wrapped and cut. Empty with no row.
     */
    std::string HelpLine::text() const
    {
        return _name + _help;
    }
}
