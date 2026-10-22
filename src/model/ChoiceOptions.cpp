#include "ChoiceOptions.h"

#include <format>

#include "GameValues.h"
#include "common/CommonUtils.h"

using namespace mods_config;

namespace
{
    /**
     * Adds the choices among the items that read their options in the game or from their mod's client, with
     * those of the groups among them.
     */
    void addChoices(const std::vector<model::Item>& items, std::vector<const model::Setting*>& choices)
    {
        for (const auto& item : items) {
            if (const auto* setting = std::get_if<model::Setting>(&item); setting && (setting->valueOptions.gameOptions || setting->valueOptions.clientOptions)) {
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
     * Registers the callback that gives the options of a setting of a mod. False for a setting that has one
     * already, which keeps the first.
     */
    bool ClientOptions::add(const std::string_view modName, const std::string_view settingId, Callback callback)
    {
        return _callbacks.try_emplace({ common::str_tolower(std::string(modName)), common::str_tolower(std::string(settingId)) }, std::move(callback)).second;
    }

    /**
     * The callback of a setting of a mod. None for a setting no callback is registered for.
     */
    const ClientOptions::Callback* ClientOptions::find(const std::string_view modName, const std::string_view settingId) const
    {
        const auto found = _callbacks.find({ common::str_tolower(std::string(modName)), common::str_tolower(std::string(settingId)) });
        return found != _callbacks.end() ? &found->second : nullptr;
    }

    /**
     * Reads the options of the choices of a mod that read them in the game or from its client, as the mod is
     * selected. It forgets those of the mod before it.
     */
    void ChoiceOptions::read(const Mod& mod, const ClientOptions* clients)
    {
        std::vector<const Setting*> choices;
        addChoices(mod.frontPage.items, choices);
        addChoices(mod.pages, choices);
        _read.clear();
        _modName = mod.name;
        _clients = clients;
        for (const auto* setting : choices) {
            _read.try_emplace(setting);
        }
        readAgain();
    }

    /**
     * Reads the options of the choices of the mod again, as a script can change its list.
     */
    void ChoiceOptions::readAgain()
    {
        for (auto& [setting, read] : _read) {
            read = setting->valueOptions.gameOptions ? readList(*setting->valueOptions.gameOptions) : readClient(*setting);
        }
    }

    /**
     * Forgets the options of the mod, as its session ends: the mod can be gone after it.
     */
    void ChoiceOptions::clear()
    {
        _read.clear();
        _modName.clear();
        _clients = nullptr;
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

    /**
     * The options a choice's mod's client gives, by the callback it registered for the setting. None for a
     * setting with no callback, and for one whose callback gives none, with the reason.
     */
    ChoiceOptions::Read ChoiceOptions::readClient(const Setting& setting) const
    {
        const auto* callback = _clients ? _clients->find(_modName, setting.id) : nullptr;
        if (!callback) {
            return { .options = {}, .notAvailable = "Not available: its mod's client gives no options for it." };
        }
        auto options = (*callback)(setting);
        if (!options) {
            return { .options = {}, .notAvailable = std::move(options.error()) };
        }
        return { .options = std::move(*options), .notAvailable = {} };
    }
}
