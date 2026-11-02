#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace mods_config::ui
{
    /**
     * What the dialog asks about: the mod with unsaved changes, how many it has and how many of them are
     * invalid, what the player was about to do, as `close the menu`, and whether they are made already, as an MCM
     * mod's are, so the dialog asks to keep or revert them.
     */
    struct UnsavedChanges
    {
        std::string_view mod;
        std::size_t count = 0;
        std::size_t invalid = 0;
        std::string next;
        bool made = false;
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
