#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ConfigDefinition.h"

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
     * What a run of an action did: whether it ran, whether it failed, as a function of a mod's DLL can, and the
     * message of its result, which can be none.
     */
    struct ActionResult
    {
        bool ran = false;
        bool failed = false;
        std::string message;
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
         * When a change of a setting takes effect in its running mod, as its config spec says: unknown for an MCM
         * mod's. None for one that is not set, as a read-only one.
         */
        virtual std::optional<Applies> applies(const Setting& setting) const = 0;

        /**
         * How many dirty settings take effect after the game is restarted.
         */
        virtual std::size_t dirtyRestartCount() const = 0;

        /**
         * The last Save, until the next change, Discard or the end of the session: how many of the settings it
         * wrote take effect after the game is restarted. None while there is no such Save.
         */
        virtual std::optional<std::size_t> lastSave() const = 0;

        /**
         * How many settings of a mod that take effect after the game is restarted the Saves of this run of the
         * game wrote. Each counts once, also one that was set back and saved again: the store does not know
         * what the mod has read.
         */
        virtual std::size_t savedRestartCount(const Mod& mod) const = 0;

        /**
         * Writes the dirty settings, which are then clean: the values written are their new baseline. Returns
         * why, for a Save that does not write every one, as `showcase.toml cannot be written`. Nothing is
         * written while a dirty setting is invalid.
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
         * The value of a state of the selected mod's client, by its name, which a `clientState` condition asks: not
         * available for a mod with no client, and while the client does not answer it.
         */
        virtual StoredValue clientState(std::string_view)
        {
            return {};
        }

        /**
         * The page of the selected mod that is shown, after its session starts and as another is shown, which a store
         * tells its mod's client: the mod's front page for none.
         */
        virtual void showPage(const Page*)
        {}

        /**
         * Reads the values of the selected mod again, as its own UI can have changed them: a clean setting takes its
         * new value, and a pending one stays the player's. Nothing for a store that reads its values as they are asked.
         */
        virtual void readAgain()
        {}

        /**
         * A frame of the panel starts: a store asks the states of its mod's client again as they are next asked.
         */
        virtual void startFrame()
        {}

        /**
         * Why an action of the selected mod does not run, which its button's help line says: empty for one that runs,
         * or that the store does not know to be unable to. The UI tells an action the menu does not run, and one that
         * runs nothing, by itself.
         */
        virtual std::string notRunnable(const Action&) const
        {
            return {};
        }

        /**
         * Runs an action. Returns whether it is run: not one the store does not run, and not one whose
         * target is not there, and what it did.
         */
        virtual ActionResult run(const Action& action) = 0;
    };
}
