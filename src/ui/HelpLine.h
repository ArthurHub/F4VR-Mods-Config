#pragma once

#include <string>
#include <string_view>

#include <imgui.h>

namespace mods_config::ui
{
    /**
     * The row the ray is on: what the row says, which is the model's own text, and its help. A row whose
     * value is not available has the reason as its help. A row that warns has the text of its warning, the
     * model's own too.
     */
    struct PointedRow
    {
        std::string_view text;
        std::string help;
        std::string_view warning;
    };

    /**
     * The help line, on the left of the Full panel's footer: the help of the row the ray is on, as
     * `Name: help`, with the warning of a row that warns before its help. It keeps the row it was last given, so the line does not change while the ray is in the
     * gap between two rows. With no row it has the status of the editing session, where there is one.
     */
    class HelpLine
    {
    public:
        void show(const PointedRow& row);
        void clear();
        void setStatus(std::string_view status, bool pending = false);
        void draw(const ImVec2& pos, float width) const;
        std::string text() const;

        const std::string& status() const
        {
            return _status;
        }

        bool isStatusPending() const
        {
            return _pending;
        }

    private:
        // the row's text, with the colon and the space that stand before a help, the row's warning, with the
        // space before a help, and the row's help
        std::string _name;
        std::string _warning;
        std::string _help;

        // what the line says with no row: empty for nothing
        std::string _status;

        // the status tells of changes that wait for a Save
        bool _pending = false;
    };
}
