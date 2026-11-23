#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>
#include <string>

#include "TestStore.h"
#include "model/SettingValues.h"
#include "ui/RowView.h"

using namespace mods_config;
using model::Condition;
using ui::TextMark;
using ui::ValueLook;

namespace
{
    /**
     * A store whose settings are available, invalid and dirty by their ids, with the values a condition asks by id.
     * A setting is available unless it is named as not, and its value is off unless the test sets another, as it
     * sets its default. A dirty change takes effect after a restart.
     */
    class RowStore final : public test_store::RecordingStore
    {
    public:
        std::set<std::string, std::less<>> notAvailable;
        std::set<std::string, std::less<>> invalid;
        std::set<std::string, std::less<>> dirty;
        std::map<std::string, model::Value, std::less<>> values;
        std::map<std::string, model::Value, std::less<>> settings;
        std::map<std::string, model::Value, std::less<>> defaults;
        std::set<std::string, std::less<>> overridden;

        model::StoredValue value(const model::Setting& setting) override
        {
            const bool isInvalid = invalid.contains(setting.id);
            const auto found = settings.find(setting.id);
            return { .value = found != settings.end() ? found->second : model::Value(false),
                .available = !notAvailable.contains(setting.id),
                .invalid = isInvalid,
                .reason = isInvalid ? "Not a value of it." : "",
                .overrides = overridden.contains(setting.id) ? std::vector<model::Override>{ { .owner = "ROCK", .value = model::Value(true), .text = "true" } }
                                                             : std::vector<model::Override>{} };
        }

        std::optional<model::Value> defaultValue(const model::Setting& setting) override
        {
            const auto found = defaults.find(setting.id);
            return found != defaults.end() ? std::optional(found->second) : std::nullopt;
        }

        model::StoredValue valueById(const std::string_view id) override
        {
            const auto found = values.find(id);
            return found != values.end() ? model::StoredValue{ .value = found->second, .available = true, .reason = {} } : model::StoredValue{};
        }

        bool isDirty(const model::Setting& setting) const override
        {
            return dirty.contains(setting.id);
        }

        std::optional<model::Applies> applies(const model::Setting&) const override
        {
            return model::Applies::Restart;
        }
    };

    Condition on(std::string id)
    {
        return { .kind = Condition::Kind::On, .setting = std::move(id), .conditions = {} };
    }

    /**
     * A switcher by its id, which warns while the setting `loud` is on.
     */
    model::Setting setting(std::string id)
    {
        model::Setting made{ .id = std::move(id), .type = model::SettingType::Switcher, .text = {}, .help = {}, .valueOptions = {}, .storage = {} };
        made.warnings.push_back({ .when = on("loud"), .text = "It is loud." });
        return made;
    }
}

TEST_CASE("RowView: a saved row is enabled and editable, in the saved look, with its warning while that holds")
{
    RowStore store;
    const auto row = setting("a");

    auto view = ui::rowViewOf(row, &store);
    CHECK(view.stored.available);
    CHECK(view.enabled);
    CHECK(view.editable);
    CHECK_FALSE(view.dirty);
    CHECK(view.look == ValueLook::Saved);
    CHECK(view.warning.empty());
    CHECK(view.mark == TextMark::None);
    CHECK_FALSE(view.applies);

    store.values["loud"] = true;
    view = ui::rowViewOf(row, &store);
    CHECK(view.warning == "It is loud.");
    CHECK(view.mark == TextMark::Warning);
}

TEST_CASE("RowView: a dirty row is in the pending look and tells when its change takes effect, which a saved one does not")
{
    RowStore store;
    store.values["loud"] = true;
    store.dirty.insert("a");
    const auto row = setting("a");

    const auto view = ui::rowViewOf(row, &store);
    CHECK(view.dirty);
    CHECK(view.look == ValueLook::Pending);
    CHECK(view.applies == model::Applies::Restart);
    CHECK(view.warning == "It is loud.");
    CHECK(view.mark == TextMark::Warning);

    CHECK_FALSE(ui::rowViewOf(setting("b"), &store).applies);
}

TEST_CASE("RowView: an invalid row is in the invalid look with its mark and reason in the place of its warning, also while dirty")
{
    RowStore store;
    store.values["loud"] = true;
    store.invalid.insert("a");
    store.dirty.insert("a");
    const auto row = setting("a");

    const auto view = ui::rowViewOf(row, &store);
    CHECK(view.editable);
    CHECK(view.dirty);
    CHECK(view.look == ValueLook::Invalid);
    CHECK(view.mark == TextMark::Invalid);
    CHECK(view.warning.empty());
    CHECK(view.stored.reason == "Not a value of it.");
}

TEST_CASE("RowView: an overridden row has the warning mark and stays editable, and its change tells no time it takes effect")
{
    RowStore store;
    store.overridden.insert("a");
    store.dirty.insert("a");
    const auto row = setting("a");

    const auto view = ui::rowViewOf(row, &store);
    CHECK(view.editable);
    CHECK(view.look == ValueLook::Pending);
    CHECK(view.mark == TextMark::Warning);
    CHECK(view.warning.empty());
    CHECK_FALSE(view.applies);

    // an invalid value's mark comes first
    store.invalid.insert("a");
    CHECK(ui::rowViewOf(row, &store).mark == TextMark::Invalid);
}

TEST_CASE("RowView: a row whose value is not available cannot be edited and is never dirty")
{
    RowStore store;
    store.notAvailable.insert("a");
    store.dirty.insert("a");

    const auto view = ui::rowViewOf(setting("a"), &store);
    CHECK(view.enabled);
    CHECK_FALSE(view.editable);
    CHECK_FALSE(view.dirty);
    CHECK(view.look == ValueLook::Saved);
    CHECK_FALSE(view.applies);
}

TEST_CASE("RowView: a row whose enabledWhen does not hold is disabled, and a read-only one is enabled but cannot be edited")
{
    RowStore store;
    auto disabled = setting("a");
    disabled.enabledWhen = on("master");
    store.values["master"] = false;

    auto view = ui::rowViewOf(disabled, &store);
    CHECK_FALSE(view.enabled);
    CHECK_FALSE(view.editable);

    store.values["master"] = true;
    view = ui::rowViewOf(disabled, &store);
    CHECK(view.enabled);
    CHECK(view.editable);

    auto readOnly = setting("b");
    readOnly.readOnly = true;
    view = ui::rowViewOf(readOnly, &store);
    CHECK(view.enabled);
    CHECK_FALSE(view.editable);
}

TEST_CASE("RowView: a row with no store has no value and cannot be edited")
{
    const auto view = ui::rowViewOf(setting("a"), nullptr);
    CHECK_FALSE(view.stored.available);
    CHECK(view.enabled);
    CHECK_FALSE(view.editable);
    CHECK_FALSE(view.dirty);
    CHECK(view.look == ValueLook::Saved);
}

TEST_CASE("RowView: a row with a default its control shows has it as its Reset value, which Reset sets while the value is clean, editable and not the default")
{
    RowStore store;
    store.defaults["a"] = true;
    auto row = setting("a");

    auto view = ui::rowViewOf(row, &store);
    REQUIRE(view.resetValue);
    CHECK(model::sameValue(*view.resetValue, true));
    CHECK(view.resets);

    // a dirty row's slot is its Discard
    store.dirty.insert("a");
    view = ui::rowViewOf(row, &store);
    CHECK(view.resetValue);
    CHECK_FALSE(view.resets);
    store.dirty.clear();

    // the value is the default already, unless it is invalid
    store.settings["a"] = true;
    CHECK_FALSE(ui::rowViewOf(row, &store).resets);
    store.invalid.insert("a");
    CHECK(ui::rowViewOf(row, &store).resets);
    store.invalid.clear();
    store.settings.clear();

    // it cannot be edited
    row.readOnly = true;
    view = ui::rowViewOf(row, &store);
    CHECK(view.resetValue);
    CHECK_FALSE(view.resets);
}

TEST_CASE("RowView: a row has no Reset with no default, a value that is not available, a slider with no range and a default that is none of the options")
{
    RowStore store;
    CHECK_FALSE(ui::rowViewOf(setting("a"), &store).resetValue);

    store.defaults["a"] = true;
    store.notAvailable.insert("a");
    CHECK_FALSE(ui::rowViewOf(setting("a"), &store).resetValue);

    auto slider = setting("slider");
    slider.type = model::SettingType::Slider;
    slider.valueOptions.min = 0.0f;
    slider.valueOptions.max = 10.0f;
    store.settings["slider"] = 1.0f;
    store.defaults["slider"] = 5.0f;
    CHECK(ui::rowViewOf(slider, &store).resetValue);
    slider.valueOptions.max = 0.0f;
    const auto noRange = ui::rowViewOf(slider, &store);
    CHECK_FALSE(noRange.resetValue);
    CHECK_FALSE(noRange.resets);

    auto stepper = setting("stepper");
    stepper.type = model::SettingType::Stepper;
    stepper.valueOptions.options = { { .value = 1, .text = "One" }, { .value = 2, .text = "Two" } };
    store.settings["stepper"] = 1;
    store.defaults["stepper"] = 2;
    CHECK(ui::rowViewOf(stepper, &store).resetValue);
    store.defaults["stepper"] = 3;
    CHECK_FALSE(ui::rowViewOf(stepper, &store).resetValue);
}
