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
    class DirtyStore final : public model::StoreReader
    {
    public:
        std::set<std::string, std::less<>> dirty;
        std::set<std::string, std::less<>> notAvailable;
        std::set<std::string, std::less<>> invalid;
        std::map<std::string, model::Value, std::less<>> values;

        model::StoredValue value(const model::Setting& setting) override
        {
            return { .value = false, .available = !notAvailable.contains(setting.id), .invalid = invalid.contains(setting.id), .reason = {} };
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
            return std::nullopt;
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

TEST_CASE("A choice that sets other settings is not counted, and they are", "[unsaved]")
{
    auto choice = setting("preset");
    choice.storage = model::SetsStorage{};
    const model::Page shown = page("Page", { std::move(choice), setting("target") });
    DirtyStore store;
    store.dirty = { "preset", "target" };
    store.invalid = { "preset" };
    CHECK(model::unsavedChanges(shown, store, false) == 1);
    CHECK(model::foldedMarks(shown.items, store) == model::FoldedMarks{ .invalid = 0, .unsaved = 1 });
}

TEST_CASE("A folded section marks its invalid settings, saved or not, and its other unsaved ones", "[unsaved]")
{
    const auto mod = someMod();
    DirtyStore store;
    store.dirty = { "parent", "inSection1" };
    store.invalid = { "parent", "inSection2" };
    // an invalid change is marked invalid alone, and an invalid value of its file is marked too
    CHECK(model::foldedMarks(mod.pages[0].items, store) == model::FoldedMarks{ .invalid = 2, .unsaved = 1 });
}

TEST_CASE("A folded section marks the settings of the sections in it, and not a hidden or unavailable one", "[unsaved]")
{
    model::Group inner{ .text = "Inner", .items = {}, .visibleWhen = {} };
    inner.items.emplace_back(setting("deep"));
    inner.items.emplace_back(setting("hidden", on("bShow")));
    inner.items.emplace_back(setting("missing"));
    const std::vector<model::Item> items{ setting("top"), std::move(inner) };
    DirtyStore store;
    store.dirty = { "top", "deep", "hidden", "missing" };
    store.notAvailable = { "missing" };
    store.values = { { "bShow", false } };
    CHECK(model::foldedMarks(items, store) == model::FoldedMarks{ .invalid = 0, .unsaved = 2 });

    store.values = { { "bShow", true } };
    CHECK(model::foldedMarks(items, store) == model::FoldedMarks{ .invalid = 0, .unsaved = 3 });
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
