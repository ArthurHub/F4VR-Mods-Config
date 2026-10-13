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
     * writes on every set. A value is the text of its key, by the name MCM gives it: `fStrength:Main` is the
     * key `fStrength` of the section `[Main]`, in the letter case of the file.
     * One is set through MCM's own Papyrus function, so MCM has the value in its memory and writes it to the
     * player's file. With no game, in a test, MCM is not called.
     */
    class McmModSettings
    {
    public:
        void load(const std::filesystem::path& mcmFolder, std::string_view modName);
        const std::string* find(std::string_view name) const;
        bool set(std::string_view name, const model::Value& value);

        std::size_t size() const
        {
            return _values.size();
        }

    private:
        // the name of the mod they are of, as MCM has it
        std::string _modName;

        // the text of each ModSetting, by its name
        std::map<std::string, std::string, std::less<>> _values;
    };
}
