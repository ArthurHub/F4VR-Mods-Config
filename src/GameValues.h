#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "model/ConfigDefinition.h"

// The values a store reads and writes in the game, for a setting of an MCM mod and of a config spec. The mod's are
// in GameGlobals.cpp, GameProperties.cpp and GameIniSettings.cpp, and the tests' in tests/linked/TestGlobals.cpp,
// tests/linked/TestProperties.cpp and tests/linked/TestIniSettings.cpp, which keep them in a map.
namespace mods_config
{
    // The value of a global of the game, and none for one that is not in the game. Setting one returns false
    // for one that is not in the game.
    std::optional<float> getGlobal(const model::GlobalStorage& global);
    bool setGlobal(const model::GlobalStorage& global, float value);

    // The value of a property of a script, in the type the script keeps it in, and none for one that is not in
    // the game. Setting one returns false for one that is not in the game.
    std::optional<model::Value> getProperty(const model::PropertyStorage& property);
    bool setProperty(const model::PropertyStorage& property, const model::Value& value);

    // The texts of the options a choice reads in the game: the names of the forms of a FormList, or the texts
    // of a property of a script whose value is an array of texts. None for a list that is not in the game.
    std::optional<std::vector<std::string>> getOptionTexts(const model::GameOptions& options);

    // The name or the description of a form of the game, as a GameText of either kind names it. Empty for a form
    // that is not in the game, or has none.
    std::string getFormText(const model::GameText& text);

    // The value of a setting of the game's INI as the game runs with it, by its name in any case, as
    // `iShadowMapResolution:Display`, in its type. None for one that is not in the game, and for a type a condition
    // does not compare, as a color.
    std::optional<model::Value> getGameIniSetting(std::string_view name);
}
