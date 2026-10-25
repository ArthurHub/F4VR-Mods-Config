#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "model/ConfigDefinition.h"

namespace mods_config::ui
{
    /**
     * The button of the confirmation that was pressed in a frame, and none.
     */
    enum class ConfirmAnswer : std::uint8_t
    {
        None,
        Confirm,
        RunWithoutSaving,
        Cancel,
    };

    /**
     * The unsaved changes of a button's mod, which its confirmation can save before it runs: how many, and how many
     * of them are invalid, which cannot be saved.
     */
    struct SaveFirst
    {
        std::size_t count = 0;
        std::size_t invalid = 0;
    };

    ConfirmAnswer drawConfirmDialog(const model::Button* button, const SaveFirst* saveFirst = nullptr);
}
