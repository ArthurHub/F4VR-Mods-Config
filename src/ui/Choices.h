#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Drawing.h"
#include "model/ConfigDefinition.h"
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

    /**
     * What an open list of options did in the frame that is drawn: the option a press picked, and the one the ray is on.
     */
    struct ListDrawn
    {
        std::optional<std::size_t> picked;
        std::optional<std::size_t> pointed;
    };

    // draws more of an option of a list, in its box, after its text: whether it is the chosen one says its colors
    using OptionDecoration = std::function<void(std::size_t option, const ImRect& box, bool chosen)>;

    std::optional<std::size_t> chosenOption(const std::vector<model::Option>& options, const model::StoredValue& stored);
    ListDrawn drawList(ImGuiID id, const ImRect& box, std::span<const std::string_view> texts, std::optional<std::size_t> chosen, const OptionDecoration& decorate = {});

    /**
     * The steppers, the dropdowns and the haptics of the page that is shown: how one is drawn, and how the player picks
     * an option of one. A stepper goes to the option before and after, by its buttons and by the thumbstick,
     * and a dropdown opens its list over the page, as a haptic does, which has its pattern's shape and a Play button.
     * A choice has the options its store answers, which are given with it.
     * It points into the page that is shown, so it is reset as another page is.
     */
    class Choices
    {
    public:
        void startFrame(float thumbstickSideValue);
        std::optional<std::size_t> edit(const model::Setting& setting, const std::vector<model::Option>& options, float right, float top, const ChoiceRow& row,
            std::optional<std::size_t> chosen, bool& pulse);
        float draw(const model::Setting& setting, const std::vector<model::Option>& options, float right, float top, const ChoiceRow& row, std::optional<std::size_t> chosen,
            ValueLook look, std::string_view unchosenText = {}) const;
        void endFrame();
        bool isListOpen(const model::Setting& setting) const;

        // the option of the open list the ray is on, and none
        std::optional<std::size_t> pointedOption() const
        {
            return _pointedOption;
        }

        bool closeList();
        void reset();

    private:
        std::optional<std::size_t> editList(const model::Setting& setting, const std::vector<model::Option>& options, float right, float top, const ChoiceRow& row,
            std::optional<std::size_t> chosen, bool& pulse);

        // the dropdown whose list is open, and none
        const model::Setting* _open = nullptr;

        // the list of that dropdown was open in the frame that is drawn
        bool _openSeen = false;

        // the option of that list the ray is on
        std::optional<std::size_t> _pointedOption;

        // the list is closed as the next frame starts
        bool _close = false;

        // the side the thumbstick is pushed to, as -1 or 1, and 0 while it is not pushed
        int _pushSide = 0;

        // the step of the stepper under the ray in the frame that is drawn: the side of a push that began in it
        int _step = 0;
    };
}
