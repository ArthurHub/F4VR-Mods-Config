#include "McmModSettings.h"

#include <format>

#include <SimpleIni.h>

using namespace mods_config;

namespace
{
    /**
     * Reads the keys of an INI file into the values, each by its key and its section, over what is there.
     * The names are in the letter case of the file, as MCM has them, and a key before the first section is
     * none, as MCM reads none there. A file that is not there, or is no INI, has no keys.
     */
    void readIni(const std::filesystem::path& file, std::map<std::string, std::string, std::less<>>& values)
    {
        CSimpleIniCaseA ini;
        ini.SetUnicode();
        if (ini.LoadFile(file.c_str()) < 0) {
            return;
        }
        CSimpleIniCaseA::TNamesDepend sections;
        ini.GetAllSections(sections);
        for (const auto& section : sections) {
            if (!*section.pItem) {
                continue;
            }
            CSimpleIniCaseA::TNamesDepend keys;
            ini.GetAllKeys(section.pItem, keys);
            for (const auto& key : keys) {
                values.insert_or_assign(std::format("{}:{}", key.pItem, section.pItem), ini.GetValue(section.pItem, key.pItem, ""));
            }
        }
    }
}

namespace mods_config::mcm
{
    /**
     * Reads the ModSettings of a mod from MCM's folder, which is `Data\MCM` in the game, in place of those
     * it has. A file that is not there has no values, and with no folder there are none.
     */
    void McmModSettings::load(const std::filesystem::path& mcmFolder, const std::string_view modName)
    {
        _values.clear();
        if (mcmFolder.empty()) {
            return;
        }
        const std::string mod(modName);
        readIni(mcmFolder / "Config" / mod / "settings.ini", _values);
        readIni(mcmFolder / "Settings" / (mod + ".ini"), _values);
    }

    /**
     * The text of a ModSetting, and none for one that is in neither file.
     */
    const std::string* McmModSettings::find(const std::string_view name) const
    {
        const auto found = _values.find(name);
        return found != _values.end() ? &found->second : nullptr;
    }
}
