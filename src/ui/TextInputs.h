#pragma once

#include <optional>
#include <string>

#include "Drawing.h"
#include "model/ConfigDefinition.h"

namespace mods_config::ui
{
    /**
     * The text inputs of the page that is shown, on the framework's text input: how one is drawn, and how the player
     * edits one. A press on its row starts its edit, which the framework shows its keyboard for, and the text is
     * written as the field is left with another text than it started with. A dropdown with a typed entry is edited so
     * too, in its box, from an empty text, as the entry is picked.
     * It points into the page that is shown, so it is reset as another page is.
     */
    class TextInputs
    {
    public:
        std::optional<std::string> edit(const model::Setting& setting, float right, float top, bool pressed, const std::string& text, bool& pulse);
        float draw(const model::Setting& setting, float right, float top, const std::string& text, ValueLook look) const;
        bool isEdited(const model::Setting& setting) const;

        const model::Setting* editedSetting() const
        {
            return _edited;
        }

        void reset();

    private:
        // the setting of the text input that is edited, and none: the framework keeps its edit
        const model::Setting* _edited = nullptr;
    };
}
