#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "Definition.h"

// The store of the settings model: the values of the mod that is selected, which the UI asks for and sets.
// See docs/design/model.md.

namespace mods_config::model
{
    /**
     * The value of a setting as its store has it now.
     */
    struct StoredValue
    {
        // in the type of its setting: a bool for a switcher, a float for a slider, and the value of the
        // chosen option for a stepper and a dropdown
        Value value;

        // false for a value that cannot be read or written: its row is disabled
        bool available = false;

        // why it is not available, for the player. Empty for a storage the menu does not read yet
        std::string reason;
    };

    /**
     * The values of the mod that is selected. The UI reads the definition to know what to draw, and asks the
     * store for every value: it never reads or writes a global, a file or a script property itself.
     */
    class Store
    {
    public:
        virtual ~Store() = default;

        /**
         * Starts the editing session of a mod, as the mod is selected. It ends the session of the mod before it.
         */
        virtual void open(const Mod& mod) = 0;

        /**
         * Ends the editing session, as the mod is left for no other. Nothing with no session.
         */
        virtual void close() = 0;

        /**
         * Another page of the mod whose session it is is shown: a store reads the values it keeps in files again.
         */
        virtual void pageShown() = 0;

        virtual StoredValue value(const Setting& setting) = 0;

        /**
         * The value of the setting of an id, of the mod whose session it is. It is not available for an id
         * that no setting of the mod has.
         */
        virtual StoredValue valueById(std::string_view id) = 0;

        /**
         * Sets the value of a setting. Returns whether it is set.
         */
        virtual bool set(const Setting& setting, const Value& value) = 0;

        /**
         * The default of a setting, in the type of its value, which Reset sets as any other value is set.
         * None for a setting that has no default.
         */
        virtual std::optional<Value> defaultValue(const Setting& setting) = 0;

        /**
         * The key a hotkey is bound to, as a text: its name, with the keys that are held with it. It is not
         * available for a hotkey its source does not have.
         */
        virtual StoredValue hotkey(const Hotkey& hotkey) = 0;

        /**
         * Runs an action. Returns whether it is run: not one the store does not run, and not one whose
         * target is not there.
         */
        virtual bool run(const Action& action) = 0;
    };
}
