#pragma once

#include <cstdint>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "ConfigSpecLoader.h"
#include "ModFile.h"
#include "model/ConfigDefinition.h"
#include "model/Translations.h"

// What the parts of the config spec loader share: the reading of a config spec, where it is, and the helpers each part
// reads its rows with. Only the loader's own files include it.
namespace mods_config::spec
{
    using Json = nlohmann::json;

    // what a problem of the config spec itself names
    constexpr auto SPEC_FILE = "spec.json";

    // what stands between the pages and the row a problem is on: U+203A, as its bytes in UTF-8
    constexpr auto WHERE_SEPARATOR = " \xE2\x80\xBA ";

    // A row the loader makes for a key of an activation sphere has this field, which is no field of the schema: where it
    // is in spec.json from the sphere, as `keys.tZone`, and empty for a section of them, which is not in spec.json
    constexpr auto MADE_ROW_PATH = "madeRowPath";

    // the field of a made row that says which of the framework's words its dropdown has
    constexpr auto MADE_WORDS = "madeWords";

    /**
     * What a condition knows of the setting it names: its id as its config spec has it, and what its value is. And what
     * an option that sets it checks its value by: its table's form, its value options, and whether it is a choice that
     * sets other settings itself, which no option sets.
     */
    struct SettingInfo
    {
        std::string id;
        model::SettingType type = model::SettingType::Switcher;
        model::StoredType valueType = model::StoredType::Bool;
        model::TableForm tableForm = model::TableForm::Toml;
        model::ValueOptions valueOptions;
        bool setsOthers = false;
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

        // the config spec's `modName`, which names the folders of a CommonFramework mod's assets
        std::string modName;

        // the `sharedLists` and the `sharedPresetLists` of its root. None for a config spec with none
        const Json* sharedLists = nullptr;
        const Json* sharedPresetLists = nullptr;
        const spec::PluginChecks& plugins;

        // where its files and its choices of files are, which a folder with no files is reported by. None for a config
        // spec read alone
        spec::FileFolders folders;

        // the files of the config spec that are there, read as they are now, by their names: a setting's path that
        // one of them does not have is reported
        std::map<std::string, std::unique_ptr<spec::ModFile>> fileValues;
        std::unordered_map<std::string, SettingInfo> settings;
        std::vector<model::Problem> problems;
        Location at;

        // the section each `cf.activationSphere` row was made into, as every reading of the rows reads it
        std::map<const Json*, Json> spheres;

        // the ids of its sections, spheres and buttons so far, in lower case, apart from its settings': no condition
        // names them
        std::set<std::string, std::less<>> itemIds;

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
     * What the items of a page or a section get from above them: the file a setting is kept in where its
     * storage names none, and when its change takes effect.
     */
    struct Inherited
    {
        std::string defaultFile;
        model::Applies applies = model::Applies::Unknown;
    };

    std::string getText(const Json& value, const char* key);
    bool getBool(const Json& value, const char* key);
    std::string getLabel(const Json& row, ConfigSpecReading& reading);
    std::vector<model::TextRun> getLabelRuns(const Json& row, const model::Translations& translations);
    std::string getShownText(const Json& value, const char* key, const model::Translations& translations);
    std::string readShownText(const Json& text, ConfigSpecReading& reading);
    std::string readShownText(const Json& value, const char* key, ConfigSpecReading& reading);
    void checkFields(const Json& object, std::span<const std::string_view> known, ConfigSpecReading& reading);
    std::optional<model::Applies> findApplies(const Json& value);
    Inherited inherit(const Json& value, const Inherited& above, ConfigSpecReading& reading);
    std::optional<std::pair<std::string, std::uint32_t>> parseForm(const std::string& text);
    model::Storage loadStorage(const Json& row, const std::string& defaultFile, model::StoredType valueType, ConfigSpecReading& reading);
    std::string idOf(const model::Storage& storage);
}
