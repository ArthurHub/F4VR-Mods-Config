#include "ConfigSpecReading.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <format>

#include "common/CommonUtils.h"
#include "model/TextMarkup.h"

namespace mods_config::spec
{
    namespace
    {
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

        // a text the player sees, written as an object
        constexpr std::array<std::string_view, 2> TEXT_FIELDS{ "token", "fallback" };

        constexpr std::array<std::string_view, 9> STORAGE_FIELDS{ "file", "section", "key", "path", "global", "form", "scriptName", "propertyName", "clientState" };

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
    }

    /**
     * The text of a key. It is empty when the value has no such key, or the key holds something else than a text.
     */
    std::string getText(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_string() ? found->get<std::string>() : std::string();
    }

    /**
     * Whether a key is true. False for a value with no such key, or with one that holds something else than a bool.
     */
    bool getBool(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_boolean() && found->get<bool>();
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
     * Reports each field of an object that is none of the ones it can have, which is not read: a typo, or a
     * field of a newer schema. A field near one it can have names it, as `Did you mean "visibleWhen"?`. A row the loader
     * made is not its author's: what its author wrote of it is checked as the row is made.
     */
    void checkFields(const Json& object, const std::span<const std::string_view> known, ConfigSpecReading& reading)
    {
        if (!object.is_object() || object.contains(MADE_ROW_PATH)) {
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

    /**
     * What is wrong with a path from a root of a config spec, as the end of a sentence of it: one that is absolute, or
     * goes up with `..`, could be any file of the system. Empty for one that is a path from its root.
     */
    std::string pathProblem(const std::string& path)
    {
        const std::filesystem::path checked(std::u8string(path.begin(), path.end()));
        if (path.empty() || checked.has_root_name() || checked.has_root_directory()) {
            return std::format("its path '{}' is not one from its root", path);
        }
        if (std::ranges::any_of(checked, [](const std::filesystem::path& part) {
                return part == "..";
            })) {
            return std::format("its path '{}' goes up out of its folder", path);
        }
        return {};
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
     * a state of the mod's client, by its `clientState`, or a value of the file its `storage` names, or else of the
     * nearest `defaultFile` above it. The value of a file is named by its `path`, or by its `section` and its `key`,
     * as an INI key is. A storage that is not written as one, as a form that is not a plugin and an id in hex, a
     * property or a state with no name, or no file of the config spec or no value of it, as a path of more than two
     * in an INI file, is one the menu does not read, and is reported.
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
        if (storage->contains("clientState")) {
            if (auto name = getText(*storage, "clientState"); !name.empty()) {
                return model::ClientStateStorage{ .name = std::move(name) };
            }
            reading.report(model::Severity::Error, "Its clientState names no state of its mod's client: it is not available.");
            return model::UnknownStorage{ .type = "clientState" };
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
     * its form, as `MyMod.esp|1234`, a property of a script its name and its form, as `fStrength:MyMod.esp|800`, and a
     * state of the mod's client its name. Empty for a storage the menu does not read.
     */
    std::string idOf(const model::Storage& storage)
    {
        if (const auto* state = std::get_if<model::ClientStateStorage>(&storage)) {
            return state->name;
        }
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
}
