#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "GameActions.h"
#include "GameValues.h"
#include "McmKeybinds.h"
#include "McmModSettings.h"
#include "model/Definition.h"
#include "model/Store.h"

namespace mods_config::mcm
{
    // What the store does in the game is in files of their own, which the unit tests do not compile: they
    // link their own in place of each, so the store is tested with no game.

    // Sends an event of MCM's menu to the scripts that wait for it, with the texts their function takes.
    // The mod's is in McmEvents.cpp, which calls the scripts of the game, and the tests' in
    // tests/TestEvents.cpp, which keeps what is sent.
    void sendEvent(const std::string& event, const std::vector<std::string>& arguments = {});

    // Tells the game's listeners of its menus that the pause menu opened and then closed, which it did not: a
    // DLL reads its MCM file as the pause menu closes, which MCM's menu is in. The mod's is in McmEvents.cpp,
    // and the tests' in tests/TestEvents.cpp, which keeps the two events as sent.
    void sendPauseMenuEvents();

    /**
     * The store of an MCM mod: a value is read from where its storage says, and a change is written there at
     * once, as MCM's own menu does. It has no Save.
     * A setting with no storage is held here, from the value a setting has with none, while its mod is selected.
     * A ModSetting is read from MCM's files as its mod is selected, and the player's file again as another
     * of its pages is shown, as a script can set one. It is written through MCM. It alone has a default: the
     * value of the mod's own file.
     * The key of a hotkey is read from MCM's files then too, and is only shown.
     * A global is read and written with `getGlobal` and `setGlobal`, and a property of a script with
     * `getProperty` and `setProperty`.
     * It sends the events MCM's menu sends, so a script acts on this menu as on MCM's: the menu opened and
     * closed, a mod shown and left, and a setting changed. They are sent with `sendEvent`. As the menu closes
     * after a ModSetting was written, the pause menu is told as opened and closed, with `sendPauseMenuEvents`.
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
        void pageShown() override;
        model::StoredValue value(const model::Setting& setting) override;
        model::StoredValue valueById(std::string_view id) override;
        bool set(const model::Setting& setting, const model::Value& value) override;
        std::optional<model::Value> defaultValue(const model::Setting& setting) override;

        bool savesAtOnce() const override
        {
            return true;
        }

        bool isDirty(const model::Setting&) const override
        {
            return false;
        }

        std::size_t dirtyCount() const override
        {
            return 0;
        }

        bool save() override
        {
            return true;
        }

        void discard() override
        {}

        model::StoredValue hotkey(const model::Hotkey& hotkey) override;
        bool run(const model::Action& action) override;

    private:
        bool write(const model::Setting& setting, const model::Value& value);

        // MCM's folder, which is `Data\MCM` in the game. With none no ModSetting is available
        std::filesystem::path _mcmFolder;

        // the name of the mod that is selected, as MCM has it. Empty with none
        std::string _modName;

        // the ModSettings of the mod that is selected, as MCM's files had them when they were last read, with
        // those that were set since
        McmModSettings _modSettings;

        // the hotkeys of the mod that is selected, with the key of each as MCM's files had it when it was
        McmKeybinds _keybinds;

        // the settings of the mod that is selected, by their ids
        std::map<std::string, const model::Setting*, std::less<>> _settings;

        // the values of the settings with no storage that were set, of the mod that is selected
        std::unordered_map<const model::Setting*, model::Value> _held;

        // a ModSetting of any mod was written through MCM since the menu opened
        bool _modSettingWritten = false;
    };
}
