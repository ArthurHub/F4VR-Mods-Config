#pragma once

#include <string_view>

#include "imgui/controls/Targets.h"

// The kinds of the panel's targets that are the mod's, which a row records with `controls::targets().add` and devbench's
// targets action tells; a button, a dropdown's option, the search field and the title bar's crumbs and buttons are the
// framework's (`controls::target::BUTTON`, `OPTION`, `SEARCH`, `CRUMB`, `TITLE_BUTTON`), and so are the scroll areas of the tree, the page and an open list
// (`TREE_AREA`, `PAGE_AREA`, `LIST_AREA`), which its panes and list record.
namespace mods_config::ui::target
{
    // a mod's row of the tree, or a page's: the key is the mod's name, and the path the names of the pages to it
    inline constexpr std::string_view TREE = "tree";
    // a category's row of the tree, by its name
    inline constexpr std::string_view CATEGORY = "category";
    // a setting's row of the page, by its id
    inline constexpr std::string_view SETTING = "setting";
    // a button's row of the page, by its text
    inline constexpr std::string_view PAGE_BUTTON = "pageButton";
    // a section's heading on the page, by its text
    inline constexpr std::string_view SECTION = "section";
    // the track of a channel of the color editor that is open, by the channel's name
    inline constexpr std::string_view COLOR_CHANNEL = "colorChannel";
}
