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
    api::NativeClients clients(options);
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
    api::NativeClients clients(options);
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
    api::NativeClients clients(options);
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
    api::NativeClients clients(options);
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
