#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "TestPanel.h"
#include "TestStore.h"
#include "ui/FullPanel.h"

using namespace mods_config;
using test_panel::drawFrames;
using test_panel::search;
using test_store::RecordingStore;
using ui::FullPanel;
using ui::Selection;

// The search session as the panel's interface drives it: what a query finds as the mods change, as a mod is selected
// and as its conditions hide rows, when the texts from the game are read, and when the page is filtered or whole.
namespace
{
    model::Item row(std::string text)
    {
        return model::Setting{ .id = text, .type = model::SettingType::Switcher, .text = text, .help = "What it does.", .valueOptions = {} };
    }

    /**
     * A config spec of a folder named as the mod, with rows on its front page by their texts.
     */
    model::Mod configSpec(const std::string& name, std::vector<model::Item> rows)
    {
        model::Mod mod{ .name = name, .displayName = name, .frontPage = {}, .pages = {} };
        mod.source = model::Source::ConfigSpec;
        mod.folder = name;
        mod.frontPage.items = std::move(rows);
        return mod;
    }

    /**
     * The same as an MCM mod.
     */
    model::Mod mcmMod(const std::string& name, std::vector<model::Item> rows)
    {
        auto mod = configSpec(name, std::move(rows));
        mod.source = model::Source::Mcm;
        mod.folder.clear();
        return mod;
    }

    Selection selectionOf(const std::size_t mod)
    {
        return { .mod = mod, .page = {} };
    }

    std::size_t readsOf(const RecordingStore& store, const std::string& mod)
    {
        return static_cast<std::size_t>(std::ranges::count(store.calls, "readTexts " + mod));
    }
}

TEST_CASE("a new mod list empties the query and what it found")
{
    const test_panel::PanelContext context;
    FullPanel panel;
    panel.setMods({ configSpec("Alpha", { row("Speed") }), configSpec("Beta", { row("Strength") }) });
    drawFrames(panel, 2);

    search(panel, "spe");
    REQUIRE(panel.search().matches() == 1);

    panel.setMods({ configSpec("Alpha", { row("Speed") }), configSpec("Gamma", { row("Spell") }) });
    drawFrames(panel, 1);
    CHECK(panel.search().text().empty());
    CHECK(panel.search().query().empty());
    CHECK(panel.search().found().empty());
    CHECK(panel.search().matches() == 0);
}

TEST_CASE("a config spec read again is searched again")
{
    const test_panel::PanelContext context;
    FullPanel panel;
    panel.setMods({ configSpec("Alpha", { row("Enable") }), configSpec("Beta", { row("Strength") }) });
    drawFrames(panel, 2);

    search(panel, "speed");
    REQUIRE(panel.search().matches() == 0);
    CHECK_FALSE(panel.search().foundIn(0));

    // the live copy has the row now, and its mod sorts after Beta by its new name
    auto replaced = configSpec("Alpha", { row("Enable"), row("Speed") });
    replaced.displayName = "Zeta";
    panel.replaceMod(std::move(replaced), "12:00");
    drawFrames(panel, 1);
    CHECK(panel.search().matches() == 1);
    CHECK_FALSE(panel.search().foundIn(0));
    REQUIRE(panel.search().foundIn(1));
    CHECK(panel.search().foundIn(1)->total == 1);
}

TEST_CASE("a row a condition hides is found while its mod is not selected, and not while it is")
{
    const test_panel::PanelContext context;
    auto alpha = configSpec("Alpha", { row("Show advanced"), row("Intensity") });
    std::get<model::Setting>(alpha.frontPage.items[1]).visibleWhen = model::Condition{ .kind = model::Condition::Kind::On, .setting = "Show advanced", .conditions = {} };
    RecordingStore store;
    store.byId = { .value = false, .available = true };
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(alpha), configSpec("Beta", { row("Strength") }) });
    drawFrames(panel, 2);

    // another mod is searched with no store, so its conditions hold
    search(panel, "intens");
    REQUIRE(panel.search().foundIn(0));
    CHECK(panel.search().matches() == 1);

    // selected, its condition is asked of its store, which hides the row: the page says so
    panel.select(selectionOf(0));
    drawFrames(panel, 1);
    CHECK_FALSE(panel.search().foundIn(0));
    CHECK(panel.search().matches() == 0);
    CHECK(panel.search().hiddenByConditions(0, panel.mods()[0], *panel.selectedPage()));

    // the condition holds now: it is found, and nothing is hidden
    store.byId = { .value = true, .available = true };
    drawFrames(panel, 1);
    CHECK(panel.search().foundIn(0));
    CHECK_FALSE(panel.search().hiddenByConditions(0, panel.mods()[0], *panel.selectedPage()));

    // another mod selected, Alpha is searched with no store again
    store.byId = { .value = false, .available = true };
    panel.select(selectionOf(1));
    drawFrames(panel, 1);
    CHECK(panel.search().foundIn(0));
    CHECK(panel.search().matches() == 1);
}

TEST_CASE("the texts from the game are read once as the first search starts, of every available mod's store")
{
    const test_panel::PanelContext context;
    RecordingStore mcmStore;
    RecordingStore specStore;
    auto unavailable = mcmMod("Delta", { row("Speed") });
    unavailable.notAvailable = "its plugin is not loaded";
    FullPanel panel;
    panel.setStore(model::Source::Mcm, &mcmStore);
    panel.setStore(model::Source::ConfigSpec, &specStore);
    panel.setMods({ configSpec("Alpha", { row("Speed") }), mcmMod("Gamma", { row("Strength") }), std::move(unavailable) });
    drawFrames(panel, 2);

    // no search reads none
    CHECK(readsOf(mcmStore, "Gamma") == 0);

    search(panel, "s");
    CHECK(readsOf(mcmStore, "Gamma") == 1);
    CHECK(readsOf(specStore, "Alpha") == 1);
    CHECK(readsOf(mcmStore, "Delta") == 0);

    // another query reads them no more
    search(panel, "t");
    drawFrames(panel, 2);
    CHECK(readsOf(mcmStore, "Gamma") == 1);
    CHECK(readsOf(specStore, "Alpha") == 1);

    // the menu opened again, the next search reads them again
    panel.reset();
    drawFrames(panel, 1);
    search(panel, "s");
    CHECK(readsOf(mcmStore, "Gamma") == 2);
    CHECK(readsOf(specStore, "Alpha") == 2);
}

TEST_CASE("Back shows the filtered page whole with the query kept, until another page is shown or the query changes")
{
    const test_panel::PanelContext context;
    auto alpha = configSpec("Alpha", { row("Holster size"), row("Strength"), row("Left holster") });
    alpha.pages.push_back({ .name = "Extra", .items = { row("Holster offset") } });
    FullPanel panel;
    panel.setMods({ std::move(alpha) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    search(panel, "hol");
    REQUIRE(panel.search().filtered());
    CHECK(panel.back());
    drawFrames(panel, 1);
    CHECK_FALSE(panel.search().filtered());
    CHECK(panel.search().text() == "hol");
    CHECK(panel.selection() == selectionOf(0));

    // another page is filtered again
    panel.select({ .mod = 0, .page = { 0 } });
    drawFrames(panel, 1);
    CHECK(panel.search().filtered());

    // and so is the page after a new query
    CHECK(panel.back());
    drawFrames(panel, 1);
    REQUIRE_FALSE(panel.search().filtered());
    search(panel, "s");
    CHECK(panel.search().filtered());
}
