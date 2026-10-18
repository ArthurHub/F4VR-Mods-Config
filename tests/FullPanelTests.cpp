#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "TestActions.h"
#include "TestEvents.h"
#include "TestFiles.h"
#include "TestGlobals.h"
#include "TestPanel.h"
#include "Theme.h"
#include "mcm/McmStore.h"
#include "package/PackageLoader.h"
#include "package/PackageStore.h"
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

    constexpr auto PLUGIN = "MyMod.esp";

    model::Item globalSetting(const model::SettingType type, std::string text, const std::uint32_t formId)
    {
        return model::Setting{ .type = type,
            .text = std::move(text),
            .help = "What it does.",
            .valueOptions = {},
            .storage = model::GlobalStorage{ .plugin = PLUGIN, .formId = formId } };
    }

    /**
     * Globals for the mod with values: 0x800 is 0 and 0x801 is 1.
     */
    test_globals::TestGlobals someGlobals()
    {
        test_globals::TestGlobals globals;
        globals.values = { { { PLUGIN, 0x800 }, 0.0f }, { { PLUGIN, 0x801 }, 1.0f } };
        return globals;
    }

    /**
     * A panel with one mod, shown on its front page, with its values in a store: two switchers of a global,
     * a slider of one, a switcher of a global that is not in the game, and a switcher that is kept nowhere.
     */
    FullPanel modWithValues(model::Store& store)
    {
        auto mod = modOf("Alpha");
        mod.frontPage.items.push_back(globalSetting(model::SettingType::Switcher, "Off", 0x800));
        mod.frontPage.items.push_back(globalSetting(model::SettingType::Switcher, "On", 0x801));
        mod.frontPage.items.push_back(globalSetting(model::SettingType::Slider, "Strength", 0x801));
        mod.frontPage.items.push_back(globalSetting(model::SettingType::Switcher, "Missing", 0x1234));
        mod.frontPage.items.push_back(model::Setting{ .type = model::SettingType::Switcher, .text = "Unfold", .help = {}, .valueOptions = {}, .storage = {} });

        FullPanel panel;
        panel.setStore(model::Source::Mcm, &store);
        panel.setMods({ std::move(mod), modOf("Beta") });
        panel.select(selectionOf(0));
        return panel;
    }

    /**
     * A panel with one mod, shown on its front page, with a store: a button that calls a function, one with
     * no action, and one with an action the menu does not run.
     */
    FullPanel modWithButtons(model::Store& store)
    {
        auto mod = modOf("Alpha");
        mod.frontPage.items.push_back(model::Button{ .text = "Reset everything",
            .help = "Puts every value back.",
            .action = model::CallFunction{ .plugin = PLUGIN, .formId = 0x800, .script = {}, .function = "ResetAll", .arguments = {} } });
        mod.frontPage.items.push_back(model::Button{ .text = "Nothing", .help = "Nothing runs.", .action = {} });
        mod.frontPage.items.push_back(model::Button{ .text = "Notify",
            .help = "Shows a text.",
            .action = model::UnknownAction{ .type = "CallExternalFunction", .reason = "Not available: the menu has no Flash." } });

        FullPanel panel;
        panel.setStore(model::Source::Mcm, &store);
        panel.setMods({ std::move(mod) });
        panel.select(selectionOf(0));
        return panel;
    }

    /**
     * Globals for the mod with sliders: 0x810 is 20, 0x811 is 1 and 0x812 is 0.
     */
    test_globals::TestGlobals sliderGlobals()
    {
        test_globals::TestGlobals globals;
        globals.values = { { { PLUGIN, 0x810 }, 20.0f }, { { PLUGIN, 0x811 }, 1.0f }, { { PLUGIN, 0x812 }, 0.0f } };
        return globals;
    }

    /**
     * A panel with one mod, shown on its front page, with its values in a store: a slider from 0 to 100 by
     * 5, one from 0 to 2 by 0.1, and a switcher. The mod has a page with nothing.
     */
    FullPanel modWithSliders(model::Store& store)
    {
        const auto slider = [](std::string text, const std::uint32_t formId, const float max, const float step) {
            auto item = globalSetting(model::SettingType::Slider, std::move(text), formId);
            std::get<model::Setting>(item).valueOptions = { .min = 0.0f, .max = max, .step = step, .options = {} };
            return item;
        };
        auto mod = modOf("Alpha", { "Other" });
        mod.frontPage.items.push_back(slider("Percent", 0x810, 100.0f, 5.0f));
        mod.frontPage.items.push_back(slider("Tenths", 0x811, 2.0f, 0.1f));
        mod.frontPage.items.push_back(globalSetting(model::SettingType::Switcher, "Switch", 0x812));

        FullPanel panel;
        panel.setStore(model::Source::Mcm, &store);
        panel.setMods({ std::move(mod) });
        panel.select(selectionOf(0));
        return panel;
    }

    /**
     * A place on the track of the slider of a row of a page, by the row's place among the rows: where the
     * middle of the grab is at a part of the slider's range, from 0 at its least to 1 at its most.
     * The track is as ModPage.cpp has it.
     */
    ImVec2 sliderTrack(const int place, const float part)
    {
        constexpr float TRACK_WIDTH = 280.0f;
        const float rowsLeft = CONTENT_START + theme::NAV_WIDTH + theme::SPACE_4;
        const float rowsRight = test_panel::PANEL_WIDTH - CONTENT_START - theme::SCROLLBAR_SIZE - theme::SPACE_2;
        const float trackRight = rowsRight - theme::CONTROL_HEIGHT - theme::SPACE_2 - theme::VALUE_WIDTH - theme::SPACE_2;
        const float inset = theme::BORDER_FRAME + theme::GRAB_WIDTH * 0.5f;
        REQUIRE(trackRight - TRACK_WIDTH > rowsLeft);
        return { std::lerp(trackRight - TRACK_WIDTH + inset, trackRight - inset, part), pageRow(place).y };
    }

    /**
     * Frames with the thumbstick of the hand that points pushed to a side, and the ray at a place.
     */
    void pushFrames(FullPanel& panel, const ImVec2 at, const float push, const int count)
    {
        panel.setThumbstickSideValue(push);
        for (int i = 0; i < count; ++i) {
            test_panel::drawFrame(panel, at);
        }
    }

    /**
     * A press at a place on the panel. Returns whether it acted in one of its frames.
     */
    bool pressActs(FullPanel& panel, const ImVec2 at)
    {
        bool acted = false;
        for (const bool pressed : { false, false, true, false, false }) {
            test_panel::drawFrame(panel, at, pressed);
            acted = acted || panel.pressActed();
        }
        return acted;
    }

    /**
     * Globals for the mod with choices: 0x820 is 1, 0x821 is 0, 0x822 is 0 and 0x823 is 7.
     */
    test_globals::TestGlobals choiceGlobals()
    {
        test_globals::TestGlobals globals;
        globals.values = { { { PLUGIN, 0x820 }, 1.0f }, { { PLUGIN, 0x821 }, 0.0f }, { { PLUGIN, 0x822 }, 0.0f }, { { PLUGIN, 0x823 }, 7.0f } };
        return globals;
    }

    /**
     * A panel with one mod, shown on its front page, with its values in a store. Its rows are a stepper of
     * three options at its second, a dropdown of three at its first, a switcher, which the open list of the
     * dropdown is over, a dropdown with no options, and a stepper of three whose value is no option.
     * The mod has a page with nothing.
     */
    FullPanel modWithChoices(model::Store& store)
    {
        const auto choice = [](const model::SettingType type, std::string text, const std::uint32_t formId, const Names& options) {
            auto item = globalSetting(type, std::move(text), formId);
            auto& valueOptions = std::get<model::Setting>(item).valueOptions;
            for (const auto& option : options) {
                valueOptions.options.push_back({ .value = static_cast<int>(valueOptions.options.size()), .text = option });
            }
            return item;
        };
        auto mod = modOf("Alpha", { "Other" });
        mod.frontPage.items.push_back(choice(model::SettingType::Stepper, "Quality", 0x820, { "Low", "Medium", "High" }));
        mod.frontPage.items.push_back(choice(model::SettingType::Dropdown, "Place", 0x821, { "Head", "Offhand", "Primary hand" }));
        mod.frontPage.items.push_back(globalSetting(model::SettingType::Switcher, "Switch", 0x822));
        mod.frontPage.items.push_back(choice(model::SettingType::Dropdown, "Empty", 0x821, {}));
        mod.frontPage.items.push_back(choice(model::SettingType::Stepper, "Outside", 0x823, { "Zero", "One", "Two" }));

        FullPanel panel;
        panel.setStore(model::Source::Mcm, &store);
        panel.setMods({ std::move(mod) });
        panel.select(selectionOf(0));
        return panel;
    }

    /**
     * Where the control of a row of a page ends at the right, before the room of its Reset.
     */
    float controlRight()
    {
        return test_panel::PANEL_WIDTH - CONTENT_START - theme::SCROLLBAR_SIZE - theme::SPACE_2 - theme::CONTROL_HEIGHT - theme::SPACE_2;
    }

    /**
     * A panel with one mod, shown on its front page, with its values in a store of MCM's files in a folder:
     * a switcher, a slider from 0 to 2 by 0.1 and a dropdown of three options that are ModSettings, each
     * with a default and another value in the player's file, and a switcher of a global, which has none.
     */
    FullPanel modWithDefaults(model::Store& store, const test_files::TempFolder& mcm)
    {
        const auto modSetting = [](const model::SettingType type, std::string text, std::string name, const model::StoredType valueType) {
            return model::Setting{ .id = name,
                .sourceId = name,
                .type = type,
                .text = std::move(text),
                .help = "What it does.",
                .valueOptions = {},
                .storage = model::ModSettingStorage{ .name = name, .type = valueType } };
        };
        mcm.write("Config/Alpha/settings.ini", "[Main]\nbOn=1\nfStrength=1.0\niMode=1\n");
        mcm.write("Settings/Alpha.ini", "[Main]\nbOn=0\nfStrength=1.300000\niMode=2\n");

        auto strength = modSetting(model::SettingType::Slider, "Strength", "fStrength:Main", model::StoredType::Float);
        strength.valueOptions = { .min = 0.0f, .max = 2.0f, .step = 0.1f, .options = {} };
        auto mode = modSetting(model::SettingType::Dropdown, "Mode", "iMode:Main", model::StoredType::Int);
        mode.help.clear();
        for (const auto* option : { "Low", "Medium", "High" }) {
            mode.valueOptions.options.push_back({ .value = static_cast<int>(mode.valueOptions.options.size()), .text = option });
        }
        auto mod = modOf("Alpha");
        mod.frontPage.items.push_back(modSetting(model::SettingType::Switcher, "Enabled", "bOn:Main", model::StoredType::Bool));
        mod.frontPage.items.push_back(std::move(strength));
        mod.frontPage.items.push_back(std::move(mode));
        mod.frontPage.items.push_back(globalSetting(model::SettingType::Switcher, "Global", 0x800));

        FullPanel panel;
        panel.setStore(model::Source::Mcm, &store);
        panel.setMods({ std::move(mod) });
        panel.select(selectionOf(0));
        return panel;
    }

    /**
     * The middle of the Reset of a row of a page, by the row's place among the rows: in the room after its control.
     */
    ImVec2 resetButton(const int place)
    {
        return { controlRight() + theme::SPACE_2 + theme::CONTROL_HEIGHT * 0.5f, pageRow(place).y };
    }

    /**
     * The middle of a button of the stepper of a row of a page, by the row's place among the rows: the one
     * to the option after, or the one to the option before. The stepper is as Choices.cpp has it.
     */
    ImVec2 stepperButton(const int place, const bool after)
    {
        constexpr float OPTION_WIDTH = 280.0f;
        const float afterMiddle = controlRight() - theme::CONTROL_HEIGHT * 0.5f;
        return { after ? afterMiddle : afterMiddle - theme::CONTROL_HEIGHT - theme::SPACE_2 - OPTION_WIDTH - theme::SPACE_2, pageRow(place).y };
    }

    /**
     * The middle of an option of the open list of the dropdown of a row of a page, by the option's place in
     * the list, while the list is under its dropdown and at its top. The list is as Choices.cpp has it.
     */
    ImVec2 listOption(const int place, const int option)
    {
        const float listTop = pageRow(place).y + theme::CONTROL_HEIGHT * 0.5f;
        return { controlRight() - 100.0f, listTop + theme::SPACE_2 + static_cast<float>(option) * (theme::CONTROL_HEIGHT + theme::SPACE_1) + theme::CONTROL_HEIGHT * 0.5f };
    }

    bool isListOpen()
    {
        return !ImGui::GetCurrentContext()->OpenPopupStack.empty();
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
     * The middle of Save, at the right of the footer, and of Discard before it.
     */
    ImVec2 saveButton()
    {
        constexpr float BUTTON_WIDTH = 160.0f;
        return { test_panel::PANEL_WIDTH - CONTENT_START - theme::SPACE_2 - BUTTON_WIDTH * 0.5f, test_panel::PANEL_HEIGHT - CONTENT_START - theme::FOOTER_HEIGHT * 0.5f };
    }

    ImVec2 discardButton()
    {
        constexpr float BUTTON_WIDTH = 160.0f;
        return { saveButton().x - BUTTON_WIDTH - theme::SPACE_3, saveButton().y };
    }

    /**
     * The width of a breadcrumb, which is its text in the title's size with its padding.
     */
    float crumbWidth(const char* text)
    {
        auto* font = ImGui::GetIO().Fonts->Fonts[0];
        return std::ceil(font->CalcTextSizeA(theme::FONT_SIZE_TITLE, FLT_MAX, 0.0f, text).x) + theme::SPACE_2 * 2.0f;
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

    // the buttons of the title bar, by their place from its right
    constexpr int CLOSE_BUTTON = 0;
    constexpr int MINIMIZE_BUTTON = 1;
    constexpr int HELP_BUTTON = 2;

    /**
     * The middle of a button of the title bar, by its place from the right of the bar.
     */
    ImVec2 titleButton(const int place)
    {
        const float right = test_panel::PANEL_WIDTH - CONTENT_START - theme::SPACE_2;
        return { right - theme::CONTROL_HEIGHT * 0.5f - static_cast<float>(place) * (theme::CONTROL_HEIGHT + theme::SPACE_2), CONTENT_START + theme::TITLEBAR_HEIGHT * 0.5f };
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

TEST_CASE("FullPanel: a mod that is not available is a row of the tree, which a press does not select, and says why")
{
    const test_panel::PanelContext context;
    auto missing = modOf("Beta");
    missing.notAvailable = "its plugin Beta.esp is not loaded";
    FullPanel panel;
    panel.setMods({ modOf("Gamma"), std::move(missing), modOf("Alpha", { "First" }) });
    drawFrames(panel, 2);

    // in its place by name
    CHECK(namesOf(panel) == Names{ "Alpha", "Beta", "Gamma" });
    press(panel, treeRow(1));
    CHECK(panel.selection() == Selection{});
    CHECK(panel.helpLine().text() == "Beta: Not available: its plugin Beta.esp is not loaded.");

    // the help leaves with the ray, as the tree's rows have no help of their own
    point(panel, treeRow(2));
    CHECK(panel.helpLine().text().empty());

    // nor is it selected from outside the tree, as the devbench tool selects
    CHECK_FALSE(panel.select(selectionOf(1)));
    CHECK(panel.selection() == Selection{});
    panel.select(selectionOf(0));
    panel.select(selectionOf(1));
    CHECK(panel.selection() == Selection{});
}

TEST_CASE("FullPanel: the help line has the help of a mod and of a page whose row the ray is on in the tree")
{
    const test_panel::PanelContext context;
    auto mod = modOf("Alpha", { "First", "Second" });
    mod.help = "What Alpha is.";
    mod.pages[0].help = "What its first page is.";
    FullPanel panel;
    panel.setMods({ std::move(mod), modOf("Beta") });
    drawFrames(panel, 2);

    point(panel, treeRow(0));
    CHECK(panel.helpLine().text() == "Alpha: What Alpha is.");
    // a mod with no help has none, as an MCM mod
    point(panel, treeRow(1));
    CHECK(panel.helpLine().text().empty());

    panel.select(selectionOf(0));
    point(panel, treeRow(1));
    CHECK(panel.helpLine().text() == "First: What its first page is.");
    point(panel, treeRow(2));
    CHECK(panel.helpLine().text().empty());
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

TEST_CASE("FullPanel: Help shows the Help page over the selection, and Back shows the selection's page again")
{
    const test_panel::PanelContext context;
    auto panel = threeMods();
    panel.select(selectionOf(0, 1));
    drawFrames(panel, 2);

    CHECK(pressActs(panel, titleButton(HELP_BUTTON)));
    CHECK(panel.isHelpShown());
    CHECK(panel.selection() == selectionOf(0, 1));

    // the Help page's crumb is where the player is, and Help is what is shown: neither changes anything
    CHECK_FALSE(pressActs(panel, modCrumb()));
    CHECK_FALSE(pressActs(panel, titleButton(HELP_BUTTON)));
    CHECK(panel.isHelpShown());

    CHECK(panel.back());
    CHECK_FALSE(panel.isHelpShown());
    CHECK(panel.selection() == selectionOf(0, 1));

    // over the menu's front page too, where Back closes the menu once the Help page is left
    panel.select({});
    press(panel, titleButton(HELP_BUTTON));
    CHECK(panel.back());
    CHECK_FALSE(panel.isHelpShown());
    CHECK_FALSE(panel.back());
}

TEST_CASE("FullPanel: a row of the tree and the menu's breadcrumb leave the Help page")
{
    const test_panel::PanelContext context;
    auto panel = threeMods();
    panel.select(selectionOf(0, 1));
    drawFrames(panel, 2);

    // the row of the page that is selected shows it again
    press(panel, titleButton(HELP_BUTTON));
    CHECK(pressActs(panel, treeRow(2)));
    CHECK_FALSE(panel.isHelpShown());
    CHECK(panel.selection() == selectionOf(0, 1));

    press(panel, titleButton(HELP_BUTTON));
    press(panel, treeRow(1));
    CHECK_FALSE(panel.isHelpShown());
    CHECK(panel.selection() == selectionOf(0, 0));

    press(panel, titleButton(HELP_BUTTON));
    press(panel, menuCrumb());
    CHECK_FALSE(panel.isHelpShown());
    CHECK(panel.selection() == Selection{});

    // and so does a menu that opens again
    press(panel, titleButton(HELP_BUTTON));
    panel.reset();
    CHECK_FALSE(panel.isHelpShown());
}

TEST_CASE("FullPanel: Minimize and Close of the title bar do what the menu does, and act")
{
    const test_panel::PanelContext context;
    auto panel = threeMods();
    int minimized = 0;
    int closed = 0;
    panel.setMenuActions({ .minimize =
                               [&minimized] {
                                   ++minimized;
                               },
        .close =
            [&closed] {
                ++closed;
            } });
    drawFrames(panel, 2);

    CHECK(pressActs(panel, titleButton(MINIMIZE_BUTTON)));
    CHECK(minimized == 1);
    CHECK(closed == 0);
    CHECK(pressActs(panel, titleButton(CLOSE_BUTTON)));
    CHECK(minimized == 1);
    CHECK(closed == 1);

    // with no menu, as here, they do nothing
    panel.setMenuActions({});
    CHECK_FALSE(pressActs(panel, titleButton(CLOSE_BUTTON)));
}

TEST_CASE("FullPanel: the help line has the help of the title bar's button the ray is on")
{
    const test_panel::PanelContext context;
    auto panel = threeMods();
    drawFrames(panel, 2);

    point(panel, titleButton(HELP_BUTTON));
    CHECK(panel.helpLine().text() == "Help: What each control of the menu does, with the buttons your config has for it.");
    point(panel, titleButton(MINIMIZE_BUTTON));
    CHECK(panel.helpLine().text() == "Minimize: Hides the menu, as you left it. Resume on your offhand wrist shows it again.");
    point(panel, titleButton(CLOSE_BUTTON));
    CHECK(panel.helpLine().text() == "Close: Closes the menu.");
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

    // with no store a row of the page changes nothing, and neither does a press where the page has no row
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
        test_panel::drawFrame(panel, ImVec2(640.0f, 215.0f), pressed);
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

    // the entry's row is under the page's two lines of text and the heading of its section
    point(panel, ImVec2(640.0f, 215.0f));
    CHECK(panel.helpLine().text() == "FRIK: Opens FRIK's own configuration.");
    point(panel, treeRow(0));
    CHECK(panel.helpLine().text().empty());
}

TEST_CASE("FullPanel: a press on the row of a switcher turns it on or off in its store, and acts")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    mcm::McmStore store;
    auto panel = modWithValues(store);
    drawFrames(panel, 2);

    CHECK(pressActs(panel, pageRow(0)));
    CHECK(globals.values.at({ PLUGIN, 0x800 }) == 1.0f);
    CHECK(pressActs(panel, pageRow(0)));
    CHECK(globals.values.at({ PLUGIN, 0x800 }) == 0.0f);

    CHECK(pressActs(panel, pageRow(1)));
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 0.0f);

    // the page is the same one
    CHECK(panel.selection() == selectionOf(0));
}

TEST_CASE("FullPanel: a press on the row of a button runs its action in its store, and acts")
{
    const test_panel::PanelContext context;
    test_actions::TestActions actions;
    mcm::McmStore store;
    auto panel = modWithButtons(store);
    drawFrames(panel, 2);

    CHECK(pressActs(panel, pageRow(0)));
    REQUIRE(actions.called.size() == 1);
    CHECK(actions.called[0].function == "ResetAll");

    // a button with no action, and one with an action the menu does not run
    CHECK_FALSE(pressActs(panel, pageRow(1)));
    CHECK_FALSE(pressActs(panel, pageRow(2)));
    CHECK(actions.called.size() == 1);

    // a function of a form that is not in the game
    actions.inGame = false;
    CHECK_FALSE(pressActs(panel, pageRow(0)));
    CHECK(actions.called.size() == 1);
}

TEST_CASE("FullPanel: the help line says why a button is not run")
{
    const test_panel::PanelContext context;
    test_actions::TestActions actions;
    mcm::McmStore store;
    auto panel = modWithButtons(store);
    drawFrames(panel, 2);

    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Reset everything: Puts every value back.");
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Nothing: Nothing runs.");
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == "Notify: Not available: the menu has no Flash.");
}

TEST_CASE("FullPanel: the help line says that a hotkey is read-only, and a press on its row does nothing")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder files;
    files.write("Config/Alpha/keybinds.json", R"({ "modName": "Alpha", "keybinds": [
        { "id": "open", "desc": "Open", "action": { "type": "RunConsoleCommand", "command": "tgm" } },
        { "id": "close", "desc": "Close", "action": { "type": "RunConsoleCommand", "command": "tgm" } }
    ] })");
    mcm::McmStore store(files.path);
    auto mod = modOf("Alpha");
    mod.frontPage.items.push_back(model::Hotkey{ .id = "open", .text = "Open the menu", .help = "Opens it." });
    mod.frontPage.items.push_back(model::Hotkey{ .id = "close", .text = "Close the menu" });
    mod.frontPage.items.push_back(model::Hotkey{ .id = "missing", .text = "Not in the file", .help = "Does nothing." });
    FullPanel panel;
    panel.setStore(model::Source::Mcm, &store);
    panel.setMods({ std::move(mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Open the menu: (Read-only) Opens it.");
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Close the menu: (Read-only)");
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == "Not in the file: Not available: the hotkey missing is not in the keybinds.json of the mod.");

    CHECK_FALSE(pressActs(panel, pageRow(0)));
}

namespace
{
    /**
     * A panel with one mod, shown on its front page, with text inputs: a text that is a ModSetting with a
     * default and another value in the player's file, one that is kept nowhere, one of a global that is not
     * in the game, a number and a whole number that are ModSettings. The mod has a page with none.
     */
    FullPanel modWithTextInputs(model::Store& store, const test_files::TempFolder& mcm)
    {
        using Type = model::StoredType;
        const auto textInput = [](std::string text, std::string help, model::Storage storage, const Type valueType = Type::String) {
            // MCM's event names it by its id
            return model::Setting{ .sourceId = text,
                .type = model::SettingType::TextInput,
                .text = std::move(text),
                .help = std::move(help),
                .valueOptions = {},
                .storage = std::move(storage),
                .valueType = valueType };
        };
        mcm.write("Config/Alpha/settings.ini", "[Main]\nsName=Sanctuary Hills\nfStrength=1.0\niCount=5\n");
        mcm.write("Settings/Alpha.ini", "[Main]\nsName=Diamond City\nfStrength=3.1400\n");
        auto mod = modOf("Alpha", { "Other" });
        mod.frontPage.items.push_back(textInput("Name", "The name of the settlement.", model::ModSettingStorage{ .name = "sName:Main", .type = Type::String }));
        mod.frontPage.items.push_back(textInput("Note", {}, model::NoStorage{}));
        mod.frontPage.items.push_back(textInput("Missing", "Is not in the game.", model::GlobalStorage{ .plugin = PLUGIN, .formId = 0x1234 }));
        mod.frontPage.items.push_back(textInput("Strength", {}, model::ModSettingStorage{ .name = "fStrength:Main", .type = Type::Float }, Type::Float));
        mod.frontPage.items.push_back(textInput("Count", {}, model::ModSettingStorage{ .name = "iCount:Main", .type = Type::Int }, Type::Int));

        FullPanel panel;
        panel.setStore(model::Source::Mcm, &store);
        panel.setMods({ std::move(mod) });
        panel.select(selectionOf(0));
        return panel;
    }

    /**
     * Characters typed into the field that is edited, with the ray on no row, as the keys of the window and
     * SteamVR's keyboard type them.
     */
    void typeText(FullPanel& panel, const char* text)
    {
        ImGui::GetIO().AddInputCharactersUTF8(text);
        test_panel::drawFrame(panel, test_panel::POINTER_AWAY);
    }

    /**
     * A key pressed in one frame and let go of in the next, as the framework sends one.
     */
    void pressKey(FullPanel& panel, const ImGuiKey key)
    {
        for (const bool down : { true, false }) {
            ImGui::GetIO().AddKeyEvent(key, down);
            test_panel::drawFrame(panel, test_panel::POINTER_AWAY);
        }
    }

    /**
     * The text of a text input of the mod with text inputs, as the store has it.
     */
    model::Value textOf(model::Store& store, const FullPanel& panel, const std::size_t row)
    {
        return store.value(std::get<model::Setting>(panel.mods()[0].frontPage.items[row])).value;
    }
}

TEST_CASE("FullPanel: a press on the row of a text input edits it, and Enter writes its text, and acts")
{
    const test_panel::PanelContext context;
    test_events::TestEvents events;
    const test_globals::TestGlobals globals;
    const test_files::TempFolder mcm;
    mcm::McmStore store(mcm.path);
    auto panel = modWithTextInputs(store, mcm);
    drawFrames(panel, 2);
    events.sent.clear();

    CHECK(pressActs(panel, pageRow(0)));
    CHECK(ImGui::GetActiveID() != 0);

    // the caret is at the end of the text, and nothing is written while it is typed
    typeText(panel, " East");
    CHECK(textOf(store, panel, 0) == model::Value("Diamond City"s));
    pressKey(panel, ImGuiKey_Enter);
    CHECK(ImGui::GetActiveID() == 0);
    CHECK(textOf(store, panel, 0) == model::Value("Diamond City East"s));
    CHECK(events.sent.size() == 2);
}

TEST_CASE("FullPanel: a text input that is left by the keyboard's Done writes its text, and one that is cancelled writes nothing")
{
    const test_panel::PanelContext context;
    const test_globals::TestGlobals globals;
    const test_files::TempFolder mcm;
    mcm::McmStore store(mcm.path);
    auto panel = modWithTextInputs(store, mcm);
    drawFrames(panel, 2);

    // the framework leaves the field before the frame, as SteamVR's keyboard is done
    pressActs(panel, pageRow(0));
    typeText(panel, "!");
    ImGui::ClearActiveID();
    drawFrames(panel, 2);
    CHECK(textOf(store, panel, 0) == model::Value("Diamond City!"s));

    // the keyboard's close button is Escape
    pressActs(panel, pageRow(0));
    typeText(panel, "?");
    pressKey(panel, ImGuiKey_Escape);
    CHECK(ImGui::GetActiveID() == 0);
    CHECK(textOf(store, panel, 0) == model::Value("Diamond City!"s));

    // a text that is the one it started with is not written again
    test_events::TestEvents events;
    pressActs(panel, pageRow(0));
    pressKey(panel, ImGuiKey_Enter);
    CHECK(events.sent.empty());
}

TEST_CASE("FullPanel: Back ends the edit of a text input with nothing written, and a press beside it writes its text")
{
    const test_panel::PanelContext context;
    const test_globals::TestGlobals globals;
    const test_files::TempFolder mcm;
    mcm::McmStore store(mcm.path);
    auto panel = modWithTextInputs(store, mcm);
    drawFrames(panel, 2);

    pressActs(panel, pageRow(0));
    typeText(panel, "!");
    CHECK(panel.back());
    drawFrames(panel, 2);
    CHECK(ImGui::GetActiveID() == 0);
    CHECK(textOf(store, panel, 0) == model::Value("Diamond City"s));
    // the step after it is the one of the pages
    CHECK(panel.back());
    CHECK_FALSE(panel.back());

    panel.select(selectionOf(0));
    drawFrames(panel, 2);
    pressActs(panel, pageRow(1));
    typeText(panel, "A note");
    press(panel, pageRow(NO_PAGE_ROW));
    CHECK(textOf(store, panel, 1) == model::Value("A note"s));
}

TEST_CASE("FullPanel: the edit of a text input ends with nothing written as another page is shown")
{
    const test_panel::PanelContext context;
    const test_globals::TestGlobals globals;
    const test_files::TempFolder mcm;
    mcm::McmStore store(mcm.path);
    auto panel = modWithTextInputs(store, mcm);
    drawFrames(panel, 2);

    pressActs(panel, pageRow(0));
    typeText(panel, "!");
    panel.select(selectionOf(0, 0));
    drawFrames(panel, 2);
    CHECK(ImGui::GetActiveID() == 0);
    panel.select(selectionOf(0));
    drawFrames(panel, 2);
    CHECK(textOf(store, panel, 0) == model::Value("Diamond City"s));
}

TEST_CASE("FullPanel: a text input that takes a number keeps only the characters of one, and writes only a number")
{
    const test_panel::PanelContext context;
    const test_globals::TestGlobals globals;
    const test_files::TempFolder mcm;
    mcm::McmStore store(mcm.path);
    auto panel = modWithTextInputs(store, mcm);
    drawFrames(panel, 2);
    CHECK(textOf(store, panel, 3) == model::Value("3.14"s));

    pressActs(panel, pageRow(3));
    typeText(panel, "x5");
    pressKey(panel, ImGuiKey_Enter);
    CHECK(textOf(store, panel, 3) == model::Value("3.145"s));

    // a whole number has no point, and a minus in its middle is no number
    pressActs(panel, pageRow(4));
    typeText(panel, ".2");
    pressKey(panel, ImGuiKey_Enter);
    CHECK(textOf(store, panel, 4) == model::Value("52"s));
    pressActs(panel, pageRow(4));
    typeText(panel, "-1");
    pressKey(panel, ImGuiKey_Enter);
    CHECK(textOf(store, panel, 4) == model::Value("52"s));
}

TEST_CASE("FullPanel: a text input has its Reset, and the help line names its default in quotes")
{
    const test_panel::PanelContext context;
    const test_globals::TestGlobals globals;
    const test_files::TempFolder mcm;
    mcm::McmStore store(mcm.path);
    auto panel = modWithTextInputs(store, mcm);
    drawFrames(panel, 2);

    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Name: The name of the settlement. (Default: \"Sanctuary Hills\")");
    point(panel, resetButton(0));
    CHECK(panel.helpLine().text() == "Name: Reset to \"Sanctuary Hills\"");
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Note");
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == "Missing: Not available: the global 1234 of MyMod.esp is not in the game.");
    point(panel, pageRow(3));
    CHECK(panel.helpLine().text() == "Strength: (Default: 1)");

    // the field is not edited by it
    CHECK(pressActs(panel, resetButton(0)));
    CHECK(ImGui::GetActiveID() == 0);
    CHECK(textOf(store, panel, 0) == model::Value("Sanctuary Hills"s));

    // a text input whose value is not available is not edited
    CHECK_FALSE(pressActs(panel, pageRow(2)));
}

TEST_CASE("FullPanel: the help line says that the menu does not have a control, and a press on its row does nothing")
{
    const test_panel::PanelContext context;
    auto mod = modOf("Alpha");
    mod.frontPage.items.push_back(model::Placeholder{ .text = "Tint", .help = "The color of the light.", .type = "color" });
    mod.frontPage.items.push_back(model::Placeholder{ .text = "No type", .help = {}, .type = {} });
    FullPanel panel;
    panel.setMods({ std::move(mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Tint: Not available: the menu has no color control.");
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "No type: Not available: the row has no type of control.");

    CHECK_FALSE(pressActs(panel, pageRow(0)));
}

TEST_CASE("FullPanel: a press on a row that is not edited changes nothing")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    mcm::McmStore store;
    auto panel = modWithValues(store);
    drawFrames(panel, 2);
    const auto before = globals.values;

    // a slider with no range, and a switcher whose global is not in the game
    CHECK_FALSE(pressActs(panel, pageRow(2)));
    CHECK_FALSE(pressActs(panel, pageRow(3)));
    CHECK(globals.values == before);
}

TEST_CASE("FullPanel: a switcher that is kept nowhere is held until another mod is selected")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    mcm::McmStore store;
    auto panel = modWithValues(store);
    drawFrames(panel, 2);
    const auto& unfold = std::get<model::Setting>(panel.mods()[0].frontPage.items[4]);

    CHECK(pressActs(panel, pageRow(4)));
    CHECK(store.value(unfold).value == model::Value(true));

    // the menu's front page and back is the mod's session again, and so is another mod
    panel.select({});
    panel.select(selectionOf(0));
    CHECK(store.value(unfold).value == model::Value(false));
}

TEST_CASE("FullPanel: another page of the mod that is shown reads its ModSettings from MCM's files again")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder mcm;
    mcm.write("Config/Alpha/settings.ini", "[Main]\nbOn=0\n");
    mcm::McmStore store(mcm.path);
    auto mod = modOf("Alpha", { "First" });
    mod.frontPage.items.push_back(model::Setting{ .id = "bOn:Main",
        .type = model::SettingType::Switcher,
        .text = "Enabled",
        .help = {},
        .valueOptions = {},
        .storage = model::ModSettingStorage{ .name = "bOn:Main", .type = model::StoredType::Bool } });
    FullPanel panel;
    panel.setStore(model::Source::Mcm, &store);
    panel.setMods({ std::move(mod) });
    panel.select(selectionOf(0));
    const auto& on = std::get<model::Setting>(panel.mods()[0].frontPage.items[0]);
    REQUIRE(store.value(on).value == model::Value(false));

    // MCM writes the player's file as a script sets the value, which shows on the page after
    mcm.write("Settings/Alpha.ini", "[Main]\nbOn=1\n");
    panel.select(selectionOf(0));
    CHECK(store.value(on).value == model::Value(false));
    panel.select(selectionOf(0, 0));
    CHECK(store.value(on).value == model::Value(true));
    mcm.write("Settings/Alpha.ini", "[Main]\nbOn=0\n");
    panel.select(selectionOf(0));
    CHECK(store.value(on).value == model::Value(false));
}

TEST_CASE("FullPanel: a row of a group is shown while the switcher that controls the group is on, and has no room while it is off")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    mcm::McmStore store;

    // a switcher that is kept nowhere, a row that is shown while it is on, and a row after them
    auto mod = modOf("Alpha");
    mod.frontPage.items.push_back(model::Setting{ .id = "unfold", .type = model::SettingType::Switcher, .text = "Unfold", .help = {}, .valueOptions = {}, .storage = {} });
    mod.frontPage.items.push_back(globalSetting(model::SettingType::Switcher, "In the group", 0x800));
    std::get<model::Setting>(mod.frontPage.items.back()).visibleWhen = model::Condition{ .kind = model::Condition::Kind::On, .setting = "unfold", .conditions = {} };
    mod.frontPage.items.push_back(globalSetting(model::SettingType::Switcher, "After", 0x801));
    FullPanel panel;
    panel.setStore(model::Source::Mcm, &store);
    panel.setMods({ std::move(mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    // the row after the group is under the switcher
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "After: What it does.");
    CHECK(pressActs(panel, pageRow(1)));
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 0.0f);
    CHECK(globals.values.at({ PLUGIN, 0x800 }) == 0.0f);

    CHECK(pressActs(panel, pageRow(0)));
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "In the group: What it does.");
    CHECK(pressActs(panel, pageRow(1)));
    CHECK(globals.values.at({ PLUGIN, 0x800 }) == 1.0f);
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == "After: What it does.");

    // and is hidden again
    CHECK(pressActs(panel, pageRow(0)));
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "After: What it does.");
}

TEST_CASE("FullPanel: a row is edited while its enabledWhen holds, and is disabled while it does not")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    mcm::McmStore store;

    // a switcher that is kept nowhere, and a row that is enabled while it is on
    auto mod = modOf("Alpha");
    mod.frontPage.items.push_back(model::Setting{ .id = "enable", .type = model::SettingType::Switcher, .text = "Enable", .help = {}, .valueOptions = {}, .storage = {} });
    mod.frontPage.items.push_back(globalSetting(model::SettingType::Switcher, "Enabled by it", 0x800));
    std::get<model::Setting>(mod.frontPage.items.back()).enabledWhen = model::Condition{ .kind = model::Condition::Kind::On, .setting = "enable", .conditions = {} };
    FullPanel panel;
    panel.setStore(model::Source::Mcm, &store);
    panel.setMods({ std::move(mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    // it is shown, and its help is in the help line, but a press on it does nothing
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Enabled by it: What it does.");
    CHECK_FALSE(pressActs(panel, pageRow(1)));
    CHECK(globals.values.at({ PLUGIN, 0x800 }) == 0.0f);

    CHECK(pressActs(panel, pageRow(0)));
    CHECK(pressActs(panel, pageRow(1)));
    CHECK(globals.values.at({ PLUGIN, 0x800 }) == 1.0f);
}

TEST_CASE("FullPanel: a package's row whose condition on a plugin does not hold is hidden, with no store")
{
    const test_panel::PanelContext context;

    // a condition on a plugin that is not loaded is an Any of none, which does not hold
    auto mod = modOf("Alpha");
    mod.source = model::Source::Package;
    mod.frontPage.items.push_back(globalSetting(model::SettingType::Switcher, "Needs a plugin", 0x800));
    std::get<model::Setting>(mod.frontPage.items.back()).visibleWhen = model::Condition{ .kind = model::Condition::Kind::Any, .setting = {}, .conditions = {} };
    mod.frontPage.items.push_back(globalSetting(model::SettingType::Switcher, "After", 0x801));
    std::get<model::Setting>(mod.frontPage.items.back()).visibleWhen = model::Condition{ .kind = model::Condition::Kind::On, .setting = "missing", .conditions = {} };
    FullPanel panel;
    panel.setMods({ std::move(mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    // a condition on a value is open with no value, so its row is shown
    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "After: What it does.");
}

TEST_CASE("FullPanel: the help line says that changes are saved as they are made, while a mod is shown")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    mcm::McmStore store;
    auto panel = modWithValues(store);
    drawFrames(panel, 2);
    CHECK(panel.helpLine().status() == "Changes are saved as they are made");

    // it stays the status under the help of a row, for when the ray leaves the rows
    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Off: What it does.");
    CHECK(panel.helpLine().status() == "Changes are saved as they are made");

    // the menu's front page has no session
    panel.select({});
    CHECK(panel.helpLine().status().empty());
    panel.select(selectionOf(1));
    CHECK_FALSE(panel.helpLine().status().empty());
    panel.reset();
    CHECK(panel.helpLine().status().empty());
}

TEST_CASE("FullPanel: the help line says why the value of the row the ray is on is not available")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    mcm::McmStore store;
    auto panel = modWithValues(store);
    drawFrames(panel, 2);

    point(panel, pageRow(3));
    CHECK(panel.helpLine().text() == "Missing: Not available: the global 1234 of MyMod.esp is not in the game.");
}

TEST_CASE("FullPanel: a press on the track of a slider puts its grab there, on a step, and acts")
{
    const test_panel::PanelContext context;
    auto globals = sliderGlobals();
    mcm::McmStore store;
    auto panel = modWithSliders(store);
    drawFrames(panel, 2);

    CHECK(pressActs(panel, sliderTrack(0, 0.5f)));
    CHECK(globals.values.at({ PLUGIN, 0x810 }) == 50.0f);

    // 83 of 100 is nearest to the step at 85
    CHECK(pressActs(panel, sliderTrack(0, 0.83f)));
    CHECK(globals.values.at({ PLUGIN, 0x810 }) == 85.0f);

    CHECK(pressActs(panel, sliderTrack(1, 0.26f)));
    CHECK(globals.values.at({ PLUGIN, 0x811 }) == Catch::Approx(0.5f));
    CHECK(globals.values.at({ PLUGIN, 0x810 }) == 85.0f);
}

TEST_CASE("FullPanel: a slider that is dragged is written as the trigger is let go, also beside its track")
{
    const test_panel::PanelContext context;
    auto globals = sliderGlobals();
    mcm::McmStore store;
    auto panel = modWithSliders(store);
    drawFrames(panel, 2);
    const auto& percent = globals.values.at({ PLUGIN, 0x810 });

    int acted = 0;
    const auto frame = [&panel, &acted](const ImVec2 at, const bool held) {
        test_panel::drawFrame(panel, at, held);
        acted += panel.pressActed() ? 1 : 0;
    };
    frame(sliderTrack(0, 0.6f), false);
    frame(sliderTrack(0, 0.6f), false);
    frame(sliderTrack(0, 0.6f), true);
    frame(sliderTrack(0, 0.8f), true);

    // nothing is written while the trigger is held
    CHECK(percent == 20.0f);
    frame(sliderTrack(0, 0.8f), false);
    CHECK(percent == 80.0f);

    // past the end of the track, and on the row under it
    frame(sliderTrack(0, 0.6f), true);
    frame(ImVec2(sliderTrack(0, 1.0f).x + 200.0f, pageRow(1).y), true);
    frame(ImVec2(sliderTrack(0, 1.0f).x + 200.0f, pageRow(1).y), false);
    CHECK(percent == 100.0f);
    frame(sliderTrack(0, 0.6f), true);
    frame(ImVec2(sliderTrack(0, 0.0f).x - 200.0f, pageRow(1).y), true);
    frame(sliderTrack(0, 0.3f), true);
    CHECK(percent == 100.0f);

    // let go: the ray moves nothing, and the slider of the row it was on is not changed
    frame(sliderTrack(0, 0.3f), false);
    CHECK(percent == 30.0f);
    frame(sliderTrack(0, 0.9f), false);
    frame(sliderTrack(1, 0.9f), false);
    CHECK(percent == 30.0f);
    CHECK(globals.values.at({ PLUGIN, 0x811 }) == 1.0f);

    // the hand gets a pulse as it takes hold, and none for the steps of a drag
    CHECK(acted == 3);
}

TEST_CASE("FullPanel: a pull on the grab of a slider does not move it")
{
    const test_panel::PanelContext context;
    auto globals = sliderGlobals();
    mcm::McmStore store;
    auto panel = modWithSliders(store);
    drawFrames(panel, 2);
    const auto& percent = globals.values.at({ PLUGIN, 0x810 });

    // the grab is at 20 of 100, and the ray on its right part: 8 pixels are 3 of 100, nearest to the step at 25
    const auto onGrab = ImVec2(sliderTrack(0, 0.2f).x + 8.0f, pageRow(0).y);
    for (const bool held : { false, false, true, true }) {
        test_panel::drawFrame(panel, onGrab, held);
    }
    test_panel::drawFrame(panel, onGrab, false);
    CHECK(percent == 20.0f);

    // the grab then moves as far as the ray does
    for (const bool held : { false, true, true }) {
        test_panel::drawFrame(panel, onGrab, held);
    }
    const auto moved = ImVec2(sliderTrack(0, 0.5f).x + 8.0f, pageRow(0).y);
    test_panel::drawFrame(panel, moved, true);
    test_panel::drawFrame(panel, moved, false);
    CHECK(percent == 50.0f);
}

TEST_CASE("FullPanel: Back ends the drag of a slider with nothing written, and the rest of the press changes nothing")
{
    const test_panel::PanelContext context;
    auto globals = sliderGlobals();
    mcm::McmStore store;
    auto panel = modWithSliders(store);
    drawFrames(panel, 2);
    const auto& percent = globals.values.at({ PLUGIN, 0x810 });

    for (const bool held : { false, false, true }) {
        test_panel::drawFrame(panel, sliderTrack(0, 0.6f), held);
    }
    test_panel::drawFrame(panel, sliderTrack(0, 0.8f), true);

    CHECK(panel.back());
    CHECK(panel.selection() == selectionOf(0));
    test_panel::drawFrame(panel, sliderTrack(0, 0.9f), true);
    CHECK(percent == 20.0f);

    // Back again, with the trigger still held, is a step back in the panel
    CHECK(panel.back());
    CHECK(panel.selection() == Selection{});
    panel.select(selectionOf(0));
    test_panel::drawFrame(panel, sliderTrack(0, 0.9f), false);

    // and the slider is dragged again
    CHECK(pressActs(panel, sliderTrack(0, 0.4f)));
    CHECK(percent == 40.0f);
}

TEST_CASE("FullPanel: the thumbstick steps the slider the ray is on, and again while it is held")
{
    const test_panel::PanelContext context;
    auto globals = sliderGlobals();
    mcm::McmStore store;
    auto panel = modWithSliders(store);
    drawFrames(panel, 2);
    const auto& percent = globals.values.at({ PLUGIN, 0x810 });
    const auto& tenths = globals.values.at({ PLUGIN, 0x811 });

    // the ray is on no slider
    pushFrames(panel, test_panel::POINTER_AWAY, 1.0f, 2);
    pushFrames(panel, pageRow(2), 1.0f, 60);
    pushFrames(panel, pageRow(2), 0.0f, 1);
    CHECK(percent == 20.0f);
    CHECK(tenths == 1.0f);

    // on the name of a slider, for longer than the 0.3 seconds it waits, at 90 frames a second: one step
    // as the push begins, and none for a push that is not far
    pushFrames(panel, pageRow(1), 0.0f, 40);
    pushFrames(panel, pageRow(1), 1.0f, 1);
    CHECK(tenths == Catch::Approx(1.1f));
    // the hand gets a pulse for a step
    CHECK(panel.pressActed());
    pushFrames(panel, pageRow(1), 0.0f, 1);
    CHECK_FALSE(panel.pressActed());
    pushFrames(panel, pageRow(1), 0.4f, 60);
    pushFrames(panel, pageRow(1), 0.0f, 1);
    pushFrames(panel, pageRow(1), -1.0f, 1);
    CHECK(tenths == Catch::Approx(1.0f));
    CHECK(percent == 20.0f);

    // a push that is held steps again after 0.4 seconds
    pushFrames(panel, pageRow(1), 0.0f, 1);
    pushFrames(panel, pageRow(1), 1.0f, 30);
    CHECK(tenths == Catch::Approx(1.1f));
    pushFrames(panel, pageRow(1), 1.0f, 10);
    CHECK(tenths == Catch::Approx(1.2f));

    // and stops at the end of the range
    pushFrames(panel, pageRow(1), 1.0f, 450);
    CHECK(tenths == Catch::Approx(2.0f));

    // where the push changes nothing, and the hand gets no pulse
    CHECK_FALSE(panel.pressActed());
}

TEST_CASE("FullPanel: the thumbstick steps a slider only once the ray is on its row for a short time")
{
    const test_panel::PanelContext context;
    auto globals = sliderGlobals();
    mcm::McmStore store;
    auto panel = modWithSliders(store);
    drawFrames(panel, 2);
    const auto& percent = globals.values.at({ PLUGIN, 0x810 });
    const auto& tenths = globals.values.at({ PLUGIN, 0x811 });

    // a push as the ray comes to the row steps nothing at first, and one step once the 0.3 seconds are over
    pushFrames(panel, pageRow(1), 0.0f, 3);
    pushFrames(panel, pageRow(1), 1.0f, 10);
    CHECK(tenths == 1.0f);
    pushFrames(panel, pageRow(1), 1.0f, 30);
    CHECK(tenths == Catch::Approx(1.1f));
    pushFrames(panel, pageRow(1), 0.0f, 1);

    // a push that began off the sliders steps nothing on one, however long it is held
    pushFrames(panel, test_panel::POINTER_AWAY, 1.0f, 3);
    pushFrames(panel, pageRow(0), 1.0f, 180);
    CHECK(percent == 20.0f);

    // until the thumbstick is let go and pushed again
    pushFrames(panel, pageRow(0), 0.0f, 1);
    pushFrames(panel, pageRow(0), 1.0f, 1);
    CHECK(percent == 25.0f);

    // and neither does a push that is held as the ray goes from one slider to another
    pushFrames(panel, pageRow(1), 1.0f, 180);
    CHECK(percent == 25.0f);
    CHECK(tenths == Catch::Approx(1.1f));
}

TEST_CASE("FullPanel: a press on the name of a slider changes nothing")
{
    const test_panel::PanelContext context;
    auto globals = sliderGlobals();
    mcm::McmStore store;
    auto panel = modWithSliders(store);
    drawFrames(panel, 2);

    CHECK_FALSE(pressActs(panel, pageRow(0)));
    CHECK(globals.values.at({ PLUGIN, 0x810 }) == 20.0f);
}

TEST_CASE("FullPanel: a button of a stepper goes to the option before or after, and stops at the first and the last")
{
    const test_panel::PanelContext context;
    auto globals = choiceGlobals();
    mcm::McmStore store;
    auto panel = modWithChoices(store);
    drawFrames(panel, 2);
    const auto& quality = globals.values.at({ PLUGIN, 0x820 });

    CHECK(pressActs(panel, stepperButton(0, true)));
    CHECK(quality == 2.0f);
    CHECK_FALSE(pressActs(panel, stepperButton(0, true)));
    CHECK(quality == 2.0f);

    CHECK(pressActs(panel, stepperButton(0, false)));
    CHECK(pressActs(panel, stepperButton(0, false)));
    CHECK(quality == 0.0f);
    CHECK_FALSE(pressActs(panel, stepperButton(0, false)));
    CHECK(quality == 0.0f);

    // a stepper whose value is no option has none before it, and its first after it
    const auto& outside = globals.values.at({ PLUGIN, 0x823 });
    CHECK_FALSE(pressActs(panel, stepperButton(4, false)));
    CHECK(outside == 7.0f);
    CHECK(pressActs(panel, stepperButton(4, true)));
    CHECK(outside == 0.0f);
}

TEST_CASE("FullPanel: a press on the row of a stepper goes to the option after, and from the last to the first")
{
    const test_panel::PanelContext context;
    auto globals = choiceGlobals();
    mcm::McmStore store;
    auto panel = modWithChoices(store);
    drawFrames(panel, 2);
    const auto& quality = globals.values.at({ PLUGIN, 0x820 });

    CHECK(pressActs(panel, pageRow(0)));
    CHECK(quality == 2.0f);
    CHECK(pressActs(panel, pageRow(0)));
    CHECK(quality == 0.0f);
    CHECK(pressActs(panel, pageRow(0)));
    CHECK(quality == 1.0f);
}

TEST_CASE("FullPanel: a push of the thumbstick steps the stepper the ray is on, once for a push")
{
    const test_panel::PanelContext context;
    auto globals = choiceGlobals();
    mcm::McmStore store;
    auto panel = modWithChoices(store);
    drawFrames(panel, 2);
    const auto& quality = globals.values.at({ PLUGIN, 0x820 });

    // a push that is held is one step, with a pulse in its frame
    pushFrames(panel, pageRow(0), 0.0f, 3);
    pushFrames(panel, pageRow(0), 1.0f, 1);
    CHECK(quality == 2.0f);
    CHECK(panel.pressActed());
    pushFrames(panel, pageRow(0), 1.0f, 90);
    CHECK(quality == 2.0f);

    // it stops at the last option, with no pulse, and a push that is not far steps nothing
    pushFrames(panel, pageRow(0), 0.0f, 1);
    pushFrames(panel, pageRow(0), 1.0f, 1);
    CHECK(quality == 2.0f);
    CHECK_FALSE(panel.pressActed());
    pushFrames(panel, pageRow(0), 0.0f, 1);
    pushFrames(panel, pageRow(0), -0.4f, 2);
    CHECK(quality == 2.0f);
    pushFrames(panel, pageRow(0), -1.0f, 1);
    CHECK(quality == 1.0f);

    // a push that began off the row steps nothing on it, and neither does one on the row of a dropdown
    pushFrames(panel, pageRow(0), 0.0f, 1);
    pushFrames(panel, test_panel::POINTER_AWAY, 1.0f, 2);
    pushFrames(panel, pageRow(0), 1.0f, 10);
    CHECK(quality == 1.0f);
    pushFrames(panel, pageRow(1), 0.0f, 3);
    pushFrames(panel, pageRow(1), 1.0f, 2);
    CHECK(globals.values.at({ PLUGIN, 0x821 }) == 0.0f);
    panel.setThumbstickSideValue(0.0f);
}

TEST_CASE("FullPanel: a press on the row of a dropdown opens its list, and a press on an option picks it")
{
    const test_panel::PanelContext context;
    auto globals = choiceGlobals();
    mcm::McmStore store;
    auto panel = modWithChoices(store);
    drawFrames(panel, 2);
    const auto& place = globals.values.at({ PLUGIN, 0x821 });

    CHECK(pressActs(panel, pageRow(1)));
    CHECK(isListOpen());
    CHECK(place == 0.0f);

    CHECK(pressActs(panel, listOption(1, 2)));
    CHECK_FALSE(isListOpen());
    CHECK(place == 2.0f);

    // the option it is at already closes the list, and changes nothing
    CHECK(pressActs(panel, pageRow(1)));
    CHECK_FALSE(pressActs(panel, listOption(1, 2)));
    CHECK_FALSE(isListOpen());
    CHECK(place == 2.0f);

    // the first option is over the row of the switcher, which is not pressed
    CHECK(pressActs(panel, pageRow(1)));
    CHECK(pressActs(panel, listOption(1, 0)));
    CHECK(place == 0.0f);
    CHECK(globals.values.at({ PLUGIN, 0x822 }) == 0.0f);
    CHECK(pressActs(panel, listOption(1, 0)));
    CHECK(globals.values.at({ PLUGIN, 0x822 }) == 1.0f);
}

TEST_CASE("FullPanel: a press beside the open list of a dropdown closes it, and does nothing else")
{
    const test_panel::PanelContext context;
    auto globals = choiceGlobals();
    mcm::McmStore store;
    auto panel = modWithChoices(store);
    drawFrames(panel, 2);

    // on the name of the switcher, beside the list
    CHECK(pressActs(panel, pageRow(1)));
    CHECK_FALSE(pressActs(panel, pageRow(2)));
    CHECK_FALSE(isListOpen());
    CHECK(globals.values.at({ PLUGIN, 0x821 }) == 0.0f);
    CHECK(globals.values.at({ PLUGIN, 0x822 }) == 0.0f);

    CHECK(pressActs(panel, pageRow(2)));
    CHECK(globals.values.at({ PLUGIN, 0x822 }) == 1.0f);
}

TEST_CASE("FullPanel: Back closes the open list of a dropdown, with no option picked")
{
    const test_panel::PanelContext context;
    auto globals = choiceGlobals();
    mcm::McmStore store;
    auto panel = modWithChoices(store);
    drawFrames(panel, 2);

    press(panel, pageRow(1));
    point(panel, listOption(1, 1));
    REQUIRE(isListOpen());

    CHECK(panel.back());
    drawFrames(panel, 2);
    CHECK_FALSE(isListOpen());
    CHECK(panel.selection() == selectionOf(0));
    CHECK(globals.values.at({ PLUGIN, 0x821 }) == 0.0f);

    // Back again is a step back in the panel
    CHECK(panel.back());
    CHECK(panel.selection() == Selection{});
}

TEST_CASE("FullPanel: the list of a dropdown is closed as another page is shown")
{
    const test_panel::PanelContext context;
    auto globals = choiceGlobals();
    mcm::McmStore store;
    auto panel = modWithChoices(store);
    drawFrames(panel, 2);

    press(panel, pageRow(1));
    REQUIRE(isListOpen());
    panel.select(selectionOf(0, 0));
    panel.select(selectionOf(0));
    drawFrames(panel, 2);
    CHECK_FALSE(isListOpen());
}

TEST_CASE("FullPanel: the help line keeps the row of a dropdown while the ray is on its open list")
{
    const test_panel::PanelContext context;
    auto globals = choiceGlobals();
    mcm::McmStore store;
    auto panel = modWithChoices(store);
    drawFrames(panel, 2);

    press(panel, pageRow(1));
    point(panel, listOption(1, 2));
    REQUIRE(isListOpen());
    CHECK(panel.helpLine().text() == "Place: What it does.");
}

TEST_CASE("FullPanel: a dropdown with no options is not opened")
{
    const test_panel::PanelContext context;
    auto globals = choiceGlobals();
    mcm::McmStore store;
    auto panel = modWithChoices(store);
    drawFrames(panel, 2);

    CHECK_FALSE(pressActs(panel, pageRow(3)));
    CHECK_FALSE(isListOpen());
}

TEST_CASE("FullPanel: a mod's editing session ends as the mod is left, and as the panel is reset")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    test_events::TestEvents events;
    mcm::McmStore store;
    auto panel = modWithValues(store);
    CHECK(events.sent == Names{ "OnMCMMenuOpen", "OnMCMMenuOpen|Alpha" });

    // another mod
    events.sent.clear();
    panel.select(selectionOf(1));
    CHECK(events.sent == Names{ "OnMCMMenuClose|Alpha", "OnMCMMenuOpen", "OnMCMMenuOpen|Beta" });

    // the menu's front page, as Back goes to it
    events.sent.clear();
    CHECK(panel.back());
    CHECK(events.sent == Names{ "OnMCMMenuClose|Beta" });

    // as the menu closes
    panel.select(selectionOf(0));
    events.sent.clear();
    panel.reset();
    CHECK(events.sent == Names{ "OnMCMMenuClose|Alpha" });
    panel.reset();
    CHECK(events.sent.size() == 1);
}

TEST_CASE("FullPanel: a package's changes wait for a Save, which the help line counts while the ray is on no row")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nbEnable = false\r\nbOther = false\r\n");
    package::PackageStore store({}, myGames.path);
    auto mod = package::loadPackage(R"({
        "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "switcher", "text": "Enable", "help": "Turns it on.", "default": false, "storage": { "path": ["Main", "bEnable"] } },
            { "type": "switcher", "text": "Other", "default": false, "storage": { "path": ["Main", "bOther"] } }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::Package, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);
    CHECK(panel.helpLine().status() == "No unsaved changes");
    CHECK_FALSE(panel.helpLine().isStatusPending());

    CHECK(pressActs(panel, pageRow(0)));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "1 unsaved change");
    CHECK(panel.helpLine().isStatusPending());
    CHECK(pressActs(panel, pageRow(1)));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "2 unsaved changes");

    // a value changed back is not a change
    CHECK(pressActs(panel, pageRow(0)));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "1 unsaved change");

    // the menu's front page has no session
    panel.select({});
    CHECK(panel.helpLine().status().empty());
    CHECK_FALSE(panel.helpLine().isStatusPending());
}

TEST_CASE("FullPanel: Save writes a package's changes and Discard takes them back, and both are disabled while none waits")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    const auto file = myGames.path / "Mods_Config" / "MyMod" / "MyMod.toml";
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nbEnable = false\r\n");
    package::PackageStore store({}, myGames.path);
    auto mod = package::loadPackage(R"({
        "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [ { "type": "switcher", "text": "Enable", "default": false, "storage": { "path": ["Main", "bEnable"] } } ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::Package, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);
    const auto& setting = std::get<model::Setting>(panel.mods()[0].frontPage.items[0]);
    CHECK_FALSE(pressActs(panel, saveButton()));
    CHECK_FALSE(pressActs(panel, discardButton()));

    CHECK(pressActs(panel, pageRow(0)));
    CHECK(pressActs(panel, discardButton()));
    CHECK(store.dirtyCount() == 0);
    CHECK(store.value(setting).value == model::Value(false));
    CHECK(panel.helpLine().status() == "No unsaved changes");

    CHECK(pressActs(panel, pageRow(0)));
    CHECK(pressActs(panel, saveButton()));
    CHECK(store.dirtyCount() == 0);
    CHECK(panel.helpLine().status() == "No unsaved changes");
    std::ostringstream text;
    text << std::ifstream(file, std::ios::binary).rdbuf();
    CHECK(text.str() == "[Main]\r\nbEnable = true\r\n");

    // the menu's front page has no session to save
    panel.select({});
    drawFrames(panel, 2);
    CHECK_FALSE(pressActs(panel, saveButton()));
}

TEST_CASE("FullPanel: an invalid value of a package has its reason as its help, and its Reset sets the default")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nbEnable = \"yes\"\r\n");
    package::PackageStore store({}, myGames.path);
    auto mod = package::loadPackage(R"({
        "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [ { "type": "switcher", "text": "Enable", "help": "Turns it on.", "default": true, "storage": { "path": ["Main", "bEnable"] } } ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::Package, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);
    const auto& setting = std::get<model::Setting>(panel.mods()[0].frontPage.items[0]);
    CHECK(store.value(setting).invalid);

    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == R"(Enable: Invalid: Main.bEnable is "yes" in MyMod.toml, which is not true or false.)");

    // it shows the default, and its Reset still sets it, which replaces the file's value
    point(panel, resetButton(0));
    CHECK(panel.helpLine().text() == "Enable: Reset to on");
    CHECK(pressActs(panel, resetButton(0)));
    CHECK_FALSE(store.value(setting).invalid);
    CHECK(store.isDirty(setting));
    CHECK(store.value(setting).value == model::Value(true));
}

TEST_CASE("FullPanel: a package's Reset is a change as any other, which Save writes and Discard takes back")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    const auto file = myGames.path / "Mods_Config" / "MyMod" / "MyMod.toml";
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nfScale = 1.5\r\nbEnable = false\r\n");
    package::PackageStore store({}, myGames.path);
    auto mod = package::loadPackage(R"({
        "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "slider", "text": "Scale", "default": 1.0, "valueOptions": { "min": 0.5, "max": 2.0, "step": 0.1 }, "storage": { "path": ["Main", "fScale"] } },
            { "type": "switcher", "text": "Enable", "default": false, "storage": { "path": ["Main", "bEnable"] } }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::Package, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);
    const auto& items = panel.mods()[0].frontPage.items;
    const auto& scale = std::get<model::Setting>(items[0]);
    const auto& enable = std::get<model::Setting>(items[1]);
    const auto fileText = [&file] {
        std::ostringstream text;
        text << std::ifstream(file, std::ios::binary).rdbuf();
        return text.str();
    };

    // it waits for a Save, and Discard takes it back
    CHECK(pressActs(panel, resetButton(0)));
    CHECK(store.value(scale).value == model::Value(1.0f));
    CHECK(store.isDirty(scale));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "1 unsaved change");
    CHECK(pressActs(panel, discardButton()));
    CHECK(store.value(scale).value == model::Value(1.5f));
    CHECK_FALSE(store.isDirty(scale));

    // Save writes it, and its Reset is then disabled
    CHECK(pressActs(panel, resetButton(0)));
    CHECK(pressActs(panel, saveButton()));
    CHECK(fileText() == "[Main]\r\nfScale = 1.0\r\nbEnable = false\r\n");
    CHECK(store.dirtyCount() == 0);
    CHECK_FALSE(pressActs(panel, resetButton(0)));

    // a change of a setting whose file has its default is taken back by its Reset, which leaves it clean
    CHECK_FALSE(pressActs(panel, resetButton(1)));
    CHECK(pressActs(panel, pageRow(1)));
    CHECK(store.isDirty(enable));
    CHECK(pressActs(panel, resetButton(1)));
    CHECK(store.value(enable).value == model::Value(false));
    CHECK(store.dirtyCount() == 0);
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "No unsaved changes");
}

TEST_CASE("FullPanel: with no package store none of a package's values is available, and the MCM store's session ends for it")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    test_events::TestEvents events;
    mcm::McmStore store;
    auto package = modOf("Package");
    package.source = model::Source::Package;
    package.frontPage.items.push_back(model::Setting{ .type = model::SettingType::Switcher, .text = "Holsters", .help = "Draw at the body.", .valueOptions = {} });
    auto panel = modWithValues(store);
    auto mods = panel.mods();
    mods.push_back(std::move(package));
    panel.setMods(std::move(mods));
    panel.select(selectionOf(0));
    events.sent.clear();

    // Alpha, Beta, Package
    panel.select(selectionOf(2));
    CHECK(events.sent == Names{ "OnMCMMenuClose|Alpha" });
    CHECK(panel.helpLine().status().empty());
    drawFrames(panel, 2);
    // a press on its switcher sets nothing
    CHECK_FALSE(pressActs(panel, pageRow(0)));
    CHECK(panel.helpLine().text() == "Holsters: Draw at the body.");

    // its page and the reset of the panel tell no store
    panel.reset();
    CHECK(events.sent.size() == 1);
    panel.select(selectionOf(2));
    panel.select(selectionOf(1));
    CHECK(events.sent == Names{ "OnMCMMenuClose|Alpha", "OnMCMMenuOpen", "OnMCMMenuOpen|Beta" });
}

TEST_CASE("FullPanel: a press on the Reset of a row sets the default of its setting in its store, and acts")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    test_events::TestEvents events;
    const test_files::TempFolder mcm;
    mcm::McmStore store(mcm.path);
    auto panel = modWithDefaults(store, mcm);
    const auto& rows = panel.mods()[0].frontPage.items;
    const auto valueOf = [&store, &rows](const std::size_t row) {
        return store.value(std::get<model::Setting>(rows[row])).value;
    };
    drawFrames(panel, 2);
    events.sent.clear();

    // the switcher is turned on by it once, and not off again by a press on the room it was in
    CHECK(pressActs(panel, resetButton(0)));
    CHECK(valueOf(0) == model::Value(true));
    CHECK(events.sent == Names{ "OnMCMSettingChange(Alpha, bOn:Main)", "OnMCMSettingChange|Alpha(Alpha, bOn:Main)" });
    CHECK_FALSE(pressActs(panel, resetButton(0)));
    CHECK(valueOf(0) == model::Value(true));

    CHECK(pressActs(panel, resetButton(1)));
    CHECK(valueOf(1) == model::Value(1.0f));

    // the list of the dropdown is not opened
    CHECK(pressActs(panel, resetButton(2)));
    CHECK(valueOf(2) == model::Value(1));
    CHECK_FALSE(isListOpen());
    CHECK_FALSE(pressActs(panel, resetButton(2)));
    CHECK_FALSE(isListOpen());

    // a row that is changed is reset again
    CHECK(pressActs(panel, pageRow(0)));
    CHECK(valueOf(0) == model::Value(false));
    CHECK(pressActs(panel, resetButton(0)));
    CHECK(valueOf(0) == model::Value(true));
}

TEST_CASE("FullPanel: a row with no default has no Reset, and a press in its room is one on the row")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    const test_files::TempFolder mcm;
    mcm::McmStore store(mcm.path);
    auto panel = modWithDefaults(store, mcm);
    drawFrames(panel, 2);

    CHECK(pressActs(panel, resetButton(3)));
    CHECK(globals.values.at({ PLUGIN, 0x800 }) == 1.0f);
}

TEST_CASE("FullPanel: a slider that is dragged and let go on its Reset is not reset")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    const test_files::TempFolder mcm;
    mcm::McmStore store(mcm.path);
    auto panel = modWithDefaults(store, mcm);
    const auto& strength = std::get<model::Setting>(panel.mods()[0].frontPage.items[1]);
    drawFrames(panel, 2);

    for (const bool held : { false, false, true }) {
        test_panel::drawFrame(panel, sliderTrack(1, 0.25f), held);
    }
    test_panel::drawFrame(panel, resetButton(1), true);
    test_panel::drawFrame(panel, resetButton(1), false);
    test_panel::drawFrame(panel, resetButton(1), false);

    // at its most, where the drag left it
    CHECK(store.value(strength).value == model::Value(2.0f));
}

TEST_CASE("FullPanel: the help line names the default of the row the ray is on, and what its Reset gives")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    const test_files::TempFolder mcm;
    mcm::McmStore store(mcm.path);
    auto panel = modWithDefaults(store, mcm);
    drawFrames(panel, 2);

    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Enabled: What it does. (Default: on)");
    point(panel, resetButton(0));
    CHECK(panel.helpLine().text() == "Enabled: Reset to on");
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Strength: What it does. (Default: 1.0)");
    point(panel, resetButton(1));
    CHECK(panel.helpLine().text() == "Strength: Reset to 1.0");

    // a row with no help of its own, and one with no default
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == "Mode: (Default: Medium)");
    point(panel, resetButton(2));
    CHECK(panel.helpLine().text() == "Mode: Reset to Medium");
    point(panel, resetButton(3));
    CHECK(panel.helpLine().text() == "Global: What it does.");

    // the Reset of a row at its default is disabled, and tells nothing
    press(panel, resetButton(0));
    point(panel, resetButton(0));
    CHECK(panel.helpLine().text() == "Enabled: What it does. (Default: on)");
}
