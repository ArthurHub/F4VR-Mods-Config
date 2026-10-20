#include "TextInputs.h"

#include <format>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include "Drawing.h"
#include "Theme.h"

namespace
{
    using namespace mods_config;

    // the box of a text input's text, in the panel's pixels: as wide as a slider with the box of its value
    constexpr float FIELD_WIDTH = ui::TRACK_WIDTH + theme::SPACE_2 + theme::VALUE_WIDTH;

    // the field's name in the row: its place in the page tells it from the others
    constexpr const char* FIELD_ID = "##field";

    /**
     * The box of a text input that ends at the right of the control column.
     */
    ImRect fieldBox(const float right, const float top)
    {
        return ui::controlBox(right - FIELD_WIDTH, top, FIELD_WIDTH);
    }

    /**
     * What ImGui's field asks of a text input as it is typed. A text input that takes a number keeps only the
     * characters of one, as MCM's field: digits and a minus, and a point for a number that is no whole one,
     * and returns 1 for one it drops. A text with a most of characters is cut after its last character that
     * fits, as it is edited, so neither a key nor a paste takes it past its most.
     */
    int fieldCallback(ImGuiInputTextCallbackData* data)
    {
        const auto* setting = static_cast<const model::Setting*>(data->UserData);
        if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
            const auto c = data->EventChar;
            const bool kept = (c >= '0' && c <= '9') || c == '-' || (c == '.' && setting->valueType == model::StoredType::Float);
            return kept ? 0 : 1;
        }
        int length = 0;
        for (int place = 0; place < data->BufTextLen; ++place) {
            // a byte that starts a character of UTF-8
            if ((static_cast<unsigned char>(data->Buf[place]) & 0xC0) != 0x80 && ++length > setting->valueOptions.maxLength) {
                data->DeleteChars(place, data->BufTextLen - place);
                break;
            }
        }
        return 0;
    }
}

namespace mods_config::ui
{
    /**
     * Edits a text input whose box ends at the right: a press on its row starts the edit, and the field is
     * ImGui's from the next frame, with the caret at the end of its text, as MCM's field has it. A text input
     * that takes a number keeps only the characters of one, and one with a most of characters takes none
     * past it, with the count of its characters at the right of its box, as `12 / 32`. It is drawn while its
     * row is scrolled out of the page too, so the edit lasts.
     * The edit ends as ImGui leaves the field: on Done of the keyboard, Enter, or a press beside it, with the
     * text it has then, and on the keyboard's close button or Escape, with the text it started with. Back ends
     * it with nothing written.
     * Returns the text to write, as the edit ends with another text than it started with. The pulse is for
     * the press that starts it.
     */
    std::optional<std::string> TextInputs::edit(const model::Setting& setting, const float right, const float top, const bool pressed, const std::string& text, bool& pulse)
    {
        const ImGuiID id = ImGui::GetID(FIELD_ID);
        if (_edited != &setting) {
            if (pressed) {
                _edited = &setting;
                _startText = text;
                _text = text;
                _active = false;
                _cancel = false;
                ImGui::ActivateItemByID(id);
                pulse = true;
            }
            return std::nullopt;
        }
        if (_cancel) {
            if (ImGui::GetActiveID() == id) {
                ImGui::ClearActiveID();
            }
            _edited = nullptr;
            return std::nullopt;
        }

        // drawn under the field, which draws its text and caret alone
        const auto box = fieldBox(right, top);
        drawFrame(box);
        drawOutline(box.Min, box.Max, theme::color::GREEN);

        // the field is an item at the box, and the cursor is put back after it, so the rows go on under its row
        const auto cursor = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(box.Min);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(theme::SPACE_4, (theme::CONTROL_HEIGHT - ImGui::GetFontSize()) * 0.5f));
        // the count of a text with a most of characters is at the right of the box, where the field ends: the
        // field has the room of the widest count
        const int most = setting.valueOptions.maxLength;
        float countWidth = 0.0f;
        if (most > 0) {
            ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
            countWidth = textWidth(std::format("{} / {}", most, most)) + theme::SPACE_4;
            ImGui::PopFont();
        }
        ImGui::SetNextItemWidth(FIELD_WIDTH - countWidth);
        const bool isNumber = setting.valueType != model::StoredType::String;
        const auto flags = isNumber ? ImGuiInputTextFlags_CallbackCharFilter : most > 0 ? ImGuiInputTextFlags_CallbackEdit : ImGuiInputTextFlags_None;
        ImGui::InputText(FIELD_ID, &_text, flags, flags != ImGuiInputTextFlags_None ? fieldCallback : nullptr, const_cast<model::Setting*>(&setting));
        const bool activated = ImGui::IsItemActivated();
        const bool active = ImGui::IsItemActive();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        if (most > 0) {
            ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
            drawTextIn(box, std::format("{} / {}", model::textLength(_text), most), theme::color::GREEN_MUTED, 1.0f, theme::SPACE_4);
            ImGui::PopFont();
        }
        // not with SetCursorScreenPos: ImGui takes a cursor that is set and followed by no item, as after the
        // last row of a page, for one that grows the page past its items, which is an error
        ImGui::GetCurrentWindow()->DC.CursorPos = cursor;

        if (activated) {
            if (auto* state = ImGui::GetInputTextState(id)) {
                state->SetSelection(state->TextLen, state->TextLen);
            }
        }
        if (active) {
            _active = true;
            return std::nullopt;
        }
        // left, or never taken by ImGui: then nothing was typed
        _edited = nullptr;
        if (!_active || _text == _startText) {
            return std::nullopt;
        }
        return _text;
    }

    /**
     * Draws a text input whose box ends at the right: the text at the left of its box, in the color of its
     * value's look, and a longer one ends in an ellipsis. The box of one that is edited is drawn with its field, by `edit`.
     * Returns the left edge of the box.
     */
    float TextInputs::draw(const model::Setting& setting, const float right, const float top, const std::string& text, const ValueLook look) const
    {
        const auto box = fieldBox(right, top);
        if (!isEdited(setting)) {
            drawFrame(box, look);
            drawTextIn(box, text, valueColor(look), 0.0f, theme::SPACE_4);
        }
        return box.Min.x;
    }

    bool TextInputs::isEdited(const model::Setting& setting) const
    {
        return _edited == &setting;
    }

    /**
     * Ends the edit of a text input with nothing written, in the next frame, for Back: the field leaves
     * ImGui then. Returns false while none is edited.
     */
    bool TextInputs::cancelEdit()
    {
        if (!_edited || _cancel) {
            return false;
        }
        _cancel = true;
        return true;
    }

    /**
     * Forgets the text input that is edited, as another page is shown: it is a row of the page before, and its
     * field is no longer drawn, so ImGui leaves it. Nothing is written.
     */
    void TextInputs::reset()
    {
        _edited = nullptr;
        _cancel = false;
    }
}
