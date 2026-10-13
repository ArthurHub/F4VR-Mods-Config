#pragma once

#include <cstddef>
#include <optional>

#include "model/Definition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    /**
     * What the row of a stepper or a dropdown tells of the pointer in the frame that is drawn.
     */
    struct ChoiceRow
    {
        // the ray is on the row
        bool pointed = false;

        // the trigger is held since a pull on the row, and was let go on it in this frame
        bool held = false;
        bool pressed = false;
    };

    std::optional<std::size_t> chosenOption(const model::Setting& setting, const model::StoredValue& stored);

    /**
     * The steppers and the dropdowns of the page that is shown: how one is drawn, and how the player picks
     * an option of one. A stepper goes to the option before and after, by its buttons and by the thumbstick,
     * and a dropdown opens its list over the page.
     * It points into the page that is shown, so it is reset as another page is.
     */
    class Choices
    {
    public:
        void startFrame(float thumbstickSideValue);
        std::optional<std::size_t> edit(const model::Setting& setting, float right, float top, const ChoiceRow& row, std::optional<std::size_t> chosen, bool& pulse);
        float draw(const model::Setting& setting, float right, float top, const ChoiceRow& row, std::optional<std::size_t> chosen) const;
        void endFrame();
        bool isListOpen(const model::Setting& setting) const;
        bool closeList();
        void reset();

    private:
        std::optional<std::size_t> editList(const model::Setting& setting, float right, float top, const ChoiceRow& row, std::optional<std::size_t> chosen, bool& pulse);

        // the dropdown whose list is open, and none
        const model::Setting* _open = nullptr;

        // the list of that dropdown was open in the frame that is drawn
        bool _openSeen = false;

        // the list is closed as the next frame starts
        bool _close = false;

        // the side the thumbstick is pushed to, as -1 or 1, and 0 while it is not pushed
        int _pushSide = 0;

        // the step of the stepper under the ray in the frame that is drawn: the side of a push that began in it
        int _step = 0;
    };
}
