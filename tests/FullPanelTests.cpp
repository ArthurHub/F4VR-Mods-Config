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
#include "TestProperties.h"
#include "Theme.h"
#include "mcm/McmStore.h"
#include "spec/ConfigSpecLoader.h"
#include "spec/ConfigSpecStore.h"
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
        return { .mod = mod, .page = {} };
    }

    Selection selectionOf(const std::size_t mod, const std::size_t page)
    {
        return { .mod = mod, .page = { page } };
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

    // the buttons of the dialog that asks what to do with the unsaved changes, by their places from its right
    constexpr int DIALOG_SAVE = 0;
    constexpr int DIALOG_DISCARD = 1;
    constexpr int DIALOG_CANCEL = 2;

    // the window of the dialog that asks about unsaved changes, and of a button's confirmation
    constexpr auto UNSAVED_DIALOG = "Unsaved changes";
    constexpr auto CONFIRMATION = "##confirmation";

    /**
     * A dialog's window, which is shown while it is active.
     */
    const ImGuiWindow* dialogWindow(const char* name = UNSAVED_DIALOG)
    {
        return ImGui::FindWindowByName(name);
    }

    bool isDialogShown(const char* name = UNSAVED_DIALOG)
    {
        const auto* window = dialogWindow(name);
        return window && window->Active;
    }

    /**
     * A place in a button of the dialog, by its place from the right, while the dialog is shown: near the
     * right of the button, as a button is 112 wide or a little more for a longer word.
     */
    ImVec2 dialogButton(const int place, const char* name = UNSAVED_DIALOG)
    {
        constexpr float BUTTON_MIN_WIDTH = 112.0f;
        const auto* window = dialogWindow(name);
        REQUIRE(window);
        const float right = window->Pos.x + window->Size.x - theme::SPACE_6;
        const float bottom = window->Pos.y + window->Size.y - theme::SPACE_6;
        return { right - static_cast<float>(place) * (BUTTON_MIN_WIDTH + theme::SPACE_2) - theme::SPACE_4, bottom - theme::CONTROL_HEIGHT * 0.5f };
    }

    // the file of the mod of `twoConfigSpecs`, under the folder of My Games
    constexpr auto MY_MOD_FILE = R"(Mods_Config\MyMod\MyMod.toml)";

    /**
     * A panel with two config specs, shown on the front page of the first, with their values in a store: My Mod
     * with a switcher of its file and a text input of a number from 0.5 to 2.5, and Other Mod with a text.
     */
    FullPanel twoConfigSpecs(spec::ConfigSpecStore& store)
    {
        auto mod = spec::loadConfigSpec(R"({
            "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "save",
            "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
            "defaultFile": "main",
            "content": [
                { "type": "switcher", "text": "Enable", "default": false, "storage": { "path": ["Main", "bEnable"] } },
                { "type": "textinput", "valueType": "float", "text": "Height", "default": 1.5, "valueOptions": { "min": 0.5, "max": 2.5 },
                  "storage": { "path": ["Main", "fHeight"] } }
            ]
        })");
        auto other = spec::loadConfigSpec(R"({
            "minSchemaVersion": 1, "modName": "OtherMod", "displayName": "Other Mod", "applies": "save",
            "content": [ { "type": "text", "text": "Nothing to set." } ]
        })");
        REQUIRE(mod);
        REQUIRE(other);
        FullPanel panel;
        panel.setStore(model::Source::ConfigSpec, &store);
        panel.setMods({ std::move(*mod), std::move(*other) });
        panel.select(selectionOf(0));
        return panel;
    }

    std::string fileText(const std::filesystem::path& file)
    {
        std::ostringstream text;
        text << std::ifstream(file, std::ios::binary).rdbuf();
        return text.str();
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

    /**
     * A place on a breadcrumb, by its place among the crumbs of the title bar, which are given by their texts.
     * Each is after the one before, a separator and their gaps.
     */
    ImVec2 crumbAt(const std::vector<const char*>& crumbs, const std::size_t place)
    {
        const float separator = crumbWidth("\xC2\xBB") - theme::SPACE_2 * 2.0f;
        float left = CONTENT_START + theme::SPACE_2;
        for (std::size_t before = 0; before < place; ++before) {
            left += crumbWidth(crumbs[before]) + theme::SPACE_1 + separator + theme::SPACE_1;
        }
        return { left + 20.0f, CONTENT_START + theme::TITLEBAR_HEIGHT * 0.5f };
    }

    /**
     * A panel with a mod whose pages nest, and a mod after it. Alpha has Body and Advanced, Body has Hands and
     * Feet, and Hands has Fingers.
     */
    FullPanel nestedPages(const std::string& body = "Body", const std::string& hands = "Hands", const std::string& fingers = "Fingers")
    {
        auto mod = modOf("Alpha", { body, "Advanced" });
        mod.pages[0].pages.push_back({ .name = hands, .help = "Where the hands are.", .items = {} });
        mod.pages[0].pages[0].pages.push_back({ .name = fingers, .items = {} });
        mod.pages[0].pages.push_back({ .name = "Feet", .items = {} });
        FullPanel panel;
        panel.setMods({ std::move(mod), modOf("Beta") });
        return panel;
    }

    /**
     * A selection of a page of the first mod, by its places on the way to it.
     */
    Selection pageOf(std::vector<std::size_t> page)
    {
        return { .mod = 0, .page = std::move(page) };
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
    panel.select({ .mod = std::nullopt, .page = { 0 } });
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

TEST_CASE("FullPanel: a page's pages are rows under it while it or one of them is selected")
{
    const test_panel::PanelContext context;
    auto panel = nestedPages();
    drawFrames(panel, 2);

    // Alpha's pages are the rows under it, and Body's are not yet
    press(panel, treeRow(0));
    CHECK(panel.selection() == selectionOf(0));
    press(panel, treeRow(2));
    CHECK(panel.selection() == pageOf({ 1 }));

    // Body opens, with Hands and Feet under it
    press(panel, treeRow(1));
    CHECK(panel.selection() == pageOf({ 0 }));
    press(panel, treeRow(3));
    CHECK(panel.selection() == pageOf({ 0, 1 }));

    // Hands opens, with Fingers under it, and Body stays open
    press(panel, treeRow(2));
    CHECK(panel.selection() == pageOf({ 0, 0 }));
    point(panel, treeRow(2));
    CHECK(panel.helpLine().text() == "Hands: Where the hands are.");
    press(panel, treeRow(3));
    CHECK(panel.selection() == pageOf({ 0, 0, 0 }));
    CHECK(panel.selectedPage()->name == "Fingers");

    // Advanced is after them, and Body closes as it is selected: Beta is the fourth row again
    press(panel, treeRow(5));
    CHECK(panel.selection() == pageOf({ 1 }));
    press(panel, treeRow(3));
    CHECK(panel.selection() == selectionOf(1));
}

TEST_CASE("FullPanel: a selection of a page under a page that is not there shows the deepest page on the way that is")
{
    auto panel = nestedPages();
    panel.select(pageOf({ 0, 0, 1 }));
    CHECK(panel.selection() == pageOf({ 0, 0 }));
    panel.select(pageOf({ 0, 2, 0 }));
    CHECK(panel.selection() == pageOf({ 0 }));
    panel.select(pageOf({ 1, 0 }));
    CHECK(panel.selection() == pageOf({ 1 }));
    panel.select(pageOf({ 2, 0 }));
    CHECK(panel.selection() == selectionOf(0));
}

TEST_CASE("FullPanel: Back goes from a page under a page to the page it is under")
{
    auto panel = nestedPages();
    panel.select(pageOf({ 0, 0, 0 }));

    CHECK(panel.back());
    CHECK(panel.selection() == pageOf({ 0, 0 }));
    CHECK(panel.back());
    CHECK(panel.selection() == pageOf({ 0 }));
    CHECK(panel.back());
    CHECK(panel.selection() == selectionOf(0));
    CHECK(panel.back());
    CHECK(panel.selection() == Selection{});
}

TEST_CASE("FullPanel: a page under a page has a breadcrumb for each page on the way to it, which is pressed to go there")
{
    const test_panel::PanelContext context;
    auto panel = nestedPages();
    panel.select(pageOf({ 0, 0, 0 }));
    drawFrames(panel, 2);
    const std::vector<const char*> crumbs{ "MODS CONFIG VR", "Alpha", "Body", "Hands", "Fingers" };

    // the last is where the player is: it is not pressed
    press(panel, crumbAt(crumbs, 4));
    CHECK(panel.selection() == pageOf({ 0, 0, 0 }));

    press(panel, crumbAt(crumbs, 3));
    CHECK(panel.selection() == pageOf({ 0, 0 }));
    press(panel, crumbAt(crumbs, 2));
    CHECK(panel.selection() == pageOf({ 0 }));
    press(panel, crumbAt(crumbs, 1));
    CHECK(panel.selection() == selectionOf(0));
}

TEST_CASE("FullPanel: the pages between the mod and the page are one ellipsis while their crumbs do not fit, which is not pressed")
{
    const test_panel::PanelContext context;
    auto panel = nestedPages("Calibration of the body", "Where the hands are held");
    panel.select(pageOf({ 0, 0, 0 }));
    drawFrames(panel, 2);
    const std::vector<const char*> crumbs{ "MODS CONFIG VR", "Alpha", "\xE2\x80\xA6", "Fingers" };

    press(panel, crumbAt(crumbs, 2));
    CHECK(panel.selection() == pageOf({ 0, 0, 0 }));

    // the mod's name is not shortened while the rest fits
    press(panel, crumbAt(crumbs, 1));
    CHECK(panel.selection() == selectionOf(0));

    // the crumbs of a page whose name is short fit, so the page above it has its own
    panel.select(pageOf({ 0, 1 }));
    drawFrames(panel, 2);
    press(panel, crumbAt({ "MODS CONFIG VR", "Alpha", "Calibration of the body", "Feet" }, 2));
    CHECK(panel.selection() == pageOf({ 0 }));
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

TEST_CASE("FullPanel: the Notices heading tells its help, and a press on it folds or opens it")
{
    const test_panel::PanelContext context;
    FullPanel panel;
    panel.setMods({ model::Mod{ .name = "SS2", .displayName = "Sim Settlements 2", .notAvailable = "its plugin SS2.esm is not loaded" } });
    drawFrames(panel, 2);

    // the heading is under the page's two lines of text: the panel has no quick launch
    const ImVec2 heading(640.0f, 171.0f);
    point(panel, heading);
    CHECK(panel.helpLine().text().starts_with("Notices: "));
    CHECK(pressActs(panel, heading));
    CHECK(pressActs(panel, heading));
}

TEST_CASE("FullPanel: a press on the quick launch heading folds its entries, and the menu opens with it open")
{
    const test_panel::PanelContext context;
    FullPanel panel;
    int opened = 0;
    panel.setQuickLaunch({ { .name = "FRIK", .help = {}, .open = [&opened] {
                                ++opened;
                            } } });
    drawFrames(panel, 2);

    // the heading is under the page's two lines of text, and the entry's row under it
    const ImVec2 heading(640.0f, 171.0f);
    const ImVec2 entry(640.0f, 227.0f);
    CHECK(pressActs(panel, heading));
    press(panel, entry);
    CHECK(opened == 0);

    CHECK(pressActs(panel, heading));
    press(panel, entry);
    CHECK(opened == 1);

    CHECK(pressActs(panel, heading));
    panel.reset();
    drawFrames(panel, 2);
    press(panel, entry);
    CHECK(opened == 2);
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

TEST_CASE("FullPanel: a config spec's button runs its console command while its enabledWhen holds on the pending values")
{
    const test_panel::PanelContext context;
    test_actions::TestActions actions;
    const test_files::TempFolder myGames;
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nbEnable = false\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "id": "enable", "type": "switcher", "text": "Enable", "default": false, "storage": { "path": ["Main", "bEnable"] } },
            { "type": "button", "text": "Hide the HUD", "help": "Toggles the HUD.", "buttonText": "Toggle",
              "action": { "type": "RunConsoleCommand", "command": "tm" }, "enabledWhen": { "setting": "enable", "equals": true } },
            { "type": "button", "text": "Calibrate", "action": { "type": "CallNativeFunction", "function": "calibrate" } }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    // disabled while its enabledWhen does not hold, with its own help
    CHECK_FALSE(pressActs(panel, pageRow(1)));
    CHECK(actions.commands.empty());
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Hide the HUD: Toggles the HUD.");

    // the switcher's pending value enables it, with no Save
    CHECK(pressActs(panel, pageRow(0)));
    CHECK(pressActs(panel, pageRow(1)));
    CHECK(actions.commands == std::vector{ model::ConsoleCommand{ "tm" } });
    CHECK(store.dirtyCount() == 1);

    // a function of the mod's DLL is not run while its DLL is not loaded, and the help line says why
    CHECK_FALSE(pressActs(panel, pageRow(2)));
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == "Calibrate: Needs My Mod's DLL, which is not loaded.");
}

TEST_CASE("FullPanel: a button runs its mod's function after its confirmation, and its result is in the help line")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "save",
        "content": [
            { "type": "button", "text": "Calibrate", "help": "Measures your height.", "buttonText": "Calibrate",
              "action": { "type": "CallNativeFunction", "function": "calibrate", "params": { "samples": 30 } },
              "confirmation": { "title": "Calibrate?", "message": "Stand straight.", "dangerous": true } },
            { "type": "button", "text": "Fail", "help": "Never works.", "action": { "type": "CallNativeFunction", "function": "fail" } },
            { "type": "button", "text": "Quiet", "action": { "type": "CallNativeFunction", "function": "quiet" } }
        ]
    })");
    REQUIRE(mod);
    std::vector<std::vector<model::NamedArgument>> calls;
    auto& functions = store.clientCalls();
    functions.addClient("MyMod");
    functions.add("MyMod", "calibrate", [&calls](const std::vector<model::NamedArgument>& arguments) {
        calls.push_back(arguments);
        return model::FunctionResult{ .done = true, .message = "Done: 1.78 m." };
    });
    functions.add("MyMod", "fail", [](const auto&) {
        return model::FunctionResult{ .done = false, .message = "Stand still, then try again." };
    });
    functions.add("MyMod", "quiet", [](const auto&) {
        return model::FunctionResult{ .done = true, .message = {} };
    });
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    // a press asks first, and Back cancels it with nothing run
    CHECK(pressActs(panel, pageRow(0)));
    CHECK(isDialogShown(CONFIRMATION));
    CHECK(calls.empty());
    CHECK(panel.back());
    drawFrames(panel, 2);
    CHECK_FALSE(isDialogShown(CONFIRMATION));
    CHECK(calls.empty());

    // its confirming button runs it, and the result is told while the ray is on the row
    CHECK(pressActs(panel, pageRow(0)));
    CHECK(pressActs(panel, dialogButton(0, CONFIRMATION)));
    drawFrames(panel, 2);
    CHECK_FALSE(isDialogShown(CONFIRMATION));
    REQUIRE(calls.size() == 1);
    CHECK(calls[0] == std::vector<model::NamedArgument>{ { .name = "samples", .value = 30 } });
    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Calibrate: Done: 1.78 m. Measures your height.");

    // a run that failed has its reason in place of the help
    CHECK(pressActs(panel, pageRow(1)));
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Fail: Stand still, then try again.");
    CHECK(panel.helpLine().isInvalid());

    // one with no message only runs
    CHECK(pressActs(panel, pageRow(2)));
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == "Quiet");

    // another page forgets the results
    panel.select({});
    drawFrames(panel, 2);
    panel.select(selectionOf(0));
    drawFrames(panel, 2);
    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Calibrate: Measures your height.");
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

TEST_CASE("FullPanel: a config spec's row whose condition on a plugin does not hold is hidden, with no store")
{
    const test_panel::PanelContext context;

    // a condition on a plugin that is not loaded is an Any of none, which does not hold
    auto mod = modOf("Alpha");
    mod.source = model::Source::ConfigSpec;
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

TEST_CASE("FullPanel: a choice that reads its options in the game has the options its store read, and says why while its list is not in the game")
{
    const test_panel::PanelContext context;
    auto globals = choiceGlobals();
    test_properties::TestProperties lists;
    lists.lists[{ PLUGIN, 0x900, "", "" }] = { "Sanctuary", "Red Rocket", "Abernathy Farm" };
    const auto fromList = [](const model::SettingType type, std::string text, const std::uint32_t formId, const std::uint32_t listId) {
        auto item = globalSetting(type, std::move(text), formId);
        std::get<model::Setting>(item).valueOptions.gameOptions = model::GameOptions{ .plugin = PLUGIN, .formId = listId };
        return item;
    };
    auto mod = modOf("Alpha", {});
    mod.frontPage.items.push_back(fromList(model::SettingType::Stepper, "Settlement", 0x820, 0x900));
    mod.frontPage.items.push_back(fromList(model::SettingType::Dropdown, "Home", 0x821, 0x900));
    mod.frontPage.items.push_back(fromList(model::SettingType::Dropdown, "Missing", 0x822, 0x901));
    mcm::McmStore store;
    FullPanel panel;
    panel.setStore(model::Source::Mcm, &store);
    panel.setMods({ std::move(mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    CHECK(pressActs(panel, stepperButton(0, true)));
    CHECK(globals.values.at({ PLUGIN, 0x820 }) == 2.0f);
    CHECK_FALSE(pressActs(panel, stepperButton(0, true)));

    CHECK(pressActs(panel, pageRow(1)));
    CHECK(isListOpen());
    CHECK(pressActs(panel, listOption(1, 2)));
    CHECK(globals.values.at({ PLUGIN, 0x821 }) == 2.0f);

    CHECK_FALSE(pressActs(panel, pageRow(2)));
    CHECK_FALSE(isListOpen());
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == "Missing: Not available: the FormList MyMod.esp|901 of its options is not in the game.");
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

TEST_CASE("FullPanel: a config spec's changes wait for a Save, which the help line counts while the ray is on no row")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nbEnable = false\r\nbOther = false\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "switcher", "text": "Enable", "help": "Turns it on.", "default": false, "storage": { "path": ["Main", "bEnable"] } },
            { "type": "switcher", "text": "Other", "default": false, "storage": { "path": ["Main", "bOther"] } }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);
    CHECK(panel.helpLine().status() == "No unsaved changes");
    CHECK(panel.helpLine().statusLook() == ui::StatusLook::Plain);

    CHECK(pressActs(panel, pageRow(0)));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "1 unsaved change");
    CHECK(panel.helpLine().statusLook() == ui::StatusLook::Pending);
    CHECK(pressActs(panel, pageRow(1)));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "2 unsaved changes");

    // a value changed back is not a change
    CHECK(pressActs(panel, pageRow(0)));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "1 unsaved change");

    // the menu's front page has no session
    CHECK(pressActs(panel, discardButton()));
    panel.select({});
    CHECK(panel.helpLine().status().empty());
    CHECK(panel.helpLine().statusLook() == ui::StatusLook::Plain);
}

TEST_CASE("FullPanel: a config spec's setting says when its change takes effect, and the status counts the ones that take effect after a restart, until the next change")
{
    const test_panel::PanelContext context;
    const test_globals::TestGlobals globals = someGlobals();
    const test_files::TempFolder myGames;
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "save",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "switcher", "text": "Shadows", "help": "Draws shadows.", "default": false, "applies": "restart",
              "storage": { "path": ["Main", "bShadows"] } },
            { "type": "switcher", "text": "Beam", "default": false, "storage": { "path": ["Main", "bBeam"] } },
            { "type": "switcher", "text": "Glow", "default": false, "applies": "restart", "storage": { "path": ["Main", "bGlow"] } },
            { "type": "switcher", "text": "Live", "default": false, "applies": "live", "storage": { "global": "MyMod.esp|800" } }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    // the help says it before the default, but for one that takes effect as it is made
    test_panel::drawFrame(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Shadows: Draws shadows. Takes effect after the game is restarted. (Default: off)");
    test_panel::drawFrame(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Beam: Takes effect after Save. (Default: off)");
    test_panel::drawFrame(panel, pageRow(3));
    CHECK(panel.helpLine().text() == "Live: (Default: off)");

    CHECK(pressActs(panel, pageRow(0)));
    CHECK(pressActs(panel, pageRow(1)));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "2 unsaved changes, 1 takes effect after a restart");
    CHECK(panel.helpLine().statusLook() == ui::StatusLook::Pending);
    CHECK(pressActs(panel, pageRow(2)));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "3 unsaved changes, 2 take effect after a restart");

    CHECK(pressActs(panel, saveButton()));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "Saved. 2 changes take effect after the game is restarted.");
    CHECK(panel.helpLine().statusLook() == ui::StatusLook::Restart);
    // the next change ends it
    CHECK(pressActs(panel, pageRow(1)));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "1 unsaved change");

    // a Save of none says so
    CHECK(pressActs(panel, saveButton()));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "Saved");
    CHECK(panel.helpLine().statusLook() == ui::StatusLook::Plain);

    // the mod keeps its changes for a restart as it is left
    panel.select({});
    CHECK(store.savedRestartCount(panel.mods()[0]) == 2);
    panel.select(selectionOf(0));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "No unsaved changes");
}

TEST_CASE("FullPanel: Save writes a config spec's changes and Discard takes them back, and both are disabled while none waits")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    const auto file = myGames.path / "Mods_Config" / "MyMod" / "MyMod.toml";
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nbEnable = false\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [ { "type": "switcher", "text": "Enable", "default": false, "storage": { "path": ["Main", "bEnable"] } } ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
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
    CHECK(panel.helpLine().status() == "Saved");
    std::ostringstream text;
    text << std::ifstream(file, std::ios::binary).rdbuf();
    CHECK(text.str() == "[Main]\r\nbEnable = true\r\n");

    // the menu's front page has no session to save
    panel.select({});
    drawFrames(panel, 2);
    CHECK_FALSE(pressActs(panel, saveButton()));
}

TEST_CASE("FullPanel: a change that is invalid disables Save, and the status says how many there are")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nfHeight = 1.5\r\nbEnable = false\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "textinput", "valueType": "float", "text": "Height", "default": 1.5, "valueOptions": { "min": 0.5, "max": 2.5 },
              "storage": { "path": ["Main", "fHeight"] } },
            { "type": "switcher", "text": "Enable", "default": false, "storage": { "path": ["Main", "bEnable"] } }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);
    const auto& height = std::get<model::Setting>(panel.mods()[0].frontPage.items[0]);

    CHECK(pressActs(panel, pageRow(1)));
    CHECK(store.set(height, "3"s));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "1 invalid value");
    CHECK(panel.helpLine().statusLook() == ui::StatusLook::Invalid);

    // the row has the reason as its help
    test_panel::drawFrame(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Height: Invalid: 3 is not from 0.5 to 2.5.");
    CHECK(panel.helpLine().isInvalid());

    // Save writes nothing, and Discard takes the changes back
    CHECK_FALSE(pressActs(panel, saveButton()));
    CHECK(store.dirtyCount() == 2);
    CHECK(pressActs(panel, discardButton()));
    drawFrames(panel, 1);
    CHECK(store.dirtyCount() == 0);
    CHECK(panel.helpLine().status() == "No unsaved changes");
}

TEST_CASE("FullPanel: a Save that fails says why in the help line, until the next change, and Save is still enabled")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    const auto file = myGames.path / "Mods_Config" / "MyMod" / "MyMod.toml";
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nbEnable = false\r\nbOther = false\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "switcher", "text": "Enable", "default": false, "storage": { "path": ["Main", "bEnable"] } },
            { "type": "switcher", "text": "Other", "default": false, "storage": { "path": ["Main", "bOther"] } }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    CHECK(pressActs(panel, pageRow(0)));
    {
        const test_files::ReadOnly readOnly(file);
        CHECK(pressActs(panel, saveButton()));
        drawFrames(panel, 1);
    }
    CHECK(store.dirtyCount() == 1);
    CHECK(panel.helpLine().status() == "Not saved: MyMod.toml cannot be written.");
    CHECK(panel.helpLine().statusLook() == ui::StatusLook::Invalid);

    // the row the ray is on has its help over it, and the status is there again as the ray leaves
    test_panel::drawFrame(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Other: Takes effect after Save. (Default: off)");
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "Not saved: MyMod.toml cannot be written.");

    // a change ends it
    CHECK(pressActs(panel, pageRow(1)));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "2 unsaved changes");
    CHECK(panel.helpLine().statusLook() == ui::StatusLook::Pending);

    // and so do a Save that writes, and a Discard
    {
        const test_files::ReadOnly readOnly(file);
        CHECK(pressActs(panel, saveButton()));
    }
    CHECK(pressActs(panel, discardButton()));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "No unsaved changes");
    CHECK(pressActs(panel, pageRow(0)));
    {
        const test_files::ReadOnly readOnly(file);
        CHECK(pressActs(panel, saveButton()));
    }
    CHECK(pressActs(panel, saveButton()));
    drawFrames(panel, 1);
    CHECK(panel.helpLine().status() == "Saved");
    CHECK(store.dirtyCount() == 0);
}

TEST_CASE("FullPanel: a mod with unsaved changes asks before another mod or the menu's front page is shown, and Cancel and Back stay on it")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    myGames.write(MY_MOD_FILE, "[Main]\r\nbEnable = false\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = twoConfigSpecs(store);
    drawFrames(panel, 2);
    CHECK(pressActs(panel, pageRow(0)));

    // another mod
    CHECK(pressActs(panel, treeRow(1)));
    drawFrames(panel, 2);
    CHECK(panel.isAsking());
    CHECK(isDialogShown());
    CHECK(panel.selection() == selectionOf(0));

    // a press on the panel behind it does nothing
    CHECK_FALSE(pressActs(panel, pageRow(0)));
    CHECK(store.dirtyCount() == 1);

    CHECK(pressActs(panel, dialogButton(DIALOG_CANCEL)));
    drawFrames(panel, 2);
    CHECK_FALSE(panel.isAsking());
    CHECK_FALSE(isDialogShown());
    CHECK(panel.selection() == selectionOf(0));
    CHECK(store.dirtyCount() == 1);

    // the menu's front page, by Back, and Back again stays
    CHECK(panel.back());
    drawFrames(panel, 2);
    CHECK(isDialogShown());
    CHECK(panel.back());
    drawFrames(panel, 2);
    CHECK_FALSE(panel.isAsking());
    CHECK_FALSE(isDialogShown());
    CHECK(panel.selection() == selectionOf(0));
    CHECK(store.dirtyCount() == 1);

    // the Help page over the mod does not leave it
    CHECK(panel.showHelp(true));
    CHECK_FALSE(panel.isAsking());
    CHECK(panel.back());
    CHECK_FALSE(panel.isAsking());
}

TEST_CASE("FullPanel: the dialog's Discard takes the changes back and its Save writes them, and both then show what was selected")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    const auto file = myGames.path / MY_MOD_FILE;
    myGames.write(MY_MOD_FILE, "[Main]\r\nbEnable = false\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = twoConfigSpecs(store);
    drawFrames(panel, 2);

    CHECK(pressActs(panel, pageRow(0)));
    CHECK(panel.select(selectionOf(1)));
    drawFrames(panel, 2);
    CHECK(pressActs(panel, dialogButton(DIALOG_DISCARD)));
    drawFrames(panel, 2);
    CHECK_FALSE(isDialogShown());
    CHECK(panel.selection() == selectionOf(1));
    CHECK(fileText(file) == "[Main]\r\nbEnable = false\r\n");

    // a mod with no changes is left at once
    CHECK(panel.select(selectionOf(0)));
    CHECK_FALSE(panel.isAsking());
    drawFrames(panel, 2);

    CHECK(pressActs(panel, pageRow(0)));
    CHECK(panel.select({}));
    drawFrames(panel, 2);
    CHECK(pressActs(panel, dialogButton(DIALOG_SAVE)));
    CHECK_FALSE(panel.isAsking());
    CHECK(panel.selection() == Selection{});
    CHECK(fileText(file) == "[Main]\r\nbEnable = true\r\n");
}

TEST_CASE("FullPanel: Close asks first while the mod has unsaved changes, and the dialog's Save and Discard close the menu")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    const auto file = myGames.path / MY_MOD_FILE;
    myGames.write(MY_MOD_FILE, "[Main]\r\nbEnable = false\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = twoConfigSpecs(store);
    int closed = 0;
    panel.setMenuActions({ .minimize = {}, .close = [&closed] {
                              ++closed;
                          } });
    drawFrames(panel, 2);

    // with nothing to ask it closes at once
    CHECK(panel.close());
    CHECK(closed == 1);

    CHECK(pressActs(panel, pageRow(0)));
    CHECK(pressActs(panel, titleButton(CLOSE_BUTTON)));
    drawFrames(panel, 2);
    CHECK(panel.isAsking());
    CHECK(closed == 1);
    CHECK(pressActs(panel, dialogButton(DIALOG_CANCEL)));
    CHECK(closed == 1);

    CHECK(panel.close());
    drawFrames(panel, 2);
    CHECK(pressActs(panel, dialogButton(DIALOG_DISCARD)));
    CHECK(closed == 2);
    CHECK(store.dirtyCount() == 0);

    CHECK(pressActs(panel, pageRow(0)));
    CHECK(panel.close());
    drawFrames(panel, 2);
    CHECK(pressActs(panel, dialogButton(DIALOG_SAVE)));
    CHECK(closed == 3);
    CHECK(fileText(file) == "[Main]\r\nbEnable = true\r\n");
}

TEST_CASE("FullPanel: the dialog's Save that fails stays on the mod with the reason, and an invalid change disables it")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    const auto file = myGames.path / MY_MOD_FILE;
    myGames.write(MY_MOD_FILE, "[Main]\r\nbEnable = false\r\nfHeight = 1.5\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = twoConfigSpecs(store);
    drawFrames(panel, 2);

    CHECK(pressActs(panel, pageRow(0)));
    CHECK(panel.select(selectionOf(1)));
    drawFrames(panel, 2);
    {
        const test_files::ReadOnly readOnly(file);
        CHECK(pressActs(panel, dialogButton(DIALOG_SAVE)));
    }
    drawFrames(panel, 2);
    CHECK_FALSE(panel.isAsking());
    CHECK(panel.selection() == selectionOf(0));
    CHECK(store.dirtyCount() == 1);
    CHECK(panel.helpLine().status() == "Not saved: MyMod.toml cannot be written.");

    const auto& height = std::get<model::Setting>(panel.mods()[0].frontPage.items[1]);
    CHECK(store.set(height, "3"s));
    CHECK(panel.select(selectionOf(1)));
    drawFrames(panel, 2);
    CHECK_FALSE(pressActs(panel, dialogButton(DIALOG_SAVE)));
    CHECK(panel.isAsking());
    CHECK(pressActs(panel, dialogButton(DIALOG_DISCARD)));
    CHECK(panel.selection() == selectionOf(1));
    CHECK(fileText(file) == "[Main]\r\nbEnable = false\r\nfHeight = 1.5\r\n");
}

TEST_CASE("FullPanel: an MCM mod's changes are saved as they are made, so it is left with nothing to ask")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    mcm::McmStore store;
    auto panel = modWithValues(store);
    drawFrames(panel, 2);

    CHECK(pressActs(panel, pageRow(0)));
    CHECK(panel.select(selectionOf(1)));
    CHECK_FALSE(panel.isAsking());
    CHECK(panel.selection() == selectionOf(1));
}

TEST_CASE("FullPanel: an invalid value of a config spec has its reason as its help, and its Reset sets the default")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nbEnable = \"yes\"\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [ { "type": "switcher", "text": "Enable", "help": "Turns it on.", "default": true, "storage": { "path": ["Main", "bEnable"] } } ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);
    const auto& setting = std::get<model::Setting>(panel.mods()[0].frontPage.items[0]);
    CHECK(store.value(setting).invalid);

    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == R"(Enable: Invalid: Main.bEnable is "yes" in MyMod.toml, which is not true or false.)");
    CHECK(panel.helpLine().isInvalid());

    // it shows the default, and its Reset still sets it, which replaces the file's value
    point(panel, resetButton(0));
    CHECK(panel.helpLine().text() == "Enable: Reset to on");
    CHECK_FALSE(panel.helpLine().isInvalid());
    CHECK(pressActs(panel, resetButton(0)));
    CHECK_FALSE(store.value(setting).invalid);
    CHECK(store.isDirty(setting));
    CHECK(store.value(setting).value == model::Value(true));
}

TEST_CASE("FullPanel: a config spec's read-only setting is not changed by a press or the thumbstick, and the help line says it is read-only")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nfHeight = 1.78\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "slider", "text": "Measured height", "help": "The height FRIK measured.", "readOnly": true,
              "valueOptions": { "min": 1.0, "max": 2.5, "step": 0.01 }, "display": { "unit": "m" }, "storage": { "path": ["Main", "fHeight"] } },
            { "type": "switcher", "text": "Calibrated", "readOnly": true, "storage": { "path": ["Main", "bCalibrated"] } }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);
    const auto& height = std::get<model::Setting>(panel.mods()[0].frontPage.items[0]);

    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Measured height: (Read-only) The height FRIK measured.");
    // one with no value in its file
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Calibrated: (Read-only)");

    // its row, its track and the room of a Reset
    CHECK_FALSE(pressActs(panel, pageRow(0)));
    CHECK_FALSE(pressActs(panel, sliderTrack(0, 0.9f)));
    CHECK_FALSE(pressActs(panel, resetButton(0)));
    CHECK_FALSE(pressActs(panel, pageRow(1)));
    pushFrames(panel, sliderTrack(0, 0.5f), 1.0f, 60);
    pushFrames(panel, sliderTrack(0, 0.5f), 0.0f, 1);
    CHECK_FALSE(panel.pressActed());
    CHECK(store.value(height).value == model::Value(1.78f));
    CHECK(store.dirtyCount() == 0);
}

TEST_CASE("FullPanel: a config spec's setting and button tell the first warning that holds on the pending values before their help, and an invalid value tells none")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nbShadows = false\r\nbBad = \"yes\"\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "id": "shadows", "type": "switcher", "text": "Shadows", "help": "Draws shadows.", "default": false, "storage": { "path": ["Main", "bShadows"] },
              "warnings": [ { "when": { "setting": "shadows", "equals": true }, "text": "Costly." },
                            { "when": { "setting": "shadows", "equals": true }, "text": "Second." } ] },
            { "type": "button", "text": "Rebuild", "action": { "type": "RunConsoleCommand", "command": "tm" },
              "warnings": [ { "when": { "setting": "shadows", "equals": true }, "text": "Load the save again." } ] },
            { "type": "switcher", "text": "Bad", "default": true, "storage": { "path": ["Main", "bBad"] },
              "warnings": [ { "when": { "setting": "shadows", "equals": true }, "text": "Not told." } ] }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Shadows: Draws shadows. Takes effect after Save. (Default: off)");
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Rebuild");

    // a pending value warns, and blocks nothing
    CHECK(pressActs(panel, pageRow(0)));
    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Shadows: Costly. Draws shadows. Takes effect after Save. (Default: off)");
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Rebuild: Load the save again.");
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == R"(Bad: Invalid: Main.bBad is "yes" in MyMod.toml, which is not true or false.)");
    CHECK(store.dirtyCount() == 1);

    // and is gone with the value
    CHECK(pressActs(panel, pageRow(0)));
    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Shadows: Draws shadows. Takes effect after Save. (Default: off)");
}

TEST_CASE("FullPanel: the help line writes a number as its display does, with a percentage's number, and a unit its box has no room for")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\niOpacity = 217\r\nfDistance = 250000.0\r\nfHeight = 1.5\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "slider", "valueType": "int", "text": "Ray opacity", "help": "How solid the ray is.", "default": 255,
              "valueOptions": { "min": 0, "max": 255, "step": 1 }, "display": { "percent": true }, "storage": { "path": ["Main", "iOpacity"] } },
            { "type": "slider", "text": "Draw distance", "help": "How far it is drawn.", "default": 1000.0,
              "valueOptions": { "min": 0, "max": 1000000, "step": 1 }, "display": { "unit": "meters" }, "storage": { "path": ["Main", "fDistance"] } },
            { "type": "textinput", "valueType": "float", "text": "Height", "default": 1.75, "display": { "decimals": 2, "unit": "m" },
              "storage": { "path": ["Main", "fHeight"] } }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Ray opacity: How solid the ray is. 85% (217 of 255). Takes effect after Save. (Default: 100%)");
    point(panel, resetButton(0));
    CHECK(panel.helpLine().text() == "Ray opacity: Reset to 100%");
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Draw distance: How far it is drawn. 250000 meters. Takes effect after Save. (Default: 1000 meters)");
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == "Height: Takes effect after Save. (Default: 1.75 m)");
}

TEST_CASE("FullPanel: a config spec's Reset is a change as any other, which Save writes and Discard takes back")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    const auto file = myGames.path / "Mods_Config" / "MyMod" / "MyMod.toml";
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nfScale = 1.5\r\nbEnable = false\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "slider", "text": "Scale", "default": 1.0, "valueOptions": { "min": 0.5, "max": 2.0, "step": 0.1 }, "storage": { "path": ["Main", "fScale"] } },
            { "type": "switcher", "text": "Enable", "default": false, "storage": { "path": ["Main", "bEnable"] } }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
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

TEST_CASE("FullPanel: with no config spec store none of a config spec's values is available, and the MCM store's session ends for it")
{
    const test_panel::PanelContext context;
    auto globals = someGlobals();
    test_events::TestEvents events;
    mcm::McmStore store;
    auto configSpec = modOf("Config spec");
    configSpec.source = model::Source::ConfigSpec;
    configSpec.frontPage.items.push_back(model::Setting{ .type = model::SettingType::Switcher, .text = "Holsters", .help = "Draw at the body.", .valueOptions = {} });
    auto panel = modWithValues(store);
    auto mods = panel.mods();
    mods.push_back(std::move(configSpec));
    panel.setMods(std::move(mods));
    panel.select(selectionOf(0));
    events.sent.clear();

    // Alpha, Beta, Config spec
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

namespace
{
    /**
     * A config spec's mod with sections that fold, shown on its front page, with its values in a TOML file of a
     * My Games folder. Its rows: a switcher, the heading of Advanced, which starts folded and has a switcher
     * whose value is invalid and another in it, the heading of Open, which starts open and has a switcher in
     * it, that switcher, and the heading of a section with a help that does not fold. The mod has a page with no rows.
     */
    FullPanel modWithFolds(spec::ConfigSpecStore& store, const test_files::TempFolder& myGames)
    {
        myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nbEnable = true\r\nbInvalid = \"yes\"\r\nbInner = false\r\nbShown = false\r\n");
        auto mod = spec::loadConfigSpec(R"({
            "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
            "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
            "defaultFile": "main",
            "content": [
                { "type": "switcher", "text": "Enable", "help": "Turns it on.", "storage": { "path": ["Main", "bEnable"] } },
                { "type": "section", "text": "Advanced", "help": "Fine-tuning.", "isCollapsible": true, "collapsed": true, "content": [
                    { "type": "switcher", "text": "Invalid", "storage": { "path": ["Main", "bInvalid"] } },
                    { "type": "switcher", "text": "Inner", "help": "In the folded section.", "storage": { "path": ["Main", "bInner"] } }
                ] },
                { "type": "section", "text": "Open", "isCollapsible": true, "content": [
                    { "type": "switcher", "text": "Shown", "help": "In the open section.", "storage": { "path": ["Main", "bShown"] } }
                ] },
                { "type": "section", "text": "Plain", "help": "A heading that does not fold.", "content": [] }
            ],
            "pages": [ { "pageDisplayName": "Other", "content": [] } ]
        })");
        REQUIRE(mod);
        FullPanel panel;
        panel.setStore(model::Source::ConfigSpec, &store);
        panel.setMods({ std::move(*mod) });
        panel.select(selectionOf(0));
        return panel;
    }
}

TEST_CASE("FullPanel: a section that folds starts as its collapsed says, and a press on its heading folds or opens it, and acts")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = modWithFolds(store, myGames);
    drawFrames(panel, 2);

    // Advanced is folded, and Open is open
    point(panel, pageRow(3));
    CHECK(panel.helpLine().text() == "Shown: In the open section. Takes effect after Save.");

    CHECK(pressActs(panel, pageRow(1)));
    point(panel, pageRow(3));
    CHECK(panel.helpLine().text() == "Inner: In the folded section. Takes effect after Save.");

    CHECK(pressActs(panel, pageRow(1)));
    point(panel, pageRow(3));
    CHECK(panel.helpLine().text() == "Shown: In the open section. Takes effect after Save.");

    // the heading of Open, with no help, and the one that does not fold, where its row was
    CHECK(pressActs(panel, pageRow(2)));
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == "Open");
    point(panel, pageRow(3));
    CHECK(panel.helpLine().text() == "Plain: A heading that does not fold.");
}

TEST_CASE("FullPanel: a section that folds starts as its collapsed says each time its page is shown")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = modWithFolds(store, myGames);
    // Advanced is opened and Open is folded, and then the page is shown again
    const auto foldBoth = [&panel] {
        drawFrames(panel, 2);
        CHECK(pressActs(panel, pageRow(1)));
        CHECK(pressActs(panel, pageRow(4)));
    };
    const auto checkAsCollapsedSays = [&panel] {
        drawFrames(panel, 2);
        point(panel, pageRow(2));
        CHECK(panel.helpLine().text() == "Open");
        point(panel, pageRow(3));
        CHECK(panel.helpLine().text() == "Shown: In the open section. Takes effect after Save.");
    };

    // after another page
    foldBoth();
    panel.select(selectionOf(0, 0));
    panel.back();
    checkAsCollapsedSays();

    // after the Help page
    foldBoth();
    panel.showHelp(true);
    panel.back();
    checkAsCollapsedSays();
}

TEST_CASE("FullPanel: the help line says what a folded section hides: its invalid values, and its changes that wait for a Save")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = modWithFolds(store, myGames);
    drawFrames(panel, 2);

    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Advanced: Fine-tuning. Folded: 1 invalid value.");

    // open, it says nothing of its rows, which show their own marks
    CHECK(pressActs(panel, pageRow(1)));
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Advanced: Fine-tuning.");

    CHECK(pressActs(panel, pageRow(3)));
    CHECK(pressActs(panel, pageRow(1)));
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Advanced: Fine-tuning. Folded: 1 invalid value, 1 unsaved change.");

    // a section with no help, which hides nothing
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == "Open");
    CHECK(pressActs(panel, pageRow(3)));
    CHECK(pressActs(panel, pageRow(2)));
    point(panel, pageRow(2));
    CHECK(panel.helpLine().text() == "Open: Folded: 1 unsaved change.");
}

namespace
{
    /**
     * A config spec's mod, as the loader gives it, with its problems: an error and a warning for each count.
     */
    model::Mod configSpecWith(const std::string& name, const int errors, const int warnings, const std::string& notAvailable = {})
    {
        model::Mod mod{ .name = name, .displayName = name, .notAvailable = notAvailable, .source = model::Source::ConfigSpec };
        for (int i = 0; i < errors; ++i) {
            mod.problems.push_back({ .severity = model::Severity::Error, .row = "spec.json", .message = "It cannot be read: line 4.", .where = {}, .path = {} });
        }
        for (int i = 0; i < warnings; ++i) {
            mod.problems.push_back({ .severity = model::Severity::Warning, .row = "A row", .message = "Unknown field.", .where = {}, .path = {} });
        }
        return mod;
    }

    /**
     * The menu's front page with Developer tools on: an MCM mod, a config spec with an error and two warnings, one
     * with a warning, one with none, one that cannot be read, and one whose plugin is not loaded.
     */
    FullPanel developerPanel(ui::DeveloperView view, std::vector<ui::DeveloperView>& written)
    {
        std::vector<model::Mod> mods;
        mods.push_back(modOf("An MCM mod"));
        mods.push_back(configSpecWith("Beam", 1, 2));
        mods.push_back(configSpecWith("Clean", 0, 0));
        mods.push_back(configSpecWith("Hands", 0, 1));
        mods.push_back(configSpecWith("Holsters", 1, 0, "its spec.json cannot be read"));
        mods.push_back(configSpecWith("Settlements", 0, 0, "its plugin SS2.esm is not loaded"));
        FullPanel panel;
        panel.setMods(std::move(mods));
        view.enabled = true;
        panel.setDeveloper(view, [&written](const ui::DeveloperView& changed) {
            written.push_back(changed);
        });
        return panel;
    }

    /**
     * The place of the front page's row whose help line starts with a text, found by moving the ray down the
     * page. None where no row has it.
     */
    std::optional<ImVec2> rowWithHelp(FullPanel& panel, const std::string_view start)
    {
        for (float y = 100.0f; y < test_panel::PANEL_HEIGHT - 120.0f; y += 6.0f) {
            point(panel, ImVec2(640.0f, y));
            if (panel.helpLine().text().starts_with(start)) {
                return ImVec2(640.0f, y + 8.0f);
            }
        }
        return std::nullopt;
    }

    /**
     * The names of the config specs the Developer section lists, by the rows the ray finds under its filter.
     */
    Names listedConfigSpecs(FullPanel& panel)
    {
        Names names;
        for (const auto* name : { "An MCM mod", "Beam", "Clean", "Hands", "Holsters", "Settlements" }) {
            if (rowWithHelp(panel, std::string(name) + ":")) {
                names.push_back(name);
            }
        }
        return names;
    }
}

TEST_CASE("FullPanel: with Developer tools on, the front page lists the config specs that need attention, with their errors and warnings")
{
    const test_panel::PanelContext context;
    std::vector<ui::DeveloperView> written;

    // off, there is no section
    {
        auto panel = developerPanel({}, written);
        panel.setDeveloper({}, {});
        drawFrames(panel, 2);
        CHECK_FALSE(rowWithHelp(panel, "Developer:"));
    }

    auto panel = developerPanel({}, written);
    drawFrames(panel, 2);
    CHECK(rowWithHelp(panel, "Developer: The problems of each config spec"));
    CHECK(rowWithHelp(panel, "Show: Which config specs this section lists"));
    CHECK(listedConfigSpecs(panel) == Names{ "Beam", "Hands", "Holsters" });

    rowWithHelp(panel, "Beam:");
    CHECK(panel.helpLine().text() == "Beam: 1 error: parts of it are left out. 2 warnings: read, and likely not what is meant. Opens its report.");
    CHECK_FALSE(panel.helpLine().isInvalid());
    rowWithHelp(panel, "Holsters:");
    CHECK(panel.helpLine().text() == "Holsters: It cannot be read: line 4. Opens its report.");
    CHECK(panel.helpLine().isInvalid());
    CHECK(written.empty());
}

TEST_CASE("FullPanel: the Developer section's filter steps to all the config specs and to the one the settings name, and its fold is written at once")
{
    const test_panel::PanelContext context;
    std::vector<ui::DeveloperView> written;
    auto panel = developerPanel({ .enabled = true, .show = ui::ShowConfigSpecs::NeedingAttention, .configSpec = "hands", .folded = false }, written);
    drawFrames(panel, 2);

    // a press on the filter's row goes to the option after, which is written
    const auto filter = rowWithHelp(panel, "Show:");
    REQUIRE(filter);
    CHECK(pressActs(panel, *filter));
    CHECK(panel.developerView().show == ui::ShowConfigSpecs::All);
    REQUIRE(written.size() == 1);
    CHECK(written.back().show == ui::ShowConfigSpecs::All);
    CHECK(listedConfigSpecs(panel) == Names{ "Beam", "Clean", "Hands", "Holsters", "Settlements" });
    rowWithHelp(panel, "Settlements:");
    CHECK(panel.helpLine().text() == "Settlements: Not available: its plugin SS2.esm is not loaded. Opens its report.");
    rowWithHelp(panel, "Clean:");
    CHECK(panel.helpLine().text() == "Clean: No problems. Opens its report.");

    // the third option is the config spec the settings name, alone
    CHECK(pressActs(panel, *rowWithHelp(panel, "Show:")));
    CHECK(panel.developerView().show == ui::ShowConfigSpecs::OnlyOne);
    CHECK(listedConfigSpecs(panel) == Names{ "Hands" });

    // a press on the heading folds it, with no rows under it, and its help counts what it hides
    const auto heading = rowWithHelp(panel, "Developer:");
    REQUIRE(heading);
    CHECK(pressActs(panel, *heading));
    CHECK(written.back().folded);
    CHECK_FALSE(rowWithHelp(panel, "Show:"));
    point(panel, *heading);
    CHECK(panel.helpLine().text().ends_with(" Folded: 1 warning."));
}

TEST_CASE("FullPanel: the Developer section's filter has no option of a config spec the settings do not name")
{
    const test_panel::PanelContext context;
    std::vector<ui::DeveloperView> written;
    auto panel = developerPanel({ .enabled = true, .show = ui::ShowConfigSpecs::All, .configSpec = "", .folded = false }, written);
    drawFrames(panel, 2);
    // from All, the option after is the first again
    CHECK(pressActs(panel, *rowWithHelp(panel, "Show:")));
    CHECK(panel.developerView().show == ui::ShowConfigSpecs::NeedingAttention);
}

namespace
{
    /**
     * The menu's front page with Developer tools on, and one config spec with problems: a switcher with no storage
     * in a section of its page Beam, and an unknown field of a row of its front page, which a config spec store
     * has the values of.
     */
    FullPanel reportPanel(spec::ConfigSpecStore& store)
    {
        auto mod = spec::loadConfigSpec(R"({
            "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "save",
            "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
            "defaultFile": "main",
            "content": [ { "type": "spacer", "lines": 2 } ],
            "pages": [ { "pageDisplayName": "Beam", "content": [
                { "type": "switcher", "text": "Enable", "default": false, "storage": { "path": ["Main", "bEnable"] } },
                { "type": "section", "text": "Shape", "isCollapsible": true, "collapsed": true, "content": [
                    { "type": "switcher", "text": "Angle", "default": true }
                ] }
            ] } ]
        })");
        REQUIRE(mod);
        mod->folder = "MyFolder";
        FullPanel panel;
        panel.setStore(model::Source::ConfigSpec, &store);
        panel.setMods({ std::move(*mod) });
        panel.setDeveloper({ .enabled = true, .show = ui::ShowConfigSpecs::All, .configSpec = {}, .folded = false }, {});
        return panel;
    }
}

TEST_CASE("FullPanel: a press on a config spec of the Developer section opens its report, whose problems tell where they are, and Back goes back")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = reportPanel(store);
    drawFrames(panel, 2);

    const auto row = rowWithHelp(panel, "My Mod:");
    REQUIRE(row);
    CHECK(panel.helpLine().text() == "My Mod: 1 error: parts of it are left out. 1 warning: read, and likely not what is meant. Opens its report.");
    CHECK(pressActs(panel, *row));
    CHECK(panel.report() == 0u);
    CHECK_FALSE(panel.selection().mod);

    // a problem tells its message, where it is, and that it goes to its row
    rowWithHelp(panel, "Angle:");
    CHECK(panel.helpLine().text() == "Angle: It has no storage: it is not available. Beam \xE2\x80\xBA Angle \xC2\xB7 pages[0].content[1].content[0]. Goes to the row.");
    CHECK_FALSE(panel.helpLine().isInvalid());
    CHECK(rowWithHelp(panel, "Errors: What the menu left out"));
    // the warnings are under the errors, at the end of the page, which is scrolled to them
    point(panel, ImVec2(640.0f, 400.0f));
    ImGui::GetIO().AddMouseWheelEvent(0.0f, -10.0f);
    point(panel, ImVec2(640.0f, 400.0f));
    rowWithHelp(panel, "spacer:");
    CHECK(panel.helpLine().text() == "spacer: Unknown field \"lines\": it is not read. spacer \xC2\xB7 content[0].lines. Goes to the row.");

    // Back goes to the menu's front page, and so does its crumb
    CHECK(panel.back());
    CHECK_FALSE(panel.report());
    CHECK(pressActs(panel, *rowWithHelp(panel, "My Mod:")));
    CHECK(panel.report() == 0u);
    CHECK(pressActs(panel, ImVec2(test_panel::CONTENT_START + 100.0f, test_panel::CONTENT_START + theme::TITLEBAR_HEIGHT * 0.5f)));
    CHECK_FALSE(panel.report());
    CHECK_FALSE(panel.selection().mod);
}

TEST_CASE("FullPanel: a press on a problem of a report shows its row's page, with its section open, and Back shows the report again")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = reportPanel(store);
    drawFrames(panel, 2);
    CHECK(panel.openReport(0));
    drawFrames(panel, 2);

    CHECK(pressActs(panel, *rowWithHelp(panel, "Angle:")));
    CHECK_FALSE(panel.report());
    CHECK(panel.selection() == Selection{ .mod = 0, .page = { 0 } });
    // the section that starts folded is open, so its row is drawn
    CHECK(rowWithHelp(panel, "Angle:"));

    CHECK(panel.back());
    CHECK(panel.report() == 0u);
    CHECK_FALSE(panel.selection().mod);
    CHECK(panel.back());
    CHECK_FALSE(panel.report());

    // with a change that waits for a Save, Back asks first, and the dialog's Discard shows the report
    CHECK(panel.openReport(0));
    drawFrames(panel, 2);
    CHECK(pressActs(panel, *rowWithHelp(panel, "Angle:")));
    CHECK(pressActs(panel, *rowWithHelp(panel, "Enable:")));
    CHECK(store.dirtyCount() == 1);
    CHECK(panel.back());
    CHECK(panel.isAsking());
    drawFrames(panel, 2);
    CHECK(pressActs(panel, dialogButton(DIALOG_DISCARD)));
    CHECK(panel.report() == 0u);
    CHECK(store.dirtyCount() == 0);
}

TEST_CASE("FullPanel: a report of a config spec that cannot be read tells its error as one, and has no row to go to")
{
    const test_panel::PanelContext context;
    model::Mod broken{ .name = "Broken", .displayName = "Broken", .notAvailable = "its spec.json cannot be read", .source = model::Source::ConfigSpec };
    broken.problems.push_back({ .severity = model::Severity::Error, .row = "spec.json", .message = "It cannot be read: line 4.", .where = {}, .path = {} });
    FullPanel panel;
    panel.setMods({ std::move(broken) });
    panel.setDeveloper({ .enabled = true, .show = ui::ShowConfigSpecs::All, .configSpec = {}, .folded = false }, {});
    drawFrames(panel, 2);
    CHECK(panel.openReport(0));
    drawFrames(panel, 2);
    const auto row = rowWithHelp(panel, "spec.json:");
    REQUIRE(row);
    CHECK(panel.helpLine().text() == "spec.json: It cannot be read: line 4.");
    CHECK(panel.helpLine().isInvalid());
    CHECK_FALSE(pressActs(panel, *row));
    CHECK(panel.report() == 0u);
}

namespace
{
    // where the report says the live copy of the config spec of `reportPanel` is
    constexpr auto LIVE_COPY = R"(My Games\Fallout4VR\Mods_Config\ModsConfigVR\Dev\MyFolder\spec.json)";

    /**
     * The config spec of `reportPanel` as its live copy is read, by its name, with its page Beam or with no page.
     */
    model::Mod liveCopyOf(const std::string& displayName, const bool withPage = true)
    {
        auto mod = spec::loadConfigSpec(std::format(R"({{
            "minSchemaVersion": 1, "modName": "MyMod", "displayName": "{}", "applies": "save",
            "files": {{ "main": {{ "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" }} }},
            "defaultFile": "main",
            "pages": [ {} ]
        }})",
            displayName,
            withPage ? R"({ "pageDisplayName": "Beam", "content": [ { "type": "switcher", "text": "Enable", "default": false, "storage": { "path": ["Main", "bEnable"] } } ] })"
                     : ""));
        REQUIRE(mod);
        mod->folder = "MyFolder";
        mod->liveCopy = LIVE_COPY;
        return std::move(*mod);
    }

    /**
     * Opens the sub-section of the live copy of the report that is shown, which starts folded.
     */
    void openLiveSpec(FullPanel& panel)
    {
        // the help line keeps the row it had until the ray is on another, so the ray is on the row before first
        CHECK(rowWithHelp(panel, "Read the live copy:"));
        const auto heading = rowWithHelp(panel, "Live spec.json:");
        REQUIRE(heading);
        CHECK(pressActs(panel, *heading));
    }
}

TEST_CASE("FullPanel: a report's live copy switch asks the menu, and Reload, Open and Show wait for it")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = reportPanel(store);
    std::vector<std::pair<std::string, ui::LiveCopyAction>> asked;
    panel.setLiveCopyAction([&asked](const std::string& folder, const ui::LiveCopyAction action) {
        asked.emplace_back(folder, action);
    });
    CHECK(panel.openReport(0));
    drawFrames(panel, 2);
    openLiveSpec(panel);

    // off, the rows of the copy are disabled, and say why
    const auto reload = rowWithHelp(panel, "Reload:");
    REQUIRE(reload);
    CHECK(panel.helpLine().text() == "Reload: Reads the live copy again now. Read the live copy first.");
    CHECK_FALSE(pressActs(panel, *reload));
    CHECK(rowWithHelp(panel, "Edit on desktop:"));
    CHECK(rowWithHelp(panel, "Reveal in file explorer:"));
    CHECK(asked.empty());

    const auto toggle = rowWithHelp(panel, "Read the live copy:");
    REQUIRE(toggle);
    CHECK(pressActs(panel, *toggle));
    REQUIRE(asked.size() == 1);
    CHECK(asked.back() == std::pair{ std::string("MyFolder"), ui::LiveCopyAction::TurnOn });

    // read from the copy, a press on each asks it
    panel.replaceMod(liveCopyOf("My Mod"), "10:42:05");
    drawFrames(panel, 2);
    CHECK(pressActs(panel, *rowWithHelp(panel, "Reload:")));
    CHECK(asked.back().second == ui::LiveCopyAction::Reload);
    CHECK(pressActs(panel, *rowWithHelp(panel, "Re-copy the installed spec.json:")));
    CHECK(asked.back().second == ui::LiveCopyAction::Overwrite);
    CHECK(pressActs(panel, *rowWithHelp(panel, "Edit on desktop:")));
    CHECK(asked.back().second == ui::LiveCopyAction::Open);
    CHECK(pressActs(panel, *rowWithHelp(panel, "Reveal in file explorer:")));
    CHECK(asked.back().second == ui::LiveCopyAction::Show);
    CHECK(pressActs(panel, *rowWithHelp(panel, "Read the live copy:")));
    CHECK(asked.back().second == ui::LiveCopyAction::TurnOff);
}

TEST_CASE("FullPanel: a config spec read again drops its unsaved changes and keeps the page that is shown while it is there")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = reportPanel(store);
    panel.select(pageOf({ 0 }));
    drawFrames(panel, 2);
    CHECK(pressActs(panel, *rowWithHelp(panel, "Enable:")));
    CHECK(store.dirtyCount() == 1);

    CHECK(panel.replaceMod(liveCopyOf("My Mod"), "10:42:05") == 1);
    CHECK(panel.selection() == pageOf({ 0 }));
    CHECK(store.dirtyCount() == 0);
    CHECK(panel.mods()[0].liveCopy == LIVE_COPY);
    REQUIRE(panel.liveCopyRead("MyFolder"));
    CHECK(panel.liveCopyRead("MyFolder")->result == "Reloaded at 10:42:05: 1 unsaved change was dropped.");

    // the session is the new mod's: its row is set as before
    drawFrames(panel, 2);
    CHECK(pressActs(panel, *rowWithHelp(panel, "Enable:")));
    CHECK(store.dirtyCount() == 1);

    // a page that is gone shows the mod's front page
    CHECK(panel.replaceMod(liveCopyOf("My Mod", false), "10:43:00") == 1);
    CHECK(panel.selection() == Selection{ .mod = 0 });
    CHECK(panel.liveCopyRead("MyFolder")->result == "Reloaded at 10:43:00: 1 unsaved change was dropped.");

    // a mod that is not available now shows the menu's front page
    auto missing = liveCopyOf("My Mod");
    missing.notAvailable = "its plugin SS2.esm is not loaded";
    CHECK(panel.replaceMod(std::move(missing), "10:44:00") == 0);
    CHECK(panel.selection() == Selection{});
    CHECK(panel.liveCopyRead("MyFolder")->result == "Reloaded at 10:44:00.");

    // a folder the panel has no config spec of is not put in
    auto other = liveCopyOf("Other");
    other.folder = "OtherFolder";
    CHECK(panel.replaceMod(std::move(other), "10:45:00") == 0);
    CHECK(panel.mods().size() == 1);
    CHECK_FALSE(panel.liveCopyRead("OtherFolder"));
}

TEST_CASE("FullPanel: a config spec read again keeps its place among the mods by its name, and its report stays")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto mine = liveCopyOf("My Mod");
    mine.liveCopy.clear();
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ modOf("Beta"), std::move(mine) });
    CHECK(namesOf(panel) == Names{ "Beta", "My Mod" });
    CHECK(panel.openReport(1));

    panel.replaceMod(liveCopyOf("Alpha"), "10:42:05");
    CHECK(namesOf(panel) == Names{ "Alpha", "Beta" });
    CHECK(panel.report() == 0u);
}

TEST_CASE("FullPanel: a live copy that cannot be read keeps what was read last, with its error first, and Reload tells it")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = reportPanel(store);
    const auto problems = panel.mods()[0].problems.size();
    panel.liveCopyNotRead("MyFolder", LIVE_COPY, "10:42:05", "Not valid JSON: line 4");
    panel.liveCopyNotRead("MyFolder", LIVE_COPY, "10:42:09", "Not valid JSON: line 5");

    const auto& mod = panel.mods()[0];
    CHECK(mod.liveCopy == LIVE_COPY);
    REQUIRE(mod.problems.size() == problems + 1);
    CHECK(mod.problems[0].message == "The live copy cannot be read: Not valid JSON: line 5. The menu shows the copy it read last.");
    CHECK(mod.pages.size() == 1);

    CHECK(panel.openReport(0));
    drawFrames(panel, 2);
    CHECK(rowWithHelp(panel, "spec.json: The live copy cannot be read"));
    CHECK(panel.helpLine().isInvalid());
    openLiveSpec(panel);
    CHECK(rowWithHelp(panel, "Reload:"));
    CHECK(panel.helpLine().text() == "Reload: Read at 10:42:09: the live copy cannot be read. The menu shows the copy it read last.");
    CHECK(panel.helpLine().isInvalid());

    // the Developer section says it is read from its copy
    CHECK(panel.back());
    drawFrames(panel, 2);
    CHECK(rowWithHelp(panel, "My Mod: Read from its live copy. 2 errors"));
}

TEST_CASE("FullPanel: Reload has the info mark of a read until the ray leaves it, and the result in the help line")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = reportPanel(store);
    CHECK(panel.openReport(0));
    panel.replaceMod(liveCopyOf("My Mod"), "10:42:05");
    drawFrames(panel, 2);
    openLiveSpec(panel);
    CHECK_FALSE(panel.liveCopyRead("MyFolder")->seen);

    CHECK(rowWithHelp(panel, "Reload:"));
    CHECK(panel.helpLine().text() == "Reload: Reloaded at 10:42:05. Reads the live copy again now.");
    CHECK_FALSE(panel.liveCopyRead("MyFolder")->seen);
    CHECK(rowWithHelp(panel, "Edit on desktop:"));
    CHECK(panel.liveCopyRead("MyFolder")->seen);
}

TEST_CASE("FullPanel: Overwrite warns while the installed spec.json is newer than the live copy")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = reportPanel(store);
    CHECK(panel.openReport(0));
    panel.replaceMod(liveCopyOf("My Mod"), "10:42:05");
    drawFrames(panel, 2);
    openLiveSpec(panel);
    CHECK(rowWithHelp(panel, "Re-copy the installed spec.json:"));
    CHECK(panel.helpLine().text() == "Re-copy the installed spec.json: Copies the installed spec.json over the live copy, which loses the copy's edits, and reads it.");

    panel.setLiveCopyStale("MyFolder", true);
    // the help line keeps the row it had until the ray is on another
    CHECK(rowWithHelp(panel, "Reload:"));
    CHECK(rowWithHelp(panel, "Re-copy the installed spec.json:"));
    CHECK(panel.helpLine().text() ==
          "Re-copy the installed spec.json: The installed spec.json is newer than the live copy. Copies the installed spec.json over the live copy, which loses the copy's "
          "edits, and reads it.");

    // the next read says it again, or not
    panel.replaceMod(liveCopyOf("My Mod"), "10:43:00");
    CHECK_FALSE(panel.liveCopyRead("MyFolder")->stale);
}

TEST_CASE("FullPanel: the sub-section of the live copy starts folded, and says while it is folded that the copy is stale")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = reportPanel(store);
    CHECK(panel.openReport(0));
    panel.replaceMod(liveCopyOf("My Mod"), "10:42:05");
    panel.setLiveCopyStale("MyFolder", true);
    drawFrames(panel, 2);
    CHECK_FALSE(rowWithHelp(panel, "Reload:"));
    CHECK(rowWithHelp(panel, "Live spec.json:"));
    CHECK(panel.helpLine().text() == "Live spec.json: The installed spec.json is newer than the live copy. Reload, overwrite, edit and reveal the live copy.");

    openLiveSpec(panel);
    CHECK(rowWithHelp(panel, "Reload:"));
    CHECK(rowWithHelp(panel, "Live spec.json:"));
    CHECK(panel.helpLine().text() == "Live spec.json: Reload, overwrite, edit and reveal the live copy.");

    // it stays open while the menu is, and is folded again as it opens
    CHECK(panel.back());
    CHECK(panel.openReport(0));
    drawFrames(panel, 2);
    CHECK(rowWithHelp(panel, "Reload:"));
    panel.reset();
    CHECK(panel.openReport(0));
    drawFrames(panel, 2);
    CHECK_FALSE(rowWithHelp(panel, "Reload:"));
}

namespace
{
    /**
     * The pane of the pages, which a page and a report are scrolled in.
     */
    const ImGuiWindow& contentWindow()
    {
        const auto& windows = ImGui::GetCurrentContext()->Windows;
        const auto found = std::ranges::find_if(windows, [](const ImGuiWindow* window) {
            return std::strstr(window->Name, "/Content") != nullptr;
        });
        REQUIRE(found != windows.end());
        return **found;
    }
}

TEST_CASE("FullPanel: Back from the row of a problem shows its report where it was scrolled to")
{
    const test_panel::PanelContext context;
    auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPEC_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store(std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR).parent_path().parent_path(), myGames.path);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods(std::move(mods));
    CHECK(panel.openReport(0));
    drawFrames(panel, 2);

    // the report scrolled down a little, and a press on a problem there
    point(panel, ImVec2(640.0f, 400.0f));
    ImGui::GetIO().AddMouseWheelEvent(0.0f, -3.0f);
    point(panel, ImVec2(640.0f, 400.0f));
    const float scrolled = contentWindow().Scroll.y;
    REQUIRE(scrolled > 100.0f);
    const auto& problems = panel.mods()[0].problems;
    const auto onRow = std::ranges::find_if(problems, [](const model::Problem& problem) {
        return problem.place.has_value();
    });
    REQUIRE(onRow != problems.end());
    REQUIRE(panel.showProblem(0, static_cast<std::size_t>(onRow - problems.begin())));
    drawFrames(panel, 3);

    CHECK(panel.back());
    CHECK(panel.report() == 0u);
    drawFrames(panel, 3);
    CHECK(contentWindow().Scroll.y == Catch::Approx(scrolled));

    // opened from the Developer section, it is at its top
    CHECK(panel.back());
    CHECK(panel.openReport(0));
    drawFrames(panel, 3);
    CHECK(contentWindow().Scroll.y == Catch::Approx(0.0f));
}

TEST_CASE("FullPanel: the dialog is answered as a press on its button does, and only while it asks")
{
    const test_files::TempFolder myGames;
    const auto file = myGames.path / MY_MOD_FILE;
    myGames.write(MY_MOD_FILE, "[Main]\r\nbEnable = false\r\nfHeight = 1.5\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto panel = twoConfigSpecs(store);
    CHECK_FALSE(panel.answerDialog(ui::DialogAnswer::Cancel));

    const auto& enable = std::get<model::Setting>(panel.mods()[0].frontPage.items[0]);
    const auto& height = std::get<model::Setting>(panel.mods()[0].frontPage.items[1]);
    CHECK(store.set(enable, true));
    CHECK(panel.select(selectionOf(1)));
    CHECK(panel.isAsking());

    // Cancel stays on the mod, with its changes
    CHECK(panel.answerDialog(ui::DialogAnswer::Cancel));
    CHECK_FALSE(panel.isAsking());
    CHECK(panel.selection() == selectionOf(0));
    CHECK(store.dirtyCount() == 1);

    // Save is not pressed while a change is invalid
    CHECK(store.set(height, "3"s));
    CHECK(panel.select(selectionOf(1)));
    CHECK_FALSE(panel.answerDialog(ui::DialogAnswer::Save));
    CHECK(panel.isAsking());

    // Save writes the changes and shows what was selected
    CHECK(store.set(height, "2"s));
    CHECK(panel.answerDialog(ui::DialogAnswer::Save));
    CHECK_FALSE(panel.isAsking());
    CHECK(panel.selection() == selectionOf(1));
    CHECK(fileText(file) == "[Main]\r\nbEnable = true\r\nfHeight = 2.0\r\n");
}

TEST_CASE("FullPanel: a row follows a state of its mod's client, which is asked once a frame")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "save",
        "content": [
            { "type": "button", "text": "Calibrate", "help": "Measures your height.", "action": { "type": "RunConsoleCommand", "command": "tm" },
              "enabledWhen": { "clientState": "calibrating", "equals": false },
              "warnings": [ { "text": "Stand still until it is done.", "when": { "clientState": "calibrating" } } ] },
            { "type": "button", "text": "Unknown state", "help": "Its state is not registered.", "action": { "type": "RunConsoleCommand", "command": "tm" },
              "enabledWhen": { "clientState": "other" }, "warnings": [ { "text": "Not told.", "when": { "clientState": "other" } } ] }
        ]
    })");
    REQUIRE(mod);
    test_actions::TestActions actions;
    bool calibrating = true;
    int asked = 0;
    auto& functions = store.clientCalls();
    functions.addClient("MyMod");
    functions.addState("MyMod", "calibrating", [&calibrating, &asked] {
        ++asked;
        return std::optional<model::Value>(calibrating);
    });
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select(selectionOf(0));
    drawFrames(panel, 2);

    // disabled while its client is calibrating, and it warns
    asked = 0;
    drawFrames(panel, 3);
    CHECK(asked == 3);
    CHECK_FALSE(pressActs(panel, pageRow(0)));
    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Calibrate: Stand still until it is done. Measures your height.");

    // enabled as the state changes, with no warning
    calibrating = false;
    drawFrames(panel, 1);
    CHECK(pressActs(panel, pageRow(0)));
    CHECK(actions.commands.size() == 1);
    point(panel, pageRow(0));
    CHECK(panel.helpLine().text() == "Calibrate: Measures your height.");

    // a state that is not available leaves its condition open: shown, enabled, and with no warning
    CHECK(pressActs(panel, pageRow(1)));
    point(panel, pageRow(1));
    CHECK(panel.helpLine().text() == "Unknown state: Its state is not registered.");
}

TEST_CASE("FullPanel: a mod's client is told as its mod and the pages of its config spec that have an id are shown and left")
{
    const test_panel::PanelContext context;
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    const auto configSpec = [](const std::string& name) {
        return spec::loadConfigSpec(std::format(R"({{
            "minSchemaVersion": 1, "modName": "{0}", "displayName": "{0}", "applies": "save",
            "content": [ {{ "type": "text", "text": "Front" }} ],
            "pages": [
                {{ "pageDisplayName": "Spheres", "id": "spheres", "content": [],
                   "pages": [ {{ "pageDisplayName": "Left", "id": "leftSphere", "content": [] }} ] }},
                {{ "pageDisplayName": "No id", "content": [] }}
            ]
        }})",
            name));
    };
    auto first = configSpec("MyMod");
    auto second = configSpec("OtherMod");
    REQUIRE(first);
    REQUIRE(second);
    std::vector<std::string> events;
    auto& calls = store.clientCalls();
    for (const auto* mod : { "MyMod", "OtherMod" }) {
        calls.addClient(mod);
        calls.addEvents(mod, [&events, mod](const model::ClientEvent event, const std::string_view pageId) {
            constexpr std::array NAMES{ "MenuOpened", "MenuClosed", "ModShown", "ModLeft", "PageShown", "PageLeft" };
            events.push_back(std::format("{} {}{}{}", mod, NAMES[static_cast<int>(event)], pageId.empty() ? "" : " ", pageId));
        });
    }
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*first), std::move(*second) });

    // the mod's front page has no id
    panel.select(selectionOf(0));
    CHECK(events == std::vector<std::string>{ "MyMod ModShown" });
    events.clear();

    // a page under a page, from its page, and one with no id
    panel.select(selectionOf(0, 0));
    panel.select({ .mod = 0, .page = { 0, 0 } });
    panel.select(selectionOf(0, 1));
    CHECK(events == std::vector<std::string>{ "MyMod PageShown spheres", "MyMod PageLeft spheres", "MyMod PageShown leftSphere", "MyMod PageLeft leftSphere" });
    events.clear();

    // another mod leaves the page and the mod
    panel.select(selectionOf(0, 0));
    panel.select(selectionOf(1, 0));
    CHECK(events == std::vector<std::string>{ "MyMod PageShown spheres", "MyMod PageLeft spheres", "MyMod ModLeft", "OtherMod ModShown", "OtherMod PageShown spheres" });
    events.clear();

    // and the menu closing leaves both, as the panel is reset
    panel.reset();
    CHECK(events == std::vector<std::string>{ "OtherMod PageLeft spheres", "OtherMod ModLeft" });

    // the menu's own events go to every client
    events.clear();
    calls.sendToAll(model::ClientEvent::MenuOpened);
    CHECK(events == std::vector<std::string>{ "MyMod MenuOpened", "OtherMod MenuOpened" });
}
