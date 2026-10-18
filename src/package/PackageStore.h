#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "model/Definition.h"
#include "model/Store.h"

namespace mods_config::package
{
    /**
     * The store of a package's mod: its editing session. As the mod is selected its files are read, and the
     * value of each setting then is its baseline. A change is a pending value, and a setting whose pending
     * value is not its baseline is dirty: it waits for a Save, which writes it. A change back to the baseline
     * leaves the setting clean.
     * A value the file does not have, as in a file that is not there, is the setting's default, and is clean.
     * One that is not of its setting's type, outside its limits or no option of its choice is invalid: it is
     * shown and edited, and is clean until the player changes it.
     * The menu reads a TOML file: a setting of any other file, or kept in a global or a property of a script,
     * is not available.
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

        bool savesAtOnce() const override
        {
            return false;
        }

        bool isDirty(const model::Setting& setting) const override;
        std::size_t dirtyCount() const override;
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

        // the folders a path of a package's file is from: the game's Data folder, and
        // `Documents\My Games\Fallout4VR`
        std::filesystem::path _dataFolder;
        std::filesystem::path _myGamesFolder;

        // the settings of the mod that is selected, by their ids
        std::map<std::string, const model::Setting*, std::less<>> _settings;

        // what the session has of each setting of the mod that is selected
        std::unordered_map<const model::Setting*, Entry> _entries;
    };
}
