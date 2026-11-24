#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "model/Categories.h"
#include "ui/FullPanel.h"

using namespace mods_config;
using ui::FullPanel;

// The mod list as the panel's interface drives it: the rows of the Categories page of the menu's own config spec as the
// mods and the table are set, the mods grouped again as one is replaced, and the categories the rows offer.
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
     * The menu's own config spec, with its Categories page shown: a row for each of the mods named, as its client
     * gives them, kept in the page's file under the mod's name.
     */
    model::Mod ownMod(const std::vector<std::string>& rowsOf = {})
    {
        auto mod = configSpec(OWN, "Mods Config FO4 VR");
        model::Group section;
        section.id = model::CATEGORY_ROWS_ID;
        for (const auto& name : rowsOf) {
            model::Setting row;
            row.id = std::string(model::CATEGORY_SETTING_ID) + name;
            row.type = model::SettingType::Dropdown;
            row.valueType = model::StoredType::String;
            row.storage = model::FileStorage{ .file = model::CATEGORIES_FILE, .path = { name } };
            section.items.emplace_back(std::move(row));
        }
        mod.pages.push_back(model::Page{ .name = "Categories", .help = {}, .items = { std::move(section) }, .pages = {}, .id = model::CATEGORIES_PAGE_ID });
        return mod;
    }

    /**
     * The ids and the defaults of the rows the own mod's client gives its Categories page, as the panel lists the mods.
     */
    std::vector<std::pair<std::string, std::string>> categoryRows(const FullPanel& panel)
    {
        const auto content = nlohmann::json::parse(panel.modList().categoryPageContent());
        std::vector<std::pair<std::string, std::string>> rows;
        for (const auto& row : content.at(1).at("content")) {
            rows.emplace_back(row.at("id").get<std::string>(), row.at("default").get<std::string>());
        }
        return rows;
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

TEST_CASE("ModList: the own mod's Categories page has a row for each mod the list has, in their order, with the defaults of the table as it is")
{
    FullPanel panel;
    panel.modList().setOwnMod(OWN);
    panel.setMods({ configSpec("Beta", "Beta", "Combat"), ownMod(), configSpec("Alpha", "Alpha") });
    using Rows = std::vector<std::pair<std::string, std::string>>;
    CHECK(categoryRows(panel) == Rows{ { "category.Alpha", "" }, { "category.Beta", "Combat" }, { "category.ModsConfigFO4VR", "" } });

    // the table gives Alpha a category, at any time
    panel.modList().setCategoryTable({ .names = { "Interface" }, .mods = { { "alpha", "Interface" } } });
    CHECK(categoryRows(panel).front() == std::pair<std::string, std::string>{ "category.Alpha", "Interface" });

    // a mod read again under another name takes its place in the order
    panel.replaceMod(configSpec("Alpha", "Zeta"), "10:42:05");
    CHECK(categoryRows(panel).back().first == "category.Alpha");
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
    panel.setMods({ ownMod({ "Alpha", "Beta" }), configSpec("Alpha", "Alpha"), configSpec("Beta", "Beta") });
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
