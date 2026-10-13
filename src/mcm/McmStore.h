#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "McmModSettings.h"
#include "model/Definition.h"
#include "model/Store.h"

namespace mods_config::mcm
{
    /**
     * The globals of the game, which the store reads and writes through: the game's own in the mod, and a
     * test's without the game.
     */
    class Globals
    {
    public:
        virtual ~Globals() = default;

        /**
         * The value of a global, and none for one that is not in the game.
         */
        virtual std::optional<float> get(const model::GlobalStorage& global) = 0;

        /**
         * Sets the value of a global. Returns false for one that is not in the game.
         */
        virtual bool set(const model::GlobalStorage& global, float value) = 0;
    };

    // Sends an event of MCM's menu to the scripts that wait for it, with the texts their function takes.
    // The mod's is in McmEvents.cpp, which calls the scripts of the game. The unit tests do not compile that
    // file: they have their own in tests/TestEvents.cpp, which keeps what is sent.
    void sendEvent(const std::string& event, const std::vector<std::string>& arguments = {});

    /**
     * The store of an MCM mod: a value is read from where its storage says, and a change is written there at
     * once, as MCM's own menu does. It has no Save.
     * A setting with no storage is held here, from the value a setting has with none, while its mod is selected.
     * A ModSetting is read from MCM's files as its mod is selected, and is written through MCM.
     * A global is the game's, and a test gives the store globals of its own.
     * It sends the events MCM's menu sends, so a script acts on this menu as on MCM's: the menu opened and
     * closed, a mod shown and left, and a setting changed. They are sent with `sendEvent`.
     */
    class McmStore : public model::Store
    {
    public:
        explicit McmStore(std::filesystem::path mcmFolder);
        explicit McmStore(Globals& globals, std::filesystem::path mcmFolder = {});

        void menuOpened();
        void menuClosed();

        void open(const model::Mod& mod) override;
        void close() override;
        model::StoredValue value(const model::Setting& setting) override;
        model::StoredValue valueById(std::string_view id) override;
        bool set(const model::Setting& setting, const model::Value& value) override;

    private:
        bool write(const model::Setting& setting, const model::Value& value);

        // the globals of the running game, for a store that is given none
        std::unique_ptr<Globals> _gameGlobals;

        // the globals a value is read from and written to: the game's, or a test's
        Globals& _globals;

        // MCM's folder, which is `Data\MCM` in the game. With none no ModSetting is available
        std::filesystem::path _mcmFolder;

        // the name of the mod that is selected, as MCM has it. Empty with none
        std::string _modName;

        // the ModSettings of the mod that is selected, as MCM's files had them when it was, with those that
        // were set since
        McmModSettings _modSettings;

        // the settings of the mod that is selected, by their ids
        std::map<std::string, const model::Setting*, std::less<>> _settings;

        // the values of the settings with no storage that were set, of the mod that is selected
        std::unordered_map<const model::Setting*, model::Value> _held;
    };
}
