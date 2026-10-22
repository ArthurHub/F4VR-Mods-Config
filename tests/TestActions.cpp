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

    /**
     * The web links whose launch was attempted, of all the tests.
     */
    std::vector<mods_config::model::WebLink>& openedLinks()
    {
        static std::vector<mods_config::model::WebLink> links;
        return links;
    }

    /**
     * Whether Windows accepts the browser launch in a test.
     */
    bool& canOpenBrowser()
    {
        static bool available = true;
        return available;
    }
}

namespace test_actions
{
    TestActions::TestActions()
        : called(calledFunctions()),
          commands(runCommands()),
          links(openedLinks()),
          inGame(formsInGame()),
          browserAvailable(canOpenBrowser())
    {
        called.clear();
        commands.clear();
        links.clear();
        inGame = true;
        browserAvailable = true;
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

    /**
     * Records a web link without opening a browser, and simulates a failed launch when requested.
     */
    bool openWebLink(const model::WebLink& link)
    {
        openedLinks().push_back(link);
        return canOpenBrowser();
    }
}
