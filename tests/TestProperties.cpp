#include "TestProperties.h"

#include "GameValues.h"

namespace
{
    using Properties = std::map<std::tuple<std::string, std::uint32_t, std::string, std::string>, mods_config::model::Value>;

    /**
     * The properties of the tests, in place of the game's.
     */
    Properties& propertyValues()
    {
        static Properties values;
        return values;
    }

    /**
     * The property of the tests a storage names, and the end of the map for one that is not in it.
     */
    Properties::iterator findProperty(const mods_config::model::PropertyStorage& property)
    {
        return propertyValues().find({ property.plugin, property.formId, property.script, property.name });
    }
}

namespace test_properties
{
    TestProperties::TestProperties()
        : values(propertyValues())
    {
        values.clear();
    }
}

namespace mods_config
{
    /**
     * The value of a property of the tests: the tests' own, which is linked in place of the mod's.
     */
    std::optional<model::Value> getProperty(const model::PropertyStorage& property)
    {
        const auto found = findProperty(property);
        return found != propertyValues().end() ? std::optional(found->second) : std::nullopt;
    }

    /**
     * Sets a property of the tests. Returns false for one that is not in the map.
     */
    bool setProperty(const model::PropertyStorage& property, const model::Value& value)
    {
        const auto found = findProperty(property);
        if (found == propertyValues().end()) {
            return false;
        }
        found->second = value;
        return true;
    }
}
