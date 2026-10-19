#include "TestGlobals.h"

#include "GameValues.h"

namespace
{
    /**
     * The globals of the tests, in place of the game's.
     */
    std::map<std::pair<std::string, std::uint32_t>, float>& globalValues()
    {
        static std::map<std::pair<std::string, std::uint32_t>, float> values;
        return values;
    }
}

namespace test_globals
{
    TestGlobals::TestGlobals()
        : values(globalValues())
    {
        values.clear();
    }
}

namespace mods_config
{
    /**
     * The value of a global of the tests: the tests' own, which is linked in place of the mod's.
     */
    std::optional<float> getGlobal(const model::GlobalStorage& global)
    {
        const auto found = globalValues().find({ global.plugin, global.formId });
        return found != globalValues().end() ? std::optional(found->second) : std::nullopt;
    }

    /**
     * Sets a global of the tests. Returns false for one that is not in the map.
     */
    bool setGlobal(const model::GlobalStorage& global, const float value)
    {
        const auto found = globalValues().find({ global.plugin, global.formId });
        if (found == globalValues().end()) {
            return false;
        }
        found->second = value;
        return true;
    }
}
