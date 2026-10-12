#pragma once

#include <string>
#include <string_view>

#include <imgui.h>

namespace mods_config::ui
{
    /**
     * The row the ray is on: what the row says, and its help. They are the model's own texts.
     */
    struct PointedRow
    {
        std::string_view text;
        std::string_view help;
    };

    /**
     * The help line, on the left of the Full panel's footer: the help of the row the ray is on, as
     * `Name: help`. It keeps the row it was last given, so the line does not change while the ray is in the
     * gap between two rows.
     */
    class HelpLine
    {
    public:
        void show(const PointedRow& row);
        void clear();
        void draw(const ImVec2& pos, float width) const;
        std::string text() const;

    private:
        // the row's text, with the colon and the space that stand before a help, and the row's help
        std::string _name;
        std::string _help;
    };
}
