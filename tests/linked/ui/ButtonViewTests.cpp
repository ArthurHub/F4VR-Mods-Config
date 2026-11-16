#include <catch2/catch_test_macros.hpp>

#include <string>

#include "TestGlobals.h"
#include "TestStore.h"
#include "mcm/McmStore.h"
#include "ui/ButtonView.h"

using namespace mods_config;

namespace
{
    /**
     * A button of an action.
     */
    model::Button buttonOf(model::Action action)
    {
        return model::Button{ .text = "Apply", .help = {}, .action = std::move(action) };
    }

    model::Button scriptButton()
    {
        return buttonOf(model::CallFunction{ .plugin = "MyMod.esp", .formId = 0x800, .script = {}, .function = "Apply", .arguments = {} });
    }
}

TEST_CASE("ButtonView: a script's button runs, and asks to save first while its store keeps unsaved changes for a Save")
{
    test_store::RecordingStore store;
    const auto button = scriptButton();

    auto view = ui::buttonViewOf(button, &store);
    CHECK(view.enabled);
    CHECK(view.runs);
    CHECK(view.reason.empty());
    CHECK_FALSE(view.asksToSaveFirst);

    store.dirty = 2;
    view = ui::buttonViewOf(button, &store);
    CHECK(view.runs);
    CHECK(view.asksToSaveFirst);
}

TEST_CASE("ButtonView: a script's button of a store that writes a change at once runs at once with unsaved changes, as its script has them")
{
    test_store::RecordingStore store;
    store.atOnce = true;
    store.dirty = 1;

    const auto view = ui::buttonViewOf(scriptButton(), &store);
    CHECK(view.runs);
    CHECK_FALSE(view.asksToSaveFirst);
}

TEST_CASE("ButtonView: a script's button of an MCM mod runs at once with its changes, as they are written as they are made")
{
    test_globals::TestGlobals globals;
    globals.values = { { { "MyMod.esp", 0x801 }, 0.0f } };
    mcm::McmStore store;
    const model::Setting on{ .type = model::SettingType::Switcher,
        .text = "On",
        .help = {},
        .valueOptions = {},
        .storage = model::GlobalStorage{ .plugin = "MyMod.esp", .formId = 0x801 } };
    REQUIRE(store.set(on, true));
    REQUIRE(store.dirtyCount() == 1);

    const auto view = ui::buttonViewOf(scriptButton(), &store);
    CHECK(view.runs);
    CHECK_FALSE(view.asksToSaveFirst);
}

TEST_CASE("ButtonView: a console command, a link and a function of the mod's DLL run at once with unsaved changes, as they do not read the saved values")
{
    test_store::RecordingStore store;
    store.dirty = 1;

    for (const auto& action : { model::Action(model::ConsoleCommand{ .command = "tm" }),
             model::Action(model::WebLink{ .url = "https://example.com" }),
             model::Action(model::NativeFunction{ .function = "Apply" }) }) {
        const auto view = ui::buttonViewOf(buttonOf(action), &store);
        CHECK(view.runs);
        CHECK_FALSE(view.asksToSaveFirst);
    }
}

TEST_CASE("ButtonView: a button whose enabledWhen does not hold is disabled and not run, with no reason, and asks nothing")
{
    test_store::RecordingStore store;
    store.dirty = 1;
    auto button = scriptButton();
    button.enabledWhen = model::Condition{ .kind = model::Condition::Kind::On, .setting = "master", .conditions = {} };
    store.byId = { .value = false, .available = true, .reason = {} };

    auto view = ui::buttonViewOf(button, &store);
    CHECK_FALSE(view.enabled);
    CHECK_FALSE(view.runs);
    CHECK(view.reason.empty());
    CHECK_FALSE(view.asksToSaveFirst);

    store.byId.value = true;
    view = ui::buttonViewOf(button, &store);
    CHECK(view.enabled);
    CHECK(view.runs);
    CHECK(view.asksToSaveFirst);
}

TEST_CASE("ButtonView: an action the menu does not know and one its store cannot run are not run, with the reason")
{
    test_store::RecordingStore store;

    auto view = ui::buttonViewOf(buttonOf(model::UnknownAction{ .type = "Teleport", .reason = "Not available: the menu does not run Teleport." }), &store);
    CHECK(view.enabled);
    CHECK_FALSE(view.runs);
    CHECK(view.reason == "Not available: the menu does not run Teleport.");

    store.notRunnableReason = "Not available: the menu has no Flash.";
    view = ui::buttonViewOf(scriptButton(), &store);
    CHECK_FALSE(view.runs);
    CHECK(view.reason == "Not available: the menu has no Flash.");
    CHECK_FALSE(view.asksToSaveFirst);
}

TEST_CASE("ButtonView: a button with no action and one with no store are not run, and say no reason but an unknown action's")
{
    test_store::RecordingStore store;
    auto view = ui::buttonViewOf(buttonOf(model::NoAction{}), &store);
    CHECK(view.enabled);
    CHECK_FALSE(view.runs);
    CHECK(view.reason.empty());

    view = ui::buttonViewOf(scriptButton(), nullptr);
    CHECK_FALSE(view.runs);
    CHECK(view.reason.empty());
    view = ui::buttonViewOf(buttonOf(model::UnknownAction{ .type = "Teleport", .reason = "Not run." }), nullptr);
    CHECK(view.reason == "Not run.");
}
