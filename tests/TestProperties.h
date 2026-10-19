#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <tuple>

#include "model/Definition.h"

// The properties of scripts of a test, in place of the game's: TestProperties.cpp is the tests' own
// `getProperty` and `setProperty`, in place of the mod's.
namespace test_properties
{
    /**
     * The properties a store reads and writes while it lives, each by its plugin, the id of its form,
     * its script and its name, with the value its script keeps. It starts with none, and one that is not in
     * the map is not in the game. They are kept in one map for all the tests, so a test has one of these at
     * a time.
     */
    struct TestProperties
    {
        TestProperties();

        std::map<std::tuple<std::string, std::uint32_t, std::string, std::string>, mods_config::model::Value>& values;
    };
}
