#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "api/NativeClients.h"

using namespace mods_config;
using namespace std::string_literals;

namespace
{
    constexpr auto CUT_OFF = "Not available: its mod's client failed, and is cut off until the game is restarted.";

    /**
     * A client of the API, whose every method does what the test gives it, and nothing with none.
     */
    struct FakeClient final : api::IClient
    {
        std::function<bool(const char*)> has = [](const char*) {
            return false;
        };
        std::function<bool(const char*, api::IFunctionCall*)> run = [](const char*, api::IFunctionCall*) {
            return false;
        };
        std::function<bool(const char*, api::SettingValue*)> state = [](const char*, api::SettingValue*) {
            return false;
        };
        std::function<bool(const api::SettingView*, api::IOptions*)> options = [](const api::SettingView*, api::IOptions*) {
            return false;
        };
        std::function<void(const api::MenuEvent*)> event = [](const api::MenuEvent*) {
        };

        bool hasFunction(const char* name) override
        {
            return has(name);
        }

        bool runFunction(const char* name, api::IFunctionCall* call) override
        {
            return run(name, call);
        }

        bool answerState(const char* name, api::SettingValue* value) override
        {
            return state(name, value);
        }

        bool giveOptions(const api::SettingView* setting, api::IOptions* given) override
        {
            return options(setting, given);
        }

        void onEvent(const api::MenuEvent* menuEvent) override
        {
            event(menuEvent);
        }
    };

    model::Setting choiceOf(const std::string& id, const model::StoredType type)
    {
        model::Setting setting;
        setting.id = id;
        setting.valueType = type;
        return setting;
    }

    /**
     * The model's client calls and the native clients that fill them.
     */
    struct Clients
    {
        model::ClientCalls calls;
        api::NativeClients clients{ calls };

        bool add(const char* name, FakeClient& client)
        {
            return clients.registerClient(name, api::MODS_CONFIG_VR_API_VERSION, &client);
        }
    };
}

TEST_CASE("NativeClients: a client registers once by its name, without regard to case")
{
    Clients clients;
    FakeClient client;
    FakeClient other;
    CHECK(clients.add("MyMod", client));
    CHECK(*clients.clients.nameOf(&client) == "MyMod");
    CHECK_FALSE(clients.add("mymod", other));

    // the same client under another name, and no name, no client or no version
    CHECK_FALSE(clients.add("Other", client));
    CHECK_FALSE(clients.add("", other));
    CHECK_FALSE(clients.clients.registerClient("Other", api::MODS_CONFIG_VR_API_VERSION, nullptr));
    CHECK_FALSE(clients.clients.registerClient("Other", 0, &other));
    CHECK_FALSE(clients.clients.nameOf(&other));
}

TEST_CASE("NativeClients: a client gives the options of a choice, of the setting's type, with its texts translated")
{
    Clients clients;
    model::Translations translations;
    translations.add("$FRIK", "FRIK VR");
    clients.clients.setTranslations(&translations);
    FakeClient client;
    std::string asked;
    client.options = [&asked](const api::SettingView* setting, api::IOptions* options) {
        asked = setting->id;
        if (setting->valueType == api::ValueType::Int) {
            for (std::int32_t i = 1; i <= 2; ++i) {
                const api::SettingValue value{ .type = api::ValueType::Int, .intValue = i * 10 };
                options->add(&value, nullptr);
            }
            return true;
        }
        if (std::string(setting->id) != "preset") {
            return false;
        }
        const api::SettingValue none{ .type = api::ValueType::String, .text = "" };
        options->add(&none, "None");
        const api::SettingValue frik{ .type = api::ValueType::String, .text = "frik" };
        options->add(&frik, "$FRIK");
        const api::SettingValue other{ .type = api::ValueType::String, .text = "Other" };
        options->add(&other, nullptr);
        // of another type, which is left out
        const api::SettingValue number{ .type = api::ValueType::Int, .intValue = 3 };
        options->add(&number, "Three");
        return true;
    };
    REQUIRE(clients.add("MyMod", client));

    const auto preset = clients.calls.options("MYMOD", choiceOf("preset", model::StoredType::String));
    REQUIRE(preset);
    CHECK(*preset == std::vector<model::Option>{ { .value = ""s, .text = "None" }, { .value = "frik"s, .text = "FRIK VR" }, { .value = "Other"s, .text = "Other" } });
    CHECK(asked == "preset");

    // an option with no text shows its value
    const auto count = clients.calls.options("MyMod", choiceOf("count", model::StoredType::Int));
    REQUIRE(count);
    CHECK(*count == std::vector<model::Option>{ { .value = 10, .text = "10" }, { .value = 20, .text = "20" } });

    // one it gives none of, and a mod with no client
    CHECK(clients.calls.options("MyMod", choiceOf("other", model::StoredType::String)).error() == "Not available: its mod's client gives no options for it.");
    CHECK(clients.calls.options("OtherMod", choiceOf("preset", model::StoredType::String)).error() == "Not available: its mod's client gives no options for it.");
}

TEST_CASE("NativeClients: a client's function runs with its named arguments, each in its type, and gives its result")
{
    Clients clients;
    model::Translations translations;
    translations.add("$Done", "Done: 1.78 m.");
    clients.clients.setTranslations(&translations);
    FakeClient client;
    std::vector<std::pair<std::string, api::SettingValue>> params;
    std::vector<std::string> texts;
    bool done = true;
    const char* message = "$Done";
    client.has = [](const char* name) {
        return std::string(name) == "calibrate";
    };
    client.run = [&](const char* name, api::IFunctionCall* call) {
        CHECK(std::string(name) == "calibrate");
        for (std::uint32_t i = 0; i < call->paramCount(); ++i) {
            params.emplace_back(call->param(i)->name, call->param(i)->value);
            texts.emplace_back(call->param(i)->value.text);
        }
        if (message) {
            call->setMessage(message);
        }
        return done;
    };
    REQUIRE(clients.add("MyMod", client));

    CHECK(clients.calls.notRunnable("mymod", "My Mod", "calibrate").empty());
    const std::vector<model::NamedArgument> arguments{ { .name = "mode", .value = "height"s },
        { .name = "samples", .value = 30 },
        { .name = "scale", .value = 1.5f },
        { .name = "includeScale", .value = true } };
    const auto result = clients.calls.call("mymod", "calibrate", arguments);
    CHECK(result.done);
    CHECK(result.message == "Done: 1.78 m.");
    REQUIRE(params.size() == 4);
    CHECK(params[0].first == "mode");
    CHECK(params[0].second.type == api::ValueType::String);
    CHECK(texts[0] == "height");
    CHECK(params[1].second.type == api::ValueType::Int);
    CHECK(params[1].second.intValue == 30);
    CHECK(texts[1] == "30");
    CHECK(params[2].second.type == api::ValueType::Float);
    CHECK(params[2].second.floatValue == 1.5f);
    CHECK(params[3].second.type == api::ValueType::Bool);
    CHECK(params[3].second.boolValue);

    // one that fails with no message
    done = false;
    message = nullptr;
    const auto failed = clients.calls.call("MyMod", "calibrate", {});
    CHECK_FALSE(failed.done);
    CHECK(failed.message.empty());
}

TEST_CASE("NativeClients: a function does not run with no client, with no function of its name, and once its client is cut off")
{
    Clients clients;
    CHECK(clients.calls.notRunnable("MyMod", "My Mod", "calibrate") == "Needs My Mod's DLL, which is not loaded.");

    FakeClient client;
    int runs = 0;
    client.has = [](const char* name) {
        return std::string(name) != "other";
    };
    client.run = [&runs](const char* name, api::IFunctionCall*) {
        ++runs;
        if (std::string(name) == "broken") {
            throw std::runtime_error("the client's bug");
        }
        return true;
    };
    REQUIRE(clients.add("MyMod", client));
    CHECK(clients.calls.notRunnable("MyMod", "My Mod", "other") == "Not available: My Mod's DLL has no function 'other'.");

    const auto broken = clients.calls.call("MyMod", "broken", {});
    CHECK_FALSE(broken.done);
    CHECK(broken.message == CUT_OFF);
    CHECK(runs == 1);

    // nothing of it is asked again
    CHECK(clients.calls.notRunnable("MyMod", "My Mod", "calibrate") == CUT_OFF);
    CHECK_FALSE(clients.calls.call("MyMod", "calibrate", {}).done);
    CHECK(runs == 1);
    CHECK(clients.calls.options("MyMod", choiceOf("preset", model::StoredType::String)).error() == CUT_OFF);
    CHECK_FALSE(clients.calls.state("MyMod", "calibrating"));

    // and another client keeps working
    FakeClient other;
    other.has = [](const char*) {
        return true;
    };
    REQUIRE(clients.add("OtherMod", other));
    CHECK(clients.calls.notRunnable("OtherMod", "Other Mod", "calibrate").empty());
}

TEST_CASE("NativeClients: a client answers a state, in its type, and none while it does not")
{
    Clients clients;
    FakeClient client;
    int asked = 0;
    bool answers = true;
    api::SettingValue answer{ .type = api::ValueType::Bool, .boolValue = true };
    client.state = [&](const char* name, api::SettingValue* value) {
        ++asked;
        if (std::string(name) == "broken") {
            throw std::runtime_error("the client's bug");
        }
        *value = answer;
        return answers;
    };
    REQUIRE(clients.add("MyMod", client));

    CHECK(clients.calls.state("mymod", "calibrating") == model::Value(true));
    answer = { .type = api::ValueType::Int, .intValue = 3 };
    CHECK(clients.calls.state("MyMod", "calibrating") == model::Value(3));
    answer = { .type = api::ValueType::Float, .floatValue = 0.5f };
    CHECK(clients.calls.state("MyMod", "calibrating") == model::Value(0.5f));
    answer = { .type = api::ValueType::String, .text = "walk" };
    CHECK(clients.calls.state("MyMod", "calibrating") == model::Value("walk"s));
    answers = false;
    CHECK_FALSE(clients.calls.state("MyMod", "calibrating"));
    CHECK_FALSE(clients.calls.state("OtherMod", "calibrating"));
    CHECK(asked == 5);

    // one that throws cuts its client off: none of its states is available after it
    answers = true;
    CHECK_FALSE(clients.calls.state("MyMod", "broken"));
    CHECK_FALSE(clients.calls.state("MyMod", "calibrating"));
    CHECK(asked == 6);
}

TEST_CASE("NativeClients: a client is told the menu's events, and one that throws is cut off")
{
    Clients clients;
    FakeClient client;
    std::vector<std::pair<api::MenuEventType, std::string>> told;
    client.event = [&told](const api::MenuEvent* event) {
        told.emplace_back(event->type, event->pageId ? event->pageId : "");
    };
    REQUIRE(clients.add("MyMod", client));
    clients.calls.send("mymod", model::ClientEvent::PageShown, "spheres");
    clients.calls.sendToAll(model::ClientEvent::MenuClosed);
    clients.calls.send("OtherMod", model::ClientEvent::ModShown);
    CHECK(told == std::vector<std::pair<api::MenuEventType, std::string>>{ { api::MenuEventType::PageShown, "spheres" }, { api::MenuEventType::MenuClosed, "" } });

    FakeClient broken;
    broken.event = [](const api::MenuEvent*) {
        throw std::runtime_error("the client's bug");
    };
    broken.has = [](const char*) {
        return true;
    };
    REQUIRE(clients.add("Broken", broken));
    clients.calls.send("Broken", model::ClientEvent::ModShown);
    CHECK(clients.calls.notRunnable("Broken", "Broken", "any") == CUT_OFF);
}

TEST_CASE("NativeClients: a client adds a quick launch entry for each of its functions")
{
    Clients clients;
    FakeClient client;
    REQUIRE(clients.add("MyMod", client));
    CHECK(clients.clients.addQuickLaunch(&client, "$MyMod", "Opens its own UI.", nullptr, "openConfig"));
    CHECK(clients.clients.addQuickLaunch(&client, "MyMod: Pip-Boy", nullptr, "Place", "openPipboy"));
    // one function has one entry, the first
    CHECK_FALSE(clients.clients.addQuickLaunch(&client, "Again", nullptr, nullptr, "OpenConfig"));
    REQUIRE(clients.calls.quickLaunch().size() == 2);
    const auto& entry = clients.calls.quickLaunch().front();
    CHECK(entry.modName == "MyMod");
    CHECK(entry.text == "$MyMod");
    CHECK(entry.help == "Opens its own UI.");
    CHECK(entry.buttonText.empty());
    CHECK(entry.function == "openConfig");
    CHECK(clients.calls.quickLaunch()[1].buttonText == "Place");

    // one with no text or no function, and of a client that is not registered
    CHECK_FALSE(clients.clients.addQuickLaunch(&client, "", nullptr, nullptr, "open"));
    CHECK_FALSE(clients.clients.addQuickLaunch(&client, "Other", nullptr, nullptr, nullptr));
    FakeClient stranger;
    CHECK_FALSE(clients.clients.addQuickLaunch(&stranger, "Other", nullptr, nullptr, "open"));
    CHECK(clients.calls.quickLaunch().size() == 2);
}

TEST_CASE("ModsConfigVRApi: the C++ client's Params read a parameter by its name, in its own type, and the fallback for one of another type")
{
    struct Call final : api::IFunctionCall
    {
        std::vector<api::NamedValue> params;

        std::uint32_t paramCount() override
        {
            return static_cast<std::uint32_t>(params.size());
        }

        const api::NamedValue* param(const std::uint32_t index) override
        {
            return &params[index];
        }

        void setMessage(const char*) override
        {}
    };

    Call call;
    call.params = { { .name = "mode", .value = { .type = api::ValueType::String, .text = "height" } },
        { .name = "samples", .value = { .type = api::ValueType::Int, .intValue = 30 } },
        { .name = "scale", .value = { .type = api::ValueType::Float, .floatValue = 1.5f } },
        { .name = "includeScale", .value = { .type = api::ValueType::Bool, .boolValue = true } } };
    const api::Params read(&call);

    CHECK(read.getString("mode") == "height");
    CHECK(read.getInt("samples") == 30);
    CHECK(read.getBool("includeScale"));
    CHECK(read.getFloat("samples") == 30.0f);
    CHECK(read.getInt("scale") == 1);
    CHECK_FALSE(read.get("Mode"));
    CHECK(read.getInt("mode", 7) == 7);
    CHECK(read.getString("samples", "none") == "none");
}
