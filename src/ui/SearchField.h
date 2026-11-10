#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include <imgui.h>

#include "HelpLine.h"

namespace mods_config::ui
{
    /**
     * What the drawing of the search field tells.
     */
    struct SearchDrawn
    {
        // a press did something: it started the edit, or Clear emptied the query
        bool pressActed = false;

        // the field or Clear the ray is on, for the help line
        std::optional<PointedRow> pointed;
    };

    /**
     * The search field, the header of the tree, as the design system's Search has it: idle it says what it searches,
     * a press edits it on SteamVR's keyboard, and each key changes the query as it is typed. The keyboard's Done and
     * its close button both keep the query, and Clear empties it.
     */
    class SearchField
    {
    public:
        SearchDrawn draw(const ImVec2& pos, float width, std::size_t mods, std::optional<std::size_t> matches);
        bool endEdit();
        void clear();

        // the query the tree is filtered by, as it is typed: empty for none
        const std::string& query() const
        {
            return _query;
        }

        bool isEdited() const
        {
            return _edited;
        }

    private:
        std::string _query;

        // the text of ImGui's field while it is edited
        std::string _text;

        // the field is edited, from the frame after the press that started it, and ImGui took it since
        bool _edited = false;
        bool _active = false;

        // the edit ends in the next frame, with the query kept, for Back
        bool _end = false;
    };
}
