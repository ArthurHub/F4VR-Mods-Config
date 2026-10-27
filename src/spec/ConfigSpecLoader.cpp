#include "ConfigSpecLoader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "IniFile.h"
#include "TomlFile.h"
#include "common/CommonUtils.h"
#include "model/InputBindings.h"
#include "model/TextMarkup.h"
#include "vrcf/InputBindingParser.h"

using namespace mods_config;

namespace
{
    using Json = nlohmann::json;

    using spec::CONFIG_SPEC_FOLDER;

    // the most lines a spacer is high, whatever its config spec says, as in an MCM config
    constexpr float MAX_SPACER_LINES = 20.0f;

    /**
     * The text of a key. It is empty when the value has no such key, or the key holds something else than a text.
     */
    std::string getText(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_string() ? found->get<std::string>() : std::string();
    }

    /**
     * A text of a key that the player sees, in the game's language. It is plain text, a `$` token, or a token
     * with the text that is shown where the language has none, as `{ "token": "$MyMod_Height", "fallback":
     * "Height" }`. Empty for a key that is none of them.
     */
    std::string getShownText(const Json& value, const char* key, const model::Translations& translations)
    {
        const auto found = value.find(key);
        if (found == value.end()) {
            return {};
        }
        if (found->is_string()) {
            return translations.translate(found->get<std::string>());
        }
        return found->is_object() ? translations.translate(getText(*found, "token"), getText(*found, "fallback")) : std::string();
    }

    /**
     * A number of a key, or else the given one.
     */
    float getNumber(const Json& value, const char* key, const float otherwise)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_number() ? found->get<float>() : otherwise;
    }

    /**
     * A length of a key: a whole number above 0, or else 0, which is no limit.
     */
    int getLength(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_number_integer() ? (std::max)(found->get<int>(), 0) : 0;
    }

    /**
     * Whether a key is true. False for a value with no such key, or with one that holds something else than a bool.
     */
    bool getBool(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_boolean() && found->get<bool>();
    }

    // what a problem of the config spec itself names
    constexpr auto SPEC_FILE = "spec.json";

    // what stands between the pages and the row a problem is on: U+203A, as its bytes in UTF-8
    constexpr auto WHERE_SEPARATOR = " \xE2\x80\xBA ";

    // the most characters of a row's name in a problem: a text row is a paragraph
    constexpr std::size_t MAX_ROW_NAME = 48;

    /**
     * What a condition knows of the setting it names: its id as its config spec has it, and what its value is.
     */
    struct SettingInfo
    {
        std::string id;
        model::SettingType type = model::SettingType::Switcher;
        model::StoredType valueType = model::StoredType::Bool;
    };

    /**
     * Where the reading of a config spec is: the path in spec.json, the names of the pages on the way to the page,
     * and the name of the row, which a problem names.
     */
    struct Location
    {
        std::string path;
        std::vector<std::string> pages;
        std::string row;

        // the places of the pages on the way to the page, and of the row among its items and those of its sections
        std::vector<std::size_t> pagePlaces;
        std::vector<std::size_t> itemPlaces;
    };

    /**
     * What a config spec is read with: the game's translations, its files, its shared lists, the plugins its
     * conditions ask of, and its settings so far by their ids in lower case, as ids are compared without
     * regard to case. And what it found wrong so far, and where it is.
     */
    struct ConfigSpecReading
    {
        const model::Translations& translations;
        std::map<std::string, model::File> files;

        // the `sharedLists` of its root. None for a config spec with none
        const Json* sharedLists = nullptr;
        const spec::PluginChecks& plugins;

        // the files of the config spec that are there, read as they are now, by their names: a setting's path that
        // one of them does not have is reported
        std::map<std::string, std::unique_ptr<spec::ModFile>> fileValues;
        std::unordered_map<std::string, SettingInfo> settings;
        std::vector<model::Problem> problems;
        Location at;

        /**
         * Adds a problem where the reading is: on its row, with the row's place, or else on its page, or else of the
         * config spec itself.
         */
        void report(const model::Severity severity, std::string message)
        {
            std::string where;
            for (const auto& page : at.pages) {
                where += (where.empty() ? "" : WHERE_SEPARATOR) + page;
            }
            if (!at.row.empty()) {
                where += (where.empty() ? "" : WHERE_SEPARATOR) + at.row;
            }
            auto row = !at.row.empty() ? at.row : !at.pages.empty() ? at.pages.back() : std::string(SPEC_FILE);
            auto place = !at.row.empty() ? std::optional(model::RowPlace{ .page = at.pagePlaces, .item = at.itemPlaces }) : std::nullopt;
            problems.push_back(
                { .severity = severity, .row = std::move(row), .message = std::move(message), .where = std::move(where), .path = at.path, .place = std::move(place) });
        }
    };

    /**
     * A part of a config spec that is read: while it lives, the reading is at a key of spec.json, or at an element
     * of a list, of the key or of the list it is at with no key, and it is back where it was as it ends. A page or a row that is set while it lives is left
     * as it ends too.
     */
    class Into
    {
    public:
        Into(ConfigSpecReading& reading, const std::string_view key)
            : _reading(reading),
              _before(reading.at)
        {
            if (!key.empty()) {
                auto& path = reading.at.path;
                path += (path.empty() ? "" : ".") + std::string(key);
            }
        }

        Into(ConfigSpecReading& reading, const std::string_view key, const std::size_t place)
            : Into(reading, key)
        {
            reading.at.path += std::format("[{}]", place);
        }

        ~Into()
        {
            _reading.at = std::move(_before);
        }

        Into(const Into&) = delete;
        Into& operator=(const Into&) = delete;

    private:
        ConfigSpecReading& _reading;
        Location _before;
    };

    /**
     * How many single characters have to change to make one name the other, without regard to case.
     */
    std::size_t editDistance(const std::string_view from, const std::string_view to)
    {
        std::vector<std::size_t> row(to.size() + 1);
        for (std::size_t j = 0; j < row.size(); ++j) {
            row[j] = j;
        }
        for (std::size_t i = 1; i <= from.size(); ++i) {
            std::size_t diagonal = row[0];
            row[0] = i;
            for (std::size_t j = 1; j <= to.size(); ++j) {
                const std::size_t above = row[j];
                const bool same = std::tolower(static_cast<unsigned char>(from[i - 1])) == std::tolower(static_cast<unsigned char>(to[j - 1]));
                row[j] = (std::min)({ row[j] + 1, row[j - 1] + 1, diagonal + (same ? 0 : 1) });
                diagonal = above;
            }
        }
        return row[to.size()];
    }

    /**
     * Reports each field of an object that is none of the ones it can have, which is not read: a typo, or a
     * field of a newer schema. A field near one it can have names it, as `Did you mean "visibleWhen"?`.
     */
    void checkFields(const Json& object, const std::span<const std::string_view> known, ConfigSpecReading& reading)
    {
        if (!object.is_object()) {
            return;
        }
        for (const auto& [key, value] : object.items()) {
            if (std::ranges::find(known, key) != known.end()) {
                continue;
            }
            const auto nearest = (std::ranges::min)(known, {}, [&key](const std::string_view field) {
                return editDistance(key, field);
            });
            const auto distance = editDistance(key, nearest);
            auto message = std::format("Unknown field \"{}\": it is not read.", key);
            if (distance <= 2 && distance * 2 < key.size()) {
                message += std::format(" Did you mean \"{}\"?", nearest);
            }
            const Into into(reading, key);
            reading.report(model::Severity::Warning, std::move(message));
        }
    }

    // The fields each object of a config spec can have, as the schema has them (docs/design/schema.md). Any other is
    // reported as one the menu does not know. A field the schema has and the menu does not read yet is known.
    // A text the player sees, written as an object:
    constexpr std::array<std::string_view, 2> TEXT_FIELDS{ "token", "fallback" };
    // the config spec's root:
    constexpr std::array<std::string_view, 14> ROOT_FIELDS{ "minSchemaVersion",
        "modName",
        "displayName",
        "help",
        "category",
        "specVersion",
        "pluginRequirements",
        "f4sePluginRequirements",
        "files",
        "defaultFile",
        "applies",
        "sharedLists",
        "content",
        "pages" };
    // a file of its `files`:
    constexpr std::array<std::string_view, 4> FILE_FIELDS{ "format", "root", "path", "pathFromClientState" };
    // a page:
    constexpr std::array<std::string_view, 9> PAGE_FIELDS{ "pageDisplayName", "help", "id", "content", "pages", "defaultFile", "applies", "visibleWhen", "enabledWhen" };
    // a row, by its type:
    constexpr std::array<std::string_view, 10>
        SECTION_FIELDS{ "type", "text", "help", "content", "isCollapsible", "collapsed", "defaultFile", "applies", "visibleWhen", "enabledWhen" };
    constexpr std::array<std::string_view, 16> SETTING_FIELDS{ "type",
        "id",
        "text",
        "help",
        "html",
        "valueType",
        "default",
        "valueOptions",
        "display",
        "storage",
        "readOnly",
        "warnings",
        "action",
        "applies",
        "visibleWhen",
        "enabledWhen" };
    constexpr std::array<std::string_view, 10> BUTTON_FIELDS{ "type", "text", "help", "html", "buttonText", "action", "confirmation", "warnings", "visibleWhen", "enabledWhen" };
    constexpr std::array<std::string_view, 4> TEXT_ROW_FIELDS{ "type", "text", "html", "visibleWhen" };
    constexpr std::array<std::string_view, 3> SPACER_FIELDS{ "type", "numLines", "visibleWhen" };
    constexpr std::array<std::string_view, 5> IMAGE_FIELDS{ "type", "texture", "width", "height", "visibleWhen" };
    // a setting's parts:
    constexpr std::array<std::string_view, 15> VALUE_OPTIONS_FIELDS{ "min",
        "max",
        "step",
        "options",
        "sharedOptions",
        "listFromForm",
        "listFromProperty",
        "listFromClientCallback",
        "allowEmpty",
        "minLength",
        "maxLength",
        "suppress",
        "hands",
        "types",
        "buttons" };
    constexpr std::array<std::string_view, 4> OPTION_FIELDS{ "value", "text", "help", "shortText" };
    constexpr std::array<std::string_view, 3> LIST_PROPERTY_FIELDS{ "form", "scriptName", "propertyName" };
    constexpr std::array<std::string_view, 3> DISPLAY_FIELDS{ "decimals", "unit", "percent" };
    constexpr std::array<std::string_view, 8> STORAGE_FIELDS{ "file", "section", "key", "path", "global", "form", "scriptName", "propertyName" };
    constexpr std::array<std::string_view, 2> WARNING_FIELDS{ "when", "text" };
    // an action, of a button or a setting, and a button's confirmation:
    constexpr std::array<std::string_view, 8> ACTION_FIELDS{ "type", "function", "params", "form", "scriptName", "script", "command", "url" };
    constexpr std::array<std::string_view, 4> CONFIRMATION_FIELDS{ "title", "message", "confirm", "dangerous" };

    /**
     * A text the player sees, where the reading is, as `getShownText` reads one. A token that the language has
     * no text for, and that has no fallback, is shown as it is written, and is reported.
     */
    std::string readShownText(const Json& text, ConfigSpecReading& reading)
    {
        if (!text.is_string() && !text.is_object()) {
            return {};
        }
        checkFields(text, TEXT_FIELDS, reading);
        const auto token = text.is_string() ? text.get<std::string>() : getText(text, "token");
        const auto fallback = text.is_object() ? getText(text, "fallback") : std::string();
        if (token.starts_with('$') && fallback.empty() && !reading.translations.has(token)) {
            reading.report(model::Severity::Warning, std::format("The token {} has no translation and no fallback: it is shown as it is written.", token));
        }
        return reading.translations.translate(token, fallback);
    }

    /**
     * A text of a key that the player sees, which `readShownText` reads. Empty for a key that has none.
     */
    std::string readShownText(const Json& value, const char* key, ConfigSpecReading& reading)
    {
        const auto found = value.find(key);
        if (found == value.end()) {
            return {};
        }
        const Into into(reading, key);
        return readShownText(*found, reading);
    }

    /**
     * The name of a row as a problem names it: its text as the player sees it, on one line with no tags and
     * shortened, or its type for a row with no text, as a spacer.
     */
    std::string rowName(const Json& row, const model::Translations& translations)
    {
        auto text = common::trim(model::removeMarkup(getShownText(row, "text", translations)));
        if (text.empty()) {
            text = getText(row, "type");
            return text.empty() ? "A row"s : text;
        }
        if (text.size() > MAX_ROW_NAME) {
            text = common::trim(text.substr(0, MAX_ROW_NAME - 3)) + "...";
        }
        return text;
    }

    /**
     * A row's translated label, without tags where `html` is true, for search and the help line.
     */
    std::string getLabel(const Json& row, ConfigSpecReading& reading)
    {
        auto text = readShownText(row, "text", reading);
        return getBool(row, "html") ? model::removeMarkup(text) : text;
    }

    /**
     * A label's styled pieces on one line where `html` is true. Other labels are drawn plain.
     */
    std::vector<model::TextRun> getLabelRuns(const Json& row, const model::Translations& translations)
    {
        return getBool(row, "html") ? model::markupRuns(getShownText(row, "text", translations)) : std::vector<model::TextRun>{};
    }

    /**
     * The `applies` of a config spec, a page, a section or a setting. None for one that has none, and for one
     * that is not `unknown`, `live`, `save` or `restart`.
     */
    std::optional<model::Applies> findApplies(const Json& value)
    {
        const auto text = getText(value, "applies");
        if (text == "unknown") {
            return model::Applies::Unknown;
        }
        if (text == "live") {
            return model::Applies::Live;
        }
        if (text == "save") {
            return model::Applies::Save;
        }
        return text == "restart" ? std::optional(model::Applies::Restart) : std::nullopt;
    }

    /**
     * What the items of a page or a section get from above them: the file a setting is kept in where its
     * storage names none, and when its change takes effect.
     */
    struct Inherited
    {
        std::string defaultFile;
        model::Applies applies = model::Applies::Unknown;
    };

    /**
     * What a page, a section or a setting has for what is under it: its own `defaultFile` and `applies`, or
     * else what it gets from above. An `applies` that is not one is passed over, and is reported.
     */
    Inherited inherit(const Json& value, const Inherited& above, ConfigSpecReading& reading)
    {
        Inherited own = above;
        if (auto file = getText(value, "defaultFile"); !file.empty()) {
            own.defaultFile = std::move(file);
        }
        if (const auto applies = findApplies(value)) {
            own.applies = *applies;
        } else if (value.contains("applies")) {
            const Into into(reading, "applies");
            reading.report(model::Severity::Error, std::format("Its applies {} is not unknown, live, save or restart: it has the one above it.", value["applies"].dump()));
        }
        return own;
    }

    using FormatName = std::pair<std::string_view, model::FileFormat>;

    // what a file of a config spec is written in, by its `format`, which is also its extension
    constexpr std::array FILE_FORMATS{
        FormatName{ "ini", model::FileFormat::Ini },
        FormatName{ "toml", model::FileFormat::Toml },
    };

    /**
     * A file of a config spec's `files`, or the reason it is not one: a `format` of `ini` or `toml`,
     * which its extension is, a `root` of `data` or `myGames`, and a `path` from that root. The path is
     * checked as it is written: one that is absolute, or goes up with `..`, could be any file of the system.
     */
    std::expected<model::File, std::string> loadFile(const Json& declared)
    {
        const auto formatText = getText(declared, "format");
        const auto format = std::ranges::find(FILE_FORMATS, formatText, &FormatName::first);
        if (format == FILE_FORMATS.end()) {
            return std::unexpected(std::format("its format '{}' is not ini or toml", formatText));
        }
        const auto rootText = getText(declared, "root");
        if (rootText != "data" && rootText != "myGames") {
            return std::unexpected(std::format("its root '{}' is not data or myGames", rootText));
        }
        auto path = getText(declared, "path");
        const std::filesystem::path checked(std::u8string(path.begin(), path.end()));
        if (path.empty() || checked.has_root_name() || checked.has_root_directory()) {
            return std::unexpected(std::format("its path '{}' is not one from its root", path));
        }
        if (std::ranges::any_of(checked, [](const std::filesystem::path& part) {
                return part == "..";
            })) {
            return std::unexpected(std::format("its path '{}' goes up out of its folder", path));
        }
        const auto extension = checked.extension().u8string();
        if (common::str_tolower(std::string(extension.begin(), extension.end())) != "." + formatText) {
            return std::unexpected(std::format("its path '{}' does not end in .{}", path, formatText));
        }
        return model::File{ .format = format->second, .root = rootText == "data" ? model::FileRoot::Data : model::FileRoot::MyGames, .path = std::move(path) };
    }

    /**
     * The files of a config spec, by the names its `files` gives them. A file that is not written as one is
     * left out, and is reported, so its settings are not available.
     */
    std::map<std::string, model::File> loadFiles(const Json& configSpec, ConfigSpecReading& reading)
    {
        std::map<std::string, model::File> files;
        const auto found = configSpec.find("files");
        if (found == configSpec.end() || !found->is_object()) {
            return files;
        }
        const Into into(reading, "files");
        for (const auto& [name, declared] : found->items()) {
            const Into intoFile(reading, name);
            checkFields(declared, FILE_FIELDS, reading);
            auto file = declared.is_object() ? loadFile(declared) : std::unexpected("it is not an object"s);
            if (file) {
                // the state of the mod's client that gives its path, which its root and its path stand for while it is not
                // answered
                if (const auto state = declared.find("pathFromClientState"); state != declared.end()) {
                    if (state->is_string() && !state->get_ref<const std::string&>().empty()) {
                        file->pathFromClientState = state->get<std::string>();
                    } else {
                        const Into intoState(reading, "pathFromClientState");
                        reading.report(model::Severity::Warning, "Its pathFromClientState is not the name of a state: the file is read from its root and its path.");
                    }
                }
                files.emplace(name, std::move(*file));
            } else {
                reading.report(model::Severity::Error, std::format("The file \"{}\" is left out: {}. Its settings are not available.", name, file.error()));
            }
        }
        return files;
    }

    /**
     * The values of a config spec's TOML and INI files that are there, read from the folders of their roots, by
     * their names, which tell a setting whose path its file does not have. A file that cannot be read is left out,
     * as its settings are not available then. With no folders, as for a config spec read alone, none is read.
     */
    std::map<std::string, std::unique_ptr<spec::ModFile>> readFiles(const std::map<std::string, model::File>& files, const spec::FileFolders& folders)
    {
        std::map<std::string, std::unique_ptr<spec::ModFile>> read;
        if (folders.data.empty() && folders.myGames.empty()) {
            return read;
        }
        for (const auto& [name, file] : files) {
            const auto path = (file.root == model::FileRoot::Data ? folders.data : folders.myGames) / std::u8string(file.path.begin(), file.path.end());
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error)) {
                continue;
            }
            std::unique_ptr<spec::ModFile> values;
            if (file.format == model::FileFormat::Toml) {
                values = std::make_unique<spec::TomlFile>(path);
            } else {
                values = std::make_unique<spec::IniFile>(path);
            }
            if (values->read()) {
                read.emplace(name, std::move(values));
            }
        }
        return read;
    }

    /**
     * The name a config spec gives one of its files, as the config spec declares it: a storage can name it in
     * another case. None for a name of no file.
     */
    std::optional<std::string> findFile(const std::map<std::string, model::File>& files, const std::string& name)
    {
        const auto lower = common::str_tolower(name);
        const auto found = std::ranges::find_if(files, [&lower](const auto& file) {
            return common::str_tolower(file.first) == lower;
        });
        return found != files.end() ? std::optional(found->first) : std::nullopt;
    }

    /**
     * A form of a game plugin, written as its plugin and its id in hex, as `MyMod.esp|800`, which can start
     * with `0x`. None for a text that is not written so.
     */
    std::optional<std::pair<std::string, std::uint32_t>> parseForm(const std::string& text)
    {
        const auto separator = text.find('|');
        auto plugin = common::trim(text.substr(0, separator));
        auto id = separator == std::string::npos ? std::string() : common::trim(text.substr(separator + 1));
        if (id.starts_with("0x") || id.starts_with("0X")) {
            id.erase(0, 2);
        }
        std::uint32_t formId = 0;
        const auto [end, error] = std::from_chars(id.data(), id.data() + id.size(), formId, 16);
        if (plugin.empty() || id.empty() || error != std::errc() || end != id.data() + id.size()) {
            return std::nullopt;
        }
        return std::pair(std::move(plugin), formId);
    }

    /**
     * Where a setting keeps its value: a global, as `{ "global": "MyMod.esp|1234" }`, a property of a script,
     * by its `form`, its `propertyName` and its `scriptName`, which is optional, in the setting's `valueType`,
     * or a value of the file its `storage` names, or else of the nearest `defaultFile` above it. The value of a
     * file is named by its `path`, or by its `section` and its `key`, as an INI key is. A storage that is not
     * written as one, as a form that is not a plugin and an id in hex, a property with no name, or no file of
     * the config spec or no value of it, as a path of more than two in an INI file, is one the menu does not read,
     * and is reported.
     */
    model::Storage loadStorage(const Json& row, const std::string& defaultFile, const model::StoredType valueType, ConfigSpecReading& reading)
    {
        const auto storage = row.find("storage");
        if (storage == row.end() || !storage->is_object()) {
            reading.report(model::Severity::Error, "It has no storage: it is not available.");
            return model::UnknownStorage{};
        }
        const Into into(reading, "storage");
        checkFields(*storage, STORAGE_FIELDS, reading);
        if (storage->contains("global")) {
            const auto global = getText(*storage, "global");
            if (auto form = parseForm(global)) {
                return model::GlobalStorage{ .plugin = std::move(form->first), .formId = form->second };
            }
            reading.report(model::Severity::Error, std::format("Its global \"{}\" is not a plugin and an id in hex, as MyMod.esp|800: it is not available.", global));
            return model::UnknownStorage{ .type = "global" };
        }
        if (storage->contains("form")) {
            const auto formText = getText(*storage, "form");
            auto form = parseForm(formText);
            auto name = getText(*storage, "propertyName");
            if (form && !name.empty()) {
                return model::PropertyStorage{ .plugin = std::move(form->first),
                    .formId = form->second,
                    .script = getText(*storage, "scriptName"),
                    .name = std::move(name),
                    .type = valueType };
            }
            reading.report(model::Severity::Error,
                std::format("Its property \"{}\" of the form \"{}\" is not a name, and a plugin and an id in hex, as MyMod.esp|800: it is not available.", name, formText));
            return model::UnknownStorage{ .type = "property" };
        }

        std::vector<std::string> path;
        if (const auto names = storage->find("path"); names != storage->end() && names->is_array()) {
            for (const auto& name : *names) {
                if (!name.is_string() || name.get_ref<const std::string&>().empty()) {
                    path.clear();
                    break;
                }
                path.push_back(name.get<std::string>());
            }
        } else if (auto key = getText(*storage, "key"); !key.empty()) {
            auto section = getText(*storage, "section");
            path = section.empty() ? std::vector{ std::move(key) } : std::vector{ std::move(section), std::move(key) };
        }

        auto fileName = getText(*storage, "file");
        const auto file = findFile(reading.files, fileName.empty() ? defaultFile : fileName);
        if (!file || path.empty() || (reading.files.at(*file).format == model::FileFormat::Ini && path.size() > 2)) {
            reading.report(model::Severity::Error, "It names no file of its config spec, or no value of it: it is not available.");
            return model::UnknownStorage{ .type = "file" };
        }
        return model::FileStorage{ .file = *file, .path = std::move(path) };
    }

    /**
     * The id a setting gets from where its value is kept: the last name of its path, and the names before it
     * joined by dots, as `fScale:Panel` for `Panel`, `fScale`. A path of two is named as MCM names a key of an
     * INI file, so a mod that moves from INI to TOML keeps its ids. A path of one is its name alone. A global is
     * its form, as `MyMod.esp|1234`, and a property of a script its name and its form, as `fStrength:MyMod.esp|800`.
     * Empty for a storage the menu does not read.
     */
    std::string idOf(const model::Storage& storage)
    {
        if (const auto* global = std::get_if<model::GlobalStorage>(&storage)) {
            return std::format("{}|{:X}", global->plugin, global->formId);
        }
        if (const auto* property = std::get_if<model::PropertyStorage>(&storage)) {
            return std::format("{}:{}|{:X}", property->name, property->plugin, property->formId);
        }
        const auto* file = std::get_if<model::FileStorage>(&storage);
        if (!file || file->path.empty()) {
            return {};
        }
        std::string before;
        for (std::size_t i = 0; i + 1 < file->path.size(); ++i) {
            before += (i == 0 ? "" : ".") + file->path[i];
        }
        return before.empty() ? file->path.back() : std::format("{}:{}", file->path.back(), before);
    }

    using SettingTypeName = std::pair<std::string_view, model::SettingType>;

    // the rows that edit a value, by their `type`
    constexpr std::array SETTING_TYPES{
        SettingTypeName{ "switcher", model::SettingType::Switcher },
        SettingTypeName{ "slider", model::SettingType::Slider },
        SettingTypeName{ "stepper", model::SettingType::Stepper },
        SettingTypeName{ "dropdown", model::SettingType::Dropdown },
        SettingTypeName{ "textinput", model::SettingType::TextInput },
        SettingTypeName{ "cf.inputBinding", model::SettingType::InputBinding },
    };

    /**
     * Whether a setting is a choice: a stepper or a dropdown.
     */
    bool isChoice(const model::SettingType type)
    {
        return type == model::SettingType::Stepper || type == model::SettingType::Dropdown;
    }

    using ValueTypeName = std::pair<std::string_view, model::StoredType>;

    // what a setting's value is, by its `valueType`
    constexpr std::array VALUE_TYPES{
        ValueTypeName{ "bool", model::StoredType::Bool },
        ValueTypeName{ "int", model::StoredType::Int },
        ValueTypeName{ "float", model::StoredType::Float },
        ValueTypeName{ "string", model::StoredType::String },
    };

    /**
     * What a setting's value is, by its `valueType`. One with none, or with one that is not a type, has the
     * type of its control: a bool for a switcher, a number for a slider, the place of an option for a choice,
     * and a text for a text input. One that is not a type is a warning. A binding is the text of its table, so its
     * `valueType` is a string, and another is a warning too.
     */
    model::StoredType loadValueType(const Json& row, const model::SettingType type, ConfigSpecReading& reading)
    {
        const auto name = getText(row, "valueType");
        if (type == model::SettingType::InputBinding) {
            if (row.contains("valueType") && name != "string") {
                const Into into(reading, "valueType");
                reading.report(model::Severity::Warning, std::format("Its valueType {} is not string, which a binding is: it is read as string.", row["valueType"].dump()));
            }
            return model::StoredType::String;
        }
        const auto found = std::ranges::find(VALUE_TYPES, name, &ValueTypeName::first);
        if (found != VALUE_TYPES.end()) {
            return found->second;
        }
        auto valueType = model::StoredType::String;
        if (type == model::SettingType::Switcher) {
            valueType = model::StoredType::Bool;
        } else if (isChoice(type)) {
            valueType = model::StoredType::Int;
        } else if (type == model::SettingType::Slider) {
            valueType = model::StoredType::Float;
        }
        if (row.contains("valueType")) {
            const Into into(reading, "valueType");
            const auto read = std::ranges::find(VALUE_TYPES, valueType, &ValueTypeName::second)->first;
            reading.report(model::Severity::Warning, std::format("Its valueType {} is not bool, int, float or string: it is read as {}.", row["valueType"].dump(), read));
        }
        return valueType;
    }

    /**
     * The value of a key in a type: a JSON bool for a bool, a whole number for an int, any number for a
     * float, and a text for a string. None for a key that has no value of the type.
     */
    std::optional<model::Value> getValue(const Json& value, const char* key, const model::StoredType type)
    {
        const auto found = value.find(key);
        if (found == value.end()) {
            return std::nullopt;
        }
        if (type == model::StoredType::Bool && found->is_boolean()) {
            return found->get<bool>();
        }
        if (type == model::StoredType::Int && found->is_number_integer()) {
            return found->get<int>();
        }
        if (type == model::StoredType::Float && found->is_number()) {
            return found->get<float>();
        }
        if (type == model::StoredType::String && found->is_string()) {
            return found->get<std::string>();
        }
        return std::nullopt;
    }

    /**
     * The shared list of a config spec by its name, which a choice can name in another case. None for a name of
     * no list.
     */
    const Json* findSharedList(const Json* sharedLists, const std::string& name)
    {
        if (!sharedLists) {
            return nullptr;
        }
        const auto lower = common::str_tolower(name);
        for (const auto& [listName, list] : sharedLists->items()) {
            if (common::str_tolower(listName) == lower) {
                return &list;
            }
        }
        return nullptr;
    }

    /**
     * The options of a choice, in the type of its value. A text is an option of the short form, whose value
     * is its place in the list, for a choice of an int. An object is an option with its own `value` and its
     * `text`. Any other option, and one whose value is not of the choice's type, is left out, and is reported:
     * of a shared list, which can be another choice's, as an option of that list.
     * An option's own fields and its texts are checked once, as its list is read: a shared list as the config spec's
     * root is read.
     */
    std::vector<model::Option> loadOptions(const Json& list, const std::string& sharedName, const model::StoredType valueType, ConfigSpecReading& reading)
    {
        std::vector<model::Option> options;
        const bool own = sharedName.empty();
        for (std::size_t place = 0; place < list.size(); ++place) {
            const auto& option = list[place];
            std::optional<Into> into;
            if (own) {
                into.emplace(reading, "options", place);
            }
            if (option.is_string() && valueType == model::StoredType::Int) {
                options.push_back({ .value = static_cast<int>(place), .text = own ? readShownText(option, reading) : reading.translations.translate(option.get<std::string>()) });
                continue;
            }
            auto value = option.is_object() ? getValue(option, "value", valueType) : std::nullopt;
            if (!value) {
                const auto which = own ? std::format("Option {}", place + 1) : std::format("Option {} of the shared list \"{}\"", place + 1, sharedName);
                reading.report(model::Severity::Error, std::format("{} is left out: it is not a text of a choice of an int, or an object with a value of its type.", which));
                continue;
            }
            if (own) {
                checkFields(option, OPTION_FIELDS, reading);
            }
            options.push_back({ .value = std::move(*value), .text = own ? readShownText(option, "text", reading) : getShownText(option, "text", reading.translations) });
        }
        return options;
    }

    /**
     * Where a choice reads its options in the game: a `listFromForm` names a FormList, as `MyMod.esp|900`, and a
     * `listFromProperty` a property of a script, by its `form`, its `propertyName` and its `scriptName`, which
     * is optional. A choice of an int stores the place of an option, and one of a string its text. None for a
     * choice with neither, and reported for a list that is not written so, and for a choice of another type.
     */
    std::optional<model::GameOptions> loadGameOptions(const Json& valueOptions, const model::StoredType valueType, ConfigSpecReading& reading)
    {
        model::GameOptions list{ .storesText = valueType == model::StoredType::String };
        const auto property = valueOptions.find("listFromProperty");
        if (valueOptions.contains("listFromForm")) {
            const auto formList = getText(valueOptions, "listFromForm");
            auto form = parseForm(formList);
            if (!form) {
                const Into into(reading, "listFromForm");
                reading.report(model::Severity::Error, std::format("Its listFromForm \"{}\" is not a plugin and an id in hex, as MyMod.esp|900: it has no options.", formList));
                return std::nullopt;
            }
            list.plugin = std::move(form->first);
            list.formId = form->second;
        } else if (property != valueOptions.end() && property->is_object()) {
            const Into into(reading, "listFromProperty");
            checkFields(*property, LIST_PROPERTY_FIELDS, reading);
            const auto formText = getText(*property, "form");
            auto form = parseForm(formText);
            list.property = getText(*property, "propertyName");
            if (!form || list.property.empty()) {
                reading.report(model::Severity::Error,
                    std::format("Its listFromProperty \"{}\" of the form \"{}\" is not a name, and a plugin and an id in hex: it has no options.", list.property, formText));
                return std::nullopt;
            }
            list.plugin = std::move(form->first);
            list.formId = form->second;
            list.script = getText(*property, "scriptName");
        } else {
            const Into into(reading, "listFromProperty");
            reading.report(model::Severity::Error, "Its listFromProperty is not an object: it has no options.");
            return std::nullopt;
        }
        if (valueType != model::StoredType::Int && valueType != model::StoredType::String) {
            reading.report(model::Severity::Error, "It lists its options in the game, so its valueType is int or string: it has no options.");
            return std::nullopt;
        }
        return list;
    }

    /**
     * A list of words of a binding's `valueOptions`, `hands`, `types` or `buttons`, read as the framework reads them
     * in a binding. A word that is none of its part's is left out, and reported, and so is a value that is not a list.
     * None for a list that is not there: the setting takes any.
     */
    template <typename T, typename Parse>
    std::vector<T> loadBindingWords(const Json& options, const std::string_view key, Parse parse, const std::string_view part, ConfigSpecReading& reading)
    {
        std::vector<T> words;
        const auto found = options.find(key);
        if (found == options.end()) {
            return words;
        }
        if (!found->is_array()) {
            const Into into(reading, key);
            reading.report(model::Severity::Warning, std::format("It is not a list of words: the binding takes any {}.", part.substr(part.find(' ') + 1)));
            return words;
        }
        for (std::size_t place = 0; place < found->size(); ++place) {
            const auto& item = (*found)[place];
            const auto word = item.is_string() ? parse(item.get<std::string>()) : std::nullopt;
            if (!word) {
                const Into into(reading, key, place);
                reading.report(model::Severity::Warning, std::format("{} is not {}, as the framework names it: it is left out.", item.dump(), part));
                continue;
            }
            words.push_back(*word);
        }
        return words;
    }

    /**
     * The `valueOptions` of a setting: the least, the most and the step of a number, the fewest and the most
     * characters of a text input of a text, the hands, the ways and the buttons a binding takes, and the options of a
     * choice: its own `options`, the shared list
     * its `sharedOptions` names, where it reads them in the game, or that its mod's client gives them, with a
     * `listFromClientCallback` that is true. An `allowEmpty` of false is a least of one character. A length that
     * is no whole number above 0 is none.
     * A choice with none of them has no options, and is reported, so it is disabled.
     */
    model::ValueOptions loadValueOptions(const Json& row, const model::SettingType type, const model::StoredType valueType, ConfigSpecReading& reading)
    {
        model::ValueOptions valueOptions;
        const auto found = row.find("valueOptions");
        const bool hasOptions = found != row.end() && found->is_object();
        std::optional<Into> into;
        if (hasOptions) {
            into.emplace(reading, "valueOptions");
            checkFields(*found, VALUE_OPTIONS_FIELDS, reading);
            valueOptions.min = getNumber(*found, "min", valueOptions.min);
            valueOptions.max = getNumber(*found, "max", valueOptions.max);
            valueOptions.step = getNumber(*found, "step", valueOptions.step);
        }
        if (hasOptions && type == model::SettingType::TextInput && valueType == model::StoredType::String) {
            valueOptions.minLength = getLength(*found, "minLength");
            valueOptions.maxLength = getLength(*found, "maxLength");
            const auto allowEmpty = found->find("allowEmpty");
            if (allowEmpty != found->end() && allowEmpty->is_boolean() && !allowEmpty->get<bool>()) {
                valueOptions.minLength = (std::max)(valueOptions.minLength, 1);
            }
        }
        if (hasOptions && type == model::SettingType::InputBinding) {
            valueOptions.suppress = getBool(*found, "suppress");
            valueOptions.hands = loadBindingWords<vrcf::Hand>(*found, "hands", vrcf::parseHand, "a hand", reading);
            valueOptions.types = loadBindingWords<vrcf::ActivationType>(*found, "types", vrcf::parseActivationType, "a type", reading);
            valueOptions.buttons = loadBindingWords<vr::EVRButtonId>(*found, "buttons", vrcf::parseButton, "a button", reading);
        }
        if (!isChoice(type)) {
            return valueOptions;
        }

        const Json* list = nullptr;
        std::string sharedName;
        if (hasOptions) {
            const auto own = found->find("options");
            if (own != found->end()) {
                list = &*own;
            } else {
                sharedName = getText(*found, "sharedOptions");
                list = findSharedList(reading.sharedLists, sharedName);
            }
        }
        if (list && list->is_array()) {
            valueOptions.options = loadOptions(*list, sharedName, valueType, reading);
            return valueOptions;
        }
        if (hasOptions && (found->contains("listFromForm") || found->contains("listFromProperty"))) {
            valueOptions.gameOptions = loadGameOptions(*found, valueType, reading);
        } else if (hasOptions && getBool(*found, "listFromClientCallback")) {
            valueOptions.clientOptions = true;
        } else {
            reading.report(model::Severity::Error, "It has no list of options, or names no shared list of its config spec: it has no options.");
        }
        return valueOptions;
    }

    /**
     * Whether two values of a setting are the same one: two numbers are when they are as near as a number and
     * the text of it in a file.
     */
    bool isSameValue(const model::Value& value, const model::Value& other)
    {
        const auto* number = std::get_if<float>(&value);
        const auto* otherNumber = std::get_if<float>(&other);
        return number && otherNumber ? common::fEqual(*number, *otherNumber) : value == other;
    }

    /**
     * Whether a value of a setting's type is one of its values: an option of a choice, a number inside
     * the limits of a setting that has them, its least and its most among them, and a text of the length its
     * setting allows. Any value is one of a choice that reads its options in the game or from its mod's client,
     * which are not known until then.
     */
    bool isValueOf(const model::Value& value, const model::SettingType type, const model::ValueOptions& options)
    {
        if (isChoice(type)) {
            return options.gameOptions || options.clientOptions || std::ranges::any_of(options.options, [&value](const model::Option& option) {
                return isSameValue(option.value, value);
            });
        }
        if (const auto* text = std::get_if<std::string>(&value)) {
            const int length = model::textLength(*text);
            return length >= options.minLength && (options.maxLength == 0 || length <= options.maxLength);
        }
        const auto* whole = std::get_if<int>(&value);
        const auto* point = std::get_if<float>(&value);
        if ((!whole && !point) || !(options.max > options.min)) {
            return true;
        }
        const float number = whole ? static_cast<float>(*whole) : *point;
        return (number > options.min || common::fEqual(number, options.min)) && (number < options.max || common::fEqual(number, options.max));
    }

    /**
     * A JSON value as the TOML text of the same value: an object is an inline table and an array an array, whose
     * keys and texts JSON writes as TOML writes them, and a bool and a number are themselves. A null is none, which
     * no TOML value reads.
     */
    std::string tomlText(const Json& value)
    {
        if (value.is_object()) {
            std::string text;
            for (const auto& [key, item] : value.items()) {
                text += std::format("{}{} = {}", text.empty() ? "{ " : ", ", Json(key).dump(), tomlText(item));
            }
            return text.empty() ? "{}" : text + " }";
        }
        if (value.is_array()) {
            std::string text;
            for (const auto& item : value) {
                text += std::format("{}{}", text.empty() ? "[" : ", ", tomlText(item));
            }
            return text.empty() ? "[]" : text + "]";
        }
        return value.is_null() ? std::string() : value.dump();
    }

    /**
     * The `default` of a binding: the binding's table as a JSON object, as `{ "hand": "offhand", "type": "tap",
     * "button": "trigger" }`, which the framework reads as it reads the table of its config. It is the TOML text of
     * the table as the framework writes it. None for a default that is no object, or no binding, which is an error
     * of the config spec that is reported with the framework's reason, and the setting then has no Reset. So is one
     * of a hand, a way or a button the setting does not take.
     */
    std::optional<model::Value> loadBindingDefault(const Json& row, const model::ValueOptions& options, ConfigSpecReading& reading)
    {
        const auto found = row.find("default");
        if (found == row.end() || !found->is_object()) {
            reading.report(model::Severity::Error, R"(It has no default binding, an object as { "hand": "offhand", "type": "tap", "button": "trigger" }: it has no Reset.)");
            return std::nullopt;
        }
        const auto binding = model::parseBinding(tomlText(*found));
        if (!binding) {
            const Into into(reading, "default");
            reading.report(model::Severity::Error, std::format("Its default is not a binding: {}. It has no Reset.", binding.error()));
            return std::nullopt;
        }
        if (const auto untaken = model::untakenPart(*binding, options); !untaken.empty()) {
            const Into into(reading, "default");
            reading.report(model::Severity::Error, std::format("Its default is not a binding it takes: {}. It has no Reset.", untaken));
            return std::nullopt;
        }
        return model::bindingText(*binding);
    }

    /**
     * The `default` of a setting, in its value's type. None for a setting with no default of its type, or
     * with one that is not one of its values: outside its limits, of a length it does not allow, or none of
     * its options. Both are errors of the config spec that are reported, and the setting then has no Reset.
     */
    std::optional<model::Value> loadDefault(const Json& row, const model::SettingType type, const model::StoredType valueType, const model::ValueOptions& options,
        ConfigSpecReading& reading)
    {
        if (type == model::SettingType::InputBinding) {
            return loadBindingDefault(row, options, reading);
        }
        auto value = getValue(row, "default", valueType);
        if (!value) {
            reading.report(model::Severity::Error, "It has no default of the type of its value: it has no Reset.");
        } else if (isChoice(type) && options.options.empty() && !options.gameOptions && !options.clientOptions) {
            // a choice with no options is reported already, and no default is one of its values
            value.reset();
        } else if (!isValueOf(*value, type, options)) {
            const Into into(reading, "default");
            reading.report(model::Severity::Error,
                "Its default is not one of its values: outside its limits, of a length it does not allow, or none of its options. It has no Reset.");
            value.reset();
        }
        return value;
    }

    // the most decimals a number's display can have, as MCM's menu writes a number at the most
    constexpr int MAX_DECIMALS = 6;

    /**
     * The `display` of a setting: its decimals, its unit, which is a text the player sees, and whether it is a
     * percentage. Decimals that are no whole number from 0 to 6 are none, and are reported.
     */
    model::NumberDisplay loadDisplay(const Json& row, ConfigSpecReading& reading)
    {
        const auto found = row.find("display");
        if (found == row.end() || !found->is_object()) {
            return {};
        }
        const Into into(reading, "display");
        checkFields(*found, DISPLAY_FIELDS, reading);
        model::NumberDisplay display{ .decimals = {}, .unit = readShownText(*found, "unit", reading), .percent = getBool(*found, "percent") };
        if (const auto decimals = found->find("decimals"); decimals != found->end()) {
            if (decimals->is_number_integer() && decimals->get<int>() >= 0 && decimals->get<int>() <= MAX_DECIMALS) {
                display.decimals = decimals->get<int>();
            } else {
                const Into intoDecimals(reading, "decimals");
                reading.report(model::Severity::Error, std::format("Its decimals are not a whole number from 0 to {}: it has as many as its step.", MAX_DECIMALS));
            }
        }
        return display;
    }

    // the least and the most a whole number of Papyrus is
    constexpr double MIN_WHOLE = -2147483648.0;
    constexpr double MAX_WHOLE = 2147483647.0;

    /**
     * A parameter of a call, in its own type as a JSON value: a number written with no point or exponent is a
     * whole number, any other number is a number, a bool is a bool and a text is a text, which is never read
     * for a type. None for any other value, and for a whole number that Papyrus does not have.
     */
    std::optional<model::Value> loadArgument(const Json& param)
    {
        if (param.is_boolean()) {
            return param.get<bool>();
        }
        if (param.is_number_integer()) {
            const auto whole = param.get<double>();
            return whole >= MIN_WHOLE && whole <= MAX_WHOLE ? std::optional<model::Value>(static_cast<int>(whole)) : std::nullopt;
        }
        if (param.is_number()) {
            return param.get<float>();
        }
        return param.is_string() ? std::optional<model::Value>(param.get<std::string>()) : std::nullopt;
    }

    /**
     * The function a `CallFunction` or a `CallGlobalFunction` calls, with its `params`, or why it is not one.
     * A `CallFunction` names the form of its script in `form`, as `MyMod.esp|800`, and can name the script
     * in `scriptName`. A `CallGlobalFunction` names its script in `script`, and has no form.
     */
    std::expected<model::CallFunction, std::string> loadCall(const Json& action, const std::string& type)
    {
        model::CallFunction call{ .plugin = {}, .formId = 0, .script = {}, .function = getText(action, "function"), .arguments = {}, .valueArguments = {} };
        if (call.function.empty()) {
            return std::unexpected(std::format("its {} names no function", type));
        }
        if (type == "CallGlobalFunction") {
            call.script = getText(action, "script");
            if (call.script.empty()) {
                return std::unexpected("its CallGlobalFunction names no script"s);
            }
        } else {
            const auto formText = getText(action, "form");
            auto form = parseForm(formText);
            if (formText.empty()) {
                return std::unexpected("its CallFunction names no form"s);
            }
            if (!form) {
                return std::unexpected(std::format("the form '{}' of its CallFunction is not a plugin and an id in hex", formText));
            }
            call.plugin = std::move(form->first);
            call.formId = form->second;
            call.script = getText(action, "scriptName");
        }

        const auto params = action.find("params");
        if (params == action.end()) {
            return call;
        }
        if (!params->is_array()) {
            return std::unexpected(std::format("the params of its {} are not a list", type));
        }
        for (std::size_t place = 0; place < params->size(); ++place) {
            auto argument = loadArgument((*params)[place]);
            if (!argument) {
                return std::unexpected(std::format("parameter {} of its {} is not a whole number of Papyrus, a number, a bool or a text", place + 1, type));
            }
            call.arguments.push_back(std::move(*argument));
        }
        return call;
    }

    /**
     * A `CallNativeFunction`: the function of the mod's DLL it names, and its `params`, an object of named values,
     * each given to the function in its own JSON type, as a `CallFunction`'s. Or what is not written right.
     */
    std::expected<model::NativeFunction, std::string> loadNativeFunction(const Json& action)
    {
        model::NativeFunction native{ .function = getText(action, "function"), .arguments = {}, .valueArguments = {} };
        if (native.function.empty()) {
            return std::unexpected("its CallNativeFunction names no function"s);
        }
        const auto params = action.find("params");
        if (params == action.end()) {
            return native;
        }
        if (!params->is_object()) {
            return std::unexpected("the params of its CallNativeFunction are not an object of named values"s);
        }
        for (const auto& [name, param] : params->items()) {
            auto argument = loadArgument(param);
            if (!argument) {
                return std::unexpected(std::format("the parameter '{}' of its CallNativeFunction is not a whole number of Papyrus, a number, a bool or a text", name));
            }
            native.arguments.push_back({ .name = name, .value = std::move(*argument) });
        }
        return native;
    }

    /**
     * An action of a button or a setting that is not written right, which the menu does not run: the help line
     * of a button has the mistake as its reason, and it is reported.
     */
    model::UnknownAction mistakenAction(std::string type, const std::string& mistake, ConfigSpecReading& reading)
    {
        reading.report(model::Severity::Error, std::format("Its action is not run: {}.", mistake));
        return model::UnknownAction{ .type = std::move(type), .reason = std::format("Not available: {}.", mistake) };
    }

    /**
     * What a button runs, by the `type` of its `action`: a `CallFunction` and a `CallGlobalFunction`, with
     * their `params`, a `RunConsoleCommand`, an `OpenURL`, and a `CallNativeFunction`, a function that the
     * mod's DLL registered with the native API, with its named `params`. A button with no action, any other action,
     * and one that is not written right are actions the menu does not run, with the reason, and are reported.
     */
    model::Action loadAction(const Json& row, ConfigSpecReading& reading)
    {
        const auto action = row.find("action");
        if (action == row.end() || !action->is_object()) {
            return mistakenAction({}, "the button has no action", reading);
        }
        const Into into(reading, "action");
        checkFields(*action, ACTION_FIELDS, reading);
        auto type = getText(*action, "type");
        if (type == "CallFunction" || type == "CallGlobalFunction") {
            auto call = loadCall(*action, type);
            return call ? model::Action(std::move(*call)) : mistakenAction(std::move(type), call.error(), reading);
        }
        if (type == "RunConsoleCommand") {
            auto command = getText(*action, "command");
            return command.empty() ? model::Action(mistakenAction(std::move(type), "its RunConsoleCommand has no command", reading)) : model::ConsoleCommand{ std::move(command) };
        }
        if (type == "OpenURL") {
            return model::WebLink{ .url = getText(*action, "url") };
        }
        if (type.empty()) {
            return mistakenAction({}, "the action has no type", reading);
        }
        if (type == "CallNativeFunction") {
            auto native = loadNativeFunction(*action);
            return native ? model::Action(std::move(*native)) : mistakenAction(std::move(type), native.error(), reading);
        }
        reading.report(model::Severity::Error, std::format("Its action \"{}\" is not one the menu runs: it is not run.", type));
        auto reason = std::format("Not available: the menu does not run a {}.", type);
        return model::UnknownAction{ .type = std::move(type), .reason = std::move(reason) };
    }

    /**
     * What a button asks before it runs: its `confirmation`, with its `title`, its `message`, the text of the button
     * that runs it, `confirm`, and whether it is `dangerous`. None for a button with none, and for one that is no
     * object, which is reported.
     */
    std::optional<model::Confirmation> loadConfirmation(const Json& row, ConfigSpecReading& reading)
    {
        const auto found = row.find("confirmation");
        if (found == row.end()) {
            return std::nullopt;
        }
        const Into into(reading, "confirmation");
        if (!found->is_object()) {
            reading.report(model::Severity::Error, "Its confirmation is not an object: the button runs with none.");
            return std::nullopt;
        }
        checkFields(*found, CONFIRMATION_FIELDS, reading);
        return model::Confirmation{ .title = readShownText(*found, "title", reading),
            .message = readShownText(*found, "message", reading),
            .confirm = readShownText(*found, "confirm", reading),
            .dangerous = getBool(*found, "dangerous") };
    }

    /**
     * A button: its text, its action, the text on its button, `buttonText`, which is `Run` without one, and what it
     * asks before it runs. Its conditions and its warnings are read once every setting of the config spec is
     * (`loadConditions`).
     */
    model::Button loadButton(const Json& row, ConfigSpecReading& reading)
    {
        return model::Button{ .text = getLabel(row, reading),
            .textRuns = getLabelRuns(row, reading.translations),
            .help = readShownText(row, "help", reading),
            .buttonText = readShownText(row, "buttonText", reading),
            .action = loadAction(row, reading),
            .visibleWhen = {},
            .enabledWhen = {},
            .warnings = {},
            .confirmation = loadConfirmation(row, reading) };
    }

    // the parameter of a setting's action that is its value
    constexpr auto VALUE_PARAMETER = "{value}";

    /**
     * What a setting runs as Save writes its value: an action as a button's, of which a parameter that is
     * exactly `"{value}"` is the value written, in the setting's `valueType`. A setting with no `action` runs
     * nothing, and so does one whose action the menu does not run, which is reported.
     */
    model::Action loadSettingAction(const Json& row, const model::StoredType valueType, ConfigSpecReading& reading)
    {
        if (!row.contains("action")) {
            return model::NoAction{};
        }
        auto action = loadAction(row, reading);
        if (std::holds_alternative<model::WebLink>(action)) {
            return mistakenAction("OpenURL", "OpenURL is only available for buttons", reading);
        }
        if (auto* call = std::get_if<model::CallFunction>(&action)) {
            for (std::size_t place = 0; place < call->arguments.size(); ++place) {
                if (call->arguments[place] == model::Value(std::string(VALUE_PARAMETER))) {
                    call->valueArguments.push_back({ .place = place, .text = VALUE_PARAMETER, .type = valueType });
                }
            }
        } else if (auto* native = std::get_if<model::NativeFunction>(&action)) {
            for (std::size_t place = 0; place < native->arguments.size(); ++place) {
                if (native->arguments[place].value == model::Value(std::string(VALUE_PARAMETER))) {
                    native->valueArguments.push_back(place);
                }
            }
        }
        return action;
    }

    /**
     * A setting of a config spec: its author's `id`, or one it gets from where its value is kept, its own
     * `applies` or the nearest above it, its `default`, how its number is shown, the options of a choice, and
     * its action, which Save runs.
     * A read-only setting has no default, as it has no Reset. Two settings with one id are an error of the
     * config spec, which is reported: a condition names the first, and an author gives one of them an id of its
     * own. Its conditions and its warnings are read once every setting of the config spec is (`loadConditions`).
     * A binding is kept in a value of a TOML file, as the framework keeps it: in any other storage it is not
     * available, which is reported.
     */
    model::Setting loadSetting(const Json& row, const model::SettingType type, const Inherited& above, ConfigSpecReading& reading)
    {
        const auto own = inherit(row, above, reading);
        const auto valueType = loadValueType(row, type, reading);
        auto storage = loadStorage(row, own.defaultFile, valueType, reading);
        if (type == model::SettingType::InputBinding && !std::holds_alternative<model::UnknownStorage>(storage)) {
            const auto* file = std::get_if<model::FileStorage>(&storage);
            if (!file || reading.files.at(file->file).format != model::FileFormat::Toml) {
                const Into into(reading, "storage");
                reading.report(model::Severity::Error, "A binding is kept in a value of a TOML file, and it is not: it is not available.");
                storage = model::UnknownStorage{ .type = "binding" };
            }
        }
        const bool readOnly = getBool(row, "readOnly");
        if (const auto* file = std::get_if<model::FileStorage>(&storage)) {
            if (const auto values = reading.fileValues.find(file->file); values != reading.fileValues.end() && !values->second->contains(file->path)) {
                const Into into(reading, "storage");
                reading.report(model::Severity::Warning,
                    std::format("Its file \"{}\" has no value {}: {}.",
                        file->file,
                        values->second->pathName(file->path),
                        readOnly ? "it shows none" : "it shows its default, and a Save adds it"));
            }
        }
        auto id = getText(row, "id");
        if (id.empty()) {
            id = idOf(storage);
        }
        if (!id.empty() && !reading.settings.emplace(common::str_tolower(id), SettingInfo{ .id = id, .type = type, .valueType = valueType }).second) {
            reading.report(model::Severity::Error,
                std::format("A setting before it has the id \"{}\": a condition names that one. Give one of them an id of its own.",
                    reading.settings.at(common::str_tolower(id)).id));
        }
        auto valueOptions = loadValueOptions(row, type, valueType, reading);
        auto defaultValue = readOnly ? std::nullopt : loadDefault(row, type, valueType, valueOptions, reading);
        auto text = getLabel(row, reading);
        auto help = readShownText(row, "help", reading);
        auto display = loadDisplay(row, reading);
        auto action = loadSettingAction(row, valueType, reading);
        return model::Setting{ .id = std::move(id),
            .sourceId = {},
            .type = type,
            .text = std::move(text),
            .textRuns = getLabelRuns(row, reading.translations),
            .help = std::move(help),
            .valueOptions = std::move(valueOptions),
            .storage = std::move(storage),
            .valueType = valueType,
            .action = std::move(action),
            .visibleWhen = {},
            .enabledWhen = {},
            .applies = own.applies,
            .defaultValue = std::move(defaultValue),
            .display = std::move(display),
            .readOnly = readOnly,
            .warnings = {} };
    }

    /**
     * A text row: its text in lines, and with the tags of an MCM config read where its `html` is true.
     */
    model::Text loadText(const Json& row, ConfigSpecReading& reading)
    {
        const auto text = readShownText(row, "text", reading);
        return model::Text{ .lines = getBool(row, "html") ? model::parseMarkup(text) : model::plainLines(text), .visibleWhen = {} };
    }

    /**
     * A spacer: as high as its `numLines`, from 1 to 20, and one line without it.
     */
    model::Spacer loadSpacer(const Json& row)
    {
        const auto lines = std::clamp(getNumber(row, "numLines", 1.0f), 1.0f, MAX_SPACER_LINES);
        return model::Spacer{ .lines = static_cast<int>(lines), .visibleWhen = {} };
    }

    /**
     * An image: its `texture`, a path under `Data/Textures`, and its `width` and `height` in the panel's pixels,
     * of which a side that is not more than 0 is none. One with no texture shows that an image is there, and
     * is reported.
     */
    model::Image loadImage(const Json& row, ConfigSpecReading& reading)
    {
        auto texture = getText(row, "texture");
        if (texture.empty()) {
            reading.report(model::Severity::Error, "It names no texture: a box is shown in its place.");
        }
        return model::Image{ .pixels = {},
            .texture = std::move(texture),
            .width = (std::max)(getNumber(row, "width", 0.0f), 0.0f),
            .height = (std::max)(getNumber(row, "height", 0.0f), 0.0f),
            .visibleWhen = {} };
    }

    /**
     * The items of the `content` of a page or a section, by their `type`, one for each row. A section is a
     * group of the items of its own `content`, and passes its `defaultFile` and its `applies` down to them.
     * It folds with `isCollapsible`, and starts folded with `collapsed`: one that does not fold could not be
     * opened, so its `collapsed` is left out, and it is reported.
     * Any other row the menu does not have is a placeholder with its type, and a type the schema does not have
     * is reported. A row's fields that its type does not have are reported.
     */
    std::vector<model::Item> loadItems(const Json& container, const Inherited& above, ConfigSpecReading& reading)
    {
        std::vector<model::Item> items;
        const auto content = container.find("content");
        if (content == container.end() || !content->is_array()) {
            return items;
        }
        for (std::size_t place = 0; place < content->size(); ++place) {
            const auto& row = (*content)[place];
            const Into into(reading, "content", place);
            reading.at.row = rowName(row, reading.translations);
            reading.at.itemPlaces.push_back(place);
            const auto type = getText(row, "type");
            if (type == "section") {
                checkFields(row, SECTION_FIELDS, reading);
                const bool collapsible = getBool(row, "isCollapsible");
                if (getBool(row, "collapsed") && !collapsible) {
                    reading.report(model::Severity::Warning, "It is collapsed and does not fold: it is open.");
                }
                auto text = readShownText(row, "text", reading);
                auto help = readShownText(row, "help", reading);
                items.emplace_back(model::Group{ .text = std::move(text),
                    .help = std::move(help),
                    .items = loadItems(row, inherit(row, above, reading), reading),
                    .visibleWhen = {},
                    .collapsible = collapsible,
                    .collapsed = collapsible && getBool(row, "collapsed") });
                continue;
            }
            if (type == "text") {
                checkFields(row, TEXT_ROW_FIELDS, reading);
                items.emplace_back(loadText(row, reading));
                continue;
            }
            if (type == "spacer") {
                checkFields(row, SPACER_FIELDS, reading);
                items.emplace_back(loadSpacer(row));
                continue;
            }
            if (type == "image") {
                checkFields(row, IMAGE_FIELDS, reading);
                items.emplace_back(loadImage(row, reading));
                continue;
            }
            if (type == "button") {
                checkFields(row, BUTTON_FIELDS, reading);
                items.emplace_back(loadButton(row, reading));
                continue;
            }
            if (const auto setting = std::ranges::find(SETTING_TYPES, type, &SettingTypeName::first); setting != SETTING_TYPES.end()) {
                checkFields(row, SETTING_FIELDS, reading);
                items.emplace_back(loadSetting(row, setting->second, above, reading));
                continue;
            }
            reading.report(model::Severity::Warning,
                type.empty() ? "It has no type: it is shown as a row the menu does not have."s
                             : std::format("The menu has no row of the type \"{}\": it is shown as one it does not have.", type));
            auto text = getLabel(row, reading);
            auto help = readShownText(row, "help", reading);
            items.emplace_back(
                model::Placeholder{ .text = std::move(text), .textRuns = getLabelRuns(row, reading.translations), .help = std::move(help), .type = type, .visibleWhen = {} });
        }
        return items;
    }

    /**
     * A page of a config spec, with the pages under it, which get its `defaultFile` and its `applies` as its
     * items do. A problem in it is on it: the caller reads it `Into` its place, with its place among the pages,
     * which takes the page back off.
     */
    model::Page loadPage(const Json& page, const Inherited& above, ConfigSpecReading& reading)
    {
        reading.at.pages.push_back(common::trim(getShownText(page, "pageDisplayName", reading.translations)));
        checkFields(page, PAGE_FIELDS, reading);
        model::Page loaded{ .name = readShownText(page, "pageDisplayName", reading),
            .help = readShownText(page, "help", reading),
            .items = {},
            .pages = {},
            .id = getText(page, "id") };
        const auto own = inherit(page, above, reading);
        loaded.items = loadItems(page, own, reading);
        if (const auto pages = page.find("pages"); pages != page.end() && pages->is_array()) {
            for (std::size_t place = 0; place < pages->size(); ++place) {
                const Into into(reading, "pages", place);
                reading.at.pagePlaces.push_back(place);
                loaded.pages.push_back(loadPage((*pages)[place], own, reading));
            }
        }
        return loaded;
    }

    using Kind = model::Condition::Kind;
    using OperatorName = std::pair<std::string_view, Kind>;

    // what a condition compares a setting's value by, by its key
    constexpr std::array OPERATORS{
        OperatorName{ "equals", Kind::Equals },
        OperatorName{ "notEquals", Kind::NotEquals },
        OperatorName{ "lessThan", Kind::LessThan },
        OperatorName{ "lessThanOrEqual", Kind::LessThanOrEqual },
        OperatorName{ "greaterThan", Kind::GreaterThan },
        OperatorName{ "greaterThanOrEqual", Kind::GreaterThanOrEqual },
        OperatorName{ "oneOf", Kind::OneOf },
        OperatorName{ "notOneOf", Kind::NotOneOf },
    };

    /**
     * Whether a JSON value can be compared with the value of a setting of a type: a bool with a bool, a
     * number with an int and a float, and a text with a string. There is no conversion between them.
     */
    bool isComparable(const Json& value, const model::StoredType type)
    {
        switch (type) {
        case model::StoredType::Bool:
            return value.is_boolean();
        case model::StoredType::Int:
        case model::StoredType::Float:
            return value.is_number();
        default:
            return value.is_string();
        }
    }

    /**
     * A JSON value that a condition compares with, in its own type: a whole number is an int, and any other
     * number a float.
     */
    model::Value conditionValue(const Json& value)
    {
        if (value.is_boolean()) {
            return value.get<bool>();
        }
        if (value.is_number_integer()) {
            return value.get<int>();
        }
        if (value.is_number()) {
            return value.get<float>();
        }
        return value.get<std::string>();
    }

    /**
     * A condition on the value of a setting: its `setting` and one operator, as `{ "setting": "iMode",
     * "oneOf": [1, 2] }`. It names the setting by its id in any case, and has the id as the config spec gives it.
     * Or the reason it is not one: a setting the config spec does not have, a value of another type than the
     * setting's, an order of a value that is no number, and a `oneOf` or a `notOneOf` of a setting that is
     * neither a text nor a choice.
     */
    std::expected<model::Condition, std::string> loadComparison(const Json& condition, const ConfigSpecReading& reading)
    {
        const auto named = getText(condition, "setting");
        const auto found = reading.settings.find(common::str_tolower(named));
        if (found == reading.settings.end()) {
            return std::unexpected(std::format("it names no setting '{}'", named));
        }
        const auto& setting = found->second;
        const auto op = std::ranges::find_if(OPERATORS, [&condition](const OperatorName& name) {
            return condition.contains(name.first);
        });
        if (op == OPERATORS.end()) {
            return std::unexpected(std::format("it has no operator for the setting '{}'", setting.id));
        }

        const auto kind = op->second;
        const bool isList = kind == Kind::OneOf || kind == Kind::NotOneOf;
        const bool isOrder = kind != Kind::Equals && kind != Kind::NotEquals && !isList;
        const bool isNumber = setting.valueType == model::StoredType::Int || setting.valueType == model::StoredType::Float;
        if (isOrder && !isNumber) {
            return std::unexpected(std::format("{} compares numbers, and '{}' is no number", op->first, setting.id));
        }
        if (isList && setting.valueType != model::StoredType::String && !isChoice(setting.type)) {
            return std::unexpected(std::format("{} is of a text or a choice, and '{}' is neither", op->first, setting.id));
        }
        const auto& operand = condition.at(op->first);
        if (isList && !operand.is_array()) {
            return std::unexpected(std::format("its {} is not a list", op->first));
        }

        model::Condition comparison{ .kind = kind, .setting = setting.id, .conditions = {}, .offWhenNotAvailable = false, .values = {} };
        for (const auto& value : isList ? operand : Json::array({ operand })) {
            if (!isComparable(value, setting.valueType)) {
                return std::unexpected(std::format("its {} has a value that is not of the type of '{}'", op->first, setting.id));
            }
            comparison.values.push_back(conditionValue(value));
        }
        return comparison;
    }

    /**
     * A condition on a state of the mod's client: its `clientState`, and one operator, as `{ "clientState":
     * "calibrating", "equals": false }`, or none, which holds while the state is on. Its type is the client's, so a
     * value of any type is compared, but an order is of a number and a `oneOf` and a `notOneOf` of a list. Or the
     * reason it is not one.
     */
    std::expected<model::Condition, std::string> loadClientState(const Json& condition)
    {
        const auto named = condition.at("clientState");
        if (!named.is_string() || named.get_ref<const std::string&>().empty()) {
            return std::unexpected("its clientState names no state"s);
        }
        model::Condition state{ .kind = Kind::On, .setting = named.get<std::string>(), .conditions = {}, .offWhenNotAvailable = false, .values = {}, .clientState = true };
        if (condition.size() == 1) {
            return state;
        }
        const auto op = std::ranges::find_if(OPERATORS, [&condition](const OperatorName& name) {
            return condition.contains(name.first);
        });
        if (op == OPERATORS.end()) {
            return std::unexpected(std::format("it has no operator for the state '{}'", state.setting));
        }
        state.kind = op->second;
        const bool isList = state.kind == Kind::OneOf || state.kind == Kind::NotOneOf;
        const bool isOrder = state.kind != Kind::Equals && state.kind != Kind::NotEquals && !isList;
        const auto& operand = condition.at(op->first);
        if (isList && !operand.is_array()) {
            return std::unexpected(std::format("its {} is not a list", op->first));
        }
        for (const auto& value : isList ? operand : Json::array({ operand })) {
            if (!value.is_boolean() && !value.is_number() && !value.is_string()) {
                return std::unexpected(std::format("its {} has a value that is not a bool, a number or a text", op->first));
            }
            if (isOrder && !value.is_number()) {
                return std::unexpected(std::format("{} compares numbers, and its value is no number", op->first));
            }
            state.values.push_back(conditionValue(value));
        }
        return state;
    }

    /**
     * A condition of a config spec, or the reason it is not one: a comparison of a setting's value, `all` and
     * `any` of a list of conditions, `not` of one, a state of the mod's client, or whether a `plugin` or an
     * `f4sePlugin` is loaded.
     * Whether a plugin is loaded does not change while the game runs, so it is answered here, as an `all` of
     * none, which holds, or an `any` of none, which does not.
     * A condition with a part that is not one is not one as a whole.
     */
    std::expected<model::Condition, std::string> loadCondition(const Json& condition, const ConfigSpecReading& reading)
    {
        if (!condition.is_object()) {
            return std::unexpected("a condition is not an object"s);
        }
        if (condition.size() == 2 && condition.contains("setting")) {
            return loadComparison(condition, reading);
        }
        if (condition.size() <= 2 && condition.contains("clientState")) {
            return loadClientState(condition);
        }
        if (condition.size() != 1) {
            return std::unexpected(std::format("{} is not one condition", condition.dump()));
        }

        const auto first = condition.begin();
        const auto& key = first.key();
        const auto& value = first.value();
        if (key == "all" || key == "any") {
            if (!value.is_array()) {
                return std::unexpected(std::format("its {} is not a list", key));
            }
            model::Condition joined{ .kind = key == "all" ? Kind::All : Kind::Any, .setting = {}, .conditions = {} };
            for (const auto& inner : value) {
                auto loaded = loadCondition(inner, reading);
                if (!loaded) {
                    return loaded;
                }
                joined.conditions.push_back(std::move(*loaded));
            }
            return joined;
        }
        if (key == "not") {
            auto inner = loadCondition(value, reading);
            if (!inner) {
                return inner;
            }
            return model::Condition{ .kind = Kind::Not, .setting = {}, .conditions = { std::move(*inner) } };
        }
        if (key == "plugin" || key == "f4sePlugin") {
            if (!value.is_string() || value.get_ref<const std::string&>().empty()) {
                return std::unexpected(std::format("its {} names no plugin", key));
            }
            const auto& check = key == "plugin" ? reading.plugins.isGamePluginLoaded : reading.plugins.isF4sePluginLoaded;
            const bool loaded = !check || check(value.get_ref<const std::string&>());
            return model::Condition{ .kind = loaded ? Kind::All : Kind::Any, .setting = {}, .conditions = {} };
        }
        if (key == "setting") {
            return std::unexpected(std::format("it has no operator for the setting '{}'", value.is_string() ? value.get<std::string>() : value.dump()));
        }
        return std::unexpected(std::format("'{}' is no condition", key));
    }

    /**
     * The condition of a key of a row, as `visibleWhen`. None for a row with none, and for one that is not a
     * condition, which is reported: the row is then shown and enabled as if it had none, so the mistake is
     * seen and nothing is hidden by it.
     */
    std::optional<model::Condition> loadRowCondition(const Json& row, const char* key, ConfigSpecReading& reading)
    {
        const auto found = row.find(key);
        if (found == row.end()) {
            return std::nullopt;
        }
        auto condition = loadCondition(*found, reading);
        if (!condition) {
            const Into into(reading, key);
            reading.report(model::Severity::Warning, std::format("Its {} is left out: {}. The row is shown and enabled as if it had none.", key, condition.error()));
            return std::nullopt;
        }
        return std::move(*condition);
    }

    /**
     * The `warnings` of a setting or a button: a text each, which the row tells while its `when` holds. One
     * with no text, and one whose `when` is not a condition, is left out, and is reported.
     */
    std::vector<model::Warning> loadWarnings(const Json& row, ConfigSpecReading& reading)
    {
        std::vector<model::Warning> warnings;
        const auto found = row.find("warnings");
        if (found == row.end()) {
            return warnings;
        }
        const Into into(reading, "warnings");
        if (!found->is_array()) {
            reading.report(model::Severity::Error, "Its warnings are not a list: they are left out.");
            return warnings;
        }
        for (std::size_t place = 0; place < found->size(); ++place) {
            const auto& warning = (*found)[place];
            const Into intoWarning(reading, "", place);
            checkFields(warning, WARNING_FIELDS, reading);
            auto text = warning.is_object() ? readShownText(warning, "text", reading) : std::string();
            if (text.empty()) {
                reading.report(model::Severity::Error, std::format("Warning {} has no text: it is left out.", place + 1));
                continue;
            }
            const auto when = warning.find("when");
            auto condition = when != warning.end() ? loadCondition(*when, reading) : std::unexpected("it has no when"s);
            if (!condition) {
                reading.report(model::Severity::Error, std::format("Warning {} is left out: {}.", place + 1, condition.error()));
                continue;
            }
            warnings.push_back(model::Warning{ .when = std::move(*condition), .text = std::move(text) });
        }
        return warnings;
    }

    /**
     * A condition that needs both: an All of the two, or the one there is.
     */
    std::optional<model::Condition> join(const std::optional<model::Condition>& above, std::optional<model::Condition> own)
    {
        if (!above || !own) {
            return above ? above : own;
        }
        return model::Condition{ .kind = Kind::All, .setting = {}, .conditions = { *above, std::move(*own) } };
    }

    /**
     * What a row is shown and enabled by: its own conditions, and those of the sections it is in.
     */
    struct RowConditions
    {
        std::optional<model::Condition> visibleWhen;
        std::optional<model::Condition> enabledWhen;
    };

    /**
     * Gives the items of the `content` of a page or a section their conditions: a row's `visibleWhen` and
     * `enabledWhen`, each joined with those of the sections it is in, so a row needs every condition above
     * it, and the warnings of a setting and a button. A section's heading is shown by its `visibleWhen` too.
     * The items are the rows of the content, one
     * for each, in their order, as `loadItems` reads them.
     */
    void loadConditions(const Json& container, std::vector<model::Item>& items, const RowConditions& above, ConfigSpecReading& reading)
    {
        const auto content = container.find("content");
        if (content == container.end() || !content->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < content->size() && place < items.size(); ++place) {
            const auto& row = (*content)[place];
            auto& item = items[place];
            const Into into(reading, "content", place);
            reading.at.row = rowName(row, reading.translations);
            reading.at.itemPlaces.push_back(place);
            const RowConditions own{ .visibleWhen = join(above.visibleWhen, loadRowCondition(row, "visibleWhen", reading)),
                .enabledWhen = join(above.enabledWhen, loadRowCondition(row, "enabledWhen", reading)) };
            std::visit(
                [&own](auto& loaded) {
                    loaded.visibleWhen = own.visibleWhen;
                },
                item);
            if (auto* setting = std::get_if<model::Setting>(&item)) {
                setting->enabledWhen = own.enabledWhen;
                setting->warnings = loadWarnings(row, reading);
            } else if (auto* button = std::get_if<model::Button>(&item)) {
                button->enabledWhen = own.enabledWhen;
                button->warnings = loadWarnings(row, reading);
            } else if (auto* group = std::get_if<model::Group>(&item)) {
                loadConditions(row, group->items, own, reading);
            }
        }
    }

    /**
     * Gives a page, its items and its child pages their conditions, including those of its parent pages.
     */
    void loadPageConditions(const Json& page, model::Page& loaded, ConfigSpecReading& reading, const RowConditions& above = {})
    {
        reading.at.pages.push_back(common::trim(loaded.name));
        loaded.visibleWhen = join(above.visibleWhen, loadRowCondition(page, "visibleWhen", reading));
        loaded.enabledWhen = join(above.enabledWhen, loadRowCondition(page, "enabledWhen", reading));
        const RowConditions own{ .visibleWhen = loaded.visibleWhen, .enabledWhen = loaded.enabledWhen };
        loadConditions(page, loaded.items, own, reading);
        const auto pages = page.find("pages");
        if (pages == page.end() || !pages->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < pages->size() && place < loaded.pages.size(); ++place) {
            const Into into(reading, "pages", place);
            reading.at.pagePlaces.push_back(place);
            loadPageConditions((*pages)[place], loaded.pages[place], reading, own);
        }
    }

    /**
     * Whether a game plugin is loaded in the running game: one that is in the Data folder and not enabled is not.
     */
    bool isGamePluginLoaded(const std::string_view plugin)
    {
        auto* const dataHandler = RE::TESDataHandler::GetSingleton();
        return dataHandler && dataHandler->LookupLoadedFile(plugin).first;
    }

    void ignoreMessage(F4SE::MessagingInterface::Message*)
    {}

    /**
     * Whether an F4SE plugin is loaded, by the name it registers with F4SE. F4SE VR has no call that asks it,
     * so the mod registers a listener of the plugin's messages, which F4SE refuses for a plugin that is not
     * loaded. The listener does nothing, and stays: F4SE keeps the first listener the mod registers for a
     * plugin, so the mod cannot listen to the messages of a plugin it asked of.
     */
    bool isF4sePluginLoaded(const std::string_view plugin)
    {
        const auto* messaging = F4SE::GetMessagingInterface();
        const std::string name(plugin);
        return messaging && messaging->RegisterListener(ignoreMessage, name);
    }

    /**
     * A path as UTF-8 text for the log. A path's own string() throws on a name the system's code page lacks.
     */
    std::string toLogText(const std::filesystem::path& path)
    {
        const auto text = path.u8string();
        return { text.begin(), text.end() };
    }

    /**
     * Names as a reason says them: `A`, `A and B`, `A, B and C`.
     */
    std::string joinNames(const std::vector<std::string>& names)
    {
        std::string joined;
        for (std::size_t i = 0; i < names.size(); ++i) {
            joined += (i == 0 ? "" : i + 1 == names.size() ? " and " : ", ") + names[i];
        }
        return joined;
    }

    /**
     * The plugins a list of a config spec's requirements names, in its order: none where it has no list, and an entry
     * that is no text is left out.
     */
    std::vector<std::string> requiredPlugins(const Json& configSpec, const char* key)
    {
        std::vector<std::string> plugins;
        const auto required = configSpec.find(key);
        if (required == configSpec.end() || !required->is_array()) {
            return plugins;
        }
        for (const auto& plugin : *required) {
            if (plugin.is_string()) {
                plugins.push_back(plugin.get<std::string>());
            }
        }
        return plugins;
    }

    /**
     * The plugins of a list of a config spec's requirements that are not loaded, in its order. None with no check.
     */
    std::vector<std::string> findMissing(const Json& configSpec, const char* key, const spec::PluginCheck& isLoaded)
    {
        std::vector<std::string> missing;
        if (!isLoaded) {
            return missing;
        }
        for (auto& plugin : requiredPlugins(configSpec, key)) {
            if (!isLoaded(plugin)) {
                missing.push_back(std::move(plugin));
            }
        }
        return missing;
    }

    /**
     * Why a config spec's mod is not available while a plugin it requires is not loaded: a game plugin of its
     * `pluginRequirements` or an F4SE plugin of its `f4sePluginRequirements`, as `its plugin MyMod.esp and
     * its F4SE plugin MyMod are not loaded`. Empty while every one is, and with no checks, as in a test of a
     * config spec alone.
     */
    std::string findMissingRequirements(const Json& configSpec, const spec::PluginChecks& plugins)
    {
        const auto game = findMissing(configSpec, "pluginRequirements", plugins.isGamePluginLoaded);
        const auto f4se = findMissing(configSpec, "f4sePluginRequirements", plugins.isF4sePluginLoaded);
        std::vector<std::string> parts;
        if (!game.empty()) {
            parts.push_back(std::format("its plugin{} {}", game.size() == 1 ? "" : "s", joinNames(game)));
        }
        if (!f4se.empty()) {
            parts.push_back(std::format("its F4SE plugin{} {}", f4se.size() == 1 ? "" : "s", joinNames(f4se)));
        }
        if (parts.empty()) {
            return {};
        }
        return std::format("{} {} not loaded", joinNames(parts), game.size() + f4se.size() == 1 ? "is" : "are");
    }

    /**
     * The `minSchemaVersion` of a config spec: a whole number from 1. None for a config spec that has none.
     */
    std::optional<std::int64_t> findSchemaVersion(const Json& configSpec)
    {
        const auto version = configSpec.find("minSchemaVersion");
        if (version == configSpec.end() || !version->is_number_integer() || version->get<std::int64_t>() < 1) {
            return std::nullopt;
        }
        return version->get<std::int64_t>();
    }

    /**
     * Checks the options of the config spec's shared lists once, as several choices can read one: the fields of an
     * option, and its texts. An option that is not one of a choice is reported as the choice reads the list.
     */
    void checkSharedLists(ConfigSpecReading& reading)
    {
        if (!reading.sharedLists) {
            return;
        }
        const Into into(reading, "sharedLists");
        for (const auto& [name, list] : reading.sharedLists->items()) {
            if (!list.is_array()) {
                continue;
            }
            const Into intoList(reading, name);
            for (std::size_t place = 0; place < list.size(); ++place) {
                const Into intoOption(reading, "", place);
                if (list[place].is_object()) {
                    checkFields(list[place], OPTION_FIELDS, reading);
                    std::ignore = readShownText(list[place], "text", reading);
                } else {
                    std::ignore = readShownText(list[place], reading);
                }
            }
        }
    }

    /**
     * Whether a problem is before another in spec.json, by their paths: a place in a list is compared as a
     * number, so `content[2]` is before `content[10]`, and a path is before the paths in it. The keys of an
     * object have no order in the file the reader keeps, so they are in the order of their names.
     */
    bool isBefore(const model::Problem& problem, const model::Problem& other)
    {
        // the parts of a path: the names, and the places, which are numbers
        const auto parts = [](const std::string& path) {
            std::vector<std::variant<std::size_t, std::string>> parts;
            std::size_t at = 0;
            while (at < path.size()) {
                if (path[at] == '.') {
                    ++at;
                } else if (path[at] == '[') {
                    const auto end = path.find(']', at);
                    std::size_t place = 0;
                    std::from_chars(path.data() + at + 1, path.data() + end, place);
                    parts.emplace_back(place);
                    at = end + 1;
                } else {
                    const auto end = path.find_first_of(".[", at);
                    parts.emplace_back(path.substr(at, end - at));
                    at = end == std::string::npos ? path.size() : end;
                }
            }
            return parts;
        };
        return parts(problem.path) < parts(other.path);
    }

    /**
     * A count of a thing as the log says it: `1 error`, `2 errors`.
     */
    std::string countOf(const std::size_t count, const std::string_view thing)
    {
        return std::format("{} {}{}", count, thing, count == 1 ? "" : "s");
    }

    /**
     * Logs each problem of a config spec, with the config spec's name first, where it is after it, and how many
     * errors and warnings the config spec has.
     */
    void logProblems(const model::Mod& mod)
    {
        std::size_t errors = 0;
        for (const auto& problem : mod.problems) {
            const bool error = problem.severity == model::Severity::Error;
            errors += error ? 1 : 0;
            auto where = problem.where;
            if (!problem.path.empty()) {
                where += (where.empty() ? "" : ", ") + problem.path;
            }
            logger::warn("Config spec '{}': {} of {}: {}{}", mod.displayName, error ? "error" : "warning", problem.row, problem.message, where.empty() ? "" : " (" + where + ")");
        }
        logger::info("Config spec '{}' has {} and {}", mod.displayName, countOf(errors, "error"), countOf(mod.problems.size() - errors, "warning"));
    }

    /**
     * A mod that is not available, which has its names and the reason alone.
     */
    model::Mod notAvailableMod(std::string name, std::string displayName, std::string reason)
    {
        return model::Mod{ .name = std::move(name),
            .displayName = std::move(displayName),
            .frontPage = {},
            .pages = {},
            .notAvailable = std::move(reason),
            .help = {},
            .source = model::Source::ConfigSpec,
            .files = {} };
    }
}

namespace mods_config::spec
{
    /**
     * Reads the text of a config spec's `spec.json` into the model: the mod and its help, its files, its front
     * page and its pages, which nest, with their help, and their sections, which nest and fold, with their
     * help, settings, buttons, texts, spacers and images. Every
     * text the player sees is in the game's language. The config spec's root has to say its `minSchemaVersion`. A
     * setting has the file its value is kept in, and when its change takes effect, which the config spec's root has
     * to say. A choice has its options or a list of
     * the config spec's `sharedLists`. A row is shown and enabled by its conditions, which are read once every
     * setting is, as a condition can name a setting after it. A condition on a plugin is answered by the checks.
     * A mod whose config spec needs a newer schema than the menu reads, or whose required plugins are not all
     * loaded, is not available, and is its names and the reason alone: the rest of its config spec is not read.
     * The error says why a config spec cannot be read. Comments are skipped, as in an MCM config.
     * With the folders of its files, the files that are there are read, and a setting whose path its file does not
     * have is a warning.
     */
    std::expected<model::Mod, std::string> loadConfigSpec(const std::string_view text, const model::Translations& translations, const PluginChecks& plugins,
        const FileFolders& folders)
    {
        Json configSpec;
        try {
            configSpec = Json::parse(text, nullptr, true, true);
        } catch (const Json::parse_error& e) {
            return std::unexpected(std::format("Not valid JSON: {}", e.what()));
        }
        if (!configSpec.is_object()) {
            return std::unexpected("Not a JSON object"s);
        }

        model::Mod mod;
        mod.name = getText(configSpec, "modName");
        if (mod.name.empty()) {
            return std::unexpected("No modName"s);
        }
        const auto schemaVersion = findSchemaVersion(configSpec);
        if (!schemaVersion) {
            return std::unexpected("No minSchemaVersion of a whole number from 1"s);
        }
        const auto sharedLists = configSpec.find("sharedLists");
        ConfigSpecReading reading{ .translations = translations,
            .files = {},
            .sharedLists = sharedLists != configSpec.end() && sharedLists->is_object() ? &*sharedLists : nullptr,
            .plugins = plugins,
            .settings = {},
            .problems = {},
            .at = {} };
        mod.source = model::Source::ConfigSpec;
        mod.displayName = readShownText(configSpec, "displayName", reading);
        if (mod.displayName.empty()) {
            mod.displayName = mod.name;
        }
        // a newer schema can say anything in another way, so nothing more of it is read
        auto reason = *schemaVersion > SCHEMA_VERSION ? std::format("its spec.json needs a newer Mods Config FO4 VR, one that reads schema version {}", *schemaVersion)
                                                      : findMissingRequirements(configSpec, plugins);
        if (!reason.empty()) {
            return notAvailableMod(std::move(mod.name), std::move(mod.displayName), std::move(reason));
        }
        checkFields(configSpec, ROOT_FIELDS, reading);
        mod.help = readShownText(configSpec, "help", reading);
        mod.f4sePlugins = requiredPlugins(configSpec, "f4sePluginRequirements");
        reading.files = loadFiles(configSpec, reading);
        reading.fileValues = readFiles(reading.files, folders);
        checkSharedLists(reading);

        // a setting that nothing says it of takes effect at a time the menu does not know
        const auto root = inherit(configSpec, Inherited{}, reading);
        mod.frontPage.items = loadItems(configSpec, root, reading);
        loadConditions(configSpec, mod.frontPage.items, {}, reading);
        if (const auto pages = configSpec.find("pages"); pages != configSpec.end() && pages->is_array()) {
            for (std::size_t place = 0; place < pages->size(); ++place) {
                const Into into(reading, "pages", place);
                reading.at.pagePlaces.push_back(place);
                mod.pages.push_back(loadPage((*pages)[place], root, reading));
            }
            for (std::size_t place = 0; place < pages->size(); ++place) {
                const Into into(reading, "pages", place);
                reading.at.pagePlaces.push_back(place);
                loadPageConditions((*pages)[place], mod.pages[place], reading);
            }
        }
        mod.files = std::move(reading.files);
        // the conditions are read after every row, and their problems are put in their places
        std::ranges::stable_sort(reading.problems, isBefore);
        mod.problems = std::move(reading.problems);
        return mod;
    }

    /**
     * Loads the `spec.json` of every folder in the config specs' folder, which is `Data\ModsConfigFO4\Config` in
     * the game. A config spec that cannot be read is a mod that is not available, named by its folder, and so
     * are two config specs with one `modName`: neither is picked. Both are a problem of the config spec. A mod that is
     * not available has the reason in the log, and so has each problem of a config spec, with the config spec's name
     * Reads a config spec's `spec.json` from its file, as `loadConfigSpec` reads its text. The error says why it
     * cannot be read, also for a file that cannot be opened.
     */
    std::expected<model::Mod, std::string> loadConfigSpecFile(const std::filesystem::path& file, const model::Translations& translations, const PluginChecks& plugins,
        const FileFolders& folders)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream) {
            return std::unexpected("The file cannot be opened"s);
        }
        std::ostringstream text;
        text << stream.rdbuf();
        return loadConfigSpec(text.str(), translations, plugins, folders);
    }

    /**
     * The mod of a config spec whose `spec.json` cannot be read: not available, named by its folder, with the
     * reason as its error.
     */
    model::Mod notLoadedConfigSpec(const std::string& folder, const std::string& reason)
    {
        auto mod = notAvailableMod(folder, folder, "its spec.json cannot be read");
        mod.problems.push_back({ .severity = model::Severity::Error, .row = SPEC_FILE, .message = std::format("It cannot be read: {}.", reason), .where = {}, .path = {} });
        mod.folder = folder;
        return mod;
    }

    /**
     * Logs why a config spec's mod is not available, each of its problems, with the config spec's name first, and
     * how many errors and warnings it has.
     */
    void logConfigSpec(const model::Mod& mod)
    {
        if (!mod.notAvailable.empty()) {
            logger::warn("Config spec '{}' is not available: {}", mod.name, mod.notAvailable);
        }
        logProblems(mod);
    }

    /**
     * The folders of the running game a config spec's files are read from: the game's Data folder, and
     * `Documents\My Games\Fallout4VR`.
     */
    FileFolders gameFileFolders()
    {
        return { .data = "Data", .myGames = common::getRelativePathInDocuments(R"(\My Games\Fallout4VR)") };
    }

    /**
     * The checks of the running game: a game plugin it loaded, and an F4SE plugin F4SE loaded.
     */
    PluginChecks gamePluginChecks()
    {
        return { .isGamePluginLoaded = isGamePluginLoaded, .isF4sePluginLoaded = isF4sePluginLoaded };
    }

    /**
     * Loads the `spec.json` of every folder in the config specs' folder, which is `Data\ModsConfigFO4\Config` in
     * the game. A config spec that cannot be read is a mod that is not available, named by its folder, and so
     * are two config specs with one `modName`: neither is picked. Both are a problem of the config spec. A mod that is
     * not available has the reason in the log, and so has each problem of a config spec, with the config spec's name
     * first, and how many errors and warnings each config spec has. No such folder is no mods.
     */
    std::vector<model::Mod> loadConfigSpecs(const std::filesystem::path& configFolder, const model::Translations& translations, const PluginChecks& plugins,
        const FileFolders& folders)
    {
        std::vector<model::Mod> mods;
        std::error_code error;
        for (std::filesystem::directory_iterator folder(configFolder, error), end; !error && folder != end; folder.increment(error)) {
            const auto file = folder->path() / SPEC_FILE;
            std::error_code noFile;
            if (!std::filesystem::is_regular_file(file, noFile)) {
                continue;
            }
            auto mod = loadConfigSpecFile(file, translations, plugins, folders);
            const auto folderName = toLogText(folder->path().filename());
            if (!mod) {
                mod = notLoadedConfigSpec(folderName, mod.error());
            }
            mod->folder = folderName;
            mods.push_back(std::move(*mod));
        }

        std::map<std::string, int> names;
        for (const auto& mod : mods) {
            ++names[common::str_tolower(mod.name)];
        }
        for (auto& mod : mods) {
            if (mod.notAvailable.empty() && names[common::str_tolower(mod.name)] > 1) {
                auto folder = std::move(mod.folder);
                mod = notAvailableMod(mod.name, mod.displayName, "another config spec has its modName");
                mod.folder = std::move(folder);
                mod.problems.push_back({ .severity = model::Severity::Error,
                    .row = SPEC_FILE,
                    .message = std::format("Another config spec has the modName \"{}\": neither is read.", mod.name),
                    .where = {},
                    .path = "modName" });
            }
            logConfigSpec(mod);
        }
        return mods;
    }

    /**
     * The config specs of the running game, from `Data\ModsConfigFO4\Config` in the game's folder, with their
     * conditions on a plugin answered by the plugins the game loaded.
     */
    std::vector<model::Mod> loadInstalledConfigSpecs(const model::Translations& translations)
    {
        auto mods = loadConfigSpecs(CONFIG_SPEC_FOLDER, translations, gamePluginChecks(), gameFileFolders());
        logger::info("Loaded {} config specs from '{}'", mods.size(), CONFIG_SPEC_FOLDER);
        return mods;
    }

    /**
     * Every mod the menu lists: the MCM mods and then the config specs. A mod that has both is shown by its config spec
     * alone, matched by its `modName` without regard to case, also one that is not available: its MCM menu is left out.
     */
    std::vector<model::Mod> withConfigSpecs(std::vector<model::Mod> mcmMods, std::vector<model::Mod> configSpecs)
    {
        std::set<std::string> names;
        for (const auto& mod : configSpecs) {
            names.insert(common::str_tolower(mod.name));
        }
        std::erase_if(mcmMods, [&names](const model::Mod& mod) {
            if (!names.contains(common::str_tolower(mod.name))) {
                return false;
            }
            logger::info("The MCM menu of '{}' is left out: its config spec is listed", mod.name);
            return true;
        });
        std::ranges::move(configSpecs, std::back_inserter(mcmMods));
        return mcmMods;
    }
}
