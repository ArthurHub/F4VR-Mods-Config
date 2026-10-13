#include "TestEvents.h"

#include "mcm/McmStore.h"

namespace
{
    /**
     * The events that were sent, of all the tests.
     */
    std::vector<std::string>& sentEvents()
    {
        static std::vector<std::string> sent;
        return sent;
    }
}

namespace test_events
{
    TestEvents::TestEvents()
        : sent(sentEvents())
    {
        sent.clear();
    }
}

namespace mods_config::mcm
{
    /**
     * Keeps an event in place of sending it: the tests' own, which is linked in place of the mod's.
     */
    void sendEvent(const std::string& event, const std::vector<std::string>& arguments)
    {
        auto text = event;
        for (std::size_t i = 0; i < arguments.size(); ++i) {
            text += (i == 0 ? "(" : ", ") + arguments[i];
        }
        sentEvents().push_back(arguments.empty() ? text : text + ")");
    }
}
