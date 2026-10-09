#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

// The definition of the settings model: what a mod's menu holds, whatever it comes from. A loader fills it once,
// and the UI draws it. See docs/design/model.md.

namespace mods_config::model
{
    /**
     * How a setting is edited, by MCM's names.
     */
    enum class SettingType : std::uint8_t
    {
        Switcher,
        Slider,
        Stepper,
        Dropdown,
        TextInput,
    };

    /**
     * A row that edits one value.
     */
    struct Setting
    {
        SettingType type = SettingType::Switcher;
        std::string text;
    };

    /**
     * A row that runs an action.
     */
    struct Button
    {
        std::string text;
    };

    struct Text
    {
        std::string text;
    };

    struct Image
    {};

    struct Spacer
    {};

    /**
     * A control the menu does not support: the row is kept, so the page shows that it is there.
     */
    struct Placeholder
    {
        std::string text;

        // the control's type, as its config names it
        std::string type;
    };

    struct Group;

    using Item = std::variant<Setting, Group, Button, Text, Image, Spacer, Placeholder>;

    /**
     * A section: its heading and the items under it.
     */
    struct Group
    {
        std::string text;
        std::vector<Item> items;
    };

    struct Page
    {
        // empty for a mod's front page
        std::string name;
        std::vector<Item> items;
    };

    struct Mod
    {
        // the mod's identity: the modName of its config
        std::string name;
        std::string displayName;
        Page frontPage;
        std::vector<Page> pages;
    };
}
