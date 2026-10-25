#pragma once

#include <string>
#include <vector>

#include "model/ConfigDefinition.h"

// The actions a store runs in a test, which has no game: TestActions.cpp is the tests' own `callFunction`,
// `runConsoleCommand`, `openWebLink` and `sendSavedMessage`, in place of the mod's.
namespace test_actions
{
    /**
     * The functions that are called and the console commands that are run while it lives, in their order. It
     * starts with none, and with every form in the game: with `inGame` off no function is called, as for a
     * form that is not there. They are kept in one list each for all the tests, so a test has one of these at
     * a time.
     */
    struct TestActions
    {
        TestActions();

        std::vector<mods_config::model::CallFunction>& called;
        std::vector<mods_config::model::ConsoleCommand>& commands;
        std::vector<mods_config::model::WebLink>& links;
        bool& inGame;
        bool& browserAvailable;

        // each F4SE message of a Save, as the mod's name and the plugin it is sent to: `MyMod > MyPlugin`
        std::vector<std::string>& savedMessages;
    };
}
