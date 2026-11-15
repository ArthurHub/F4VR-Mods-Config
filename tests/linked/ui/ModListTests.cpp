#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "model/Categories.h"
#include "ui/FullPanel.h"

using namespace mods_config;
using ui::FullPanel;

// The mod list as the panel's interface drives it: the Categories page of the menu's own config spec refilled as the
// mods, the table and the own mod are set, the mods grouped again as one is replaced, and the categories the rows offer.
namespace
{
    constexpr auto OWN = "ModsConfigFO4VR";

    /**
     * A config spec of a folder named as the mod, with a category of its own: none for an empty one.
     */
    model::Mod configSpec(const std::string& name, const std::string& displayName, const std::string& category = {})
    {
        model::Mod mod{ .name = name, .displayName = displayName, .frontPage = {}, .pages = {} };
        mod.source = model::Source::ConfigSpec;
        mod.folder = name;
        mod.category = category;
        return mod;
    }

    /**
     * The menu's own config spec, with its Categories page and no rows on it yet.
     */
    model::Mod ownMod()
    {
        auto mod = configSpec(OWN, "Mods Config FO4 VR");
        mod.pages.push_back(model::Page{ .name = "Categories", .help = {}, .items = {}, .pages = {}, .id = model::CATEGORIES_PAGE_ID });
        return mod;
    }

    /**
     * The rows of the own mod's Categories page, as the panel lists it.
     */
    std::vector<model::Setting> categoryRows(const FullPanel& panel)
    {
        const auto& mods = panel.mods();
        const auto own = std::ranges::find(mods, std::string(OWN), &model::Mod::name);
        REQUIRE(own != mods.end());
        std::vector<model::Setting> rows;
        for (const auto& item : own->pages.front().items) {
            if (const auto* group = std::get_if<model::Group>(&item); group && group->id == model::CATEGORY_ROWS_ID) {
                for (const auto& row : group->items) {
                    rows.push_back(std::get<model::Setting>(row));
                }
            }
        }
        return rows;
    }

    std::vector<std::string> textsOf(const std::vector<model::Setting>& rows)
    {
        std::vector<std::string> texts;
        for (const auto& row : rows) {
            texts.push_back(row.text);
        }
        return texts;
    }

    /**
     * A store whose rows wait for a Save by their ids, each with its text value.
     */
    class PendingStore final : public model::StoreReader
    {
    public:
        std::map<std::string, std::string, std::less<>> pending;

        model::StoredValue value(const model::Setting& setting) override
        {
            const auto found = pending.find(setting.id);
            return { .value = found != pending.end() ? found->second : std::string(), .available = true, .reason = {} };
        }

        model::StoredValue valueById(std::string_view) override
        {
            return {};
        }

        bool isDirty(const model::Setting& setting) const override
        {
            return pending.contains(setting.id);
        }

        std::optional<model::Applies> applies(const model::Setting&) const override
        {
            return model::Applies::Save;
        }
    };
}

TEST_CASE("ModList: the own mod's Categories page has a row for each mod as the mods are set, and the table gives their defaults as it is set")
{
    FullPanel panel;
    panel.modList().setOwnMod(OWN);
    panel.setMods({ configSpec("Beta", "Beta", "Combat"), ownMod(), configSpec("Alpha", "Alpha") });

    const auto rows = categoryRows(panel);
    CHECK(textsOf(rows) == std::vector<std::string>{ "Alpha", "Beta", "Mods Config FO4 VR" });
    CHECK(rows[0].defaultValue == model::Value(std::string()));
    CHECK(rows[1].defaultValue == model::Value(std::string("Combat")));

    // the table gives Alpha a category, at any time
    panel.modList().setCategoryTable({ .names = { "Interface" }, .mods = { { "alpha", "Interface" } } });
    CHECK(categoryRows(panel)[0].defaultValue == model::Value(std::string("Interface")));
}

TEST_CASE("ModList: the own mod read again gets its rows again, and another mod read again leaves them")
{
    FullPanel panel;
    panel.modList().setOwnMod(OWN);
    panel.setMods({ ownMod(), configSpec("Alpha", "Alpha") });

    panel.replaceMod(ownMod(), "10:42:05");
    CHECK(textsOf(categoryRows(panel)) == std::vector<std::string>{ "Alpha", "Mods Config FO4 VR" });

    // the own mod can be open on its Categories page, whose rows stay as they are
    panel.replaceMod(configSpec("Alpha", "Zeta"), "10:42:09");
    CHECK(textsOf(categoryRows(panel)) == std::vector<std::string>{ "Alpha", "Mods Config FO4 VR" });
}

TEST_CASE("ModList: a mod with no own mod told has no rows filled")
{
    FullPanel panel;
    panel.setMods({ ownMod(), configSpec("Alpha", "Alpha") });
    CHECK(categoryRows(panel).empty());
}

TEST_CASE("ModList: a mod read again is in the order of the names and under its category again")
{
    FullPanel panel;
    panel.modList().setCategoriesMinMods(0);
    panel.setMods({ configSpec("Alpha", "Alpha", "Combat"), configSpec("Beta", "Beta", "Interface") });
    CHECK(panel.modList().groups() == std::vector<model::CategoryGroup>{ { .name = "Combat", .mods = { 0 } }, { .name = "Interface", .mods = { 1 } } });

    panel.replaceMod(configSpec("Alpha", "Zeta", "Interface"), "10:42:05");
    CHECK(panel.mods()[1].displayName == "Zeta");
    CHECK(panel.modList().groups() == std::vector<model::CategoryGroup>{ { .name = "Interface", .mods = { 0, 1 } } });
}

TEST_CASE("ModList: the rows offer the names in use, with the player's choices and the ones on the own mod's page that wait for a Save")
{
    FullPanel panel;
    panel.modList().setOwnMod(OWN);
    panel.setMods({ ownMod(), configSpec("Alpha", "Alpha"), configSpec("Beta", "Beta") });
    panel.modList().setCategoryChoices({ { "beta", "Saved Name" } });

    PendingStore store;
    store.pending[std::string(model::CATEGORY_SETTING_ID) + "Alpha"] = "Typed Name";

    std::set<std::string> offered;
    for (const auto& option : panel.modList().categoryOptions(store)) {
        offered.insert(option.text);
    }
    CHECK(offered.contains("Saved Name"));
    CHECK(offered.contains("Typed Name"));

    // with nothing waiting, only the saved choice
    store.pending.clear();
    offered.clear();
    for (const auto& option : panel.modList().categoryOptions(store)) {
        offered.insert(option.text);
    }
    CHECK(offered.contains("Saved Name"));
    CHECK_FALSE(offered.contains("Typed Name"));
}
