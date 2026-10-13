#include "McmModSettings.h"

#include <format>
#include <type_traits>
#include <variant>

#include <SimpleIni.h>

#include "f4sevr/PapyrusUtils.h"

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

    /**
     * The text MCM writes to the player's file for a value it keeps: 1 and 0 for a bool, and a number with
     * six decimals.
     */
    std::string textOf(const model::Value& value)
    {
        if (const auto* on = std::get_if<bool>(&value)) {
            return *on ? "1" : "0";
        }
        if (const auto* whole = std::get_if<int>(&value)) {
            return std::to_string(*whole);
        }
        if (const auto* number = std::get_if<float>(&value)) {
            return std::to_string(*number);
        }
        return std::get<std::string>(value);
    }

    /**
     * Calls the `MCM.SetModSetting*` function of the value's type, with the mod, the ModSetting and the
     * value. The call is queued to Papyrus, which runs it on its own time and gives no result: MCM changes
     * nothing for a ModSetting it does not have.
     * With no game there is no Papyrus to call, and nothing is done.
     */
    void callMcmSet(const std::string& modName, const std::string& name, const model::Value& value)
    {
        if (REL::NO_GAME) {
            return;
        }
        const auto* gameVM = F4SEVR::getGameVM();
        if (!gameVM || !gameVM->m_virtualMachine) {
            logger::warn("The ModSetting {} of '{}' is not set in MCM: Papyrus is not running", name, modName);
            return;
        }
        auto* vm = gameVM->m_virtualMachine;

        F4SEVR::VMArray<F4SEVR::VMVariable> arguments;
        F4SEVR::addArgument(arguments, F4SEVR::BSFixedString(modName.c_str()));
        F4SEVR::addArgument(arguments, F4SEVR::BSFixedString(name.c_str()));
        const char* function = std::visit(
            [&arguments]<typename T>(const T& typed) {
                if constexpr (std::is_same_v<T, bool>) {
                    F4SEVR::addArgument(arguments, typed);
                    return "SetModSettingBool";
                } else if constexpr (std::is_same_v<T, int>) {
                    F4SEVR::addArgument(arguments, static_cast<F4SEVR::SInt32>(typed));
                    return "SetModSettingInt";
                } else if constexpr (std::is_same_v<T, float>) {
                    F4SEVR::addArgument(arguments, typed);
                    return "SetModSettingFloat";
                } else {
                    F4SEVR::addArgument(arguments, F4SEVR::BSFixedString(typed.c_str()));
                    return "SetModSettingString";
                }
            },
            value);

        F4SEVR::VMValue packed;
        F4SEVR::PackValue(&packed, &arguments, vm);
        const F4SEVR::BSFixedString className("MCM");
        const F4SEVR::BSFixedString functionName(function);
        F4SEVR::CallGlobalFunctionNoWait_Internal(vm, 0, 0, &className, &functionName, &packed);
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
        _modName = modName;
        if (mcmFolder.empty()) {
            return;
        }
        readIni(mcmFolder / "Config" / _modName / "settings.ini", _values);
        readIni(mcmFolder / "Settings" / (_modName + ".ini"), _values);
    }

    /**
     * The text of a ModSetting, and none for one that is in neither file.
     */
    const std::string* McmModSettings::find(const std::string_view name) const
    {
        const auto found = _values.find(name);
        return found != _values.end() ? &found->second : nullptr;
    }

    /**
     * Sets a ModSetting in MCM, to a value of the type MCM keeps it in, and keeps the text MCM then writes
     * to the player's file: MCM sets it on Papyrus's own time, and the files are not read again for it.
     * Returns false for one that is in neither file, which MCM does not have.
     */
    bool McmModSettings::set(const std::string_view name, const model::Value& value)
    {
        const auto found = _values.find(name);
        if (found == _values.end()) {
            return false;
        }
        found->second = textOf(value);
        logger::debug("Set the ModSetting {} of '{}' to {}", found->first, _modName, found->second);
        callMcmSet(_modName, found->first, value);
        return true;
    }
}
