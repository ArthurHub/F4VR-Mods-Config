#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

        // the value is shown and edited, but is no value of its setting: one in a file that is not of its type,
        // outside its limits, of a length its text input does not allow, or no option of its choice, or one the
        // player set so, as a text that is no number. It is the value as it is read where that is of its type,
        // and the setting's default where it is not
        bool invalid = false;

        // why it is not available or is invalid, for the player. Empty for a storage the menu does not read yet
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
         * Sets the value of a setting. Returns whether it is set. A store that keeps a change for a Save sets a
         * value of the setting's type that is no value of it, as invalid.
         */
        virtual bool set(const Setting& setting, const Value& value) = 0;

        /**
         * The default of a setting, in the type of its value, which Reset sets as any other value is set.
         * None for a setting that has no default.
         */
        virtual std::optional<Value> defaultValue(const Setting& setting) = 0;

        /**
         * The options of a choice: its definition's, or the ones its store read in the game for one that
         * reads them there. None while that list cannot be read: its value is not available then, with the reason.
         */
        virtual const std::vector<Option>& options(const Setting& setting) const = 0;

        /**
         * Whether a change is saved as it is made, as an MCM mod's is: nothing waits for a Save then, and no
         * setting is dirty.
         */
        virtual bool savesAtOnce() const = 0;

        /**
         * Whether a setting is dirty: its value is not the one its session started with, and waits for a Save.
         */
        virtual bool isDirty(const Setting& setting) const = 0;

        /**
         * How many settings of the session are dirty.
         */
        virtual std::size_t dirtyCount() const = 0;

        /**
         * How many dirty settings are invalid: the player set them to no value of theirs, and Save writes
         * nothing while there is one. An invalid value that was read, and not changed, is not counted.
         */
        virtual std::size_t invalidCount() const = 0;

        /**
         * Writes the dirty settings, which are then clean: the values written are their new baseline. Returns
         * why, for a Save that does not write every one, as `showcase.toml cannot be written`. Nothing is
         * written while a dirty setting is invalid. Nothing for a store that saves a change as it is made.
         */
        virtual std::expected<void, std::string> save() = 0;

        /**
         * Why the last Save did not write every dirty setting, until the next change, Save or Discard, or the
         * end of the session. Empty while there is none.
         */
        virtual std::string_view saveFailure() const = 0;

        /**
         * Takes back every pending value, so each setting has its baseline again and is clean.
         */
        virtual void discard() = 0;

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
