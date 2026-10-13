#include "McmKeybinds.h"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

using namespace mods_config;

namespace
{
    using Json = nlohmann::json;

    // the keys that are held with a hotkey's key, as bits of its `modifiers`
    constexpr std::uint32_t MODIFIER_SHIFT = 1 << 0;
    constexpr std::uint32_t MODIFIER_CONTROL = 1 << 1;
    constexpr std::uint32_t MODIFIER_ALT = 1 << 2;

    using KeyName = std::pair<std::uint32_t, std::string_view>;

    // the keys MCM's menu has a name for, beside the letters, the digits, the function keys and the digits of
    // the number pad. A key of the keyboard is by its virtual-key code of Windows, and a button of the mouse
    // is from 256
    constexpr std::array KEY_NAMES{
        KeyName{ 8, "Backspace" },
        KeyName{ 9, "Tab" },
        KeyName{ 13, "Enter" },
        KeyName{ 16, "Shift" },
        KeyName{ 17, "Ctrl" },
        KeyName{ 18, "Alt" },
        KeyName{ 27, "Escape" },
        KeyName{ 32, "Space" },
        KeyName{ 33, "PgUp" },
        KeyName{ 34, "PgDn" },
        KeyName{ 35, "End" },
        KeyName{ 36, "Home" },
        KeyName{ 37, "Left" },
        KeyName{ 38, "Up" },
        KeyName{ 39, "Right" },
        KeyName{ 40, "Down" },
        KeyName{ 45, "Insert" },
        KeyName{ 46, "Delete" },
        KeyName{ 106, "NumPad*" },
        KeyName{ 107, "NumPad+" },
        KeyName{ 109, "NumPad-" },
        KeyName{ 110, "NumPad." },
        KeyName{ 111, "NumPad/" },
        KeyName{ 160, "L Shift" },
        KeyName{ 161, "R Shift" },
        KeyName{ 162, "L Ctrl" },
        KeyName{ 163, "R Ctrl" },
        KeyName{ 164, "L Alt" },
        KeyName{ 165, "R Alt" },
        KeyName{ 186, ";" },
        KeyName{ 187, "=" },
        KeyName{ 188, "," },
        KeyName{ 189, "-" },
        KeyName{ 190, "." },
        KeyName{ 191, "/" },
        KeyName{ 192, "`" },
        KeyName{ 219, "[" },
        KeyName{ 220, "\\" },
        KeyName{ 221, "]" },
        KeyName{ 222, "'" },
        KeyName{ 258, "Mouse3" },
        KeyName{ 259, "Mouse4" },
        KeyName{ 260, "Mouse5" },
        KeyName{ 261, "Mouse6" },
        KeyName{ 262, "Mouse7" },
        KeyName{ 263, "Mouse8" },
    };

    /**
     * The name of one key, as MCM's menu names it. A key the menu has no name for, which it binds no hotkey
     * to, is its code.
     */
    std::string nameOfKey(const std::uint32_t keycode)
    {
        if ((keycode >= 'A' && keycode <= 'Z') || (keycode >= '0' && keycode <= '9')) {
            return std::string(1, static_cast<char>(keycode));
        }
        if (keycode >= 112 && keycode <= 123) {
            return std::format("F{}", keycode - 111);
        }
        if (keycode >= 96 && keycode <= 105) {
            return std::format("NumPad{}", keycode - 96);
        }
        const auto named = std::ranges::find(KEY_NAMES, keycode, &KeyName::first);
        return named != KEY_NAMES.end() ? std::string(named->second) : std::format("Key {}", keycode);
    }

    /**
     * A text in small letters: MCM knows a mod and a hotkey by their names in any letter case.
     */
    std::string toLower(const std::string_view text)
    {
        std::string lower(text);
        std::ranges::transform(lower, lower.begin(), [](const unsigned char letter) {
            return static_cast<char>(std::tolower(letter));
        });
        return lower;
    }

    /**
     * A JSON file, with its comments skipped as MCM's reader skips them. A file that is not there, or is no
     * JSON, is a value with no keys.
     */
    Json readJson(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        auto json = stream ? Json::parse(stream, nullptr, false, true) : Json();
        return json.is_discarded() ? Json() : json;
    }

    /**
     * The text of a key. It is empty when the value has no such key, or the key holds something else than a text.
     */
    std::string getText(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_string() ? found->get<std::string>() : std::string();
    }

    /**
     * The whole number of a key, and 0 when the value has no such key, or the key holds something else than
     * a number that is not less than 0.
     */
    std::uint32_t getCode(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        const auto number = found != value.end() && found->is_number() ? found->get<double>() : 0.0;
        return number > 0.0 && number < 65536.0 ? static_cast<std::uint32_t>(number) : 0;
    }

    /**
     * The entries of the `keybinds` list of a keybind file, and none for a file without one.
     */
    std::vector<Json> getKeybinds(const Json& file)
    {
        const auto keybinds = file.find("keybinds");
        return keybinds != file.end() && keybinds->is_array() ? keybinds->get<std::vector<Json>>() : std::vector<Json>();
    }

    // what a hotkey does, by the `type` of its `action`: MCM has no hotkey that does anything else
    constexpr std::array<std::string_view, 4> ACTION_TYPES{ "CallFunction", "CallGlobalFunction", "RunConsoleCommand", "SendEvent" };
}

namespace mods_config::mcm
{
    /**
     * A key as a text, as MCM's menu shows it: the name of the key, after the keys that are held with it,
     * as `Ctrl-Shift-F5`. `None` for no key.
     */
    std::string keyName(const std::uint32_t keycode, const std::uint32_t modifiers)
    {
        if (keycode == 0) {
            return "None";
        }
        std::string name;
        if (modifiers & MODIFIER_CONTROL) {
            name += "Ctrl-";
        }
        if (modifiers & MODIFIER_ALT) {
            name += "Alt-";
        }
        if (modifiers & MODIFIER_SHIFT) {
            name += "Shift-";
        }
        return name + nameOfKey(keycode);
    }

    /**
     * Reads the hotkeys of a mod from MCM's folder, which is `Data\MCM` in the game, in place of those it
     * has. With no folder there are none, and neither are there with no `keybinds.json` of the mod, or with
     * one that names another mod: MCM binds no key to a hotkey that file does not have.
     * MCM also has none whose action is of a form that is not in the game, which is not looked up here.
     * A hotkey with two keys in the player's file has the lower one, as in MCM's menu.
     */
    void McmKeybinds::load(const std::filesystem::path& mcmFolder, const std::string_view modName)
    {
        _keys.clear();
        if (mcmFolder.empty()) {
            return;
        }
        const auto mod = toLower(modName);
        const auto modFile = readJson(mcmFolder / "Config" / modName / "keybinds.json");
        if (toLower(getText(modFile, "modName")) != mod) {
            return;
        }
        for (const auto& keybind : getKeybinds(modFile)) {
            const auto id = getText(keybind, "id");
            const auto action = keybind.find("action");
            if (!id.empty() && action != keybind.end() && std::ranges::contains(ACTION_TYPES, getText(*action, "type"))) {
                _keys.insert_or_assign(toLower(id), keyName(0, 0));
            }
        }

        const auto playerFile = readJson(mcmFolder / "Settings" / "Keybinds.json");
        if (getCode(playerFile, "version") < 1) {
            return;
        }
        std::map<std::string, std::pair<std::uint32_t, std::uint32_t>, std::less<>> bound;
        for (const auto& keybind : getKeybinds(playerFile)) {
            const auto id = toLower(getText(keybind, "id"));
            const std::pair key(getCode(keybind, "keycode"), getCode(keybind, "modifiers"));
            if (key.first == 0 || toLower(getText(keybind, "modName")) != mod || !_keys.contains(id)) {
                continue;
            }
            if (const auto [place, added] = bound.try_emplace(id, key); !added && key < place->second) {
                place->second = key;
            }
        }
        for (const auto& [id, key] : bound) {
            _keys.insert_or_assign(id, keyName(key.first, key.second));
        }
    }

    /**
     * The name of the key of a hotkey, `None` for one with no key, and none for a hotkey the mod does not have.
     */
    const std::string* McmKeybinds::find(const std::string_view id) const
    {
        const auto found = _keys.find(toLower(id));
        return found != _keys.end() ? &found->second : nullptr;
    }
}
