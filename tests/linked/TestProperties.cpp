#include "TestProperties.h"

#include "GameValues.h"

namespace
{
    using Properties = std::map<std::tuple<std::string, std::uint32_t, std::string, std::string>, mods_config::model::Value>;
    using Lists = std::map<std::tuple<std::string, std::uint32_t, std::string, std::string>, std::vector<std::string>>;
    using FormTexts = std::map<std::tuple<std::string, std::uint32_t, bool>, std::string>;

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
     * The names and the descriptions of the forms of the tests, in place of the game's.
     */
    FormTexts& formTextsOf()
    {
        static FormTexts texts;
        return texts;
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
          lists(optionLists()),
          formTexts(formTextsOf())
    {
        values.clear();
        lists.clear();
        formTexts.clear();
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

    /**
     * The name or the description of a form of the tests: the tests' own, which is linked in place of the mod's.
     */
    std::string getFormText(const model::GameText& text)
    {
        const auto found = formTextsOf().find({ text.plugin, text.formId, text.kind == model::GameText::Kind::FormDescription });
        return found != formTextsOf().end() ? found->second : std::string();
    }
}
