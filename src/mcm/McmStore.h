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
#include "model/ChoiceOptions.h"
#include "model/ConfigDefinition.h"
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
     * The store of an MCM mod: a value is read from where its storage says, and a change is pending until a
     * Save writes it there, or a Discard drops it. Nothing is previewed.
     * A setting with no storage is held here, from the value a setting has with none, while its mod is selected,
     * and is set at once: nothing writes it, so it is never dirty.
     * A ModSetting is read from MCM's files as its mod is selected, and the player's file again as another
     * of its pages is shown, as a script can set one. It is written through MCM. It alone has a default: the
     * value of the mod's own file.
     * The key of a hotkey is read from MCM's files then too, and is only shown.
     * A choice that reads its options in the game, from a FormList or a property of a script, reads them as
     * its mod is selected and again as another of its pages is shown, as MCM's menu does.
     * A global is read and written with `getGlobal` and `setGlobal`, and a property of a script with
     * `getProperty` and `setProperty`.
     * It sends the events MCM's menu sends, so a script acts on this menu as on MCM's: the menu opened and
     * closed, a mod shown and left, and a setting changed. They are sent with `sendEvent`. After a Save that
     * wrote a ModSetting, the pause menu is told as opened and closed, with `sendPauseMenuEvents`.
     * The `CallFunction` and the `CallGlobalFunction` of a button are run with `callFunction`, and so are
     * the ones of a value row, by the Save that writes its value, with the value in their arguments.
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

        const std::vector<model::Option>& options(const model::Setting& setting) const override
        {
            return _options.of(setting);
        }

        bool isDirty(const model::Setting& setting) const override;

        std::size_t dirtyCount() const override
        {
            return _pending.size();
        }

        // a value that is no value of its setting is not set
        std::size_t invalidCount() const override
        {
            return 0;
        }

        // an MCM config does not say when a change takes effect
        std::optional<model::Applies> applies(const model::Setting&) const override
        {
            return model::Applies::Unknown;
        }

        std::size_t dirtyRestartCount() const override
        {
            return 0;
        }

        std::optional<std::size_t> lastSave() const override
        {
            return _lastSave;
        }

        std::size_t savedRestartCount(const model::Mod&) const override
        {
            return 0;
        }

        std::expected<void, std::string> save() override;

        std::string_view saveFailure() const override
        {
            return _saveFailure;
        }

        void discard() override;
        void startFrame() override;
        model::StoredValue hotkey(const model::Hotkey& hotkey) override;
        model::ActionResult run(const model::Action& action) override;

    private:
        // a change of a setting that waits for a Save, in the type of its row
        struct Pending
        {
            const model::Setting* setting;
            model::Value value;
        };

        model::StoredValue stored(const model::Setting& setting);
        std::optional<model::Value> readBack(const model::Setting& setting, const model::Value& value) const;
        bool write(const model::Setting& setting, const model::Value& value);
        void written(const model::Setting& setting, const model::Value& value) const;

        // MCM's folder, which is `Data\MCM` in the game. With none no ModSetting is available
        std::filesystem::path _mcmFolder;

        // the name of the mod that is selected, as MCM has it. Empty with none
        std::string _modName;

        // the ModSettings of the mod that is selected, as MCM's files had them when they were last read, with
        // those that were set since
        McmModSettings _modSettings;

        // the hotkeys of the mod that is selected, with the key of each as MCM's files had it when it was
        McmKeybinds _keybinds;

        // the options of the choices of the mod that is selected
        model::ChoiceOptions _options;

        // the settings of the mod that is selected, by their ids
        std::map<std::string, const model::Setting*, std::less<>> _settings;

        // the settings of the mod that is selected, in the order of its config, which a Save writes them in
        std::vector<const model::Setting*> _settingList;

        // the values of the settings with no storage that were set, of the mod that is selected
        std::unordered_map<const model::Setting*, model::Value> _held;

        // the changes of the mod that is selected that wait for a Save, in the order they were made
        std::vector<Pending> _pending;

        // why the last Save did not write every change, until the next change, Save or Discard
        std::string _saveFailure;

        // the last Save, until the next change or Discard: none of its changes takes effect after a restart that
        // the store knows of
        std::optional<std::size_t> _lastSave;
    };
}
