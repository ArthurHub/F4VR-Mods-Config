#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "McmKeybinds.h"
#include "McmModSettings.h"
#include "model/Definition.h"
#include "model/Store.h"

namespace mods_config::mcm
{
    // What the store does in the game is in files of their own, which the unit tests do not compile: they
    // link their own in place of each, so the store is tested with no game.

    // The value of a global of the game, and none for one that is not in the game. Setting one returns false
    // for one that is not in the game. The mod's are in McmGlobals.cpp, and the tests' in tests/TestGlobals.cpp,
    // which keeps the globals in a map.
    std::optional<float> getGlobal(const model::GlobalStorage& global);
    bool setGlobal(const model::GlobalStorage& global, float value);

    // The value of a property of a script, in the type the script keeps it in, and none for one that is not in
    // the game. Setting one returns false for one that is not in the game. The mod's are in McmProperties.cpp,
    // and the tests' in tests/TestProperties.cpp, which keeps the properties in a map.
    std::optional<model::Value> getProperty(const model::PropertyStorage& property);
    bool setProperty(const model::PropertyStorage& property, const model::Value& value);

    // Sends an event of MCM's menu to the scripts that wait for it, with the texts their function takes.
    // The mod's is in McmEvents.cpp, which calls the scripts of the game, and the tests' in
    // tests/TestEvents.cpp, which keeps what is sent.
    void sendEvent(const std::string& event, const std::vector<std::string>& arguments = {});

    // Calls a function of a script of a form, or a global function of a script, with its arguments. Returns
    // false for one whose form or script is not in the game. The mod's is in McmActions.cpp, which queues the call to Papyrus, and the tests' in
    // tests/TestActions.cpp, which keeps what is called.
    bool callFunction(const model::CallFunction& call);

    /**
     * The store of an MCM mod: a value is read from where its storage says, and a change is written there at
     * once, as MCM's own menu does. It has no Save.
     * A setting with no storage is held here, from the value a setting has with none, while its mod is selected.
     * A ModSetting is read from MCM's files as its mod is selected, and is written through MCM. It alone
     * has a default: the value of the mod's own file.
     * The key of a hotkey is read from MCM's files then too, and is only shown.
     * A global is read and written with `getGlobal` and `setGlobal`, and a property of a script with
     * `getProperty` and `setProperty`.
     * It sends the events MCM's menu sends, so a script acts on this menu as on MCM's: the menu opened and
     * closed, a mod shown and left, and a setting changed. They are sent with `sendEvent`.
     * The `CallFunction` and the `CallGlobalFunction` of a button are run with `callFunction`, and so are
     * the ones of a value row, after its value is written, with the value in their arguments.
     */
    class McmStore : public model::Store
    {
    public:
        explicit McmStore(std::filesystem::path mcmFolder = {});

        void menuOpened();
        void menuClosed();

        void open(const model::Mod& mod) override;
        void close() override;
        model::StoredValue value(const model::Setting& setting) override;
        model::StoredValue valueById(std::string_view id) override;
        bool set(const model::Setting& setting, const model::Value& value) override;
        std::optional<model::Value> defaultValue(const model::Setting& setting) override;
        model::StoredValue hotkey(const model::Hotkey& hotkey) override;
        bool run(const model::Action& action) override;

    private:
        bool write(const model::Setting& setting, const model::Value& value);

        // MCM's folder, which is `Data\MCM` in the game. With none no ModSetting is available
        std::filesystem::path _mcmFolder;

        // the name of the mod that is selected, as MCM has it. Empty with none
        std::string _modName;

        // the ModSettings of the mod that is selected, as MCM's files had them when it was, with those that
        // were set since
        McmModSettings _modSettings;

        // the hotkeys of the mod that is selected, with the key of each as MCM's files had it when it was
        McmKeybinds _keybinds;

        // the settings of the mod that is selected, by their ids
        std::map<std::string, const model::Setting*, std::less<>> _settings;

        // the values of the settings with no storage that were set, of the mod that is selected
        std::unordered_map<const model::Setting*, model::Value> _held;
    };
}
