#pragma once

#include "model/ConfigDefinition.h"

// The actions a store runs in the game, for a button of an MCM mod and of a config spec. The mod's are in
// GameActions.cpp, and the tests' in tests/linked/TestActions.cpp, which keeps what is run.
namespace mods_config
{
    // Calls a function of a script of a form, or a global function of a script, with its arguments. Returns
    // false for one whose form or script is not in the game.
    bool callFunction(const model::CallFunction& call);

    // Runs a line of the game's console. Returns whether it is run.
    bool runConsoleCommand(const model::ConsoleCommand& command);

    // Opens a link with Windows' default handler. Returns whether Windows accepted it.
    bool openWebLink(const model::WebLink& link);

    // Sends the F4SE message of a Save of a mod to each F4SE plugin of its config spec, with the mod's name.
    void sendSavedMessage(const std::string& modName, const std::vector<std::string>& plugins);
}
