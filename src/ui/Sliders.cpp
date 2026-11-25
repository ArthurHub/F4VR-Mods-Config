#include "Sliders.h"

#include <imgui_internal.h>

#include "Numbers.h"

namespace mods_config::ui
{
    /**
     * The range of a setting's slider: its least, its most and its step.
     */
    controls::SliderRange sliderRange(const model::Setting& setting)
    {
        const auto& options = setting.valueOptions;
        return { .min = options.min, .max = options.max, .step = options.step };
    }

    /**
     * The id the framework keeps a setting's slider's state by: of the setting itself, which stays while its page is
     * shown, and not of ImGui's id stack, so its edit and its draw have the same one wherever each is called.
     */
    ImGuiID sliderId(const model::Setting& setting)
    {
        const auto* address = &setting;
        return ImHashData(&address, sizeof(address));
    }

    /**
     * The text of a slider's value in the box of its value, as its display writes it: with its unit, smaller, or the
     * number alone, so the help line has its unit. A number is never shortened.
     */
    controls::ValueText valueBoxText(const model::Setting& setting, const float value)
    {
        return controls::fitValueText(numberText(setting, value), numberText(setting, value, false));
    }
}
