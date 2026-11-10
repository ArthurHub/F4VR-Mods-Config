#pragma once

#include <map>
#include <string>

#include "model/ConfigDefinition.h"

// The settings of the game's INI in a test, in place of the game's: TestIniSettings.cpp is the tests' own
// `getGameIniSetting`, in place of the mod's GameIniSettings.cpp.
namespace test_ini_settings
{
    /**
     * The settings of the game's INI a store reads while it lives, each by its name in lower case, in its type. It starts
     * with none, and one that is not in the map is not in the game. They are kept in one map for all the tests, so a
     * test has one of these at a time.
     */
    struct TestIniSettings
    {
        TestIniSettings();

        std::map<std::string, mods_config::model::Value>& values;
    };
}
