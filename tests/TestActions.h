#pragma once

#include <vector>

#include "model/Definition.h"

// The functions the MCM store calls in a test, which has no scripts: TestActions.cpp is the tests' own
// `mcm::callFunction`, in place of the mod's.
namespace test_actions
{
    /**
     * The functions that are called while it lives, in their order. It starts with none, and with every
     * form in the game: with `inGame` off no function is called, as for a form that is not there. They are
     * kept in one list for all the tests, so a test has one of these at a time.
     */
    struct TestActions
    {
        TestActions();

        std::vector<mods_config::model::CallFunction>& called;
        bool& inGame;
    };
}
