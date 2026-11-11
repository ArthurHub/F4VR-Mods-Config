#include "ChoiceOptions.h"

#include <algorithm>
#include <format>

#include "GameValues.h"
#include "common/CommonUtils.h"

using namespace mods_config;

namespace
{
    /**
     * Adds the choices among the items that read their options in the game or from their mod's client, and the
     * choices of files where their folder is listed here, with those of the groups among them.
     */
    void addChoices(const std::vector<model::Item>& items, const bool listsFolders, std::vector<const model::Setting*>& choices)
    {
        for (const auto& item : items) {
            const auto* setting = std::get_if<model::Setting>(&item);
            if (setting && (setting->valueOptions.gameOptions || setting->valueOptions.clientOptions || (listsFolders && setting->valueOptions.folderOptions))) {
                choices.push_back(setting);
            } else if (const auto* group = std::get_if<model::Group>(&item)) {
                addChoices(group->items, listsFolders, choices);
            }
        }
    }

    /**
     * Adds the choices of pages and of the pages under them.
     */
    void addChoices(const std::vector<model::Page>& pages, const bool listsFolders, std::vector<const model::Setting*>& choices)
    {
        for (const auto& page : pages) {
            addChoices(page.items, listsFolders, choices);
            addChoices(page.pages, listsFolders, choices);
        }
    }
}

namespace mods_config::model
{
    /**
     * The value a text typed in a dropdown's entry that types a new value gives: the text without the spaces around
     * it, or the value of the option whose text it is without regard to case, so a name in use keeps its spelling.
     * None for a text that is empty then, which sets nothing.
     */
    std::optional<Value> typedOptionValue(const std::string_view text, const std::vector<Option>& options)
    {
        auto typed = common::trim(std::string(text));
        if (typed.empty()) {
            return std::nullopt;
        }
        const auto key = common::str_tolower(typed);
        const auto option = std::ranges::find_if(options, [&key](const Option& option) {
            return common::str_tolower(option.text) == key;
        });
        return option != options.end() ? option->value : Value(std::move(typed));
    }

    /**
     * Reads the options of the choices of a mod that read them in the game or from its client, as the mod is
     * selected, and of a config spec's choices of files, from the folders of their roots. It forgets those of the mod
     * before it.
     */
    void ChoiceOptions::read(const Mod& mod, const ClientCalls* clients, const FileFolders& folders)
    {
        std::vector<const Setting*> choices;
        const bool listsFolders = mod.source == Source::ConfigSpec;
        addChoices(mod.frontPage.items, listsFolders, choices);
        addChoices(mod.pages, listsFolders, choices);
        _read.clear();
        _modName = mod.name;
        _clients = clients;
        _folders = folders;
        for (const auto* setting : choices) {
            _read.try_emplace(setting);
        }
        readAgain();
    }

    /**
     * Reads the options of the choices of the mod again, as a script can change its list, and a file can be added to
     * a folder.
     */
    void ChoiceOptions::readAgain()
    {
        for (auto& [setting, read] : _read) {
            const auto& options = setting->valueOptions;
            read = options.gameOptions ? readList(*options.gameOptions) : options.clientOptions ? readClient(*setting) : readFolder(*setting);
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
        _folders = {};
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
        if (!_clients) {
            return { .options = {}, .notAvailable = "Not available: its mod's client gives no options for it." };
        }
        auto options = _clients->options(_modName, setting);
        if (!options) {
            return { .options = {}, .notAvailable = std::move(options.error()) };
        }
        return { .options = std::move(*options), .notAvailable = {} };
    }

    /**
     * The options of a config spec's choice of files: the files its folder has that its mask lists. One whose folder
     * has none, or is not there, has none, with where it looked as the reason, but for its option that is no file,
     * which it has then alone.
     */
    ChoiceOptions::Read ChoiceOptions::readFolder(const Setting& setting) const
    {
        const auto& folder = *setting.valueOptions.folderOptions;
        const auto path = folderOf(folder, _folders);
        const auto files = path.empty() ? std::vector<std::string>() : listFiles(path, folder.mask);
        Read read{ .options = fileOptions(setting, files), .notAvailable = {} };
        if (read.options.empty()) {
            read.notAvailable = noFilesReason(folder);
        }
        return read;
    }
}
