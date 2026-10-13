#pragma once

#include <cstdint>
#include <memory>
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
     * A value of a setting, in one of the types a setting's value has.
     */
    using Value = std::variant<bool, int, float, std::string>;

    /**
     * An option of a choice: the value that is stored while it is chosen, and what the player sees.
     */
    struct Option
    {
        Value value;
        std::string text;

        bool operator==(const Option&) const = default;
    };

    /**
     * What the value of a setting can be: the range of a slider, or the options of a stepper and a dropdown.
     */
    struct ValueOptions
    {
        // the least and the most of a slider, and what it moves by
        float min = 0.0f;
        float max = 0.0f;
        float step = 1.0f;

        // in the order they are shown
        std::vector<Option> options;
    };

    /**
     * A setting that is kept nowhere: the store holds its value by itself, while the mod is selected.
     */
    struct NoStorage
    {
        bool operator==(const NoStorage&) const = default;
    };

    /**
     * A global of the game: a form of a game plugin, by the id it has in that plugin.
     */
    struct GlobalStorage
    {
        // the file name of the plugin, as `MyMod.esp`
        std::string plugin;

        // the form's id without the plugin's place in the load order
        std::uint32_t formId = 0;

        bool operator==(const GlobalStorage&) const = default;
    };

    /**
     * A storage the menu does not read: the value of its setting is not available.
     */
    struct UnknownStorage
    {
        // what it is, as its config names it
        std::string type;

        bool operator==(const UnknownStorage&) const = default;
    };

    /**
     * Where the value of a setting is kept. Only the store reads it.
     */
    using Storage = std::variant<NoStorage, GlobalStorage, UnknownStorage>;

    /**
     * What a row is shown by: the value of one setting, or other conditions that are joined. It is asked of
     * the store's values, so a row shows and hides as the player edits.
     */
    struct Condition
    {
        enum class Kind : std::uint8_t
        {
            // its setting is on: a switcher that is on, and any other value that is the number 1
            On,
            // every one of its conditions holds. With none it holds
            All,
            // one of its conditions holds. With none it does not
            Any,
            // its one condition does not hold
            Not,
        };

        Kind kind = Kind::All;

        // the id of the setting of an On
        std::string setting;

        // what an All, an Any and a Not join
        std::vector<Condition> conditions;

        bool operator==(const Condition&) const = default;
    };

    /**
     * A row that edits one value.
     */
    struct Setting
    {
        // its identity, unique in its mod: its author's, or one its loader made
        std::string id;
        SettingType type = SettingType::Switcher;
        std::string text;

        // what the help line says of the row while the ray is on it: plain text on one line, and empty for none
        std::string help;
        ValueOptions valueOptions;
        Storage storage;

        // the row is shown while it holds. A row with none is always shown
        std::optional<Condition> visibleWhen;
    };

    /**
     * A row that runs an action.
     */
    struct Button
    {
        std::string text;
        std::string help;
        std::optional<Condition> visibleWhen;
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
        std::optional<Condition> visibleWhen;
    };

    /**
     * The pixels of an image: four bytes each, as red, green, blue and alpha, in rows from the top one.
     */
    struct ImagePixels
    {
        // what the image is, for the log
        std::string name;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<std::uint8_t> rgba;
    };

    /**
     * A row that shows a picture, in the middle of the rows' width.
     */
    struct Image
    {
        // none for an image that could not be read: the row then says that an image is there
        std::shared_ptr<const ImagePixels> pixels;

        // the size it is shown at, in the pixels of MCM's menu, which the drawing scales to the panel's as it
        // does the size of a text
        float width = 0.0f;
        float height = 0.0f;
        std::optional<Condition> visibleWhen;
    };

    /**
     * Empty space, as high as a number of lines of text.
     */
    struct Spacer
    {
        int lines = 1;
        std::optional<Condition> visibleWhen;
    };

    /**
     * A control the menu does not support: the row is kept, so the page shows that it is there.
     */
    struct Placeholder
    {
        std::string text;
        std::string help;

        // the control's type, as its config names it
        std::string type;
        std::optional<Condition> visibleWhen;
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

        // of the heading alone: an item is shown by its own
        std::optional<Condition> visibleWhen;
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
