#include "McmLoader.h"

#include <algorithm>
#include <array>
#include <format>

#include <nlohmann/json.hpp>

using namespace mods_config;

namespace
{
    using Json = nlohmann::json;

    /**
     * The text of a key. It is empty when the value has no such key, or the key holds something else than a text.
     */
    std::string getText(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_string() ? found->get<std::string>() : std::string();
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
    model::Item loadRow(const std::string& type, const Json& row)
    {
        auto text = getText(row, "text");

        if (const auto setting = std::ranges::find(SETTING_TYPES, type, &SettingTypeName::first); setting != SETTING_TYPES.end()) {
            return model::Setting{ .type = setting->second, .text = std::move(text) };
        }
        if (type == "button") {
            return model::Button{ .text = std::move(text) };
        }
        if (type == "text") {
            return model::Text{ .text = std::move(text) };
        }
        if (type == "image") {
            return model::Image{};
        }
        if (type == "spacer") {
            return model::Spacer{};
        }
        return model::Placeholder{ .text = std::move(text), .type = type };
    }

    /**
     * The items of a `content` array. A section becomes a group, with the rows that follow it up to the next
     * section. The rows before the first section are items of the page itself.
     */
    std::vector<model::Item> loadItems(const Json& config, const char* key)
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
                items.emplace_back(model::Group{ .text = getText(row, "text"), .items = {} });
                inGroup = true;
                continue;
            }
            auto& target = inGroup ? std::get<model::Group>(items.back()).items : items;
            target.push_back(loadRow(type, row));
        }
        return items;
    }
}

namespace mods_config::mcm
{
    /**
     * Reads the text of an MCM `config.json` into the model: the mod, its front page and its pages, and the
     * text and the type of every row. The error says why a config cannot be read.
     * Comments are skipped, as MCM's own reader skips them.
     */
    std::expected<model::Mod, std::string> loadConfig(const std::string_view text)
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
        mod.displayName = getText(config, "displayName");
        if (mod.displayName.empty()) {
            mod.displayName = mod.name;
        }
        mod.frontPage.items = loadItems(config, "content");

        if (const auto pages = config.find("pages"); pages != config.end() && pages->is_array()) {
            for (const auto& page : *pages) {
                mod.pages.push_back({ .name = getText(page, "pageDisplayName"), .items = loadItems(page, "content") });
            }
        }
        return mod;
    }
}
