#include "TestActions.h"

#include "mcm/McmStore.h"

namespace
{
    /**
     * The functions that were called, of all the tests.
     */
    std::vector<mods_config::model::CallFunction>& calledFunctions()
    {
        static std::vector<mods_config::model::CallFunction> called;
        return called;
    }

    /**
     * Whether the forms of the tests are in the game.
     */
    bool& formsInGame()
    {
        static bool inGame = true;
        return inGame;
    }
}

namespace test_actions
{
    TestActions::TestActions()
        : called(calledFunctions()),
          inGame(formsInGame())
    {
        called.clear();
        inGame = true;
    }
}

namespace mods_config::mcm
{
    /**
     * Keeps a call in place of making it: the tests' own, which is linked in place of the mod's.
     */
    bool callFunction(const model::CallFunction& call)
    {
        if (!formsInGame()) {
            return false;
        }
        calledFunctions().push_back(call);
        return true;
    }
}
