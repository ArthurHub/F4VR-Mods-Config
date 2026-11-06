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
     * The tag of when a setting's change takes effect, as the Help page tells it. None for one that is not known,
     * which most settings are, as every MCM setting.
     */
    std::string_view appliesTag(const model::Applies applies)
    {
        using model::Applies;
        switch (applies) {
        case Applies::Live:
            return "Live";
        case Applies::Save:
            return "Save";
        case Applies::Restart:
            return "Restart";
        case Applies::Unknown:
            break;
        }
        return {};
    }

    /**
     * Shows a row's text, its warning, the result of its action and its help, whether the help is why its value
     * is invalid, and when the change of its setting takes effect. A row with none of them is its text alone, which a row that
     * ends in an ellipsis has in full here. A mod sets a row in by spaces before its text: they are the page's.
     */
    void HelpLine::show(const PointedRow& row)
    {
        auto name = common::trim(std::string(row.text));
        if (!name.empty() && (!row.help.empty() || !row.warning.empty() || !row.result.empty())) {
            name += ": ";
        }
        _name = std::move(name);
        _warning = row.warning;
        if (!_warning.empty() && !row.help.empty()) {
            _warning += ' ';
        }
        _result = row.result;
        if (!_result.empty() && !row.help.empty()) {
            _result += ' ';
        }
        _help = row.help;
        _invalid = row.invalid && !_help.empty();
        _applies = row.applies;
    }

    void HelpLine::clear()
    {
        _name.clear();
        _warning.clear();
        _result.clear();
        _help.clear();
        _invalid = false;
        _applies.reset();
    }

    /**
     * What the line says while the ray is on no row: the status of the mod's editing session, and nothing
     * where there is none, as on the menu's front page. A status of changes that wait for a Save is pending,
     * one of changes that are invalid, or of a Save that failed, is invalid, and one of a Save that wrote a
     * change that takes effect after a restart is the Restart one.
     */
    void HelpLine::setStatus(const std::string_view status, const StatusLook look)
    {
        _status = status;
        _look = look;
    }

    /**
     * Draws the line from a place at the top of the footer, in the middle of the footer's height.
     * A setting whose change waits for a Save has the tag of when it takes effect first, its mark and its word in
     * amber, where it is known, and the text beside it.
     * It is two lines of body text at most. A help that needs a third is drawn in the detail size, where
     * three lines fit, and a longer one ends in an ellipsis. A warning is in amber after the warning mark,
     * a result in green after the info mark, and why a row is invalid in red after the invalid mark.
     * With no row it is the status, in the muted color, in amber after the pending mark while changes wait
     * for a Save, in green after the Restart mark for a Save that wrote a change that takes effect after a
     * restart, or in red after the invalid mark while a change is invalid and for a Save that failed,
     * wrapped as a help is.
     */
    void HelpLine::draw(const ImVec2& at, const float lineWidth) const
    {
        const bool hasRow = !_name.empty() || !_warning.empty() || !_result.empty() || !_help.empty();
        const float middle = at.y + theme::FOOTER_HEIGHT * 0.5f;
        auto pos = at;
        float width = lineWidth;
        if (const auto tag = this->tag(); !tag.empty()) {
            drawAppliesTag(pos.x, middle, *_applies, tag);
            const float tagRoom = appliesTagWidth(tag) + theme::SPACE_3;
            pos.x += tagRoom;
            width -= tagRoom;
        }
        if (!hasRow && _look == StatusLook::Pending) {
            constexpr float MARK_WIDTH = 10.0f;
            drawPendingMark(ImVec2(pos.x + MARK_WIDTH * 0.5f, middle));
            const float left = pos.x + MARK_WIDTH + theme::SPACE_2;
            drawText(left, pos.x + width, middle - theme::CONTROL_HEIGHT * 0.5f, theme::CONTROL_HEIGHT, theme::color::AMBER, _status);
            return;
        }
        if (!hasRow && _look == StatusLook::Restart) {
            drawRestartMark(pos.x, middle);
            const float left = pos.x + restartMarkWidth() + theme::SPACE_2;
            drawText(left, pos.x + width, middle - theme::CONTROL_HEIGHT * 0.5f, theme::CONTROL_HEIGHT, theme::color::GREEN, _status);
            return;
        }
        const bool invalid = hasRow ? _invalid : _look == StatusLook::Invalid;
        const auto mark = invalid ? TextMark::Invalid : !_warning.empty() ? TextMark::Warning : hasRow && !_result.empty() ? TextMark::Info : TextMark::None;
        const std::array pieces{ TextPiece{ .text = _name, .color = theme::color::GREEN },
            TextPiece{ .text = {}, .color = theme::color::AMBER, .mark = mark },
            TextPiece{ .text = _warning, .color = theme::color::AMBER },
            TextPiece{ .text = hasRow && !invalid ? std::string_view(_result) : std::string_view(), .color = theme::color::GREEN },
            TextPiece{ .text = hasRow ? _help : _status, .color = invalid ? theme::color::RED : theme::color::GREEN_MUTED } };
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
     * The tag of when the change of the line's row takes effect: empty with no row, for a row that is no setting
     * that is set, and for one where it is not known.
     */
    std::string_view HelpLine::tag() const
    {
        const bool hasRow = !_name.empty() || !_warning.empty() || !_result.empty() || !_help.empty();
        return hasRow && _applies ? appliesTag(*_applies) : std::string_view();
    }

    /**
     * The text of the line's row, as it is shown before it is wrapped and cut, without the warning mark.
     * Empty with no row.
     */
    std::string HelpLine::text() const
    {
        return _name + _warning + (_invalid ? std::string() : _result) + _help;
    }
}
