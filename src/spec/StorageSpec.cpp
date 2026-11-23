#include "StorageSpec.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <format>

#include "IniFile.h"
#include "TomlFile.h"
#include "common/CommonUtils.h"

using namespace std::string_literals;

namespace mods_config::spec
{
    namespace
    {
        // the fields of a file of a config spec's `files`, and of a storage, as the schema has them (docs/design/schema.md)
        constexpr std::array<std::string_view, 5> FILE_FIELDS{ "format", "root", "path", "pathFromClientState", "omitDefault" };
        constexpr std::array<std::string_view, 9> STORAGE_FIELDS{ "file", "section", "key", "path", "global", "form", "scriptName", "propertyName", "clientState" };

        using FormatName = std::pair<std::string_view, model::FileFormat>;

        // what a file of a config spec is written in, by its `format`, which is also its extension
        constexpr std::array FILE_FORMATS{
            FormatName{ "ini", model::FileFormat::Ini },
            FormatName{ "toml", model::FileFormat::Toml },
        };

        /**
         * A file of a config spec's `files`, or the reason it is not one: a `format` of `ini` or `toml`,
         * which its extension is, a `root` of `data` or `myGames`, and a `path` from that root. The path is
         * checked as it is written (`pathProblem`).
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
            if (auto problem = pathProblem(path); !problem.empty()) {
                return std::unexpected(std::move(problem));
            }
            const std::filesystem::path checked(std::u8string(path.begin(), path.end()));
            const auto extension = checked.extension().u8string();
            if (common::str_tolower(std::string(extension.begin(), extension.end())) != "." + formatText) {
                return std::unexpected(std::format("its path '{}' does not end in .{}", path, formatText));
            }
            return model::File{ .format = format->second, .root = rootText == "data" ? model::FileRoot::Data : model::FileRoot::MyGames, .path = std::move(path) };
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
    }

    /**
     * The files of a config spec, by the names its `files` gives them. A file that is not written as one is
     * left out, and is reported, so its settings are not available. A file's `omitDefault` is its settings' unless
     * an `omitDefault` of the config spec says otherwise for them.
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
                // a file that keeps only the values that differ from their defaults
                if (const auto omit = declared.find("omitDefault"); omit != declared.end()) {
                    if (omit->is_boolean()) {
                        file->omitDefault = omit->get<bool>();
                    } else {
                        const Into intoOmit(reading, "omitDefault");
                        reading.report(model::Severity::Warning, "Its omitDefault is not true or false: it is left out.");
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
}
