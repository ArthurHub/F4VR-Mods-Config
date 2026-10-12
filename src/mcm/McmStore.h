#pragma once

#include <optional>
#include <unordered_map>

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

    /**
     * The store of an MCM mod: a value is read from where its storage says, and a change is written there at
     * once, as MCM's own menu does. It has no Save.
     * A setting with no storage is held here, from the value a setting has with none, while its mod is selected.
     */
    class McmStore : public model::Store
    {
    public:
        explicit McmStore(Globals& globals);

        void open(const model::Mod& mod) override;
        model::StoredValue value(const model::Setting& setting) override;
        bool set(const model::Setting& setting, const model::Value& value) override;

    private:
        Globals& _globals;

        // the values of the settings with no storage that were set, of the mod that is selected
        std::unordered_map<const model::Setting*, model::Value> _held;
    };
}
