#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Drawing.h"
#include "imgui/controls/Choices.h"
#include "model/ConfigDefinition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    // the entry that a dropdown with a typed entry ends with, and what the help line says of it
    constexpr auto TYPED_OPTION_TEXT = "Type your own";
    constexpr auto TYPED_OPTION_HELP = "Opens the keyboard to type a value that is not in the list.";

    std::optional<std::size_t> chosenOption(const std::vector<model::Option>& options, const model::StoredValue& stored);

    /**
     * The steppers, the dropdowns and the haptics of the page that is shown, on the framework's controls: a setting's
     * options as they are drawn, and the option the player picks. A stepper goes to the option before and after, by its
     * buttons and by the thumbstick, and a dropdown opens its list over the page, as a haptic does, which has its
     * pattern's shape and a Play button. A choice has the options its store answers, which are given with it.
     * It points into the page that is shown, so it is reset as another page is.
     */
    class Choices
    {
    public:
        void startFrame();
        std::optional<std::size_t> edit(const model::Setting& setting, const std::vector<model::Option>& options, float right, float top, const controls::ChoiceRow& row,
            std::optional<std::size_t> chosen, bool& pulse);
        float draw(const model::Setting& setting, const std::vector<model::Option>& options, float right, float top, const controls::ChoiceRow& row,
            std::optional<std::size_t> chosen, ValueLook look, std::string_view unchosenText = {}) const;
        void endFrame();
        bool isListOpen(const model::Setting& setting) const;

        const model::Setting* listSetting() const
        {
            return _open;
        }

        // the option of the open list the ray is on, and none
        std::optional<std::size_t> pointedOption() const
        {
            return _pointedOption;
        }

        void reset();

    private:
        std::optional<std::size_t> editList(const model::Setting& setting, const std::vector<model::Option>& options, float right, float top, const controls::ChoiceRow& row,
            std::optional<std::size_t> chosen, bool& pulse);

        // the dropdown whose list is open, and none
        const model::Setting* _open = nullptr;

        // the list of that dropdown was open in the frame that is drawn
        bool _openSeen = false;

        // the option of that list the ray is on
        std::optional<std::size_t> _pointedOption;
    };
}
