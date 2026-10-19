#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
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
     * What a storage keeps its value as, by the `sourceType` of an MCM row.
     */
    enum class StoredType : std::uint8_t
    {
        Bool,
        Int,
        Float,
        String,
    };

    /**
     * A ModSetting of MCM: a value MCM keeps for a mod, in the mod's settings file.
     */
    struct ModSettingStorage
    {
        using Type = StoredType;

        // as MCM names it, by its key and its section in the file: `fStrength:Main`
        std::string name;
        Type type = Type::Bool;

        bool operator==(const ModSettingStorage&) const = default;
    };

    /**
     * A property of a script: a value a Papyrus script keeps, on a form of a game plugin the script is
     * attached to. The game keeps it in the save.
     */
    struct PropertyStorage
    {
        using Type = StoredType;

        // the file name of the form's plugin, as `MyMod.esp`
        std::string plugin;

        // the form's id without the plugin's place in the load order
        std::uint32_t formId = 0;

        // the script that has the property. Empty for any script of the form
        std::string script;

        // the name of the property
        std::string name;
        Type type = Type::Bool;

        bool operator==(const PropertyStorage&) const = default;
    };

    /**
     * A value in a file of a package's mod: a key of an INI file, or a value of a TOML or a JSON file.
     */
    struct FileStorage
    {
        // the name its package gives the file, which is a file of its mod
        std::string file;

        // the names that lead to the value: its section and its key in an INI file, and in a TOML or a JSON
        // file the tables it is in and its key
        std::vector<std::string> path;

        bool operator==(const FileStorage&) const = default;
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
    using Storage = std::variant<NoStorage, GlobalStorage, ModSettingStorage, PropertyStorage, FileStorage, UnknownStorage>;

    /**
     * When a change of a package's setting takes effect in the running mod.
     */
    enum class Applies : std::uint8_t
    {
        // as it is made
        Live,
        // after Save
        Save,
        // after the game is restarted
        Restart,
    };

    /**
     * What a row is shown or enabled by: the value of one setting, or other conditions that are joined. It is
     * asked of the store's values, so a row shows and hides as the player edits.
     */
    struct Condition
    {
        enum class Kind : std::uint8_t
        {
            // its setting is on: a switcher that is on, and any other value that is the number 1
            On,
            // its setting's value is its value, or is not. A number is its value when they are as near as a
            // number and the text of it in a file
            Equals,
            NotEquals,
            // its setting's value is a number below or above its value
            LessThan,
            LessThanOrEqual,
            GreaterThan,
            GreaterThanOrEqual,
            // its setting's value is one of its values, or is none of them
            OneOf,
            NotOneOf,
            // every one of its conditions holds. With none it holds
            All,
            // one of its conditions holds. With none it does not
            Any,
            // its one condition does not hold
            Not,
        };

        Kind kind = Kind::All;

        // the id of the setting of an On and of a comparison
        std::string setting;

        // what an All, an Any and a Not join
        std::vector<Condition> conditions;

        // an On whose setting no row shows, as MCM's hidden switcher: its value that is not available is
        // off, as MCM's menu reads it, where any other leaves the condition open
        bool offWhenNotAvailable = false;

        // what a comparison compares its setting's value with: one value, or the list of a OneOf and a NotOneOf
        std::vector<Value> values;

        bool operator==(const Condition&) const = default;
    };

    /**
     * A row with no action: nothing runs.
     */
    struct NoAction
    {
        bool operator==(const NoAction&) const = default;
    };

    /**
     * An argument of a call that has the value of its row in it: the value a setting is set to.
     */
    struct ValueArgument
    {
        // its place among the arguments of its call
        std::size_t place = 0;

        // the text the value is put in, in place of its first `{value}`
        std::string text;

        // what the text is given to the script as
        StoredType type = StoredType::String;

        bool operator==(const ValueArgument&) const = default;
    };

    /**
     * A call of a function of a Papyrus script, on a form of a game plugin the script is attached to. One
     * with no form, an empty plugin, calls a global function of the script it names.
     */
    struct CallFunction
    {
        // the file name of the form's plugin, as `MyMod.esp`. Empty for a global function
        std::string plugin;

        // the form's id without the plugin's place in the load order
        std::uint32_t formId = 0;

        // the script that has the function. Empty for the form's first script, which a global function does not have
        std::string script;

        // the name of the function
        std::string function;

        // what the function is called with, each in the type the script gets it in
        std::vector<Value> arguments;

        // those of them that have the value of the row in them, which the store fills as the value is set.
        // None for a button
        std::vector<ValueArgument> valueArguments;

        bool operator==(const CallFunction&) const = default;
    };

    /**
     * An action the menu does not run: a button of it is disabled, and a value row is set with nothing run.
     */
    struct UnknownAction
    {
        // what it is, as its config names it
        std::string type;

        // what the help line of its button says of why it is not run
        std::string reason;

        bool operator==(const UnknownAction&) const = default;
    };

    /**
     * What a row runs. Only the store runs it.
     */
    using Action = std::variant<NoAction, CallFunction, UnknownAction>;

    /**
     * A piece of a line that is drawn in one size, one color and one face.
     */
    struct TextRun
    {
        std::string text;

        // the size of its font as its config has it, which is a size of MCM's menu. A run without one is in the
        // size of the text it is in
        std::optional<int> size;

        // as 0xRRGGBB. A run without one is in the color of the text it is in
        std::optional<std::uint32_t> color;

        // inside a <b>, an <i> and a <u> of its text
        bool bold = false;
        bool italic = false;
        bool underline = false;

        bool operator==(const TextRun&) const = default;
    };

    /**
     * A row that edits one value.
     */
    struct Setting
    {
        // its identity, unique in its mod: its author's, or one its loader made
        std::string id;

        // the id its source has for it, which MCM's events name an MCM row by. Empty for a row with none
        std::string sourceId;
        SettingType type = SettingType::Switcher;

        // as plain text on one line, as the help line shows it
        std::string text;

        // the text's pieces, with the size, the color and the face its tags give them, on one line. Empty for a
        // text whose tags give it none of them, which is drawn as its plain text
        std::vector<TextRun> textRuns;

        // what the help line says of the row while the ray is on it: plain text on one line, and empty for none
        std::string help;
        ValueOptions valueOptions;
        Storage storage;

        // what its value is kept as. An MCM row has it for a text input alone, which takes a text, a whole
        // number or a number: the value of every other is of its type of control
        StoredType valueType = StoredType::String;

        // what runs after its value is set, with the value. Most have none
        Action action;

        // the row is shown while it holds. A row with none is always shown
        std::optional<Condition> visibleWhen;

        // the row is edited while it holds, and is disabled while it does not. A row with none is always edited
        std::optional<Condition> enabledWhen;

        // when its change takes effect: its own, or else the nearest its package gives above it. None for an
        // MCM row
        std::optional<Applies> applies;

        // the value Reset gives it, in its value's type: a package's setting has it. None for an MCM row,
        // whose default is its store's to answer
        std::optional<Value> defaultValue;
    };

    /**
     * A row that runs an action.
     */
    struct Button
    {
        std::string text;
        std::vector<TextRun> textRuns;
        std::string help;
        Action action;
        std::optional<Condition> visibleWhen;
    };

    /**
     * A row that shows the key a hotkey of the mod is bound to. The menu does not bind it and does not run
     * it: its source does, as MCM does for an MCM mod.
     */
    struct Hotkey
    {
        // what its source knows the hotkey by. Empty for a row with none, which has no key
        std::string id;
        std::string text;
        std::vector<TextRun> textRuns;
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
     * A row that shows a picture, in the middle of the rows' width: the pixels an MCM image is read into, or a
     * package's texture, which the game loads.
     */
    struct Image
    {
        // the pixels of an MCM image. None for a package's, and for an image that could not be read and has no
        // texture: the row then says that an image is there
        std::shared_ptr<const ImagePixels> pixels;

        // a package's texture, by its path under `Data/Textures`, as `MyMod/config/logo.dds`. Empty for an MCM
        // image
        std::string texture;

        // the size it is shown at. Of pixels in the pixels of MCM's menu, which the drawing scales to the
        // panel's as it does the size of a text. Of a texture in the panel's pixels, and 0 for a side its
        // package does not give
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
        std::vector<TextRun> textRuns;
        std::string help;

        // the control's type, as its config names it
        std::string type;
        std::optional<Condition> visibleWhen;
    };

    struct Group;

    using Item = std::variant<Setting, Group, Button, Hotkey, Text, Image, Spacer, Placeholder>;

    /**
     * A section: its heading and the items under it. A package's sections nest: a group can be an item of a
     * group.
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

        // what the help line says of its row in the tree: empty for none, as an MCM page has
        std::string help;
        std::vector<Item> items;

        // the pages under it, which a package's page can have
        std::vector<Page> pages;
    };

    /**
     * What a file of a package is written in.
     */
    enum class FileFormat : std::uint8_t
    {
        Ini,
        Json,
        Toml,
    };

    /**
     * The folder a path of a package's file is from.
     */
    enum class FileRoot : std::uint8_t
    {
        // the game's Data folder
        Data,
        // `Documents\My Games\Fallout4VR`
        MyGames,
    };

    /**
     * A file a package's settings are kept in.
     */
    struct File
    {
        FileFormat format = FileFormat::Toml;
        FileRoot root = FileRoot::MyGames;

        // from its root, with `/` between its folders, as its package writes it
        std::string path;

        bool operator==(const File&) const = default;
    };

    /**
     * Where the menu of a mod comes from: the store of its values is of the same source.
     */
    enum class Source : std::uint8_t
    {
        Mcm,
        Package,
    };

    struct Mod
    {
        // the mod's identity: the modName of its config
        std::string name;
        std::string displayName;
        Page frontPage;
        std::vector<Page> pages;

        // why the mod is not available, as `its plugin SS2.esm is not loaded`: empty for one that is. A mod
        // that is not available has its names and this, and no pages
        std::string notAvailable;

        // what the help line says of its row in the tree: empty for none, as an MCM mod has
        std::string help;
        Source source = Source::Mcm;

        // a package's files, by the names it gives them. None for an MCM mod
        std::map<std::string, File> files;
    };
}
