#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "ui/Breadcrumbs.h"

using namespace mods_config;
using ui::Crumb;
using ui::CrumbLook;
using ui::CrumbTarget;
using ui::Selection;

namespace
{
    /**
     * Alpha, with Body and Advanced, Hands and Feet under Body, and Fingers under Hands, as MCM sets them in with
     * spaces before their names. Then Beta, with no pages.
     */
    std::vector<model::Mod> mods()
    {
        model::Mod alpha{ .name = "alpha", .displayName = "Alpha", .frontPage = {}, .pages = {} };
        alpha.pages.push_back({ .name = "Body", .items = {} });
        alpha.pages.push_back({ .name = "Advanced", .items = {} });
        alpha.pages[0].pages.push_back({ .name = "  Hands", .items = {} });
        alpha.pages[0].pages.push_back({ .name = "  Feet", .items = {} });
        alpha.pages[0].pages[0].pages.push_back({ .name = "    Fingers", .items = {} });
        return { std::move(alpha), model::Mod{ .name = "beta", .displayName = "Beta", .frontPage = {}, .pages = {} } };
    }

    std::vector<std::string> textsOf(const std::vector<Crumb>& crumbs)
    {
        std::vector<std::string> texts;
        for (const auto& crumb : crumbs) {
            texts.push_back(crumb.text);
        }
        return texts;
    }

    std::vector<CrumbLook> looksOf(const std::vector<Crumb>& crumbs)
    {
        std::vector<CrumbLook> looks;
        for (const auto& crumb : crumbs) {
            looks.push_back(crumb.look);
        }
        return looks;
    }
}

TEST_CASE("Breadcrumbs: the menu's front page has the menu's crumb alone, selected, which goes nowhere")
{
    const auto crumbs = ui::breadcrumbsOf(mods(), {}, false, nullptr, false);
    CHECK(textsOf(crumbs) == std::vector<std::string>{ "MODS CONFIG FO4" });
    CHECK(crumbs[0].look == CrumbLook::Selected);
    CHECK_FALSE(crumbs[0].target);
}

TEST_CASE("Breadcrumbs: a page has the menu's, the mod's and its own crumb, and each before the last goes there")
{
    const auto crumbs = ui::breadcrumbsOf(mods(), { .mod = 0, .page = { 1 } }, false, nullptr, false);
    CHECK(textsOf(crumbs) == std::vector<std::string>{ "MODS CONFIG FO4", "Alpha", "Advanced" });
    CHECK(looksOf(crumbs) == std::vector{ CrumbLook::Link, CrumbLook::Link, CrumbLook::Current });
    CHECK(crumbs[0].target == CrumbTarget(Selection{}));
    CHECK(crumbs[1].target == CrumbTarget(Selection{ .mod = 0, .page = {} }));
    CHECK_FALSE(crumbs[2].target);
}

TEST_CASE("Breadcrumbs: a page under a page has a crumb for each page on the way to it, by its name without the spaces before it")
{
    const auto crumbs = ui::breadcrumbsOf(mods(), { .mod = 0, .page = { 0, 0, 0 } }, false, nullptr, false);
    CHECK(textsOf(crumbs) == std::vector<std::string>{ "MODS CONFIG FO4", "Alpha", "Body", "Hands", "Fingers" });
    CHECK(looksOf(crumbs) == std::vector{ CrumbLook::Link, CrumbLook::Link, CrumbLook::Link, CrumbLook::Link, CrumbLook::Current });
    CHECK(crumbs[2].target == CrumbTarget(Selection{ .mod = 0, .page = { 0 } }));
    CHECK(crumbs[3].target == CrumbTarget(Selection{ .mod = 0, .page = { 0, 0 } }));
    CHECK_FALSE(crumbs[4].target);
}

TEST_CASE("Breadcrumbs: a mod's front page is its last crumb, and only the last says the page is filtered")
{
    auto crumbs = ui::breadcrumbsOf(mods(), { .mod = 1, .page = {} }, false, nullptr, true);
    CHECK(textsOf(crumbs) == std::vector<std::string>{ "MODS CONFIG FO4", "Beta" });
    CHECK(crumbs[1].look == CrumbLook::Current);
    CHECK(crumbs[1].filtered);
    CHECK_FALSE(crumbs[0].filtered);

    crumbs = ui::breadcrumbsOf(mods(), { .mod = 0, .page = { 0 } }, false, nullptr, true);
    CHECK_FALSE(crumbs[1].filtered);
    CHECK(crumbs[2].filtered);
}

TEST_CASE("Breadcrumbs: the Help page has the menu's crumb and its own, over the selection it keeps")
{
    const auto crumbs = ui::breadcrumbsOf(mods(), { .mod = 0, .page = { 0 } }, true, nullptr, true);
    CHECK(textsOf(crumbs) == std::vector<std::string>{ "MODS CONFIG FO4", "Help" });
    CHECK(looksOf(crumbs) == std::vector{ CrumbLook::Link, CrumbLook::Current });
    CHECK(crumbs[0].target == CrumbTarget(Selection{}));
    CHECK_FALSE(crumbs[1].target);
    CHECK_FALSE(crumbs[1].filtered);
}

TEST_CASE("Breadcrumbs: a config spec's report has the menu's crumb, the Developer crumb that goes to its section, and its own")
{
    const auto all = mods();
    const auto crumbs = ui::breadcrumbsOf(all, {}, false, &all[1], false);
    CHECK(textsOf(crumbs) == std::vector<std::string>{ "MODS CONFIG FO4", "Developer", "Beta" });
    CHECK(looksOf(crumbs) == std::vector{ CrumbLook::Link, CrumbLook::Link, CrumbLook::Current });
    CHECK(crumbs[0].target == CrumbTarget(Selection{}));
    CHECK(crumbs[1].target == CrumbTarget(ui::ToDeveloperSection{}));
    CHECK_FALSE(crumbs[2].target);
}
