#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>

// The globals of a test, in place of the game's: TestGlobals.cpp is the tests' own `mcm::getGlobal` and
// `mcm::setGlobal`, in place of the mod's.
namespace test_globals
{
    /**
     * The globals the MCM store reads and writes while it lives, each by its plugin and its id. It starts
     * with none, and one that is not in the map is not in the game. They are kept in one map for all the
     * tests, so a test has one of these at a time.
     */
    struct TestGlobals
    {
        TestGlobals();

        std::map<std::pair<std::string, std::uint32_t>, float>& values;
    };
}
