#include "SearchField.h"

#include <format>

#include <imgui.h>
#include <imgui_internal.h>

#include "Drawing.h"
#include "Targets.h"
#include "Theme.h"
#include "imgui/controls/TextInputs.h"

namespace
{
    using namespace mods_config;

    // the field's height, not a token of the design system, in the panel's pixels
    constexpr float FIELD_HEIGHT = 40.0f;

    // the magnifier before the text, Clear at the right and its cross, as the design system draws them
    constexpr float MAGNIFIER_SIZE = 20.0f;
    constexpr float CLEAR_SIZE = 32.0f;
    constexpr float CROSS_SIZE = 18.0f;

    // the field of ImGui that is edited, by its name in the tree's window
    constexpr const char* QUERY_ID = "##searchQuery";

    // the query is written as it is typed, and its field is bare in the field's look, which the field draws
    constexpr controls::TextOptions QUERY_OPTIONS{ .live = true };
    constexpr controls::TextLook QUERY_LOOK{ .framed = false, .padding = 0.0f };

    constexpr auto FIELD_HELP = "Finds a setting by its name in every mod: the start of each word, in any order.";
    constexpr auto CLEAR_HELP = "Empties the search, and shows every mod again.";

    /**
     * The magnifier, in a box of its size from a corner: a circle with a handle, as the design system draws it in a
     * box of 24.
     */
    void drawMagnifier(const ImVec2& min, const ImU32 color)
    {
        constexpr float SCALE = MAGNIFIER_SIZE / 24.0f;
        auto* drawList = ImGui::GetWindowDrawList();
        const auto ink = ImGui::GetColorU32(color);
        drawList->AddCircle(ImVec2(min.x + 10.5f * SCALE, min.y + 10.5f * SCALE), 6.0f * SCALE, ink, 0, controls::ICON_THICKNESS * SCALE);
        drawList->AddLine(ImVec2(min.x + 15.0f * SCALE, min.y + 15.0f * SCALE), ImVec2(min.x + 20.0f * SCALE, min.y + 20.0f * SCALE), ink, controls::ICON_THICKNESS * SCALE);
    }

    /**
     * The cross of Clear around a point, as the design system draws it in a box of 24.
     */
    void drawCross(const ImVec2& center)
    {
        constexpr float HALF = 6.0f * CROSS_SIZE / 24.0f;
        constexpr float THICKNESS = controls::ICON_THICKNESS * CROSS_SIZE / 24.0f;
        const auto color = ImGui::GetColorU32(theme::color::GREEN);
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddLine(ImVec2(center.x - HALF, center.y - HALF), ImVec2(center.x + HALF, center.y + HALF), color, THICKNESS);
        drawList->AddLine(ImVec2(center.x + HALF, center.y - HALF), ImVec2(center.x - HALF, center.y + HALF), color, THICKNESS);
    }
}

namespace mods_config::ui
{
    /**
     * Draws the field at a place, in a width: the magnifier, then idle "Search 11 mods", or the query, then how many
     * settings match and Clear while it has one. A press on the field edits it on the framework's live text input,
     * bare in the field's look, and the query is its text as it is typed, which a physical keyboard's Escape keeps.
     */
    SearchDrawn SearchField::draw(const ImVec2& pos, const float width, const std::size_t mods, const std::optional<std::size_t> matches)
    {
        SearchDrawn drawn;
        const ImRect box(pos, ImVec2(pos.x + width, pos.y + FIELD_HEIGHT));
        const float middle = box.GetCenter().y;
        _queryId = ImGui::GetID(QUERY_ID);
        const bool edited = isEdited();

        const bool hasQuery = !_query.empty();
        const ImRect clearBox(ImVec2(box.Max.x - theme::SPACE_1 - CLEAR_SIZE, middle - CLEAR_SIZE * 0.5f), ImVec2(box.Max.x - theme::SPACE_1, middle + CLEAR_SIZE * 0.5f));
        const float fieldRight = hasQuery ? clearBox.Min.x : box.Max.x;

        bool pointed = false;
        bool pressed = false;
        if (!edited) {
            ImGui::SetCursorScreenPos(box.Min);
            pressed = ImGui::InvisibleButton("##searchField", ImVec2(fieldRight - box.Min.x, FIELD_HEIGHT));
            pointed = ImGui::IsItemHovered();
            controls::targets().add(target::SEARCH, "Field", ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()));
        }
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(box.Min, box.Max, ImGui::GetColorU32(pointed ? theme::color::HOVER : theme::color::TRACK), theme::RADIUS_FRAME);
        controls::drawOutline(box.Min, box.Max, edited ? theme::color::GREEN : theme::color::FRAME_BORDER);
        drawMagnifier(ImVec2(box.Min.x + theme::SPACE_2, middle - MAGNIFIER_SIZE * 0.5f), pointed ? theme::color::GREEN : theme::color::GREEN_MUTED);

        // how many settings match, at the right of the text
        const float textLeft = box.Min.x + theme::SPACE_2 + MAGNIFIER_SIZE + theme::SPACE_2;
        float textRight = fieldRight - theme::SPACE_2;
        if (hasQuery && matches) {
            ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
            const auto count = std::to_string(*matches);
            const float countLeft = textRight - controls::textWidth(count);
            controls::drawText(countLeft, textRight, box.Min.y, FIELD_HEIGHT, theme::color::GREEN_MUTED, count);
            ImGui::PopFont();
            textRight = countLeft - theme::SPACE_2;
        }

        const ImRect textBox(ImVec2(textLeft, box.Min.y), ImVec2(textRight, box.Max.y));
        if (edited) {
            if (auto edit = controls::textEdit(_queryId, textBox, false, _query, QUERY_OPTIONS, QUERY_LOOK); edit.write) {
                _query = std::move(*edit.write);
            }
        } else if (hasQuery) {
            controls::drawText(textLeft, textRight, box.Min.y, FIELD_HEIGHT, theme::color::GREEN, _query);
        } else {
            ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
            controls::drawText(textLeft, textRight, box.Min.y, FIELD_HEIGHT, pointed ? theme::color::GREEN : theme::color::GREEN_MUTED, std::format("Search {} mods", mods));
            ImGui::PopFont();
        }
        if (pointed) {
            drawn.pointed = PointedRow{ .text = "Search", .help = FIELD_HELP };
        }

        if (hasQuery) {
            ImGui::SetCursorScreenPos(clearBox.Min);
            if (ImGui::InvisibleButton("##searchClear", clearBox.GetSize())) {
                clear();
                drawn.pressActed = true;
            }
            controls::targets().add(target::SEARCH, "Clear", clearBox);
            controls::drawRowState();
            if (ImGui::IsItemHovered()) {
                drawn.pointed = PointedRow{ .text = "Clear", .help = CLEAR_HELP };
            }
            drawCross(clearBox.GetCenter());
        }

        if (pressed) {
            controls::textEdit(_queryId, textBox, true, _query, QUERY_OPTIONS, QUERY_LOOK);
            drawn.pressActed = true;
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
