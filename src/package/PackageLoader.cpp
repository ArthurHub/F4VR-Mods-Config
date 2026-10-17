#include "PackageLoader.h"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "common/CommonUtils.h"

using namespace mods_config;

namespace
{
    using Json = nlohmann::json;

    // where the packages are, from the game's folder: a folder each, with its `config.json`
    constexpr auto PACKAGE_CONFIG_FOLDER = R"(Data\ModsConfigVR\Config)";

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
     * The `applies` of a package, a page, a section or a setting. None for one that has none, and for one
     * that is not `live`, `save` or `restart`.
     */
    std::optional<model::Applies> findApplies(const Json& value)
    {
        const auto text = getText(value, "applies");
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
        model::Applies applies = model::Applies::Live;
    };

    /**
     * What a page, a section or a setting has for what is under it: its own `defaultFile` and `applies`, or
     * else what it gets from above. An `applies` that is not one is passed over, with the reason in the log.
     */
    Inherited inherit(const Json& value, const Inherited& above)
    {
        Inherited own = above;
        if (auto file = getText(value, "defaultFile"); !file.empty()) {
            own.defaultFile = std::move(file);
        }
        if (const auto applies = findApplies(value)) {
            own.applies = *applies;
        } else if (value.contains("applies")) {
            logger::warn("The applies of '{}' is not live, save or restart", getText(value, "text"));
        }
        return own;
    }

    using FormatName = std::pair<std::string_view, model::FileFormat>;

    // what a file of a package is written in, by its `format`, which is also its extension
    constexpr std::array FILE_FORMATS{
        FormatName{ "ini", model::FileFormat::Ini },
        FormatName{ "json", model::FileFormat::Json },
        FormatName{ "toml", model::FileFormat::Toml },
    };

    /**
     * A file of a package's `files`, or the reason it is not one: a `format` of `ini`, `json` or `toml`,
     * which its extension is, a `root` of `data` or `myGames`, and a `path` from that root. The path is
     * checked as it is written: one that is absolute, or goes up with `..`, could be any file of the system.
     */
    std::expected<model::File, std::string> loadFile(const Json& declared)
    {
        const auto formatText = getText(declared, "format");
        const auto format = std::ranges::find(FILE_FORMATS, formatText, &FormatName::first);
        if (format == FILE_FORMATS.end()) {
            return std::unexpected(std::format("its format '{}' is not ini, json or toml", formatText));
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
            return std::unexpected(std::format("its path '{}' goes up with ..", path));
        }
        const auto extension = checked.extension().u8string();
        if (common::str_tolower(std::string(extension.begin(), extension.end())) != "." + formatText) {
            return std::unexpected(std::format("its path '{}' does not end in .{}", path, formatText));
        }
        return model::File{ .format = format->second, .root = rootText == "data" ? model::FileRoot::Data : model::FileRoot::MyGames, .path = std::move(path) };
    }

    /**
     * The files of a package, by the names its `files` gives them. A file that is not written as one is
     * left out, with the reason in the log, so its settings are not available.
     */
    std::map<std::string, model::File> loadFiles(const Json& package)
    {
        std::map<std::string, model::File> files;
        const auto found = package.find("files");
        if (found == package.end() || !found->is_object()) {
            return files;
        }
        for (const auto& [name, declared] : found->items()) {
            auto file = declared.is_object() ? loadFile(declared) : std::unexpected("it is not an object"s);
            if (file) {
                files.emplace(name, std::move(*file));
            } else {
                logger::warn("The file '{}' of package '{}' is left out: {}", name, getText(package, "modName"), file.error());
            }
        }
        return files;
    }

    /**
     * What a package is read with: the game's translations, its files, and the ids its settings have so
     * far, in lower case, as ids are compared without regard to case.
     */
    struct PackageReading
    {
        const model::Translations& translations;
        const std::map<std::string, model::File>& files;
        std::unordered_set<std::string> takenIds;
    };

    /**
     * The name a package gives one of its files, as the package declares it: a storage can name it in
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
     * Where a setting keeps its value: a value of the file its `storage` names, or else of the nearest
     * `defaultFile` above it. The value is named by its `path`, or by its `section` and its `key`, as an INI
     * key is. A storage that names no file of the package or no value, and a global or a property, are ones
     * the menu does not read, with the reason in the log.
     */
    model::Storage loadStorage(const Json& row, const std::string& defaultFile, const PackageReading& reading)
    {
        const auto storage = row.find("storage");
        if (storage == row.end() || !storage->is_object()) {
            logger::warn("The setting '{}' has no storage", getText(row, "text"));
            return model::UnknownStorage{};
        }
        if (storage->contains("global") || storage->contains("form")) {
            logger::warn("The setting '{}' is kept in a global or a property, which a package's menu does not read", getText(row, "text"));
            return model::UnknownStorage{ .type = storage->contains("global") ? "global" : "property" };
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
        if (!file || path.empty()) {
            logger::warn("The setting '{}' names no file of its package, or no value of it", getText(row, "text"));
            return model::UnknownStorage{ .type = "file" };
        }
        return model::FileStorage{ .file = *file, .path = std::move(path) };
    }

    /**
     * The id a setting gets from where its value is kept: the last name of its path, and the names before it
     * joined by dots, as `fScale:Panel` for `Panel`, `fScale`. A path of two is named as MCM names a key of an
     * INI file, so a mod that moves from INI to TOML keeps its ids. A path of one is its name alone. Empty for
     * a storage the menu does not read.
     */
    std::string idOf(const model::Storage& storage)
    {
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
        SettingTypeName{ "textinput", model::SettingType::TextInput },
    };

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
     * type of its control: a bool for a switcher, a number for a slider, and a text for a text input.
     */
    model::StoredType loadValueType(const Json& row, const model::SettingType type)
    {
        const auto found = std::ranges::find(VALUE_TYPES, getText(row, "valueType"), &ValueTypeName::first);
        if (found != VALUE_TYPES.end()) {
            return found->second;
        }
        if (type == model::SettingType::Switcher) {
            return model::StoredType::Bool;
        }
        return type == model::SettingType::Slider ? model::StoredType::Float : model::StoredType::String;
    }

    /**
     * The `valueOptions` of a setting: the least, the most and the step of a number.
     */
    model::ValueOptions loadValueOptions(const Json& row)
    {
        model::ValueOptions valueOptions;
        const auto found = row.find("valueOptions");
        if (found != row.end() && found->is_object()) {
            valueOptions.min = getNumber(*found, "min", valueOptions.min);
            valueOptions.max = getNumber(*found, "max", valueOptions.max);
            valueOptions.step = getNumber(*found, "step", valueOptions.step);
        }
        return valueOptions;
    }

    /**
     * The `default` of a setting, in its value's type: a JSON bool for a bool, a whole number for an int, any
     * number for a float, and a text for a string. None for a setting with no default of its type, which is an
     * error of the package that the log has.
     */
    std::optional<model::Value> loadDefault(const Json& row, const model::StoredType type)
    {
        const auto found = row.find("default");
        if (found != row.end()) {
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
        }
        logger::warn("The setting '{}' has no default of the type of its value", getText(row, "text"));
        return std::nullopt;
    }

    /**
     * A setting of a package: its author's `id`, or one it gets from where its value is kept, and its own
     * `applies` or the nearest above it, and its `default`. Two settings with one id are an error of the package, which the log
     * has: an author gives one of them an id of its own.
     */
    model::Setting loadSetting(const Json& row, const model::SettingType type, const Inherited& above, PackageReading& reading)
    {
        const auto own = inherit(row, above);
        auto storage = loadStorage(row, own.defaultFile, reading);
        auto id = getText(row, "id");
        if (id.empty()) {
            id = idOf(storage);
        }
        if (!id.empty() && !reading.takenIds.insert(common::str_tolower(id)).second) {
            logger::warn("Two settings have the id '{}': give one of them an id of its own", id);
        }
        const auto valueType = loadValueType(row, type);
        return model::Setting{ .id = std::move(id),
            .sourceId = {},
            .type = type,
            .text = getShownText(row, "text", reading.translations),
            .textRuns = {},
            .help = getShownText(row, "help", reading.translations),
            .valueOptions = loadValueOptions(row),
            .storage = std::move(storage),
            .valueType = valueType,
            .action = {},
            .visibleWhen = {},
            .applies = own.applies,
            .defaultValue = loadDefault(row, valueType) };
    }

    /**
     * The items of the `content` of a page or a section, by their `type`. A section is a group of the items
     * of its own `content`, and passes its `defaultFile` and its `applies` down to them. Any other row the
     * menu does not have is a placeholder with its type.
     */
    std::vector<model::Item> loadItems(const Json& container, const Inherited& above, PackageReading& reading)
    {
        std::vector<model::Item> items;
        const auto content = container.find("content");
        if (content == container.end() || !content->is_array()) {
            return items;
        }
        for (const auto& row : *content) {
            const auto type = getText(row, "type");
            if (type == "section") {
                items.emplace_back(
                    model::Group{ .text = getShownText(row, "text", reading.translations), .items = loadItems(row, inherit(row, above), reading), .visibleWhen = {} });
                continue;
            }
            if (const auto setting = std::ranges::find(SETTING_TYPES, type, &SettingTypeName::first); setting != SETTING_TYPES.end()) {
                items.emplace_back(loadSetting(row, setting->second, above, reading));
                continue;
            }
            items.emplace_back(model::Placeholder{ .text = getShownText(row, "text", reading.translations),
                .textRuns = {},
                .help = getShownText(row, "help", reading.translations),
                .type = type,
                .visibleWhen = {} });
        }
        return items;
    }

    /**
     * A page of a package, with the pages under it, which get its `defaultFile` and its `applies` as its
     * items do.
     */
    model::Page loadPage(const Json& page, const Inherited& above, PackageReading& reading)
    {
        const auto own = inherit(page, above);
        model::Page loaded{ .name = getShownText(page, "pageDisplayName", reading.translations),
            .help = getShownText(page, "help", reading.translations),
            .items = loadItems(page, own, reading),
            .pages = {} };
        if (const auto pages = page.find("pages"); pages != page.end() && pages->is_array()) {
            for (const auto& under : *pages) {
                loaded.pages.push_back(loadPage(under, own, reading));
            }
        }
        return loaded;
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
            .source = model::Source::Package,
            .files = {} };
    }
}

namespace mods_config::package
{
    /**
     * Reads the text of a package's `config.json` into the model: the mod and its help, its files, its front
     * page and its pages, which nest, with their help, and their sections, which nest, and settings. Every
     * text the player sees is in the game's language. A setting has the file its value is kept in, and when
     * its change takes effect, which the package's root has to say.
     * The error says why a package cannot be read. Comments are skipped, as in an MCM config.
     */
    std::expected<model::Mod, std::string> loadPackage(const std::string_view text, const model::Translations& translations)
    {
        Json package;
        try {
            package = Json::parse(text, nullptr, true, true);
        } catch (const Json::parse_error& e) {
            return std::unexpected(std::format("Not valid JSON: {}", e.what()));
        }
        if (!package.is_object()) {
            return std::unexpected("Not a JSON object"s);
        }

        model::Mod mod;
        mod.name = getText(package, "modName");
        if (mod.name.empty()) {
            return std::unexpected("No modName"s);
        }
        const auto applies = findApplies(package);
        if (!applies) {
            return std::unexpected("No applies of live, save or restart"s);
        }
        mod.source = model::Source::Package;
        mod.displayName = getShownText(package, "displayName", translations);
        if (mod.displayName.empty()) {
            mod.displayName = mod.name;
        }
        mod.help = getShownText(package, "help", translations);

        auto files = loadFiles(package);
        PackageReading reading{ .translations = translations, .files = files, .takenIds = {} };
        const Inherited root{ .defaultFile = getText(package, "defaultFile"), .applies = *applies };
        mod.frontPage.items = loadItems(package, root, reading);
        if (const auto pages = package.find("pages"); pages != package.end() && pages->is_array()) {
            for (const auto& page : *pages) {
                mod.pages.push_back(loadPage(page, root, reading));
            }
        }
        mod.files = std::move(files);
        return mod;
    }

    /**
     * Loads the `config.json` of every folder in the packages' folder, which is `Data\ModsConfigVR\Config` in
     * the game. A package that cannot be read is a mod that is not available, named by its folder, and so
     * are two packages with one `modName`: neither is picked. A mod that is not available has the reason in
     * the log. No such folder is no mods.
     */
    std::vector<model::Mod> loadPackages(const std::filesystem::path& configFolder, const model::Translations& translations)
    {
        std::vector<model::Mod> mods;
        std::error_code error;
        for (std::filesystem::directory_iterator folder(configFolder, error), end; !error && folder != end; folder.increment(error)) {
            const auto file = folder->path() / "config.json";
            std::error_code noFile;
            if (!std::filesystem::is_regular_file(file, noFile)) {
                continue;
            }
            std::ostringstream text;
            text << std::ifstream(file, std::ios::binary).rdbuf();

            auto mod = loadPackage(text.str(), translations);
            if (!mod) {
                logger::warn("Package '{}' cannot be read: {}", toLogText(file), mod.error());
                const auto folderName = toLogText(folder->path().filename());
                mod = notAvailableMod(folderName, folderName, "its package cannot be read");
            }
            mods.push_back(std::move(*mod));
        }

        std::map<std::string, int> names;
        for (const auto& mod : mods) {
            ++names[common::str_tolower(mod.name)];
        }
        for (auto& mod : mods) {
            if (mod.notAvailable.empty() && names[common::str_tolower(mod.name)] > 1) {
                mod = notAvailableMod(mod.name, mod.displayName, "its package has the modName of another package");
            }
            if (!mod.notAvailable.empty()) {
                logger::warn("Package mod '{}' is not available: {}", mod.name, mod.notAvailable);
            }
        }
        return mods;
    }

    /**
     * The packages of the running game, from `Data\ModsConfigVR\Config` in the game's folder.
     */
    std::vector<model::Mod> loadInstalledPackages(const model::Translations& translations)
    {
        auto mods = loadPackages(PACKAGE_CONFIG_FOLDER, translations);
        logger::info("Loaded {} packages from '{}'", mods.size(), PACKAGE_CONFIG_FOLDER);
        return mods;
    }
}
