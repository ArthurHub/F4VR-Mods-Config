#pragma once

#include <optional>
#include <string>

#include "Drawing.h"
#include "model/ConfigDefinition.h"

namespace mods_config::ui
{
    /**
     * The text inputs of the page that is shown: how one is drawn, and how the player edits one. A press on
     * its row starts its edit in ImGui's field, which the framework shows SteamVR's keyboard for, and the
     * text is written as the field is left with another text than it started with. A dropdown with a typed entry is
     * edited so too, in its box, from an empty text, as the entry is picked.
     * It points into the page that is shown, so it is reset as another page is.
     */
    class TextInputs
    {
    public:
        std::optional<std::string> edit(const model::Setting& setting, float right, float top, bool pressed, const std::string& text, bool& pulse);
        float draw(const model::Setting& setting, float right, float top, const std::string& text, ValueLook look) const;
        bool isEdited(const model::Setting& setting) const;
        bool cancelEdit();
        void reset();

    private:
        // the text input that is edited, and none
        const model::Setting* _edited = nullptr;

        // its text as the edit started, and the text of its field
        std::string _startText;
        std::string _text;

        // the field was edited by ImGui since the edit started: the frame of the press is before it
        bool _active = false;

        // the edit ends with nothing written in the next frame, for Back
        bool _cancel = false;
    };
}
