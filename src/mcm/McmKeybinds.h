#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace mods_config::mcm
{
    std::string keyName(std::uint32_t keycode, std::uint32_t modifiers);

    /**
     * The hotkeys of one mod, as MCM's two files have them: the hotkeys the mod has, in
     * `Config\<mod>\keybinds.json`, and the key the player bound each to, in `Settings\Keybinds.json`, which
     * has the keys of every mod and which MCM writes as its menu closes.
     * A hotkey is known by its id, in any letter case, as MCM knows it. They are only read: MCM binds a key
     * in its own menu, and runs the hotkey as its key is pressed.
     */
    class McmKeybinds
    {
    public:
        void load(const std::filesystem::path& mcmFolder, std::string_view modName);
        const std::string* find(std::string_view id) const;

        std::size_t size() const
        {
            return _keys.size();
        }

    private:
        // the name of the key of each hotkey the mod has, by its id in small letters. `None` for one with no key
        std::map<std::string, std::string, std::less<>> _keys;
    };
}
