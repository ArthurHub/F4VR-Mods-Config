#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "TestPanel.h"
#include "ui/FullPanel.h"

using namespace mods_config;
using ui::FullPanel;
using ui::Selection;

// The store session as the panel's interface drives it: what a store is told of the session of the mod that is shown,
// and in which order, as the player selects, pages, points, steps back, closes the menu and answers the dialog.
namespace
{
    using Calls = std::vector<std::string>;

    /**
     * A store that records each lifecycle call it gets, with Save and Discard, in order: the mod, or the page's name
     * ("front" for a mod's front page, "none" for no page), or whether a row is pointed. Its changes are a count the
     * test sets, which Save and Discard clear.
     */
    class RecordingStore final : public model::Store
    {
    public:
        Calls calls;
        std::size_t dirty = 0;

        void open(const model::Mod& mod) override
        {
            calls.push_back("open " + mod.name);
        }

        void close() override
        {
            calls.emplace_back("close");
        }

        void pageShown() override
        {
            calls.emplace_back("pageShown");
        }

        void readTexts(model::Mod& mod) override
        {
            calls.push_back("readTexts " + mod.name);
        }

        void showPage(const model::Page* page) override
        {
            calls.push_back("showPage " + (!page ? std::string("none") : page->name.empty() ? std::string("front") : page->name));
        }

        void showPointed(const model::Item* item) override
        {
            calls.push_back(item ? "showPointed row" : "showPointed none");
        }

        void startFrame() override
        {
            calls.emplace_back("startFrame");
        }

        model::StoredValue value(const model::Setting&) override
        {
            return {};
        }

        model::StoredValue valueById(std::string_view) override
        {
            return {};
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

        bool isDirty(const model::Setting&) const override
        {
            return false;
        }

        std::size_t dirtyCount() const override
        {
            return dirty;
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
            calls.emplace_back("save");
            dirty = 0;
            return {};
        }

        std::string_view saveFailure() const override
        {
            return {};
        }

        void discard() override
        {
            calls.emplace_back("discard");
            dirty = 0;
        }

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

    /**
     * A config spec of a folder named as the mod, with pages by their names. Its front page has two rows.
     */
    model::Mod configSpec(const std::string& name, const std::vector<std::string>& pages = {})
    {
        model::Mod mod{ .name = name, .displayName = name, .frontPage = {}, .pages = {} };
        mod.source = model::Source::ConfigSpec;
        mod.folder = name;
        mod.frontPage.items.push_back(model::Setting{ .type = model::SettingType::Switcher, .text = "Enable", .help = {}, .valueOptions = {} });
        mod.frontPage.items.push_back(model::Setting{ .type = model::SettingType::Switcher, .text = "Show", .help = {}, .valueOptions = {} });
        for (const auto& page : pages) {
            mod.pages.push_back({ .name = page, .items = {} });
        }
        return mod;
    }

    /**
     * The same config spec as an MCM mod.
     */
    model::Mod mcmMod(const std::string& name)
    {
        auto mod = configSpec(name);
        mod.source = model::Source::Mcm;
        mod.folder.clear();
        return mod;
    }

    Selection selectionOf(const std::size_t mod)
    {
        return { .mod = mod, .page = {} };
    }

    Selection selectionOf(const std::size_t mod, const std::size_t page)
    {
        return { .mod = mod, .page = { page } };
    }

    /**
     * A panel with two config specs in a recording store: Alpha with the pages First and Second, and Beta with none.
     */
    FullPanel twoConfigSpecs(RecordingStore& store)
    {
        FullPanel panel;
        panel.setMods({ configSpec("Alpha", { "First", "Second" }), configSpec("Beta") });
        panel.setStore(model::Source::ConfigSpec, &store);
        return panel;
    }

    /**
     * A row of the front page of the selected mod, by its place.
     */
    const model::Item* frontRow(const FullPanel& panel, const std::size_t place)
    {
        return &panel.selectedPage()->items[place];
    }
}

TEST_CASE("selecting a mod opens its session, reads its texts and shows its front page")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);

    panel.select(selectionOf(0));

    CHECK(store.calls == Calls{ "open Alpha", "readTexts Alpha", "showPage front" });
}

TEST_CASE("another page of the mod is shown after the store reads its texts again")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    panel.select(selectionOf(0));
    store.calls.clear();

    panel.select(selectionOf(0, 1));

    CHECK(store.calls == Calls{ "pageShown", "readTexts Alpha", "showPage Second" });
}

TEST_CASE("Back shows the mod's front page, and from there the menu's front page, which ends the session")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    panel.select(selectionOf(0, 0));
    store.calls.clear();

    panel.back();
    CHECK(store.calls == Calls{ "pageShown", "readTexts Alpha", "showPage front" });

    store.calls.clear();
    panel.back();
    CHECK(store.calls == Calls{ "close" });
}

TEST_CASE("another mod of the same store opens its session, which ends the one before")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    panel.select(selectionOf(0));
    store.calls.clear();

    panel.select(selectionOf(1));

    CHECK(store.calls == Calls{ "open Beta", "readTexts Beta", "showPage front" });
}

TEST_CASE("a mod of another store ends the session of the store before")
{
    RecordingStore mcmStore;
    RecordingStore specStore;
    FullPanel panel;
    panel.setMods({ mcmMod("Alpha"), configSpec("Beta") });
    panel.setStore(model::Source::Mcm, &mcmStore);
    panel.setStore(model::Source::ConfigSpec, &specStore);
    panel.select(selectionOf(0));
    mcmStore.calls.clear();

    panel.select(selectionOf(1));

    CHECK(mcmStore.calls == Calls{ "close" });
    CHECK(specStore.calls == Calls{ "open Beta", "readTexts Beta", "showPage front" });
}

TEST_CASE("the pointed row is told once as it changes")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    panel.select(selectionOf(0));
    store.calls.clear();

    panel.point(frontRow(panel, 0));
    panel.point(frontRow(panel, 0));
    panel.point(frontRow(panel, 1));
    panel.point(nullptr);
    panel.point(nullptr);

    CHECK(store.calls == Calls{ "showPointed row", "showPointed row", "showPointed none" });
}

TEST_CASE("another page shown while a row is pointed tells that none is, after the page")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    panel.select(selectionOf(0));
    panel.point(frontRow(panel, 0));
    store.calls.clear();

    panel.select(selectionOf(0, 1));

    CHECK(store.calls == Calls{ "pageShown", "readTexts Alpha", "showPage Second", "showPointed none" });
}

TEST_CASE("another mod shown while a row is pointed is told no row: the session before ended with its own")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    panel.select(selectionOf(0));
    panel.point(frontRow(panel, 0));
    store.calls.clear();

    panel.select(selectionOf(1));

    CHECK(store.calls == Calls{ "open Beta", "readTexts Beta", "showPage front" });
}

TEST_CASE("a frame of the panel starts a frame of the store")
{
    const test_panel::PanelContext context;
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    panel.select(selectionOf(0));
    store.calls.clear();

    test_panel::drawFrame(panel, test_panel::POINTER_AWAY);

    CHECK(store.calls == Calls{ "startFrame" });
}

TEST_CASE("a frame on the menu's front page tells no store")
{
    const test_panel::PanelContext context;
    RecordingStore store;
    auto panel = twoConfigSpecs(store);

    test_panel::drawFrame(panel, test_panel::POINTER_AWAY);

    CHECK(store.calls.empty());
}

TEST_CASE("the menu closing ends the session")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    panel.select(selectionOf(0));
    store.calls.clear();

    panel.reset();
    panel.reset();

    CHECK(store.calls == Calls{ "close" });
}

TEST_CASE("a new list of mods ends the session first")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    panel.select(selectionOf(1));
    store.calls.clear();

    panel.setMods({ configSpec("Gamma") });

    CHECK(store.calls == Calls{ "close" });
}

TEST_CASE("the dialog's answers: Cancel tells nothing, Save and Discard end before the next mod opens")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    panel.select(selectionOf(0));
    store.dirty = 1;
    store.calls.clear();

    panel.select(selectionOf(1));
    REQUIRE(panel.isAsking());

    SECTION("Cancel")
    {
        panel.answerDialog(ui::DialogAnswer::Cancel);
        CHECK(store.calls.empty());
    }

    SECTION("Save")
    {
        panel.answerDialog(ui::DialogAnswer::Save);
        CHECK(store.calls == Calls{ "save", "open Beta", "readTexts Beta", "showPage front" });
    }

    SECTION("Discard")
    {
        panel.answerDialog(ui::DialogAnswer::Discard);
        CHECK(store.calls == Calls{ "discard", "open Beta", "readTexts Beta", "showPage front" });
    }
}

TEST_CASE("the dialog's Save before the menu closes ends the session as the menu closes")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    bool closed = false;
    panel.setMenuActions({ .minimize = {}, .close = [&] {
                              closed = true;
                              panel.reset();
                          } });
    panel.select(selectionOf(0));
    store.dirty = 1;
    store.calls.clear();

    panel.close();
    REQUIRE(panel.isAsking());
    panel.answerDialog(ui::DialogAnswer::Save);

    CHECK(closed);
    CHECK(store.calls == Calls{ "save", "close" });
}

TEST_CASE("a config spec read again from its live copy while its mod is shown is told the page that stays shown")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    panel.select(selectionOf(0, 1));
    store.calls.clear();

    panel.replaceMod(configSpec("Alpha", { "First", "Second" }), "10:42:05");

    CHECK(store.calls == Calls{ "close", "open Alpha", "readTexts Alpha", "showPage Second" });
}

TEST_CASE("a config spec read again with no page that was shown shows its front page")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    panel.select(selectionOf(0, 1));
    store.calls.clear();

    panel.replaceMod(configSpec("Alpha"), "10:42:05");

    CHECK(store.calls == Calls{ "close", "open Alpha", "readTexts Alpha", "showPage front" });
}

TEST_CASE("a config spec read again while another mod is shown tells the store nothing, also as the mods are sorted again")
{
    RecordingStore store;
    auto panel = twoConfigSpecs(store);
    panel.select(selectionOf(1));
    store.calls.clear();

    // its new display name sorts it after Beta, which is the first mod now
    auto renamed = configSpec("Alpha");
    renamed.displayName = "Zeta";
    panel.replaceMod(std::move(renamed), "10:42:05");
    CHECK(store.calls.empty());

    panel.select(selectionOf(1));
    CHECK(store.calls == Calls{ "open Alpha", "readTexts Alpha", "showPage front" });
}
