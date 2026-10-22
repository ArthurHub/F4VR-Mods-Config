#pragma once

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
        Cancel,
    };

    ConfirmAnswer drawConfirmDialog(const model::Button* button);
}
