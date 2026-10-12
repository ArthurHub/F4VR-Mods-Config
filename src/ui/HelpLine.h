#pragma once

#include <string>
#include <string_view>

#include <imgui.h>

namespace mods_config::ui
{
    /**
     * The row the ray is on: what the row says, which is the model's own text, and its help. A row whose
     * value is not available has the reason as its help.
     */
    struct PointedRow
    {
        std::string_view text;
        std::string help;
    };

    /**
     * The help line, on the left of the Full panel's footer: the help of the row the ray is on, as
     * `Name: help`. It keeps the row it was last given, so the line does not change while the ray is in the
     * gap between two rows. With no row it has the status of the editing session, where there is one.
     */
    class HelpLine
    {
    public:
        void show(const PointedRow& row);
        void clear();
        void setStatus(std::string_view status);
        void draw(const ImVec2& pos, float width) const;
        std::string text() const;

        const std::string& status() const
        {
            return _status;
        }

    private:
        // the row's text, with the colon and the space that stand before a help, and the row's help
        std::string _name;
        std::string _help;

        // what the line says with no row: empty for nothing
        std::string _status;
    };
}
