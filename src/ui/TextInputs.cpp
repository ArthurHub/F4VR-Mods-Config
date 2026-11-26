#include "TextInputs.h"

#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "Drawing.h"
#include "Theme.h"
#include "imgui/controls/TextInputs.h"

namespace
{
    using namespace mods_config;

    /**
     * The box of a text input's text, in the panel's pixels: as wide as a slider with the box of its value.
     */
    float fieldWidth()
    {
        return ui::TRACK_WIDTH + ui::look().space2 + theme::VALUE_WIDTH;
    }

    // the field's name in the row: its place in the page tells it from the others
    constexpr const char* FIELD_ID = "##field";

    /**
     * The box of a text input that ends at the right of the control column, and of a dropdown whose new value is typed:
     * the dropdown's own box.
     */
    ImRect fieldBox(const model::Setting& setting, const float right, const float top)
    {
        const float width = setting.type == model::SettingType::Dropdown ? ui::dropdownWidth() : fieldWidth();
        return controls::controlBox(right - width, top, width);
    }

    /**
     * What a setting's text input takes: the characters of a number for a number, as MCM's field (digits and a minus,
     * and a point for a number that is no whole one), and its most of characters.
     */
    controls::TextOptions fieldOptions(const model::Setting& setting)
    {
        const auto chars = setting.valueType == model::StoredType::String  ? controls::TextChars::Any
                           : setting.valueType == model::StoredType::Float ? controls::TextChars::Decimal
                                                                           : controls::TextChars::Integer;
        return { .chars = chars, .maxLength = setting.valueOptions.maxLength };
    }
}

namespace mods_config::ui
{
    /**
     * Edits a text input whose box ends at the right, on the framework's text input: a press on its row starts the
     * edit, with the caret at the end of its text, as MCM's field has it. It is called while its row is scrolled out
     * of the page too, so the edit lasts. Back ends it with nothing written.
     * Returns the text to write, as the edit ends with another text than it started with. The pulse is for the press
     * that starts it.
     */
    std::optional<std::string> TextInputs::edit(const model::Setting& setting, const float right, const float top, const bool pressed, const std::string& text, bool& pulse)
    {
        if (_edited != &setting && !pressed) {
            return std::nullopt;
        }
        const ImGuiID id = ImGui::GetID(FIELD_ID);
        auto edit = controls::textEdit(id, fieldBox(setting, right, top), pressed, text, fieldOptions(setting));
        if (edit.pulse) {
            pulse = true;
        }
        if (controls::get().isTextEdited(id)) {
            _edited = &setting;
        } else if (_edited == &setting) {
            _edited = nullptr;
        }
        return std::move(edit.write);
    }

    /**
     * Draws a text input whose box ends at the right: the text at the left of its box, in the color of its
     * value's look, and a longer one ends in an ellipsis. The box of one that is edited is drawn with its field, by `edit`.
     * Returns the left edge of the box.
     */
    float TextInputs::draw(const model::Setting& setting, const float right, const float top, const std::string& text, const ValueLook look) const
    {
        const auto box = fieldBox(setting, right, top);
        if (!isEdited(setting)) {
            controls::drawTextInput(box, text, toneOf(look));
        }
        return box.Min.x;
    }

    bool TextInputs::isEdited(const model::Setting& setting) const
    {
        return _edited == &setting;
    }

    /**
     * Forgets the text input that is edited, as another page is shown: it is a row of the page before, and its
     * field is no longer drawn, so the edit ends with nothing written.
     */
    void TextInputs::reset()
    {
        _edited = nullptr;
    }
}
