#include "TestIniSettings.h"

#include "GameValues.h"
#include "common/CommonUtils.h"

namespace
{
    /**
     * The settings of the game's INI of the tests, in place of the game's.
     */
    std::map<std::string, mods_config::model::Value>& iniValues()
    {
        static std::map<std::string, mods_config::model::Value> values;
        return values;
    }
}

namespace test_ini_settings
{
    TestIniSettings::TestIniSettings()
        : values(iniValues())
    {
        values.clear();
    }
}

namespace mods_config
{
    /**
     * The value of a setting of the game's INI of the tests, by its name in any case: the tests' own, which is linked in
     * place of the mod's.
     */
    std::optional<model::Value> getGameIniSetting(const std::string_view name)
    {
        const auto found = iniValues().find(common::str_tolower(std::string(name)));
        return found != iniValues().end() ? std::optional(found->second) : std::nullopt;
    }
}
