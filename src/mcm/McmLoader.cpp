#include "McmLoader.h"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>

#include <nlohmann/json.hpp>

#include "FallUITranslations.h"
#include "common/CommonUtils.h"
#include "model/TextMarkup.h"

using namespace mods_config;

namespace
{
    using Json = nlohmann::json;

    // MCM's plugin in Fallout 4 VR, and where its mods have their configs, from the game's folder
    constexpr auto MCM_DLL = "mcm_vr.dll";
    constexpr auto MCM_CONFIG_FOLDER = R"(Data\MCM\Config)";

    // the game's folder of its menus, which a FallUI mod names its language files from
    constexpr auto INTERFACE_FOLDER = R"(Data\Interface)";

    // the most lines a spacer is high, whatever its config says
    constexpr auto MAX_SPACER_LINES = 20.0;

    // the key of a row by which a FallUI mod names its language files
    constexpr auto FALLUI_LANGUAGE_FILE = "$fallUILanguageFile";

    /**
     * The text of a key. It is empty when the value has no such key, or the key holds something else than a text.
     */
    std::string getText(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_string() ? found->get<std::string>() : std::string();
    }

    /**
     * Whether a key is set to true.
     */
    bool getFlag(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_boolean() && found->get<bool>();
    }

    /**
     * The number of lines a spacer is high: its `numLines`, and one line without it.
     */
    int getSpacerLines(const Json& row)
    {
        const auto found = row.find("numLines");
        return found != row.end() && found->is_number() ? static_cast<int>(std::clamp(found->get<double>(), 1.0, MAX_SPACER_LINES)) : 1;
    }

    /**
     * What translates the texts of a config: the game's translations, and before them FallUI's own for a
     * FallUI mod.
     */
    struct ConfigTexts
    {
        const model::Translations& game;
        std::optional<mcm::FallUITranslations> fallUI;
    };

    /**
     * A text that the player sees, in the game's language. A FallUI mod can name the text's token beside it.
     */
    std::string toShownText(std::string text, const std::string& fallUIToken, const ConfigTexts& texts)
    {
        if (texts.fallUI) {
            text = texts.fallUI->translate(text, fallUIToken);
        }
        return texts.game.translate(std::move(text));
    }

    /**
     * The text of a key that the player sees, in the game's language. A FallUI mod can name the text's token
     * in a key of its own beside it: the key's name after a `$`.
     */
    std::string getShownText(const Json& value, const char* key, const ConfigTexts& texts)
    {
        return toShownText(getText(value, key), getText(value, ("$"s + key).c_str()), texts);
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
     * The `valueOptions` of a row: the range of a slider, and the options of a stepper and a dropdown.
     * An MCM option is a text, and the value that is stored for it is its place in the list. So an entry that
     * is no text stays in the list, with no text, and the options after it keep their values.
     */
    model::ValueOptions loadValueOptions(const Json& row, const ConfigTexts& texts)
    {
        model::ValueOptions valueOptions;
        const auto found = row.find("valueOptions");
        if (found == row.end() || !found->is_object()) {
            return valueOptions;
        }
        valueOptions.min = getNumber(*found, "min", valueOptions.min);
        valueOptions.max = getNumber(*found, "max", valueOptions.max);
        valueOptions.step = getNumber(*found, "step", valueOptions.step);

        if (const auto options = found->find("options"); options != found->end() && options->is_array()) {
            for (const auto& option : *options) {
                const auto index = static_cast<int>(valueOptions.options.size());
                valueOptions.options.push_back({ .value = index, .text = toShownText(option.is_string() ? option.get<std::string>() : std::string(), {}, texts) });
            }
        }
        return valueOptions;
    }

    /**
     * The language file a FallUI mod names in a row of its front page or of a page. Empty for any other mod.
     */
    std::string findFallUILanguageFile(const Json& config)
    {
        const auto inRows = [](const Json& page) {
            if (const auto content = page.find("content"); content != page.end() && content->is_array()) {
                for (const auto& row : *content) {
                    if (auto file = getText(row, FALLUI_LANGUAGE_FILE); !file.empty()) {
                        return file;
                    }
                }
            }
            return std::string();
        };

        auto file = inRows(config);
        if (const auto pages = config.find("pages"); file.empty() && pages != config.end() && pages->is_array()) {
            for (const auto& page : *pages) {
                if (file = inRows(page); !file.empty()) {
                    break;
                }
            }
        }
        return file;
    }

    using SettingTypeName = std::pair<std::string_view, model::SettingType>;

    // the rows that edit a value, by the type MCM's own menu knows each by
    constexpr std::array SETTING_TYPES{
        SettingTypeName{ "switcher", model::SettingType::Switcher },
        SettingTypeName{ "slider", model::SettingType::Slider },
        SettingTypeName{ "stepper", model::SettingType::Stepper },
        SettingTypeName{ "dropdown", model::SettingType::Dropdown },
        SettingTypeName{ "textinput", model::SettingType::TextInput },
        SettingTypeName{ "textinputInt", model::SettingType::TextInput },
        SettingTypeName{ "textinputFloat", model::SettingType::TextInput },
    };

    /**
     * A row of a config, by its type. A type the menu does not support is a placeholder, and so is a row
     * with no type.
     */
    model::Item loadRow(const std::string& type, const Json& row, const ConfigTexts& texts)
    {
        auto text = getShownText(row, "text", texts);

        if (const auto setting = std::ranges::find(SETTING_TYPES, type, &SettingTypeName::first); setting != SETTING_TYPES.end()) {
            return model::Setting{ .type = setting->second, .text = std::move(text), .valueOptions = loadValueOptions(row, texts) };
        }
        if (type == "button") {
            return model::Button{ .text = std::move(text) };
        }
        if (type == "text") {
            // the tags are read after the translation: a FallUI mod has its tokens inside them
            return model::Text{ .lines = getFlag(row, "html") ? model::parseMarkup(text) : model::plainLines(text) };
        }
        if (type == "image") {
            return model::Image{};
        }
        if (type == "spacer") {
            return model::Spacer{ .lines = getSpacerLines(row) };
        }
        return model::Placeholder{ .text = std::move(text), .type = type };
    }

    /**
     * The items of a `content` array. A section becomes a group, with the rows that follow it up to the next
     * section. The rows before the first section are items of the page itself.
     */
    std::vector<model::Item> loadItems(const Json& config, const char* key, const ConfigTexts& texts)
    {
        std::vector<model::Item> items;
        const auto content = config.find(key);
        if (content == config.end() || !content->is_array()) {
            return items;
        }

        bool inGroup = false;
        for (const auto& row : *content) {
            const auto type = getText(row, "type");
            if (type == "section") {
                items.emplace_back(model::Group{ .text = getShownText(row, "text", texts), .items = {} });
                inGroup = true;
                continue;
            }
            auto& target = inGroup ? std::get<model::Group>(items.back()).items : items;
            target.push_back(loadRow(type, row, texts));
        }
        return items;
    }

    /**
     * A path as UTF-8 text for the log. A path's own string() throws on a name the system's code page lacks.
     */
    std::string toLogText(const std::filesystem::path& path)
    {
        const auto text = path.u8string();
        return { text.begin(), text.end() };
    }
}

namespace mods_config::mcm
{
    /**
     * Reads the text of an MCM `config.json` into the model: the mod, its front page and its pages, and the
     * text and the type of every row, the lines of a text row, the height of a spacer, the range of a slider
     * and the options of a stepper and a dropdown.
     * A text that is a `$` token is looked up in the translations.
     * A FallUI mod is translated from its own language files, which it names from the interface folder.
     * The error says why a config cannot be read. Comments are skipped, as MCM's own reader skips them.
     */
    std::expected<model::Mod, std::string> loadConfig(const std::string_view text, const model::Translations& translations, const std::filesystem::path& interfaceFolder)
    {
        Json config;
        try {
            config = Json::parse(text, nullptr, true, true);
        } catch (const Json::parse_error& e) {
            return std::unexpected(std::format("Not valid JSON: {}", e.what()));
        }
        if (!config.is_object()) {
            return std::unexpected("Not a JSON object"s);
        }

        model::Mod mod;
        mod.name = getText(config, "modName");
        if (mod.name.empty()) {
            return std::unexpected("No modName"s);
        }

        ConfigTexts texts{ .game = translations, .fallUI = std::nullopt };
        if (const auto languageFile = findFallUILanguageFile(config); !languageFile.empty()) {
            texts.fallUI.emplace(translations).loadFiles(languageFile, interfaceFolder);
        }

        mod.displayName = getShownText(config, "displayName", texts);
        if (mod.displayName.empty()) {
            mod.displayName = mod.name;
        }
        mod.frontPage.items = loadItems(config, "content", texts);

        if (const auto pages = config.find("pages"); pages != config.end() && pages->is_array()) {
            for (const auto& page : *pages) {
                mod.pages.push_back({ .name = getShownText(page, "pageDisplayName", texts), .items = loadItems(page, "content", texts) });
            }
        }
        return mod;
    }

    /**
     * Loads the `config.json` of every folder in MCM's config folder, which is `Data\MCM\Config` in the game.
     * A config that cannot be read is left out, with the reason in the log. No such folder is no mods.
     */
    std::vector<model::Mod> loadConfigs(const std::filesystem::path& configFolder, const model::Translations& translations, const std::filesystem::path& interfaceFolder)
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

            if (auto mod = loadConfig(text.str(), translations, interfaceFolder)) {
                mods.push_back(std::move(*mod));
            } else {
                logger::warn("MCM config '{}' is not loaded: {}", toLogText(file), mod.error());
            }
        }
        return mods;
    }

    /**
     * The MCM mods of the running game. There are none while MCM itself is not loaded: a mod with an MCM menu
     * needs MCM for its settings, so its menu would change nothing.
     */
    std::vector<model::Mod> loadInstalledMods(const model::Translations& translations)
    {
        if (!common::isDLLModLoaded(MCM_DLL)) {
            logger::info("MCM is not loaded ({}): no MCM mods are listed", MCM_DLL);
            return {};
        }
        auto mods = loadConfigs(MCM_CONFIG_FOLDER, translations, INTERFACE_FOLDER);
        logger::info("Loaded {} MCM mods from '{}'", mods.size(), MCM_CONFIG_FOLDER);
        return mods;
    }
}
