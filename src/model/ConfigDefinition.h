#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
     * Where a choice reads its options in the game, for a list its author does not know: a FormList, whose
     * options are the names of its forms, or a property of a script whose value is an array of texts. Its
     * store reads them as its mod is selected.
     */
    struct GameOptions
    {
        // the file name of the plugin of the FormList, or of the form the script is attached to, as `MyMod.esp`
        std::string plugin;

        // the form's id without the plugin's place in the load order
        std::uint32_t formId = 0;

        // the script that has the property. Empty for any script of the form
        std::string script;

        // the name of the property. Empty for a FormList
        std::string property;

        // the value of an option is its text. Else it is its place in the list, as MCM stores it
        bool storesText = false;

        bool operator==(const GameOptions&) const = default;
    };

    /**
     * What the value of a setting can be: the range of a slider and of a text input of a number, the length of
     * a text input of a text, or the options of a stepper and a dropdown.
     */
    struct ValueOptions
    {
        // the least and the most of a slider, and what it moves by
        float min = 0.0f;
        float max = 0.0f;
        float step = 1.0f;

        // the fewest and the most characters of a text input of a text, and 0 for no limit. A config spec's
        // `allowEmpty` of false is a least of 1
        int minLength = 0;
        int maxLength = 0;

        // in the order they are shown. None for a choice that reads them in the game or from its mod's client
        std::vector<Option> options;

        // where a choice reads its options in the game. None for one whose options are these
        std::optional<GameOptions> gameOptions;

        // a config spec's choice whose options its mod's client gives, by a callback it registers for the setting's
        // id (`listFromClientCallback`)
        bool clientOptions = false;
    };

    /**
     * The length of a text in characters, as the limits of a text input count it: a character of UTF-8 is
     * one, whatever its bytes.
     */
    inline int textLength(const std::string_view text)
    {
        return static_cast<int>(std::ranges::count_if(text, [](const char c) {
            return (static_cast<unsigned char>(c) & 0xC0) != 0x80;
        }));
    }

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
     * attached to. The game keeps it in the save. A config spec's is in the type of its setting's value.
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
     * A value in a file of a config spec's mod: a key of an INI file, or a value of a TOML or a JSON file.
     */
    struct FileStorage
    {
        // the name its config spec gives the file, which is a file of its mod
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
     * When a change of a config spec's setting takes effect in the running mod.
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

        // its `setting` names a state of its mod's client in place of a setting: a config spec's `clientState`
        bool clientState = false;

        bool operator==(const Condition&) const = default;
    };

    /**
     * A warning of a setting or a button: a text the row tells while its condition holds. It blocks nothing.
     */
    struct Warning
    {
        Condition when;

        // what the help line says before the row's help
        std::string text;
    };

    /**
     * A row with no action: nothing runs.
     */
    struct NoAction
    {
        bool operator==(const NoAction&) const = default;
    };

    /**
     * An argument of a call that has the value of its row in it: the value a setting is set to. A config spec's is
     * a parameter that is exactly `{value}`, which is the value itself, in its setting's type.
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
     * A line of the game's console, which the game runs as the player would type it.
     */
    struct ConsoleCommand
    {
        std::string command;

        bool operator==(const ConsoleCommand&) const = default;
    };

    // A web link opened in the default desktop browser.
    struct WebLink
    {
        std::string url;

        bool operator==(const WebLink&) const = default;
    };

    /**
     * A parameter of a function of a mod's DLL: its name, and its value in its own type.
     */
    struct NamedArgument
    {
        std::string name;
        Value value;

        bool operator==(const NamedArgument&) const = default;
    };

    /**
     * A call of a function that a config spec's mod's DLL registered with the native API, by its name.
     */
    struct NativeFunction
    {
        std::string function;
        std::vector<NamedArgument> arguments;

        // the places of the arguments that are the value a setting is written with, as Save runs its action.
        // None for a button
        std::vector<std::size_t> valueArguments;

        bool operator==(const NativeFunction&) const = default;
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
    using Action = std::variant<NoAction, CallFunction, ConsoleCommand, WebLink, NativeFunction, UnknownAction>;

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
     * How a number is shown, in its control and in the help line: the value that is kept is the number itself.
     */
    struct NumberDisplay
    {
        // the decimals it is written with. None for as many as the step of its slider has
        std::optional<int> decimals;

        // a text after the number, as `m` or `°`. Empty for none
        std::string unit;

        // it is written as a percentage of its range, from 0% at its least to 100% at its most
        bool percent = false;

        bool operator==(const NumberDisplay&) const = default;
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

        // what runs after its value is set, with the value: an MCM row's as it is set, and a config spec's as Save
        // writes it. Most have none
        Action action;

        // the row is shown while it holds. A row with none is always shown
        std::optional<Condition> visibleWhen;

        // the row is edited while it holds, and is disabled while it does not. A row with none is always edited
        std::optional<Condition> enabledWhen;

        // when its change takes effect: its own, or else the nearest its config spec gives above it. None for an
        // MCM row
        std::optional<Applies> applies;

        // the value Reset gives it, in its value's type: a config spec's setting has it. None for an MCM row,
        // whose default is its store's to answer
        std::optional<Value> defaultValue;

        // how its number is shown: a config spec's slider and text input of a number can have it
        NumberDisplay display;

        // its value is shown and not edited, and it has no Reset: a config spec's setting can be
        bool readOnly = false;

        // the row warns of the first whose condition holds: a config spec's setting can have them
        std::vector<Warning> warnings;
    };

    /**
     * What a config spec's button asks before it runs: a dialog with a title, a message, and the text of the button
     * that runs it, which is empty for the button's own text. A dangerous one is red.
     */
    struct Confirmation
    {
        std::string title;
        std::string message;
        std::string confirm;
        bool dangerous = false;
    };

    /**
     * A row that runs an action.
     */
    struct Button
    {
        std::string text;
        std::vector<TextRun> textRuns;
        std::string help;

        // the text on its button: a config spec's button can have one. Empty for `Run`
        std::string buttonText;
        Action action;
        std::optional<Condition> visibleWhen;

        // it runs while it holds, and is disabled while it does not: a config spec's button can have it
        std::optional<Condition> enabledWhen;

        // as a setting's: a config spec's button can have them
        std::vector<Warning> warnings;

        // what it asks before it runs: a config spec's button can have it
        std::optional<Confirmation> confirmation;
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
     * config spec's texture, which the game loads.
     */
    struct Image
    {
        // the pixels of an MCM image. None for a config spec's, and for an image that could not be read and has no
        // texture: the row then says that an image is there
        std::shared_ptr<const ImagePixels> pixels;

        // a config spec's texture, by its path under `Data/Textures`, as `MyMod/config/logo.dds`. Empty for an MCM
        // image
        std::string texture;

        // the size it is shown at. Of pixels in the pixels of MCM's menu, which the drawing scales to the
        // panel's as it does the size of a text. Of a texture in the panel's pixels, and 0 for a side its
        // config spec does not give
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
     * A section: its heading and the items under it. A config spec's sections nest: a group can be an item of a
     * group, and a config spec's section can fold.
     */
    struct Group
    {
        std::string text;

        // what the help line says of its heading: empty for none, as an MCM section has
        std::string help;
        std::vector<Item> items;

        // of the heading alone: an item is shown by its own
        std::optional<Condition> visibleWhen;

        // a press on its heading hides its items, and another shows them again
        bool collapsible = false;

        // it starts folded each time its page is shown. Only a section that folds has it
        bool collapsed = false;
    };

    struct Page
    {
        // empty for a mod's front page
        std::string name;

        // what the help line says of its row in the tree: empty for none, as an MCM page has
        std::string help;
        std::vector<Item> items;

        // the pages under it, which a config spec's page can have
        std::vector<Page> pages;

        // what its mod's client is told it by as it is shown and left: a config spec's page can have it. Empty for none
        std::string id;

        // includes its parent pages' conditions; a hidden page hides its subtree
        std::optional<Condition> visibleWhen;

        // a disabled page stays in the tree but cannot be opened
        std::optional<Condition> enabledWhen;
    };

    /**
     * What a file of a config spec is written in.
     */
    enum class FileFormat : std::uint8_t
    {
        Ini,
        Json,
        Toml,
    };

    /**
     * The folder a path of a config spec's file is from.
     */
    enum class FileRoot : std::uint8_t
    {
        // the game's Data folder
        Data,
        // `Documents\My Games\Fallout4VR`
        MyGames,
    };

    /**
     * A file a config spec's settings are kept in.
     */
    struct File
    {
        FileFormat format = FileFormat::Toml;
        FileRoot root = FileRoot::MyGames;

        // from its root, with `/` between its folders, as its config spec writes it
        std::string path;

        bool operator==(const File&) const = default;
    };

    /**
     * Where the menu of a mod comes from: the store of its values is of the same source.
     */
    enum class Source : std::uint8_t
    {
        Mcm,
        ConfigSpec,
    };

    /**
     * How much a problem of a config spec matters: an error leaves something out, the whole config spec or a part of
     * it, and a warning is read, but is likely not what its author meant.
     */
    enum class Severity : std::uint8_t
    {
        Error,
        Warning,
    };

    /**
     * Where a row is in its mod: the page it is on, as a selection names a page, empty for the mod's front page,
     * and its place among the page's items, then among the items of each section on the way to it.
     */
    struct RowPlace
    {
        std::vector<std::size_t> page;
        std::vector<std::size_t> item;

        bool operator==(const RowPlace&) const = default;
    };

    /**
     * A problem of a config spec, for its author: what is wrong, what the menu did with it, and where it is.
     */
    struct Problem
    {
        Severity severity = Severity::Error;

        // the text of the row it is on, the name of its page, or `spec.json` for the config spec itself
        std::string row;

        // what is wrong, and what the menu did with it, as `It has no storage: it is not available.`
        std::string message;

        // the pages and the row it is on, as `Beam › Beam angle`. Empty for a problem of the config spec itself
        std::string where;

        // where it is in spec.json, as `pages[0].content[4]`. Empty for the file as a whole
        std::string path;

        // the row it is on, which the menu can show. None for a problem of a page or of the config spec itself
        std::optional<RowPlace> place;

        bool operator==(const Problem&) const = default;
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

        // a config spec's files, by the names it gives them. None for an MCM mod
        std::map<std::string, File> files;

        // a config spec's problems, in the order of its spec.json. None for an MCM mod
        std::vector<Problem> problems;

        // the folder of a config spec's spec.json, in `Data\ModsConfigVR\Config`. Empty for an MCM mod, and for a
        // config spec that was read alone
        std::string folder;

        // the live copy a config spec's spec.json is read from, as its author's report says where it is. Empty
        // while it is read from its folder in `Data\ModsConfigVR\Config`
        std::string liveCopy;
    };
}
