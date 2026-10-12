#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <format>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "TestPanel.h"
#include "Theme.h"
#include "ui/FullPanel.h"

using namespace mods_config;
using test_panel::CONTENT_START;
using ui::FullPanel;
using ui::Selection;

namespace
{
    using Names = std::vector<std::string>;

    // the tree's header and the distance from a row of it to the next, as ModTree.cpp has them
    constexpr float TREE_HEADER_HEIGHT = 40.0f;
    constexpr float TREE_ROW_PITCH = theme::CONTROL_HEIGHT + theme::SPACE_1;

    model::Mod modOf(const std::string& name, const Names& pages = {})
    {
        model::Mod mod{ .name = name, .displayName = name, .frontPage = {}, .pages = {} };
        for (const auto& page : pages) {
            mod.pages.push_back({ .name = page, .items = {} });
        }
        return mod;
    }

    /**
     * A panel with three mods. In its order they are Alpha with two pages, Beta with none and Gamma with one.
     */
    FullPanel threeMods()
    {
        FullPanel panel;
        panel.setMods({ modOf("Gamma", { "Only" }), modOf("Alpha", { "First", "Second" }), modOf("Beta") });
        return panel;
    }

    Names namesOf(const FullPanel& panel)
    {
        Names names;
        for (const auto& mod : panel.mods()) {
            names.push_back(mod.displayName);
        }
        return names;
    }

    Selection selectionOf(const std::size_t mod)
    {
        return { .mod = mod, .page = std::nullopt };
    }

    Selection selectionOf(const std::size_t mod, const std::size_t page)
    {
        return { .mod = mod, .page = page };
    }

    void drawFrames(FullPanel& panel, const int count)
    {
        for (int i = 0; i < count; ++i) {
            test_panel::drawFrame(panel, test_panel::POINTER_AWAY);
        }
    }

    /**
     * A press of the trigger with the ray at a place on the panel: the ray comes there, presses and lets go.
     */
    void press(FullPanel& panel, const ImVec2 at)
    {
        for (const bool pressed : { false, false, true, false, false }) {
            test_panel::drawFrame(panel, at, pressed);
        }
    }

    /**
     * The middle of a row of the tree, by its place among the rows the tree has now, while they are at their top.
     */
    ImVec2 treeRow(const int place)
    {
        const float rowsTop = CONTENT_START + theme::TITLEBAR_HEIGHT + theme::SPACE_3 + theme::SPACE_2 + TREE_HEADER_HEIGHT + theme::SPACE_1;
        return { CONTENT_START + theme::SPACE_2 + 100.0f, rowsTop + static_cast<float>(place) * TREE_ROW_PITCH + theme::CONTROL_HEIGHT * 0.5f };
    }

    /**
     * The middle of a row of a page, by its place among the rows, while the page is at its top and has rows of
     * controls alone.
     */
    ImVec2 pageRow(const int place)
    {
        const float rowsTop = CONTENT_START + theme::TITLEBAR_HEIGHT + theme::SPACE_3;
        return { CONTENT_START + theme::NAV_WIDTH + theme::SPACE_4 + 200.0f, rowsTop + static_cast<float>(place) * theme::ROW_PITCH + theme::CONTROL_HEIGHT * 0.5f };
    }

    // a place of the page under the rows of the mod with a help, where the page is empty
    constexpr int NO_PAGE_ROW = 6;

    /**
     * A panel with one mod, shown on its front page: a switcher and a slider with a help, and between them a
     * button with none, which is set in by spaces. The mod has a page with one switcher.
     */
    FullPanel modWithHelp()
    {
        auto mod = modOf("Alpha");
        mod.frontPage.items.push_back(model::Setting{ .type = model::SettingType::Switcher, .text = "Enable the feature", .help = "Turns the feature on.", .valueOptions = {} });
        mod.frontPage.items.push_back(model::Button{ .text = "    Reset everything", .help = {} });
        mod.frontPage.items.push_back(model::Setting{ .type = model::SettingType::Slider, .text = "Strength", .help = "How strong the feature is.", .valueOptions = {} });
        mod.pages.push_back(
            { .name = "Advanced", .items = { model::Setting{ .type = model::SettingType::Switcher, .text = "Mode", .help = "Low or high.", .valueOptions = {} } } });

        FullPanel panel;
        panel.setMods({ std::move(mod) });
        panel.select(selectionOf(0));
        return panel;
    }

    /**
     * The ray at a place on the panel, for the frames ImGui needs to find what it is on.
     */
    void point(FullPanel& panel, const ImVec2 at)
    {
        for (int i = 0; i < 3; ++i) {
            test_panel::drawFrame(panel, at);
        }
    }

    /**
     * The width of a breadcrumb, which is its text in the title's size with its padding.
     */
    float crumbWidth(const char* text)
    {
        const auto* font = ImGui::GetIO().Fonts->Fonts[0];
        return std::ceil(font->CalcTextSizeA(test_panel::FONT_SIZE * theme::FONT_SCALE_TITLE, FLT_MAX, 0.0f, text).x) + theme::SPACE_2 * 2.0f;
    }

    /**
     * A place on the menu's breadcrumb, and one on the mod's, which is after it and a separator.
     */
    ImVec2 menuCrumb()
    {
        return { CONTENT_START + theme::SPACE_2 + 20.0f, CONTENT_START + theme::TITLEBAR_HEIGHT * 0.5f };
    }

    ImVec2 modCrumb()
    {
        const float separator = crumbWidth("\xC2\xBB") - theme::SPACE_2 * 2.0f;
        const float left = CONTENT_START + theme::SPACE_2 + crumbWidth("MODS CONFIG VR") + theme::SPACE_1 + separator + theme::SPACE_1;
        return { left + 20.0f, CONTENT_START + theme::TITLEBAR_HEIGHT * 0.5f };
    }

    /**
     * The window that scrolls the rows of the tree.
     */
    const ImGuiWindow& treeRowsWindow()
    {
        const auto& windows = ImGui::GetCurrentContext()->Windows;
        const auto found = std::ranges::find_if(windows, [](const ImGuiWindow* window) {
            return std::strstr(window->Name, "ModRows") != nullptr;
        });
        REQUIRE(found != windows.end());
        return **found;
    }

    /**
     * Whether a row of the tree is whole in the rows that are shown, by its place among the rows the tree has now.
     */
    bool isRowShown(const int place)
    {
        const auto& rows = treeRowsWindow();
        const float top = static_cast<float>(place) * TREE_ROW_PITCH - rows.Scroll.y;
        return top >= 0.0f && top + theme::CONTROL_HEIGHT <= rows.Size.y;
    }
}

TEST_CASE("FullPanel: the mods are kept in order of their display names, in any letter case")
{
    FullPanel panel;
    panel.setMods({ modOf("XDI"), modOf("auto Eat"), modOf("Sim Settlements 2"), modOf("FallUI") });

    CHECK(namesOf(panel) == Names{ "auto Eat", "FallUI", "Sim Settlements 2", "XDI" });
}

TEST_CASE("FullPanel: it shows the menu's front page until a mod is selected")
{
    auto panel = threeMods();
    CHECK(panel.selection() == Selection{});

    // a mod's front page, by the mod's place in the order of the names
    panel.select(selectionOf(2));
    CHECK(panel.selection() == selectionOf(2));
    CHECK(panel.mods()[2].displayName == "Gamma");

    // one of its pages
    panel.select(selectionOf(0, 1));
    CHECK(panel.selection() == selectionOf(0, 1));

    panel.select({});
    CHECK(panel.selection() == Selection{});
}

TEST_CASE("FullPanel: a selection of what is not there shows the nearest that is")
{
    auto panel = threeMods();

    // a page the mod does not have: the mod's front page
    panel.select(selectionOf(0, 2));
    CHECK(panel.selection() == selectionOf(0));
    panel.select(selectionOf(1, 0));
    CHECK(panel.selection() == selectionOf(1));

    // a mod that is not there: the menu's front page
    panel.select(selectionOf(3));
    CHECK(panel.selection() == Selection{});
    panel.select(selectionOf(3, 0));
    CHECK(panel.selection() == Selection{});

    // a page of no mod
    panel.select({ .mod = std::nullopt, .page = 0 });
    CHECK(panel.selection() == Selection{});
}

TEST_CASE("FullPanel: other mods, and a menu that opens again, show the menu's front page")
{
    auto panel = threeMods();
    panel.select(selectionOf(0, 1));
    panel.reset();
    CHECK(panel.selection() == Selection{});

    panel.select(selectionOf(0, 1));
    panel.setMods({ modOf("Delta", { "One", "Two" }) });
    CHECK(panel.selection() == Selection{});
}

TEST_CASE("FullPanel: a press on a mod's row shows the mod, and a press on a page's row shows the page")
{
    const test_panel::PanelContext context;
    auto panel = threeMods();
    drawFrames(panel, 2);

    // Beta, the second mod
    press(panel, treeRow(1));
    CHECK(panel.selection() == selectionOf(1));

    // Alpha opens, with its two pages as the rows under it
    press(panel, treeRow(0));
    CHECK(panel.selection() == selectionOf(0));
    press(panel, treeRow(2));
    CHECK(panel.selection() == selectionOf(0, 1));
    press(panel, treeRow(1));
    CHECK(panel.selection() == selectionOf(0, 0));

    // the mod's own row is its front page
    press(panel, treeRow(0));
    CHECK(panel.selection() == selectionOf(0));

    // another mod is selected with no step between: Gamma is after Alpha's pages and Beta
    press(panel, treeRow(4));
    CHECK(panel.selection() == selectionOf(2));
    // and Alpha is closed again, so Gamma's page is the fourth row
    press(panel, treeRow(3));
    CHECK(panel.selection() == selectionOf(2, 0));
}

TEST_CASE("FullPanel: a press where the tree has no row changes nothing")
{
    const test_panel::PanelContext context;
    auto panel = threeMods();
    drawFrames(panel, 2);

    press(panel, treeRow(5));
    CHECK(panel.selection() == Selection{});

    // Beta has no pages, so no row comes under it
    press(panel, treeRow(1));
    press(panel, treeRow(3));
    CHECK(panel.selection() == selectionOf(1));
}

TEST_CASE("FullPanel: a breadcrumb before the last is pressed to go there")
{
    const test_panel::PanelContext context;
    auto panel = threeMods();
    panel.select(selectionOf(0, 1));
    drawFrames(panel, 2);

    // from a page to its mod's front page
    press(panel, modCrumb());
    CHECK(panel.selection() == selectionOf(0));

    // the mod's crumb is now the last, where the player is: it is not pressed
    press(panel, modCrumb());
    CHECK(panel.selection() == selectionOf(0));

    // from the mod to the menu's front page
    press(panel, menuCrumb());
    CHECK(panel.selection() == Selection{});

    // and from a page
    panel.select(selectionOf(2, 0));
    press(panel, menuCrumb());
    CHECK(panel.selection() == Selection{});
}

TEST_CASE("FullPanel: Back goes from a page to its mod's front page, and from there to the menu's front page")
{
    auto panel = threeMods();
    panel.select(selectionOf(0, 1));

    CHECK(panel.back());
    CHECK(panel.selection() == selectionOf(0));
    CHECK(panel.back());
    CHECK(panel.selection() == Selection{});

    // the menu's front page has nothing before it: the menu is closed there
    CHECK_FALSE(panel.back());
    CHECK(panel.selection() == Selection{});
}

TEST_CASE("FullPanel: a press that shows another page acts, and one that changes nothing does not")
{
    const test_panel::PanelContext context;
    auto panel = modWithHelp();
    drawFrames(panel, 2);

    // whether a press at a place acted in one of its frames
    const auto acts = [&panel](const ImVec2 at) {
        bool acted = false;
        for (const bool pressed : { false, false, true, false, false }) {
            test_panel::drawFrame(panel, at, pressed);
            acted = acted || panel.pressActed();
        }
        return acted;
    };

    // the mod's page, which is the row under the mod's own
    CHECK(acts(treeRow(1)));
    CHECK(panel.selection() == selectionOf(0, 0));
    // it is shown already
    CHECK_FALSE(acts(treeRow(1)));

    // a row of the page changes nothing yet, and neither does a press where the page has no row
    CHECK_FALSE(acts(pageRow(0)));
    CHECK_FALSE(acts(pageRow(NO_PAGE_ROW)));

    // a breadcrumb that goes somewhere
    CHECK(acts(modCrumb()));
    CHECK(panel.selection() == selectionOf(0));

    // it acts in the frame of the press alone
    drawFrames(panel, 2);
    CHECK_FALSE(panel.pressActed());
}

TEST_CASE("FullPanel: a press that opens a quick launch entry acts")
{
    const test_panel::PanelContext context;
    FullPanel panel;
    int opened = 0;
    panel.setQuickLaunch({ { .name = "FRIK", .help = {}, .open = [&opened] {
                                ++opened;
                            } } });
    drawFrames(panel, 2);

    bool acted = false;
    for (const bool pressed : { false, false, true, false, false }) {
        test_panel::drawFrame(panel, ImVec2(640.0f, 250.0f), pressed);
        acted = acted || panel.pressActed();
    }
    CHECK(opened == 1);
    CHECK(acted);
}

TEST_CASE("FullPanel: a row that is selected outside the shown rows of the tree is scrolled to")
{
    const test_panel::PanelContext context;
    Names pages;
    for (int i = 1; i <= 20; ++i) {
        pages.push_back("Page " + std::to_string(i));
    }
    // more mods than the tree shows, so it scrolls with every mod closed too
    std::vector<model::Mod> mods;
    for (int i = 1; i <= 14; ++i) {
        mods.push_back(modOf(std::format("Mod {:02}", i), pages));
    }
    FullPanel panel;
    panel.setMods(std::move(mods));
    drawFrames(panel, 2);
    CHECK(treeRowsWindow().Scroll.y == 0.0f);
    CHECK_FALSE(isRowShown(13));

    // the last mod, as from the devbench tool
    panel.select(selectionOf(13));
    drawFrames(panel, 3);
    CHECK(isRowShown(13));

    // the first mod's last page, which is the 21st row
    panel.select(selectionOf(0, 19));
    drawFrames(panel, 3);
    CHECK(isRowShown(20));

    // the first mod closes, and the row of the last one is the 14th again
    panel.select(selectionOf(13));
    drawFrames(panel, 3);
    CHECK(isRowShown(13));

    // a row that is shown already is left where it is
    const float scrolledTo = treeRowsWindow().Scroll.y;
    panel.select(selectionOf(13, 0));
    drawFrames(panel, 3);
    CHECK(treeRowsWindow().Scroll.y == scrolledTo);
}

TEST_CASE("FullPanel: a menu that opens again has the tree at its top")
{
    const test_panel::PanelContext context;
    std::vector<model::Mod> mods;
    for (int i = 1; i <= 14; ++i) {
        mods.push_back(modOf(std::format("Mod {:02}", i)));
    }
    FullPanel panel;
    panel.setMods(std::move(mods));
    panel.select(selectionOf(13));
    drawFrames(panel, 4);
    REQUIRE(treeRowsWindow().Scroll.y > 0.0f);

    panel.reset();
    drawFrames(panel, 3);
    CHECK(treeRowsWindow().Scroll.y == 0.0f);
}

TEST_CASE("FullPanel: the help line has the text and the help of the row the ray is on")
{
    const test_panel::PanelContext context;
    auto panel = modWithHelp();
    drawFrames(panel, 2);
    CHECK(panel.helpLine().text().empty());

    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Enable the feature: Turns the feature on.");
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == "Strength: How strong the feature is.");

    // a row with no help is its text alone, without the spaces that set it in
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Reset everything");
}

TEST_CASE("FullPanel: the help line keeps its row where the page has none, until the ray leaves the page")
{
    const test_panel::PanelContext context;
    auto panel = modWithHelp();
    point(panel, pageRow(0));

    point(panel, pageRow(NO_PAGE_ROW));
    CHECK(panel.helpLine().text() == "Enable the feature: Turns the feature on.");

    // on the tree
    point(panel, treeRow(0));
    CHECK(panel.helpLine().text().empty());

    // and off the panel
    point(panel, pageRow(2));
    REQUIRE_FALSE(panel.helpLine().text().empty());
    point(panel, test_panel::POINTER_AWAY);
    CHECK(panel.helpLine().text().empty());
}

TEST_CASE("FullPanel: the help line does not keep a row of the page that was shown before")
{
    const test_panel::PanelContext context;
    auto panel = modWithHelp();
    point(panel, pageRow(2));
    point(panel, pageRow(NO_PAGE_ROW));
    REQUIRE(panel.helpLine().text() == "Strength: How strong the feature is.");

    // another page is shown with the ray where it was, as from the devbench tool
    panel.select(selectionOf(0, 0));
    point(panel, pageRow(NO_PAGE_ROW));
    CHECK(panel.helpLine().text().empty());

    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Mode: Low or high.");
    panel.reset();
    CHECK(panel.helpLine().text().empty());
}

TEST_CASE("FullPanel: the help line has the help of the quick launch entry the ray is on")
{
    const test_panel::PanelContext context;
    FullPanel panel;
    panel.setQuickLaunch({ { .name = "FRIK", .help = "Opens FRIK's own configuration.", .open = [] {
                            } } });
    drawFrames(panel, 2);

    // the entry's row is under the page's three lines of text and the heading of its section
    point(panel, ImVec2(640.0f, 250.0f));
    CHECK(panel.helpLine().text() == "FRIK: Opens FRIK's own configuration.");
    point(panel, treeRow(0));
    CHECK(panel.helpLine().text().empty());
}
