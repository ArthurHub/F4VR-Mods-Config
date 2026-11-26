#include "SearchField.h"

#include <format>

#include <imgui.h>

#include "Drawing.h"
#include "imgui/controls/SearchFields.h"

namespace
{
    // the field of ImGui that is edited, by its name in the tree's window
    constexpr const char* QUERY_ID = "##searchQuery";

    constexpr auto FIELD_HELP = "Finds a setting by its name in every mod: the start of each word, in any order.";
    constexpr auto CLEAR_HELP = "Empties the search, and shows every mod again.";
}

namespace mods_config::ui
{
    /**
     * Draws the field at a place, in a width, as the framework's search field: idle "Search 11 mods", or the query, then
     * how many settings match and Clear while it has one. A press edits it on the framework's live text input, and the
     * query is its text as it is typed, which a physical keyboard's Escape keeps.
     */
    SearchDrawn SearchField::draw(const ImVec2& pos, const float width, const std::size_t mods, const std::optional<std::size_t> matches)
    {
        _queryId = ImGui::GetID(QUERY_ID);
        const auto placeholder = std::format("Search {} mods", mods);
        auto field = controls::searchField(QUERY_ID, pos, width, _query, { .placeholder = placeholder, .count = matches });
        if (field.write) {
            _query = std::move(*field.write);
        }
        if (field.cleared) {
            clear();
        }
        SearchDrawn drawn{ .pressActed = field.pulse };
        if (field.clearPointed) {
            drawn.pointed = PointedRow{ .text = "Clear", .help = CLEAR_HELP };
        } else if (field.pointed) {
            drawn.pointed = PointedRow{ .text = "Search", .help = FIELD_HELP };
        }
        return drawn;
    }

    /**
     * The field is edited, and Back has not ended its edit.
     */
    bool SearchField::isEdited() const
    {
        return controls::get().isTextEdited(_queryId);
    }

    /**
     * Empties the query, and ends an edit: as Clear is pressed, and as the menu opens.
     */
    void SearchField::clear()
    {
        setQuery({});
    }

    /**
     * Gives the field a query, as the keyboard types it, and ends an edit: for devbench, which types no keys.
     */
    void SearchField::setQuery(std::string query)
    {
        _query = std::move(query);
        if (isEdited()) {
            controls::get().endTextEdit();
        }
    }
}
