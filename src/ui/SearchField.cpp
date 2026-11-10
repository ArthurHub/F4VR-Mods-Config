#include "SearchField.h"

#include <format>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include "Drawing.h"
#include "Theme.h"

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
        drawList->AddCircle(ImVec2(min.x + 10.5f * SCALE, min.y + 10.5f * SCALE), 6.0f * SCALE, ink, 0, ui::ICON_THICKNESS * SCALE);
        drawList->AddLine(ImVec2(min.x + 15.0f * SCALE, min.y + 15.0f * SCALE), ImVec2(min.x + 20.0f * SCALE, min.y + 20.0f * SCALE), ink, ui::ICON_THICKNESS * SCALE);
    }

    /**
     * The cross of Clear around a point, as the design system draws it in a box of 24.
     */
    void drawCross(const ImVec2& center)
    {
        constexpr float HALF = 6.0f * CROSS_SIZE / 24.0f;
        constexpr float THICKNESS = ui::ICON_THICKNESS * CROSS_SIZE / 24.0f;
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
     * settings match and Clear while it has one. A press on the field edits it in ImGui's field from the next frame,
     * with the caret at the end, which the framework shows SteamVR's keyboard for, and the query is its text as it
     * is typed. ImGui gives the field back the text it had on the keyboard's close button and Escape: the query
     * keeps what was typed then.
     */
    SearchDrawn SearchField::draw(const ImVec2& pos, const float width, const std::size_t mods, const std::optional<std::size_t> matches)
    {
        SearchDrawn drawn;
        const ImRect box(pos, ImVec2(pos.x + width, pos.y + FIELD_HEIGHT));
        const float middle = box.GetCenter().y;
        const ImGuiID queryId = ImGui::GetID(QUERY_ID);

        if (_end) {
            if (ImGui::GetActiveID() == queryId) {
                ImGui::ClearActiveID();
            }
            _edited = false;
            _end = false;
        }

        const bool hasQuery = !_query.empty();
        const ImRect clearBox(ImVec2(box.Max.x - theme::SPACE_1 - CLEAR_SIZE, middle - CLEAR_SIZE * 0.5f), ImVec2(box.Max.x - theme::SPACE_1, middle + CLEAR_SIZE * 0.5f));
        const float fieldRight = hasQuery ? clearBox.Min.x : box.Max.x;

        bool pointed = false;
        bool pressed = false;
        if (!_edited) {
            ImGui::SetCursorScreenPos(box.Min);
            pressed = ImGui::InvisibleButton("##searchField", ImVec2(fieldRight - box.Min.x, FIELD_HEIGHT));
            pointed = ImGui::IsItemHovered();
        }
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(box.Min, box.Max, ImGui::GetColorU32(pointed ? theme::color::HOVER : theme::color::TRACK), theme::RADIUS_FRAME);
        drawOutline(box.Min, box.Max, _edited ? theme::color::GREEN : theme::color::FRAME_BORDER);
        drawMagnifier(ImVec2(box.Min.x + theme::SPACE_2, middle - MAGNIFIER_SIZE * 0.5f), pointed ? theme::color::GREEN : theme::color::GREEN_MUTED);

        // how many settings match, at the right of the text
        const float textLeft = box.Min.x + theme::SPACE_2 + MAGNIFIER_SIZE + theme::SPACE_2;
        float textRight = fieldRight - theme::SPACE_2;
        if (hasQuery && matches) {
            ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
            const auto count = std::to_string(*matches);
            const float countLeft = textRight - textWidth(count);
            drawText(countLeft, textRight, box.Min.y, FIELD_HEIGHT, theme::color::GREEN_MUTED, count);
            ImGui::PopFont();
            textRight = countLeft - theme::SPACE_2;
        }

        if (_edited) {
            const auto cursor = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(textLeft, box.Min.y));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, theme::color::GREEN);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, (FIELD_HEIGHT - ImGui::GetFontSize()) * 0.5f));
            ImGui::SetNextItemWidth(textRight - textLeft);
            ImGui::InputText(QUERY_ID, &_text);
            const bool activated = ImGui::IsItemActivated();
            const bool active = ImGui::IsItemActive();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
            ImGui::GetCurrentWindow()->DC.CursorPos = cursor;
            if (activated) {
                if (auto* state = ImGui::GetInputTextState(queryId)) {
                    state->SetSelection(state->TextLen, state->TextLen);
                }
            }
            if (active) {
                _active = true;
                _query = _text;
            } else {
                // left: on Escape ImGui gave the field back the text it had, and the query keeps what was typed
                if (_active && !ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                    _query = _text;
                }
                _edited = false;
            }
        } else if (hasQuery) {
            drawText(textLeft, textRight, box.Min.y, FIELD_HEIGHT, theme::color::GREEN, _query);
        } else {
            ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
            drawText(textLeft, textRight, box.Min.y, FIELD_HEIGHT, pointed ? theme::color::GREEN : theme::color::GREEN_MUTED, std::format("Search {} mods", mods));
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
            drawRowState();
            if (ImGui::IsItemHovered()) {
                drawn.pointed = PointedRow{ .text = "Clear", .help = CLEAR_HELP };
            }
            drawCross(clearBox.GetCenter());
        }

        if (pressed) {
            _edited = true;
            _active = false;
            _text = _query;
            ImGui::ActivateItemByID(queryId);
            drawn.pressActed = true;
        }
        return drawn;
    }

    /**
     * Ends the edit of the field in the next frame, for Back, with the query it has. Returns false while it is not
     * edited.
     */
    bool SearchField::endEdit()
    {
        if (!_edited || _end) {
            return false;
        }
        _end = true;
        return true;
    }

    /**
     * Empties the query, and ends an edit: as Clear is pressed, and as the menu opens.
     */
    void SearchField::clear()
    {
        _query.clear();
        _text.clear();
        _end = _edited;
    }
}
