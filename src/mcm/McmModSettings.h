#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "model/Definition.h"

namespace mods_config::mcm
{
    /**
     * The ModSettings of one mod, as MCM's two files have them: the defaults the mod ships, in
     * `Config\<mod>\settings.ini`, and over them the player's values, in `Settings\<mod>.ini`, which MCM
     * writes on every set, and which is read again for a value a script sets. A value is the text of its key,
     * by the name MCM gives it: `fStrength:Main` is the key `fStrength` of the section `[Main]`, in the letter
     * case of the file.
     * One is set through MCM's own Papyrus function, so MCM has the value in its memory and writes it to the
     * player's file. With no game, in a test, MCM is not called.
     * The text of the mod's own file is kept beside it: it is the default of the ModSetting.
     */
    class McmModSettings
    {
    public:
        void load(const std::filesystem::path& mcmFolder, std::string_view modName);
        std::size_t readAgain();
        const std::string* find(std::string_view name) const;
        const std::string* findDefault(std::string_view name) const;
        bool set(std::string_view name, const model::Value& value);

        std::size_t size() const
        {
            return _values.size();
        }

    private:
        // the name of the mod they are of, as MCM has it
        std::string _modName;

        // the player's file of the mod, which MCM writes on every set. Empty with no folder of MCM
        std::filesystem::path _playerFile;

        // the text of each ModSetting, by its name
        std::map<std::string, std::string, std::less<>> _values;

        // the text the mod's own file has for each, which is its default
        std::map<std::string, std::string, std::less<>> _defaults;
    };
}
