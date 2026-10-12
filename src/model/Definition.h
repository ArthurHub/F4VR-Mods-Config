#pragma once

#include <cstdint>
#include <optional>
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

    enum class TextAlign : std::uint8_t
    {
        Left,
        Center,
        Right,
    };

    /**
     * A piece of a line that is drawn in one size and one color.
     */
    struct TextRun
    {
        std::string text;

        // the size of its font as its config has it, which is a size of MCM's menu. A run without one is in the
        // size of the text it is in
        std::optional<int> size;

        // as 0xRRGGBB. A run without one is in the color of the text it is in
        std::optional<std::uint32_t> color;

        bool operator==(const TextRun&) const = default;
    };

    /**
     * A line of a text, which is wrapped where it is wider than its room. A line with no runs is an empty one.
     */
    struct TextLine
    {
        TextAlign align = TextAlign::Left;
        std::vector<TextRun> runs;

        bool operator==(const TextLine&) const = default;
    };

    /**
     * A row of text that holds no value: its lines, with the look its tags gave them.
     */
    struct Text
    {
        std::vector<TextLine> lines;
    };

    struct Image
    {};

    /**
     * Empty space, as high as a number of lines of text.
     */
    struct Spacer
    {
        int lines = 1;
    };

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
