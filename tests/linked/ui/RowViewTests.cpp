#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>
#include <string>

#include "ui/RowView.h"

using namespace mods_config;
using model::Condition;
using ui::TextMark;
using ui::ValueLook;

namespace
{
    /**
     * A store whose settings are available, invalid and dirty by their ids, with the values a condition asks by id.
     * A setting is available unless it is named as not. A dirty change takes effect after a restart.
     */
    class RowStore final : public model::StoreReader
    {
    public:
        std::set<std::string, std::less<>> notAvailable;
        std::set<std::string, std::less<>> invalid;
        std::set<std::string, std::less<>> dirty;
        std::map<std::string, model::Value, std::less<>> values;

        model::StoredValue value(const model::Setting& setting) override
        {
            const bool isInvalid = invalid.contains(setting.id);
            return { .value = false, .available = !notAvailable.contains(setting.id), .invalid = isInvalid, .reason = isInvalid ? "Not a value of it." : "" };
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
