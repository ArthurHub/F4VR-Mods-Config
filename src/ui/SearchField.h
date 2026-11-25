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
     * a press edits it on the framework's live text input and keyboard, and each key changes the query as it is typed.
     * The keyboard's Done and Back, and a physical keyboard's Escape, all keep the query, and Clear empties it.
     */
    class SearchField
    {
    public:
        SearchDrawn draw(const ImVec2& pos, float width, std::size_t mods, std::optional<std::size_t> matches);
        bool isEdited() const;
        void clear();
        void setQuery(std::string query);

        // the query the tree is filtered by, as it is typed: empty for none
        const std::string& query() const
        {
            return _query;
        }

    private:
        std::string _query;

        // the id of the field's text input, which the framework keeps the edit by, as of its last frame
        ImGuiID _queryId = 0;
    };
}
