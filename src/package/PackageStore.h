#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "model/ChoiceOptions.h"
#include "model/Definition.h"
#include "model/Store.h"

namespace mods_config::package
{
    /**
     * The store of a package's mod: its editing session. As the mod is selected its files, its globals and its
     * properties of scripts are read, and the value of each setting then is its baseline. A change is a pending value, and a setting whose pending
     * value is not its baseline is dirty: it waits for a Save, which writes it and makes it the baseline, or a
     * Discard, which takes it back. A change back to the baseline leaves the setting clean.
     * A value the file does not have, as in a file that is not there, is the setting's default, and is clean.
     * One that is not of its setting's type, outside its limits or no option of its choice is invalid: it is
     * shown and edited, and is clean until the player changes it.
     * A read-only setting is not set: it has the value of its file as it is, and none where the file has none.
     * The menu reads a TOML file: a setting of any other file is not available.
     * A choice that reads its options in the game, from a FormList or a property of a script, reads them as the
     * mod is selected, with the values they are checked against.
     * A global and a property are read and written with `getGlobal`, `setGlobal`, `getProperty` and
     * `setProperty`. A `live` one is previewed: its pending value is written into the game as it is set, and
     * its baseline is written back as it is set back, discarded, or its session ends. Save writes them after
     * the files, and then runs the action of each setting it wrote.
     * The action of a button runs at once, with `callFunction` and `runConsoleCommand`.
     */
    class PackageStore : public model::Store
    {
    public:
        PackageStore(std::filesystem::path dataFolder, std::filesystem::path myGamesFolder);

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
        bool save() override;
        void discard() override;
        model::StoredValue hotkey(const model::Hotkey& hotkey) override;
        bool run(const model::Action& action) override;

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

        void preview(const model::Setting& setting, const Entry& entry);
        void rollBack();

        // the folders a path of a package's file is from: the game's Data folder, and
        // `Documents\My Games\Fallout4VR`
        std::filesystem::path _dataFolder;
        std::filesystem::path _myGamesFolder;

        // the name of the mod that is selected, for the log
        std::string _modName;

        // the TOML files of the mod that is selected, absolute, by their names in its package. A file that is
        // not there is one too
        std::map<std::string, std::filesystem::path, std::less<>> _tomlFiles;

        // the options of the choices of the mod that is selected
        model::ChoiceOptions _options;

        // the settings of the mod that is selected, in the order of its package
        std::vector<const model::Setting*> _settingList;

        // and by their ids
        std::map<std::string, const model::Setting*, std::less<>> _settings;

        // what the session has of each setting of the mod that is selected
        std::unordered_map<const model::Setting*, Entry> _entries;

        // the settings of the game that are previewed, by the setting, which is not read through it
        std::unordered_map<const model::Setting*, Preview> _previews;
    };
}
