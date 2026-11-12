#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "model/UnsavedPages.h"

using namespace mods_config;
using model::Condition;

namespace
{
    /**
     * A store whose settings are dirty by their ids, with the values a condition asks by id. A setting is available
     * unless it is named as not, and invalid where it is named so.
     */
    class DirtyStore final : public model::Store
    {
    public:
        std::set<std::string, std::less<>> dirty;
        std::set<std::string, std::less<>> notAvailable;
        std::set<std::string, std::less<>> invalid;
        std::map<std::string, model::Value, std::less<>> values;

        void open(const model::Mod&) override
        {}

        void close() override
        {}

        void pageShown() override
        {}

        model::StoredValue value(const model::Setting& setting) override
        {
            return { .value = false, .available = !notAvailable.contains(setting.id), .invalid = invalid.contains(setting.id), .reason = {} };
        }

        model::StoredValue valueById(const std::string_view id) override
        {
            const auto found = values.find(id);
            return found != values.end() ? model::StoredValue{ .value = found->second, .available = true, .reason = {} } : model::StoredValue{};
        }

        bool set(const model::Setting&, const model::Value&) override
        {
            return false;
        }

        std::optional<model::Value> defaultValue(const model::Setting&) override
        {
            return std::nullopt;
        }

        const std::vector<model::Option>& options(const model::Setting& setting) const override
        {
            return setting.valueOptions.options;
        }

        bool isDirty(const model::Setting& setting) const override
        {
            return dirty.contains(setting.id);
        }

        std::size_t dirtyCount() const override
        {
            return dirty.size();
        }

        std::size_t invalidCount() const override
        {
            return 0;
        }

        std::optional<model::Applies> applies(const model::Setting&) const override
        {
            return std::nullopt;
        }

        std::size_t dirtyRestartCount() const override
        {
            return 0;
        }

        std::optional<std::size_t> lastSave() const override
        {
            return std::nullopt;
        }

        std::size_t savedRestartCount(const model::Mod&) const override
        {
            return 0;
        }

        std::expected<void, std::string> save() override
        {
            return {};
        }

        std::string_view saveFailure() const override
        {
            return {};
        }

        void discard() override
        {}

        void discard(const model::Setting&) override
        {}

        std::optional<model::Value> savedValue(const model::Setting&) override
        {
            return std::nullopt;
        }

        model::StoredValue hotkey(const model::Hotkey&) override
        {
            return {};
        }

        model::ActionResult run(const model::Action&) override
        {
            return {};
        }
    };

    Condition on(std::string id)
    {
        return { .kind = Condition::Kind::On, .setting = std::move(id), .conditions = {} };
    }

    model::Setting setting(std::string id, std::optional<Condition> visibleWhen = std::nullopt)
    {
        model::Setting made{ .id = std::move(id), .type = model::SettingType::Switcher, .text = {}, .help = {}, .valueOptions = {}, .storage = {} };
        made.visibleWhen = std::move(visibleWhen);
        return made;
    }

    model::Page page(std::string name, std::vector<model::Item> items, std::vector<model::Page> pages = {})
    {
        return { .name = std::move(name), .help = {}, .items = std::move(items), .pages = std::move(pages) };
    }

    /**
     * A mod with a setting on its front page, and a page `Parent` with a setting, a section of two, and a page
     * `Child` under it with one setting.
     */
    model::Mod someMod()
    {
        model::Group section{ .text = "A section", .items = {}, .visibleWhen = {} };
        section.items.emplace_back(setting("inSection1"));
        section.items.emplace_back(setting("inSection2"));
        model::Mod mod{ .name = "MyMod", .displayName = "My Mod", .frontPage = page({}, { setting("front") }), .pages = {} };
        mod.pages.push_back(page("Parent", { setting("parent"), std::move(section) }, { page("Child", { setting("child") }) }));
        return mod;
    }
}

TEST_CASE("A page counts its dirty settings, in its sections too", "[unsaved]")
{
    const auto mod = someMod();
    DirtyStore store;
    store.dirty = { "parent", "inSection2" };
    CHECK(model::unsavedChanges(mod.pages[0], store, false) == 2);
    CHECK(model::unsavedChanges(mod.frontPage, store, false) == 0);
}

TEST_CASE("A closed page counts the pages under it, and an open one its own rows", "[unsaved]")
{
    const auto mod = someMod();
    DirtyStore store;
    store.dirty = { "parent", "child" };
    CHECK(model::unsavedChanges(mod.pages[0], store, false) == 1);
    CHECK(model::unsavedChanges(mod.pages[0], store, true) == 2);
}

TEST_CASE("A setting whose row is hidden is not counted", "[unsaved]")
{
    model::Page shown = page("Page", { setting("hidden", on("bShow")), setting("plain") });
    DirtyStore store;
    store.dirty = { "hidden", "plain" };
    store.values = { { "bShow", false } };
    CHECK(model::unsavedChanges(shown, store, false) == 1);

    store.values = { { "bShow", true } };
    CHECK(model::unsavedChanges(shown, store, false) == 2);
}

TEST_CASE("A hidden page under a closed one counts nothing", "[unsaved]")
{
    auto mod = someMod();
    mod.pages[0].pages[0].visibleWhen = on("bShow");
    DirtyStore store;
    store.dirty = { "child" };
    store.values = { { "bShow", false } };
    CHECK(model::unsavedChanges(mod.pages[0], store, true) == 0);
    CHECK(model::unsavedChanges(mod, store) == 0);
}

TEST_CASE("A dirty setting that is invalid is counted, and one that is not available is not", "[unsaved]")
{
    const auto mod = someMod();
    DirtyStore store;
    store.dirty = { "parent", "inSection1" };
    store.invalid = { "parent" };
    store.notAvailable = { "inSection1" };
    CHECK(model::unsavedChanges(mod.pages[0], store, false) == 1);
}

TEST_CASE("A mod counts its front page and every page in the tree", "[unsaved]")
{
    auto mod = someMod();
    mod.pages.push_back(page("Hidden", { setting("onHidden") }));
    mod.pages.back().visibleWhen = on("bShow");
    DirtyStore store;
    store.dirty = { "front", "child", "inSection1", "onHidden" };
    store.values = { { "bShow", false } };
    CHECK(model::unsavedChanges(mod, store) == 3);
}
