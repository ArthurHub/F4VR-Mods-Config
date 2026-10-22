#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <vector>

#include "api/NativeClients.h"

using namespace mods_config;
using namespace std::string_literals;

namespace
{
    api::ClientInfo infoOf(const char* name)
    {
        return { .version = api::MODS_CONFIG_VR_API_VERSION, .name = name, .ctx = nullptr };
    }

    model::Setting choiceOf(const std::string& id, const model::StoredType type)
    {
        model::Setting setting;
        setting.id = id;
        setting.valueType = type;
        return setting;
    }

    /**
     * What a callback was called with, and how many times.
     */
    struct Calls
    {
        int count = 0;
        std::string settingId;
        api::ValueType valueType = api::ValueType::Bool;
    };

    /**
     * Gives the options of a string choice: none, a token, one with no text, and one of another type.
     */
    void MODS_CONFIG_VR_CALL stringOptions(void* ctx, api::OptionsCall* call)
    {
        auto& calls = *static_cast<Calls*>(ctx);
        ++calls.count;
        calls.settingId = call->setting->id;
        calls.valueType = call->setting->valueType;
        const api::SettingValue none{ .type = api::ValueType::String, .text = "" };
        call->addOption(call, &none, "None");
        const api::SettingValue frik{ .type = api::ValueType::String, .text = "frik" };
        call->addOption(call, &frik, "$FRIK");
        const api::SettingValue other{ .type = api::ValueType::String, .text = "Other" };
        call->addOption(call, &other, nullptr);
        const api::SettingValue number{ .type = api::ValueType::Int, .intValue = 3 };
        call->addOption(call, &number, "Three");
    }

    /**
     * Gives the options of an int choice, with no texts.
     */
    void MODS_CONFIG_VR_CALL intOptions(void*, api::OptionsCall* call)
    {
        for (std::int32_t i = 1; i <= 2; ++i) {
            const api::SettingValue value{ .type = api::ValueType::Int, .intValue = i * 10 };
            call->addOption(call, &value, nullptr);
        }
    }

    /**
     * Throws, as a client's code can, after it counts its call.
     */
    void MODS_CONFIG_VR_CALL throwingOptions(void* ctx, api::OptionsCall*)
    {
        ++static_cast<Calls*>(ctx)->count;
        throw std::runtime_error("the client's bug");
    }

    std::expected<std::vector<model::Option>, std::string> readOptions(const model::ClientOptions& options, const std::string_view modName, const model::Setting& setting)
    {
        const auto* callback = options.find(modName, setting.id);
        REQUIRE(callback);
        return (*callback)(setting);
    }
}

TEST_CASE("NativeClients: a client registers once by its name, without regard to case")
{
    model::ClientOptions options;
    model::ClientFunctions functions;
    api::NativeClients clients(options, functions);
    const auto info = infoOf("MyMod");
    CHECK(clients.registerClient(&info) != nullptr);

    const auto again = infoOf("mymod");
    CHECK(clients.registerClient(&again) == nullptr);

    // an info with no name or no version, and none
    const auto noName = infoOf("");
    CHECK(clients.registerClient(&noName) == nullptr);
    auto noVersion = infoOf("Other");
    noVersion.version = 0;
    CHECK(clients.registerClient(&noVersion) == nullptr);
    CHECK(clients.registerClient(nullptr) == nullptr);
}

TEST_CASE("NativeClients: a client gives the options of a choice of its config spec, of the setting's type, with its texts translated")
{
    model::ClientOptions options;
    model::ClientFunctions functions;
    api::NativeClients clients(options, functions);
    model::Translations translations;
    translations.add("$FRIK", "FRIK VR");
    clients.setTranslations(&translations);
    const auto info = infoOf("MyMod");
    auto* client = clients.registerClient(&info);
    Calls calls;
    REQUIRE(clients.registerOptions(client, "Preset", &stringOptions, &calls));
    REQUIRE(clients.registerOptions(client, "Count", &intOptions, nullptr));

    // its mod's name and the setting's id are matched without regard to case
    const auto preset = readOptions(options, "MYMOD", choiceOf("preset", model::StoredType::String));
    REQUIRE(preset);
    CHECK(*preset == std::vector<model::Option>{ { .value = ""s, .text = "None" }, { .value = "frik"s, .text = "FRIK VR" }, { .value = "Other"s, .text = "Other" } });
    CHECK(calls.count == 1);
    CHECK(calls.settingId == "preset");
    CHECK(calls.valueType == api::ValueType::String);

    // an option with no text shows its value
    const auto count = readOptions(options, "MyMod", choiceOf("Count", model::StoredType::Int));
    REQUIRE(count);
    CHECK(*count == std::vector<model::Option>{ { .value = 10, .text = "10" }, { .value = 20, .text = "20" } });
}

TEST_CASE("NativeClients: the options of a setting are registered once, which keeps the first")
{
    model::ClientOptions options;
    model::ClientFunctions functions;
    api::NativeClients clients(options, functions);
    const auto info = infoOf("MyMod");
    auto* client = clients.registerClient(&info);
    Calls first;
    Calls second;
    CHECK(clients.registerOptions(client, "Preset", &stringOptions, &first));
    CHECK_FALSE(clients.registerOptions(client, "preset", &stringOptions, &second));
    CHECK(readOptions(options, "MyMod", choiceOf("Preset", model::StoredType::String)));
    CHECK(first.count == 1);
    CHECK(second.count == 0);

    // a handle that is no client's, no setting and no callback
    api::Client stranger;
    CHECK_FALSE(clients.registerOptions(&stranger, "Other", &stringOptions, nullptr));
    CHECK_FALSE(clients.registerOptions(nullptr, "Other", &stringOptions, nullptr));
    CHECK_FALSE(clients.registerOptions(client, "", &stringOptions, nullptr));
    CHECK_FALSE(clients.registerOptions(client, "Other", nullptr, nullptr));
}

TEST_CASE("NativeClients: a client whose callback throws is cut off, and its choices are not available")
{
    model::ClientOptions options;
    model::ClientFunctions functions;
    api::NativeClients clients(options, functions);
    const auto info = infoOf("MyMod");
    auto* client = clients.registerClient(&info);
    Calls throwing;
    Calls other;
    REQUIRE(clients.registerOptions(client, "Broken", &throwingOptions, &throwing));
    REQUIRE(clients.registerOptions(client, "Preset", &stringOptions, &other));

    constexpr auto reason = "Not available: its mod's client failed, and is cut off until the game is restarted.";
    const auto broken = readOptions(options, "MyMod", choiceOf("Broken", model::StoredType::String));
    REQUIRE_FALSE(broken);
    CHECK(broken.error() == reason);
    CHECK(throwing.count == 1);

    // nothing of it is called again, and it registers nothing more
    CHECK(readOptions(options, "MyMod", choiceOf("Broken", model::StoredType::String)).error() == reason);
    CHECK(readOptions(options, "MyMod", choiceOf("Preset", model::StoredType::String)).error() == reason);
    CHECK(throwing.count == 1);
    CHECK(other.count == 0);
    CHECK_FALSE(clients.registerOptions(client, "More", &stringOptions, nullptr));

    // another client keeps working
    const auto otherInfo = infoOf("OtherMod");
    auto* otherClient = clients.registerClient(&otherInfo);
    REQUIRE(clients.registerOptions(otherClient, "Preset", &stringOptions, &other));
    CHECK(readOptions(options, "OtherMod", choiceOf("Preset", model::StoredType::String)));
    CHECK(other.count == 1);
}

namespace
{
    /**
     * What a function was called with, and what it answers.
     */
    struct FunctionCalls
    {
        int count = 0;
        std::vector<std::pair<std::string, api::SettingValue>> params;
        std::vector<std::string> texts;
        bool done = true;
        const char* message = nullptr;
    };

    /**
     * Keeps its parameters, with their texts, and answers as its calls say.
     */
    bool MODS_CONFIG_VR_CALL calibrate(void* ctx, api::FunctionCall* call)
    {
        auto& calls = *static_cast<FunctionCalls*>(ctx);
        ++calls.count;
        for (std::uint32_t i = 0; i < call->paramCount; ++i) {
            calls.params.emplace_back(call->params[i].name, call->params[i].value);
            calls.texts.emplace_back(call->params[i].value.text);
        }
        if (calls.message) {
            call->setMessage(call, calls.message);
        }
        return calls.done;
    }

    /**
     * Throws, as a client's code can, after it counts its call.
     */
    bool MODS_CONFIG_VR_CALL throwingFunction(void* ctx, api::FunctionCall*)
    {
        ++static_cast<FunctionCalls*>(ctx)->count;
        throw std::runtime_error("the client's bug");
    }
}

TEST_CASE("NativeClients: a client's function runs with its named arguments, each in its type, and gives its result")
{
    model::ClientOptions options;
    model::ClientFunctions functions;
    api::NativeClients clients(options, functions);
    model::Translations translations;
    translations.add("$Done", "Done: 1.78 m.");
    clients.setTranslations(&translations);
    const auto info = infoOf("MyMod");
    auto* client = clients.registerClient(&info);
    FunctionCalls calls{ .message = "$Done" };
    REQUIRE(clients.registerFunction(client, "Calibrate", &calibrate, &calls));

    // its mod's name and its own are matched without regard to case
    CHECK(functions.notRunnable("mymod", "My Mod", "calibrate").empty());
    const std::vector<model::NamedArgument> arguments{ { .name = "mode", .value = "height"s },
        { .name = "samples", .value = 30 },
        { .name = "scale", .value = 1.5f },
        { .name = "includeScale", .value = true } };
    const auto result = functions.call("mymod", "calibrate", arguments);
    CHECK(result.done);
    CHECK(result.message == "Done: 1.78 m.");
    REQUIRE(calls.params.size() == 4);
    CHECK(calls.params[0].first == "mode");
    CHECK(calls.params[0].second.type == api::ValueType::String);
    CHECK(calls.texts[0] == "height");
    CHECK(calls.params[1].second.type == api::ValueType::Int);
    CHECK(calls.params[1].second.intValue == 30);
    CHECK(calls.texts[1] == "30");
    CHECK(calls.params[2].second.type == api::ValueType::Float);
    CHECK(calls.params[2].second.floatValue == 1.5f);
    CHECK(calls.params[3].second.type == api::ValueType::Bool);
    CHECK(calls.params[3].second.boolValue);

    // one that fails with no message
    calls.done = false;
    calls.message = nullptr;
    const auto failed = functions.call("MyMod", "Calibrate", {});
    CHECK_FALSE(failed.done);
    CHECK(failed.message.empty());
    CHECK(calls.count == 2);
}

TEST_CASE("NativeClients: a function does not run with no client, with no function of its name, and once its client is cut off")
{
    model::ClientOptions options;
    model::ClientFunctions functions;
    api::NativeClients clients(options, functions);
    CHECK(functions.notRunnable("MyMod", "My Mod", "calibrate") == "Needs My Mod's DLL, which is not loaded.");

    const auto info = infoOf("MyMod");
    auto* client = clients.registerClient(&info);
    CHECK(functions.notRunnable("MyMod", "My Mod", "calibrate") == "Not available: My Mod's DLL has no function 'calibrate'.");

    FunctionCalls throwing;
    FunctionCalls calls;
    Calls optionCalls;
    REQUIRE(clients.registerFunction(client, "Broken", &throwingFunction, &throwing));
    REQUIRE(clients.registerFunction(client, "Calibrate", &calibrate, &calls));
    REQUIRE(clients.registerOptions(client, "Preset", &stringOptions, &optionCalls));
    CHECK_FALSE(clients.registerFunction(client, "calibrate", &calibrate, nullptr));

    constexpr auto reason = "Not available: its mod's client failed, and is cut off until the game is restarted.";
    const auto broken = functions.call("MyMod", "Broken", {});
    CHECK_FALSE(broken.done);
    CHECK(broken.message == reason);
    CHECK(throwing.count == 1);

    // nothing of it runs again: its functions and its options
    CHECK(functions.notRunnable("MyMod", "My Mod", "calibrate") == reason);
    CHECK_FALSE(functions.call("MyMod", "Calibrate", {}).done);
    CHECK(calls.count == 0);
    CHECK(readOptions(options, "MyMod", choiceOf("Preset", model::StoredType::String)).error() == reason);
    CHECK(optionCalls.count == 0);
    CHECK_FALSE(clients.registerFunction(client, "More", &calibrate, nullptr));
}
