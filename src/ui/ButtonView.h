#pragma once

#include <string>

#include "model/ConfigDefinition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    /**
     * Whether a button runs and how a press on it goes, from the button and its store. The button row draws and
     * presses from it and devbench reports and runs from it, so an e2e test's press is the player's.
     */
    struct ButtonView
    {
        // its enabledWhen holds
        bool enabled = false;

        // why it is not run, which its help line says: the reason of an action the menu does not know, or the store's
        // of one it cannot run. Empty for one that runs and for one with no action
        std::string reason;

        // a press runs it: it has an action the menu and its store run, and it is enabled
        bool runs = false;

        // a press asks to save the unsaved changes of its mod first, as its action reads the saved values
        bool asksToSaveFirst = false;
    };

    ButtonView buttonViewOf(const model::Button& button, model::Store* store);
}
