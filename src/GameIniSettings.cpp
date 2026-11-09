#include "GameValues.h"

#include <map>
#include <string>

#include "common/CommonUtils.h"

using namespace mods_config;

namespace
{
    /**
     * The setting of a collection of the game's INI by its name in any case, and none for one it does not have.
     * CommonLibF4's own lookup compares the name in its case, which an author would have to match.
     */
    RE::Setting* findIn(RE::INISettingCollection* collection, const std::string_view name)
    {
        if (!collection) {
            return nullptr;
        }
        for (auto* setting : collection->settings) {
            if (setting && setting->GetKey().size() == name.size() && _strnicmp(setting->GetKey().data(), name.data(), name.size()) == 0) {
                return setting;
            }
        }
        return nullptr;
    }

    /**
     * The setting of the game's INI by its name, looked up in the prefs and then in the INI, as the game's console does,
     * and none for one the game does not have. A page asks for it in every frame, and the game's collections are lists
     * it searches one by one, so each name is looked up once. One that is not found is in the log once.
     */
    RE::Setting* findSetting(const std::string_view name)
    {
        // the settings that were looked up, by their names in lower case: none for one that is not in the game
        static std::map<std::string, RE::Setting*, std::less<>> looked;

        const auto [found, isNew] = looked.try_emplace(common::str_tolower(std::string(name)), nullptr);
        if (isNew) {
            found->second = findIn(RE::INIPrefSettingCollection::GetSingleton(), name);
            if (!found->second) {
                found->second = findIn(RE::INISettingCollection::GetSingleton(), name);
            }
            if (!found->second) {
                logger::warn("The game's INI has no setting '{}': a condition or a text that names it has no value", name);
            }
        }
        return found->second;
    }
}

namespace mods_config
{
    /**
     * The value a setting of the game's INI has as the game runs now, which its settings menu changes, in its type: an
     * unsigned int is an int. None for one that is not in the game, and for a char and a color, which a condition does
     * not compare.
     */
    std::optional<model::Value> getGameIniSetting(const std::string_view name)
    {
        const auto* setting = findSetting(name);
        if (!setting) {
            return std::nullopt;
        }
        using Type = RE::Setting::SETTING_TYPE;
        switch (setting->GetType()) {
        case Type::kBinary:
            return setting->GetBinary();
        case Type::kInt:
            return setting->GetInt();
        case Type::kUInt:
            return static_cast<int>(setting->GetUInt());
        case Type::kFloat:
            return setting->GetFloat();
        case Type::kString:
            return std::string(setting->GetString());
        default:
            return std::nullopt;
        }
    }
}
