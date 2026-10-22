#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ModFile.h"
#include "model/ChoiceOptions.h"
#include "model/ClientCalls.h"
#include "model/ConfigDefinition.h"
#include "model/Store.h"

namespace mods_config::spec
{
    /**
     * The store of a config spec's mod: its editing session. As the mod is selected its files, its globals and its
     * properties of scripts are read, and the value of each setting then is its baseline. A change is a pending value, and a setting whose pending
     * value is not its baseline is dirty: it waits for a Save, which writes it and makes it the baseline, or a
     * Discard, which takes it back. A change back to the baseline leaves the setting clean.
     * A value the file does not have, as in a file that is not there, is the setting's default, and is clean.
     * One that is not of its setting's type, outside its limits, of a length its text input does not allow or
     * no option of its choice is invalid: it is shown and edited, and is clean until the player changes it.
     * The player can set such a value too, as a text that is no number: it is pending and invalid, with the
     * reason, and Save writes nothing while it is.
     * A read-only setting is not set: it has the value of its file as it is, and none where the file has none.
     * The menu reads a TOML and an INI file: a setting of a JSON file is not available.
     * A choice that reads its options in the game, from a FormList or a property of a script, or from its mod's
     * client, by the callback the client registered, reads them as the mod is selected, with the values they
     * are checked against.
     * A global and a property are read and written with `getGlobal`, `setGlobal`, `getProperty` and
     * `setProperty`. A `live` one is previewed: its pending value is written into the game as it is set, but
     * for an invalid one, which leaves the preview before it, and
     * its baseline is written back as it is set back, discarded, or its session ends. Save writes them after
     * the files, and then runs the action of each setting it wrote.
     * A setting takes effect as its `applies` says, but a `live` one of a file, which has no native client to
     * preview it, takes effect after Save. The store keeps the ones that take effect after a restart that a
     * Save wrote, for each mod, until the game is restarted.
     * The action of a button runs at once, with `callFunction` and `runConsoleCommand`.
     */
    class ConfigSpecStore : public model::Store
    {
    public:
        ConfigSpecStore(std::filesystem::path dataFolder, std::filesystem::path myGamesFolder);

        // the clients of mods' DLLs, which the native API registers: the menu asks them for functions, states and options
        model::ClientCalls& clientCalls()
        {
            return _clientCalls;
        }

        void open(const model::Mod& mod) override;
        void close() override;
        void pageShown() override;
        model::StoredValue value(const model::Setting& setting) override;
        model::StoredValue valueById(std::string_view id) override;
        bool set(const model::Setting& setting, const model::Value& value) override;
        std::optional<model::Value> defaultValue(const model::Setting& setting) override;

        const std::vector<model::Option>& options(const model::Setting& setting) const override
        {
            return _options.of(setting);
        }

        bool savesAtOnce() const override
        {
            return false;
        }

        bool isDirty(const model::Setting& setting) const override;
        std::size_t dirtyCount() const override;
        std::size_t invalidCount() const override;
        std::optional<model::Applies> applies(const model::Setting& setting) const override;
        std::size_t dirtyRestartCount() const override;

        std::optional<std::size_t> lastSave() const override
        {
            return _lastSave;
        }

        std::size_t savedRestartCount(const model::Mod& mod) const override;
        std::expected<void, std::string> save() override;

        std::string_view saveFailure() const override
        {
            return _saveFailure;
        }

        void discard() override;
        model::StoredValue hotkey(const model::Hotkey& hotkey) override;
        model::StoredValue clientState(std::string_view name) override;
        void startFrame() override;
        void showPage(const model::Page* page) override;
        void readAgain() override;
        std::string notRunnable(const model::Action& action) const override;
        model::ActionResult run(const model::Action& action) override;

    private:
        /**
         * What the session has of a setting. A value is in the type of the setting's control, as the UI has it.
         */
        struct Entry
        {
            // the value the session started with. None for one that is not available, and for a value in a
            // file that is not of the setting's type
            std::optional<model::Value> baseline;

            // why the value is not available, and empty for one that is
            std::string notAvailable;

            // why the value in the file is invalid, and empty for a valid one
            std::string invalid;

            // the player's value, which waits for a Save. None for a setting that is clean
            std::optional<model::Value> pending;

            // why the player's value is invalid, and empty for a valid one: it blocks Save
            std::string pendingInvalid;
        };

        /**
         * A setting of the game whose pending value the game has, as a preview: where it is kept, and its
         * baseline as the game keeps it, which is written back. It does not need the mod, which can be gone
         * as the session ends.
         */
        struct Preview
        {
            model::Storage storage;
            model::Value baseline;
        };

        void readValues(const model::Mod& mod);
        void preview(const model::Setting& setting, const Entry& entry);
        void rollBack();

        // the folders a path of a config spec's file is from: the game's Data folder, and
        // `Documents\My Games\Fallout4VR`
        std::filesystem::path _dataFolder;
        std::filesystem::path _myGamesFolder;

        // the name of the mod that is selected, for the log
        std::string _modName;

        // the mod of the session, and none
        const model::Mod* _mod = nullptr;

        // the `id` of the page that is shown, which its mod's client is told as it is left: empty for one with none
        std::string _pageId;
        std::string _modDisplayName;

        // the files of the mod that is selected that the menu reads, by their names in its config spec. A file that
        // is not there is one too
        std::map<std::string, std::unique_ptr<ModFile>, std::less<>> _files;

        // the options of the choices of the mod that is selected, and the callbacks that give the options of a choice
        // that takes them from its mod's client
        model::ChoiceOptions _options;
        model::ClientCalls _clientCalls;

        // the states of the mod's client asked in this frame, by their names in lower case: each is asked once a frame
        std::map<std::string, std::optional<model::Value>, std::less<>> _clientStates;

        // the settings of the mod that is selected, in the order of its config spec
        std::vector<const model::Setting*> _settingList;

        // and by their ids
        std::map<std::string, const model::Setting*, std::less<>> _settings;

        // what the session has of each setting of the mod that is selected
        std::unordered_map<const model::Setting*, Entry> _entries;

        // the settings of the game that are previewed, by the setting, which is not read through it
        std::unordered_map<const model::Setting*, Preview> _previews;

        // why the last Save did not write every dirty setting, until the next change, Save or Discard
        std::string _saveFailure;

        // how many settings that take effect after a restart the last Save wrote, until the next change or
        // Discard. None while there is no such Save
        std::optional<std::size_t> _lastSave;

        // the ids of the settings that take effect after a restart that a Save wrote in this run of the game,
        // by the name of their mod. It outlives the session
        std::map<std::string, std::set<std::string, std::less<>>, std::less<>> _savedForRestart;
    };
}
