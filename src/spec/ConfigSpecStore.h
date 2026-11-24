#pragma once

#include <chrono>
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

#include "ConfigSpecLoader.h"
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
     * A choice that reads its options in the game, from a FormList or a property of a script, from its mod's
     * client, by the callback the client registered, or from the files of a folder, reads them as the mod is selected,
     * with the values they are checked against. A value of a choice of files that names a file in another spelling is
     * that file's option, in the form of the setting's default.
     * A global and a property are read and written with `getGlobal`, `setGlobal`, `getProperty` and
     * `setProperty`. A `live` one is previewed: its pending value is written into the game as it is set, but
     * for an invalid one, which leaves the preview before it, and
     * its baseline is written back as it is set back, discarded, or its session ends. Save writes them after
     * the files, and then runs the action of each setting it wrote.
     * A setting takes effect as its `applies` says. A `live` one is previewed by the mod's client: it is told each valid
     * pending value, and a Discard as the value goes back to its baseline, by a set, Discard, or the end of the
     * session, one of the game after the game has it. A Save tells it no Discard: it tells the client once what it wrote,
     * every setting with its value before and the files, after the actions ran, and then each F4SE plugin of its
     * config spec is sent the F4SE message of the Save. The store keeps the ones that take effect after a restart that a Save wrote, for each mod, until the
     * game is restarted.
     * A setting can be kept in a state of the mod's client, which the client answers as the mod is selected and applies
     * as it is told a change: a preview of a `live` one, its Discard, and what a Save wrote. The store writes nothing.
     * The mod's client can set a pending value as the player does, as its own UI hands its edits back: only a valid one.
     * The action of a button runs at once, with `callFunction` and `runConsoleCommand`.
     * A choice whose options set other settings is kept in them: its value is the first option whose values they have,
     * and none while no option's are; a pick sets them, each a pending value of its own. It is dirty while one of them is,
     * a Discard of it takes them back, and its default and its saved value are the option of their defaults and baselines.
     * A section's `maxOn` limits the places its switchers that are on take: one that would go over it is held off
     * (`limitReason`), and each one on in a section over it is invalid, which blocks Save while the section has a change.
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

        // what the rows a mod's client gives a page are read with: the game's texts, which outlive the store, and the
        // plugin checks
        void setPageContentReading(const model::Translations* translations, PluginChecks plugins)
        {
            _translations = translations;
            _plugins = std::move(plugins);
        }

        void open(const model::Mod& mod) override;
        void close() override;
        void pageShown() override;
        void readTexts(model::Mod& mod) override;
        bool readPageContent(model::Mod& mod, model::Page& page) override;
        model::StoredValue value(const model::Setting& setting) override;
        model::StoredValue valueById(std::string_view id) override;
        bool set(const model::Setting& setting, const model::Value& value) override;
        std::optional<model::Value> defaultValue(const model::Setting& setting) override;

        const std::vector<model::Option>& options(const model::Setting& setting) const override;

        bool isDirty(const model::Setting& setting) const override;
        std::size_t dirtyCount() const override;
        std::size_t invalidCount() const override;
        std::string limitReason(const model::Setting& setting) override;
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
        void discard(const model::Setting& setting) override;
        std::optional<model::Value> savedValue(const model::Setting& setting) override;
        model::StoredValue hotkey(const model::Hotkey& hotkey) override;
        model::StoredValue clientState(std::string_view name) override;
        model::StoredValue gameIniSetting(std::string_view name) override;
        void startFrame() override;
        void showPage(const model::Page* page) override;
        void showPointed(const model::Item* item) override;
        void readAgain() override;
        std::expected<void, std::string> setPendingValue(std::string_view modName, std::string_view id, const model::Value& value);
        void frameUpdate();

        // the clock that paces the previews told to a mod's client, which a test sets
        using Clock = std::function<std::chrono::steady_clock::time_point()>;

        void setPreviewPacing(std::chrono::milliseconds interval, Clock clock)
        {
            _previewInterval = interval;
            _clock = std::move(clock);
        }

        // the shortest time between two previews of a setting that its mod's client is told, as the client can take long
        // to apply one, and a value dragged changes in every frame
        static constexpr std::chrono::milliseconds PREVIEW_INTERVAL{ 50 };

        std::string notRunnable(const model::Action& action) const override;
        std::string notPreviewed(const model::Setting& setting) const override;
        model::ActionResult run(const model::Action& action) override;

    private:
        /**
         * A file with dirty settings that a Save writes: its name in the config spec, the file, which has its new text
         * prepared, and its dirty settings, in the order of the config spec.
         */
        struct FileSave
        {
            std::string_view name;
            ModFile* file;
            std::vector<const model::Setting*> settings;
        };

        // each setting a Save wrote, with the value it had before, which its mod's client is told
        using SettingsWritten = std::unordered_map<const model::Setting*, model::Value>;

        std::expected<void, std::string> saveChanges();
        std::expected<std::vector<FileSave>, std::string> prepareFileSaves();
        std::expected<void, std::string> checkStoresReady() const;
        std::expected<void, std::string> commitFiles(const std::vector<FileSave>& saves) const;
        std::expected<void, std::string> applySaved(const std::vector<FileSave>& saves, SettingsWritten& written);
        void announceSave(const std::vector<FileSave>& saves, const SettingsWritten& written);

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

            // what the mod's client says is set in place of the value for the session, the one in effect first
            std::vector<model::Override> overrides;

            // the options of a choice that takes a typed value, with its value that is none of them last, as `options`
            // gives them
            mutable std::vector<model::Option> typedOptions;
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

        /**
         * A `live` setting whose pending value the mod's client was told: the change it was told last, and the
         * setting's baseline, which a Discard tells it. It does not need the mod, which can be gone as the session ends.
         */
        struct ClientPreview
        {
            model::SettingChange change;
            model::Value baseline;

            // when the client was told its last preview, and the value set since that it is told once the interval is past
            std::chrono::steady_clock::time_point toldAt;
            std::optional<model::Value> held;
        };

        /**
         * A file of a config spec as the session read it. A file that is not there has neither its values nor a
         * reason: its settings have their defaults.
         */
        struct FileOfSession
        {
            // the file, read. None for a file that is not there or is not available
            const ModFile* file = nullptr;

            // why its settings are not available, and empty for a file that is read or is not there
            std::string notAvailable;
        };

        // the files of a config spec as the session read them, by their names in it
        using FilesOfSession = std::map<std::string, FileOfSession, std::less<>>;

        void readValues(const model::Mod& mod);
        FilesOfSession openFiles(const model::Mod& mod);
        void readBaseline(const model::Setting& setting, Entry& entry, const FilesOfSession& files, const model::Mod& mod) const;
        void readFileBaseline(const model::Setting& setting, Entry& entry, const FilesOfSession& files, const model::Mod& mod) const;
        void readClientState(const model::Setting& setting, const model::ClientStateStorage& state, Entry& entry) const;
        std::vector<model::Override> readOverrides(const model::Setting& setting) const;
        std::vector<model::Override> setOverrides(const model::Setting& choice) const;
        void preview(const model::Setting& setting, const Entry& entry);
        void previewToClient(const model::Setting& setting, const Entry& entry);
        void discardOnClient(ClientPreview& preview) const;
        void tellHeld(ClientPreview& preview, std::chrono::steady_clock::time_point now) const;
        model::ClientSetting clientSetting(const model::Setting& setting) const;
        model::ClientFile clientFile(const std::string& name) const;
        std::filesystem::path pathOf(const model::File& file) const;
        void rollBack();
        bool hasPendingTyped() const;

        // which values of the settings a choice sets are matched with its options
        enum class Reading : std::uint8_t
        {
            Current,
            Default,
            Saved,
        };

        std::vector<const model::Setting*> setTargets(const model::Setting& choice) const;
        std::optional<std::size_t> setOption(const model::Setting& choice, Reading reading);
        bool pickSets(const model::Setting& choice, const model::Value& picked);
        std::vector<const model::Setting*> colorParts(const model::ColorPartsStorage& storage) const;

        /**
         * A section with a `maxOn`: the places its switchers can take while they are on, and its switchers, in its
         * nested sections too.
         */
        struct Limit
        {
            int maxOn = 0;
            std::vector<const model::Setting*> switchers;

            // the places its switchers that are on take, as `placesOn` counted them last: none until counted again, after
            // any switcher's value changed (`forgetPlaces`). Every row of a large section asks for it in every frame
            mutable std::optional<int> places;
        };

        model::StoredValue ownValue(const model::Setting& setting);
        void addLimits(const std::vector<model::Item>& items);
        void addLimits(const std::vector<model::Page>& pages);
        bool isOn(const model::Setting& setting) const;
        int placesOn(const Limit& limit) const;
        void forgetPlaces();
        std::string overLimitReason(const Limit& limit) const;

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

        // what its mod's client was told the ray is on last: the ids of the row and of its groups, innermost first
        std::vector<std::string> _pointed;
        std::string _modDisplayName;

        // the files of the mod that is selected that the menu reads, by their names in its config spec. A file that
        // is not there is one too
        std::map<std::string, std::unique_ptr<ModFile>, std::less<>> _files;

        // where each of those files is, as the session started: its root and its path, or the path its mod's client gives
        std::map<std::string, std::filesystem::path, std::less<>> _filePaths;

        // what the rows a mod's client gives a page are read with: none for no texts of the game
        const model::Translations* _translations = nullptr;
        PluginChecks _plugins;

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

        // the color kept in sliders that each of its sliders is of, of the mod that is selected
        std::unordered_map<const model::Setting*, const model::Setting*> _colorOfPart;

        // what the session has of each setting of the mod that is selected
        std::unordered_map<const model::Setting*, Entry> _entries;

        // the sections of the mod that is selected that have a `maxOn`, and the ones each switcher is in, innermost
        // first, by their places in the list
        std::vector<Limit> _limits;
        std::unordered_map<const model::Setting*, std::vector<std::size_t>> _limitsOf;

        // the settings of the game that are previewed, by the setting, which is not read through it
        std::unordered_map<const model::Setting*, Preview> _previews;

        // the `live` settings whose pending value the mod's client was told, by the setting, which is not read through it
        std::unordered_map<const model::Setting*, ClientPreview> _clientPreviews;

        // the shortest time between two previews of a setting told to the client, and the clock that times it
        std::chrono::milliseconds _previewInterval = PREVIEW_INTERVAL;
        Clock _clock = std::chrono::steady_clock::now;

        // why the last Save did not write every dirty setting, until the next change, Save or Discard
        std::string _saveFailure;

        // how many settings that take effect after a restart the last Save wrote, until the next change or
        // Discard. None while there is no such Save
        std::optional<std::size_t> _lastSave;

        // the ids of the settings that take effect after a restart that a Save wrote in this run of the game,
        // by the name of their mod. It outlives the session
        std::map<std::string, std::set<std::string, std::less<>>, std::less<>> _savedForRestart;

        // how many Saves of each mod wrote something in this run of the game, by its name, which its client is told
        std::map<std::string, std::uint64_t, std::less<>> _saveRevisions;
    };
}
