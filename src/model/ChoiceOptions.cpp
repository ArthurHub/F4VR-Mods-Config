#include "ChoiceOptions.h"

#include <format>

#include "GameValues.h"

using namespace mods_config;

namespace
{
    /**
     * Adds the choices among the items that read their options in the game, with those of the groups among
     * them.
     */
    void addChoices(const std::vector<model::Item>& items, std::vector<const model::Setting*>& choices)
    {
        for (const auto& item : items) {
            if (const auto* setting = std::get_if<model::Setting>(&item); setting && setting->valueOptions.gameOptions) {
                choices.push_back(setting);
            } else if (const auto* group = std::get_if<model::Group>(&item)) {
                addChoices(group->items, choices);
            }
        }
    }

    /**
     * Adds the choices of pages and of the pages under them.
     */
    void addChoices(const std::vector<model::Page>& pages, std::vector<const model::Setting*>& choices)
    {
        for (const auto& page : pages) {
            addChoices(page.items, choices);
            addChoices(page.pages, choices);
        }
    }
}

namespace mods_config::model
{
    /**
     * Reads the options of the choices of a mod that read them in the game, as the mod is selected. It forgets
     * those of the mod before it.
     */
    void ChoiceOptions::read(const Mod& mod)
    {
        std::vector<const Setting*> choices;
        addChoices(mod.frontPage.items, choices);
        addChoices(mod.pages, choices);
        _read.clear();
        for (const auto* setting : choices) {
            _read.try_emplace(setting);
        }
        readAgain();
    }

    /**
     * Reads the options of the choices of the mod in the game again, as a script can change its list.
     */
    void ChoiceOptions::readAgain()
    {
        for (auto& [setting, read] : _read) {
            read = readList(*setting->valueOptions.gameOptions);
        }
    }

    /**
     * Forgets the options of the mod, as its session ends: the mod can be gone after it.
     */
    void ChoiceOptions::clear()
    {
        _read.clear();
    }

    /**
     * The options of a choice: the ones read in the game for one that reads them there, and else its own.
     */
    const std::vector<Option>& ChoiceOptions::of(const Setting& setting) const
    {
        const auto found = _read.find(&setting);
        return found != _read.end() ? found->second.options : setting.valueOptions.options;
    }

    /**
     * Why a choice that reads its options in the game has none: its list cannot be read. Empty for a choice
     * that has its options.
     */
    std::string_view ChoiceOptions::notAvailable(const Setting& setting) const
    {
        const auto found = _read.find(&setting);
        return found != _read.end() ? std::string_view(found->second.notAvailable) : std::string_view();
    }

    /**
     * The options of a list of the game, each its text and its text or its place as its value, as the list
     * says. None for one that cannot be read, with the reason.
     */
    ChoiceOptions::Read ChoiceOptions::readList(const GameOptions& list)
    {
        const auto texts = getOptionTexts(list);
        if (!texts) {
            const auto form = std::format("{}|{:X}", list.plugin, list.formId);
            return { .options = {},
                .notAvailable = list.property.empty() ? std::format("Not available: the FormList {} of its options is not in the game.", form)
                                                      : std::format("Not available: the property {} of {} is not in the game, or is no array of texts.", list.property, form) };
        }
        Read read;
        for (const auto& text : *texts) {
            read.options.push_back({ .value = list.storesText ? Value(text) : Value(static_cast<int>(read.options.size())), .text = text });
        }
        return read;
    }
}
