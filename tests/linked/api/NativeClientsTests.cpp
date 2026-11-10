#include <catch2/catch_test_macros.hpp>

#include <array>
#include <expected>
#include <format>
#include <functional>
#include <memory>
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
        std::function<void(const api::SettingChange*)> change = [](const api::SettingChange*) {
        };
        std::function<void(const api::SaveBatch*)> saved = [](const api::SaveBatch*) {
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

        void onChange(const api::SettingChange* settingChange) override
        {
            change(settingChange);
        }

        void onSaved(const api::SaveBatch* batch) override
        {
            saved(batch);
        }
    };

    /**
     * The setting `fScale:Main` of a TOML file.
     */
    model::ClientSetting scaleSetting()
    {
        return { .id = "fScale:Main",
            .valueType = model::StoredType::Float,
            .file = model::ClientFile{ .name = "main", .format = model::FileFormat::Toml, .path = R"(C:\My Games\MyMod.toml)" },
            .path = { "Main", "fScale" } };
    }

    /**
     * A change of the setting `fScale:Main` of a TOML file, from 1.25 to 1.5.
     */
    model::SettingChange scaleChange(const model::ChangePhase phase = model::ChangePhase::Preview)
    {
        return { .setting = scaleSetting(), .previous = 1.25f, .value = 1.5f, .phase = phase };
    }

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
            return clients.registerClient(name, api::MODS_CONFIG_FO4_API_VERSION, &client);
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
    CHECK_FALSE(clients.clients.registerClient("Other", api::MODS_CONFIG_FO4_API_VERSION, nullptr));
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

TEST_CASE("NativeClients: a client is told what the ray is on as a PointedAt event, and only its own mod's")
{
    Clients clients;
    FakeClient client;
    std::vector<std::vector<std::string>> told;
    client.event = [&told](const api::MenuEvent* event) {
        REQUIRE(event->type == api::MenuEventType::PointedAt);
        CHECK(event->pageId == nullptr);
        auto& ids = told.emplace_back();
        for (std::uint32_t i = 0; i < event->pointedCount; ++i) {
            ids.emplace_back(event->pointedIds[i]);
        }
    };
    REQUIRE(clients.add("MyMod", client));
    clients.calls.sendPointed("mymod", { "tZone:BodyGrab", "BodyGrab" });
    clients.calls.sendPointed("OtherMod", { "fScale:Main" });
    clients.calls.sendPointed("MyMod", {});
    CHECK(told == std::vector<std::vector<std::string>>{ { "tZone:BodyGrab", "BodyGrab" }, {} });
}

TEST_CASE("ModsConfigFO4Api: the C++ client keeps what the ray is on and tells its pointing handlers, not its menu-event handlers")
{
    auto& client = api::ModsConfigFO4Client::get();
    auto told = std::make_shared<std::vector<std::vector<std::string>>>();
    auto menuEvents = std::make_shared<int>(0);
    client.subscribeToPointing([told](const std::vector<std::string>& ids) {
        told->push_back(ids);
    });
    client.subscribeToMenuEvents([menuEvents](api::MenuEventType, std::string_view) {
        ++*menuEvents;
    });
    const std::array<const char*, 2> ids{ "tZone:BodyGrab", "BodyGrab" };
    const api::MenuEvent pointed{ .type = api::MenuEventType::PointedAt, .pageId = nullptr, .pointedIds = ids.data(), .pointedCount = 2 };
    static_cast<api::IClient&>(client).onEvent(&pointed);
    CHECK(client.isPointedAt("BodyGrab"));
    CHECK_FALSE(client.isPointedAt("Body"));
    CHECK(client.pointedIds() == std::vector<std::string>{ "tZone:BodyGrab", "BodyGrab" });

    const api::MenuEvent none{ .type = api::MenuEventType::PointedAt, .pageId = nullptr, .pointedIds = nullptr, .pointedCount = 0 };
    static_cast<api::IClient&>(client).onEvent(&none);
    CHECK_FALSE(client.isPointedAt("BodyGrab"));
    CHECK(*told == std::vector<std::vector<std::string>>{ { "tZone:BodyGrab", "BodyGrab" }, {} });
    CHECK(*menuEvents == 0);
}

TEST_CASE("NativeClients: a client is told a change of a live setting with where it is kept, and one that throws is cut off")
{
    Clients clients;
    FakeClient client;
    std::vector<std::string> told;
    client.change = [&told](const api::SettingChange* change) {
        const auto& setting = *change->setting;
        std::string path;
        for (std::uint32_t i = 0; i < setting.storage.pathCount; ++i) {
            path += std::string(i > 0 ? "." : "") + setting.storage.path[i];
        }
        told.push_back(std::format("{} {} {} {} {} {} -> {}",
            change->phase == api::ChangePhase::Discard ? "Discard" : "Preview",
            setting.id,
            setting.storage.file->name,
            setting.storage.file->format == api::FileFormat::Toml ? "toml" : "other",
            path,
            change->previousValue.floatValue,
            change->value.floatValue));
        CHECK(std::string_view(setting.storage.file->path) == R"(C:\My Games\MyMod.toml)");
        CHECK(change->value.type == api::ValueType::Float);
    };
    REQUIRE(clients.add("MyMod", client));
    clients.calls.sendChange("mymod", scaleChange());
    clients.calls.sendChange("MyMod", scaleChange(model::ChangePhase::Discard));
    CHECK(told == std::vector<std::string>{ "Preview fScale:Main main toml Main.fScale 1.25 -> 1.5", "Discard fScale:Main main toml Main.fScale 1.25 -> 1.5" });

    // a setting of the game has no file, and its form, with a property's script and name
    std::string game;
    client.change = [&game](const api::SettingChange* change) {
        const auto& storage = change->setting->storage;
        game = std::format("{} {} {} {}", storage.file ? "file" : "no file", storage.form, storage.scriptName, storage.propertyName);
    };
    auto property = scaleChange();
    property.setting.file.reset();
    property.setting.path.clear();
    property.setting.form = "MyMod.esp|800";
    property.setting.script = "MyMod:Config";
    property.setting.property = "fScale";
    clients.calls.sendChange("MyMod", property);
    CHECK(game == "no file MyMod.esp|800 MyMod:Config fScale");

    // a setting of a state of the client has no file and no form, and the state's name
    std::string state;
    client.change = [&state](const api::SettingChange* change) {
        const auto& storage = change->setting->storage;
        state = std::format("{} {} {}", storage.file ? "file" : "no file", storage.form ? "form" : "no form", storage.clientState);
    };
    auto kept = scaleChange();
    kept.setting.file.reset();
    kept.setting.path.clear();
    kept.setting.clientState = "scale";
    clients.calls.sendChange("MyMod", kept);
    CHECK(state == "no file no form scale");

    // one that throws is cut off
    FakeClient broken;
    broken.change = [](const api::SettingChange*) {
        throw std::runtime_error("the client's bug");
    };
    REQUIRE(clients.add("Broken", broken));
    clients.calls.sendChange("Broken", scaleChange());
    CHECK(clients.calls.notRunnable("Broken", "Broken", "any") == CUT_OFF);
}

TEST_CASE("NativeClients: a client sets a pending value of its mod in the model's type, but not from its onChange or onSaved, or when it is not registered or is cut off")
{
    Clients clients;
    std::vector<std::string> set;
    std::string refusal;
    clients.clients.setPendingValueAction(
        [&set, &refusal](const std::string_view modName, const std::string_view id, const model::Value& value) -> std::expected<void, std::string> {
            if (!refusal.empty()) {
                return std::unexpected(refusal);
            }
            set.push_back(std::format("{} {} {}",
                modName,
                id,
                std::visit(
                    []<typename T>(const T& typed) {
                        return std::format("{}", typed);
                    },
                    value)));
            return {};
        });
    FakeClient client;
    REQUIRE(clients.add("MyMod", client));
    const api::SettingValue scale{ .type = api::ValueType::Float, .floatValue = 1.5f };
    const api::SettingValue name{ .type = api::ValueType::String, .text = "You" };
    CHECK(clients.clients.setPendingValue(&client, "fScale:Main", &scale));
    CHECK(clients.clients.setPendingValue(&client, "sName:Main", &name));
    CHECK(set == std::vector<std::string>{ "MyMod fScale:Main 1.5", "MyMod sName:Main You" });

    // the menu's refusal, and a call with no id, no value, or a text with none
    refusal = "the menu does not show its mod";
    CHECK_FALSE(clients.clients.setPendingValue(&client, "fScale:Main", &scale));
    refusal.clear();
    const api::SettingValue noText{ .type = api::ValueType::String };
    CHECK_FALSE(clients.clients.setPendingValue(&client, nullptr, &scale));
    CHECK_FALSE(clients.clients.setPendingValue(&client, "fScale:Main", nullptr));
    CHECK_FALSE(clients.clients.setPendingValue(&client, "sName:Main", &noText));

    // one from inside its onChange or onSaved, and one after them is set
    bool fromChange = true;
    bool fromSave = true;
    client.change = [&clients, &client, &scale, &fromChange](const api::SettingChange*) {
        fromChange = clients.clients.setPendingValue(&client, "fScale:Main", &scale);
    };
    client.saved = [&clients, &client, &scale, &fromSave](const api::SaveBatch*) {
        fromSave = clients.clients.setPendingValue(&client, "fScale:Main", &scale);
    };
    clients.calls.sendChange("MyMod", scaleChange());
    clients.calls.sendSave("MyMod", model::SaveBatch{});
    CHECK_FALSE(fromChange);
    CHECK_FALSE(fromSave);
    CHECK(clients.clients.setPendingValue(&client, "fScale:Main", &scale));
    CHECK(set.size() == 3);

    // a client that is not registered, and one that is cut off
    FakeClient other;
    CHECK_FALSE(clients.clients.setPendingValue(&other, "fScale:Main", &scale));
    client.event = [](const api::MenuEvent*) {
        throw std::runtime_error("the client's bug");
    };
    clients.calls.send("MyMod", model::ClientEvent::ModShown);
    CHECK_FALSE(clients.clients.setPendingValue(&client, "fScale:Main", &scale));
    CHECK(set.size() == 3);
}

TEST_CASE("ModsConfigFO4Api: the C++ client sets no pending value while it is not connected")
{
    CHECK_FALSE(api::ModsConfigFO4Client::get().setPendingValue("fScale:Main", 1.5f));
}

TEST_CASE("ModsConfigFO4Api: the C++ client hands a change to its handlers in its own types")
{
    // the client is one in the DLL, so the handler keeps what it is told in what it owns
    auto told = std::make_shared<std::vector<std::string>>();
    auto& client = api::ModsConfigFO4Client::get();
    client.subscribeToChanges([told](const api::Change& change) {
        std::string path;
        for (const auto name : change.path) {
            path += std::string(path.empty() ? "" : ".") + std::string(name);
        }
        told->push_back(std::format("{} {} {} {} {} {} -> {}",
            change.discard ? "Discard" : "Preview",
            change.id,
            change.fileName,
            change.filePath,
            path,
            std::get<std::string>(change.previous),
            std::get<std::string>(change.value)));
    });
    const api::FileView file{ .name = "main", .format = api::FileFormat::Ini, .path = "MyMod.ini" };
    const std::array<const char*, 2> path{ "Main", "sName" };
    const api::SettingView setting{ .id = "sName:Main", .valueType = api::ValueType::String, .storage = { .file = &file, .path = path.data(), .pathCount = 2 } };
    const api::SettingChange change{ .setting = &setting,
        .previousValue = { .type = api::ValueType::String, .text = "Me" },
        .value = { .type = api::ValueType::String, .text = "You" },
        .phase = api::ChangePhase::Discard };
    static_cast<api::IClient&>(client).onChange(&change);
    CHECK(*told == std::vector<std::string>{ "Discard sName:Main main MyMod.ini Main.sName Me -> You" });
}

TEST_CASE("NativeClients: a client is told what a Save wrote, each setting with where it is kept, and one that throws is cut off")
{
    Clients clients;
    FakeClient client;
    std::vector<std::string> told;
    client.saved = [&told](const api::SaveBatch* batch) {
        told.push_back(std::format("Save {}", batch->revision));
        for (std::uint32_t i = 0; i < batch->fileCount; ++i) {
            const auto& file = batch->writtenFiles[i];
            told.push_back(std::format("file {} {} {}", file.name, file.format == api::FileFormat::Ini ? "ini" : "toml", file.path));
        }
        for (std::uint32_t i = 0; i < batch->settingCount; ++i) {
            const auto& saved = batch->settings[i];
            const auto& storage = saved.setting->storage;
            std::string where = storage.file ? storage.file->name : storage.form;
            for (std::uint32_t name = 0; name < storage.pathCount; ++name) {
                where += std::string(name > 0 ? "." : ":") + storage.path[name];
            }
            if (storage.propertyName) {
                where += std::format(" {}", storage.propertyName);
            }
            const auto text = [](const api::SettingValue& value) {
                return value.type == api::ValueType::Float ? std::format("{}", value.floatValue) : std::string(value.text);
            };
            told.push_back(std::format("{} {} {} -> {}", saved.setting->id, where, text(saved.baselineValue), text(saved.value)));
        }
    };
    REQUIRE(clients.add("MyMod", client));

    model::SavedSetting name{ .setting = { .id = "sName:MyMod.esp|800", .valueType = model::StoredType::String, .form = "MyMod.esp|800", .property = "sName" },
        .before = "Me"s,
        .value = "You"s };
    const model::SaveBatch batch{ .settings = { { .setting = scaleSetting(), .before = 1.25f, .value = 1.5f }, name },
        .files = { { .name = "main", .format = model::FileFormat::Toml, .path = R"(C:\My Games\MyMod.toml)" } },
        .revision = 3 };
    clients.calls.sendSave("mymod", batch);
    CHECK(told == std::vector<std::string>{ "Save 3",
                      R"(file main toml C:\My Games\MyMod.toml)",
                      "fScale:Main main:Main.fScale 1.25 -> 1.5",
                      "sName:MyMod.esp|800 MyMod.esp|800 sName Me -> You" });

    // one that throws is cut off
    FakeClient broken;
    broken.saved = [](const api::SaveBatch*) {
        throw std::runtime_error("the client's bug");
    };
    REQUIRE(clients.add("Broken", broken));
    clients.calls.sendSave("Broken", batch);
    CHECK(clients.calls.notRunnable("Broken", "Broken", "any") == CUT_OFF);
}

TEST_CASE("ModsConfigFO4Api: the C++ client hands what a Save wrote to its handlers in its own types")
{
    auto told = std::make_shared<std::vector<std::string>>();
    auto& client = api::ModsConfigFO4Client::get();
    client.subscribeToSaves([told](const api::Saved& saved) {
        told->push_back(std::format("Save {}", saved.revision));
        for (const auto& file : saved.files) {
            told->push_back(std::format("file {} {}", file.name, file.path));
        }
        for (const auto& setting : saved.settings) {
            told->push_back(std::format("{} {} {} {} -> {}", setting.id, setting.form, setting.propertyName, std::get<int>(setting.previous), std::get<int>(setting.value)));
        }
    });
    const api::FileView file{ .name = "main", .format = api::FileFormat::Ini, .path = "MyMod.ini" };
    const api::SettingView setting{ .id = "iMode", .valueType = api::ValueType::Int, .storage = { .form = "MyMod.esp|800", .propertyName = "iMode" } };
    const api::SavedSetting saved{ .setting = &setting, .baselineValue = { .type = api::ValueType::Int, .intValue = 2 }, .value = { .type = api::ValueType::Int, .intValue = 1 } };
    const api::SaveBatch batch{ .settings = &saved, .settingCount = 1, .writtenFiles = &file, .fileCount = 1, .revision = 7 };
    static_cast<api::IClient&>(client).onSaved(&batch);
    CHECK(*told == std::vector<std::string>{ "Save 7", "file main MyMod.ini", "iMode MyMod.esp|800 iMode 2 -> 1" });
}

TEST_CASE("ModsConfigFO4Api: the C++ client applies a change of a setting kept in a state with the state's setter, before its handlers")
{
    // the handlers of the tests before stay on the one client: they take a change of a text and a Save of a whole number
    auto told = std::make_shared<std::vector<std::string>>();
    auto& client = api::ModsConfigFO4Client::get();
    CHECK(client.addState(
        "sTheme",
        [] {
            return std::optional<api::Value>("dark"s);
        },
        [told](const api::Value& value) {
            told->push_back("set " + std::get<std::string>(value));
        }));
    CHECK(client.addState(
        "iPipboy",
        [] {
            return std::optional<api::Value>(0);
        },
        [told](const api::Value& value) {
            told->push_back(std::format("set {}", std::get<int>(value)));
        }));
    CHECK(client.addState("sShown", [] {
        return std::optional<api::Value>("on"s);
    }));
    client.subscribeToChanges([told](const api::Change& change) {
        told->push_back(std::format("told {} {}", change.id, change.clientState));
    });

    const api::SettingView theme{ .id = "theme", .valueType = api::ValueType::String, .storage = { .clientState = "sTheme" } };
    const api::SettingChange preview{ .setting = &theme,
        .previousValue = { .type = api::ValueType::String, .text = "dark" },
        .value = { .type = api::ValueType::String, .text = "light" },
        .phase = api::ChangePhase::Preview };
    const api::SettingChange discard{ .setting = &theme,
        .previousValue = { .type = api::ValueType::String, .text = "light" },
        .value = { .type = api::ValueType::String, .text = "dark" },
        .phase = api::ChangePhase::Discard };
    static_cast<api::IClient&>(client).onChange(&preview);
    static_cast<api::IClient&>(client).onChange(&discard);
    // a Discard has the baseline as its value
    CHECK(*told == std::vector<std::string>{ "set light", "told theme sTheme", "set dark", "told theme sTheme" });

    // a state with no setter is not applied
    told->clear();
    const api::SettingView shown{ .id = "shown", .valueType = api::ValueType::String, .storage = { .clientState = "sShown" } };
    const api::SettingChange change{ .setting = &shown,
        .previousValue = { .type = api::ValueType::String, .text = "on" },
        .value = { .type = api::ValueType::String, .text = "off" },
        .phase = api::ChangePhase::Preview };
    static_cast<api::IClient&>(client).onChange(&change);
    CHECK(*told == std::vector<std::string>{ "told shown sShown" });

    // what a Save wrote
    told->clear();
    const api::SettingView pipboy{ .id = "pipboy", .valueType = api::ValueType::Int, .storage = { .clientState = "iPipboy" } };
    const api::SavedSetting saved{ .setting = &pipboy, .baselineValue = { .type = api::ValueType::Int, .intValue = 0 }, .value = { .type = api::ValueType::Int, .intValue = 1 } };
    const api::SaveBatch batch{ .settings = &saved, .settingCount = 1, .revision = 1 };
    static_cast<api::IClient&>(client).onSaved(&batch);
    CHECK(*told == std::vector<std::string>{ "set 1" });
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

TEST_CASE("ModsConfigFO4Api: the C++ client's Params read a parameter by its name, in its own type, and the fallback for one of another type")
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
