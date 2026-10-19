#include "TestActions.h"

#include "GameActions.h"

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
     * The console commands that were run, of all the tests.
     */
    std::vector<mods_config::model::ConsoleCommand>& runCommands()
    {
        static std::vector<mods_config::model::ConsoleCommand> run;
        return run;
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
          commands(runCommands()),
          inGame(formsInGame())
    {
        called.clear();
        commands.clear();
        inGame = true;
    }
}

namespace mods_config
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

    /**
     * Keeps a console command in place of running it: the tests' own, which is linked in place of the mod's.
     */
    bool runConsoleCommand(const model::ConsoleCommand& command)
    {
        runCommands().push_back(command);
        return true;
    }
}
