#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace mods_config::ui
{
    /**
     * What the dialog asks about: the mod with unsaved changes, how many it has and how many of them are
     * invalid, and what the player was about to do, as `close the menu`.
     */
    struct UnsavedChanges
    {
        std::string_view mod;
        std::size_t count = 0;
        std::size_t invalid = 0;
        std::string next;
    };

    /**
     * The button of the dialog that was pressed in a frame, and none.
     */
    enum class DialogAnswer : std::uint8_t
    {
        None,
        Save,
        Discard,
        Cancel,
    };

    DialogAnswer drawUnsavedDialog(const UnsavedChanges* changes);
}
