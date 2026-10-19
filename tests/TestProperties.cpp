#include "TestProperties.h"

#include "GameValues.h"

namespace
{
    using Properties = std::map<std::tuple<std::string, std::uint32_t, std::string, std::string>, mods_config::model::Value>;
    using Lists = std::map<std::tuple<std::string, std::uint32_t, std::string, std::string>, std::vector<std::string>>;

    /**
     * The properties of the tests, in place of the game's.
     */
    Properties& propertyValues()
    {
        static Properties values;
        return values;
    }

    /**
     * The lists of the tests, in place of the game's.
     */
    Lists& optionLists()
    {
        static Lists lists;
        return lists;
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
        : values(propertyValues()),
          lists(optionLists())
    {
        values.clear();
        lists.clear();
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

    /**
     * The texts of a list of the tests: the tests' own, which is linked in place of the mod's.
     */
    std::optional<std::vector<std::string>> getOptionTexts(const model::GameOptions& options)
    {
        const auto found = optionLists().find({ options.plugin, options.formId, options.script, options.property });
        return found != optionLists().end() ? std::optional(found->second) : std::nullopt;
    }
}
