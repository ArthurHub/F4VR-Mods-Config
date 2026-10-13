#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace mods_config::mcm
{
    /**
     * The ModSettings of one mod, as MCM's two files have them: the defaults the mod ships, in
     * `Config\<mod>\settings.ini`, and over them the player's values, in `Settings\<mod>.ini`, which MCM
     * writes on every set. A value is the text of its key, by the name MCM gives it: `fStrength:Main` is the
     * key `fStrength` of the section `[Main]`, in the letter case of the file.
     */
    class McmModSettings
    {
    public:
        void load(const std::filesystem::path& mcmFolder, std::string_view modName);
        const std::string* find(std::string_view name) const;

        std::size_t size() const
        {
            return _values.size();
        }

    private:
        // the text of each ModSetting, by its name
        std::map<std::string, std::string, std::less<>> _values;
    };
}
