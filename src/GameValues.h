#pragma once

#include <optional>

#include "model/Definition.h"

// The values a store reads and writes in the game, for a setting of an MCM mod and of a package. The mod's are
// in GameGlobals.cpp and GameProperties.cpp, and the tests' in tests/TestGlobals.cpp and
// tests/TestProperties.cpp, which keep them in a map.
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
}
