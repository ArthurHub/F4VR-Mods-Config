#pragma once

#include <string>
#include <vector>

// The events the MCM store sends in a test, which has no scripts: TestEvents.cpp is the tests' own
// `mcm::sendEvent`, in place of the mod's.
namespace test_events
{
    /**
     * The events that are sent while it lives, in their order: each as its name, and with its texts after it
     * in brackets, as `OnMCMSettingChange(MyMod, bOn:Main)`. It starts with none. They are kept in one list
     * for all the tests, so a test has one of these at a time.
     */
    struct TestEvents
    {
        TestEvents();

        std::vector<std::string>& sent;
    };
}
