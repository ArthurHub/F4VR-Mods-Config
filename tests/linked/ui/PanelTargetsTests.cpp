#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "TestFiles.h"
#include "TestPanel.h"
#include "spec/ConfigSpecLoader.h"
#include "spec/ConfigSpecStore.h"
#include "ui/FullPanel.h"
#include "ui/PanelTargets.h"

using namespace mods_config;
using test_panel::drawFrames;
using test_panel::pressActs;
using ui::TargetKind;

namespace
{
    constexpr auto MY_MOD_FILE = R"(Mods_Config\MyMod\MyMod.toml)";

    /**
     * Keeps the panel's targets while a test runs, and stops as it ends: the recorder is the one of the process.
     */
    struct Recording
    {
        Recording()
        {
            ui::panelTargets().setRecording(true);
        }

        Recording(const Recording&) = delete;
        Recording& operator=(const Recording&) = delete;

        ~Recording()
        {
            ui::panelTargets().setRecording(false);
        }
    };

    const ui::Target* findTarget(const TargetKind kind, const std::string_view key)
    {
        const auto& targets = ui::panelTargets().targets();
        const auto found = std::ranges::find_if(targets, [kind, key](const ui::Target& target) {
            return target.kind == kind && target.key == key;
        });
        return found != targets.end() ? &*found : nullptr;
    }

    /**
     * The middle of a target of the last frame, by its kind and key, where the ray presses it.
     */
    ImVec2 middleOf(const TargetKind kind, const std::string_view key)
    {
        const auto* target = findTarget(kind, key);
        REQUIRE(target);
        return target->rect.GetCenter();
    }

    /**
     * My Mod: a section with a switcher, a color and a dropdown under it, and a page under its front page.
     */
    model::Mod myMod()
    {
        auto mod = spec::loadConfigSpec(R"({
            "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod",
            "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
            "defaultFile": "main",
            "content": [
                { "type": "section", "text": "Look", "content": [
                    { "type": "switcher", "id": "enabled", "text": "Enabled", "default": false, "storage": { "path": ["Main", "bEnabled"] } },
                    { "type": "cf.color", "id": "ray", "text": "Ray", "default": { "rgb": [190, 255, 190] }, "valueOptions": { "alpha": false },
                      "storage": { "path": ["Main", "cRay"] } },
                    { "type": "dropdown", "id": "hand", "valueType": "string", "text": "Hand", "default": "offhand", "storage": { "path": ["Main", "sHand"] },
                      "valueOptions": { "options": [ { "value": "primary", "text": "Primary" }, { "value": "offhand", "text": "Offhand" } ] } }
                ] }
            ],
            "pages": [ { "pageDisplayName": "Second", "content": [] } ]
        })");
        REQUIRE(mod);
        return std::move(*mod);
    }
}

TEST_CASE("PanelTargets: a frame keeps nothing until recording is on")
{
    const test_panel::PanelContext context;
    ui::FullPanel panel;
    panel.setMods({ myMod() });
    drawFrames(panel, 2);
    CHECK(ui::panelTargets().targets().empty());
}

TEST_CASE("PanelTargets: the rows of the tree and the page, the title bar, the color editor and a dropdown's options are where a press on them acts")
{
    const test_panel::PanelContext context;
    const Recording recording;
    const test_files::TempFolder myGames;
    myGames.write(MY_MOD_FILE, "[Main]\r\nbEnabled = false\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    ui::FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ myMod() });
    drawFrames(panel, 2);

    const auto scrolled = [](const ui::ScrollArea area) {
        return std::ranges::any_of(ui::panelTargets().scrolls(), [area](const ui::Scroll& scroll) {
            return scroll.area == area;
        });
    };
    CHECK(scrolled(ui::ScrollArea::Tree));
    CHECK(scrolled(ui::ScrollArea::Page));
    CHECK(findTarget(TargetKind::TitleButton, "Help"));
    CHECK(findTarget(TargetKind::TitleButton, "Close"));
    CHECK(findTarget(TargetKind::Crumb, "MODS CONFIG FO4"));

    // the mod's row selects it, and then its page's row is there, by the mod's name and the page's
    CHECK(pressActs(panel, middleOf(TargetKind::Tree, "MyMod")));
    drawFrames(panel, 2);
    CHECK(panel.selection().mod == 0u);
    const auto& targets = ui::panelTargets().targets();
    const auto page = std::ranges::find_if(targets, [](const ui::Target& target) {
        return target.kind == TargetKind::Tree && target.path == std::vector<std::string>{ "Second" };
    });
    REQUIRE(page != targets.end());
    CHECK(page->key == "MyMod");

    // a setting's row by its id, under its section's heading
    CHECK(findTarget(TargetKind::Section, "Look"));
    const auto& enabled = std::get<model::Setting>(std::get<model::Group>(panel.mods()[0].frontPage.items[0]).items[0]);
    CHECK(pressActs(panel, middleOf(TargetKind::Setting, "enabled")));
    CHECK(std::get<bool>(store.value(enabled).value));

    // the color's row opens its editor, whose channels and buttons are there while it is open
    CHECK(pressActs(panel, middleOf(TargetKind::Setting, "ray")));
    drawFrames(panel, 2);
    const auto* red = findTarget(TargetKind::ColorChannel, "Red");
    REQUIRE(red);
    const auto editor = panel.openEditor();
    REQUIRE(editor);
    CHECK(editor->kind == ui::ModPage::EditorKind::Color);
    CHECK(editor->setting->id == "ray");
    CHECK_FALSE(findTarget(TargetKind::ColorChannel, "Alpha"));
    CHECK(pressActs(panel, ImVec2(red->rect.Min.x + 1.0f, red->rect.GetCenter().y)));
    const auto& ray = std::get<model::Setting>(std::get<model::Group>(panel.mods()[0].frontPage.items[0]).items[1]);
    CHECK(std::get<std::string>(store.value(ray).value) == "{ rgb = [0, 255, 190] }");
    CHECK(pressActs(panel, middleOf(TargetKind::Button, "Cancel")));
    drawFrames(panel, 2);
    CHECK_FALSE(findTarget(TargetKind::ColorChannel, "Red"));
    CHECK_FALSE(panel.openEditor());

    // the dropdown's list has its options, and one picked is its value
    CHECK(pressActs(panel, middleOf(TargetKind::Setting, "hand")));
    drawFrames(panel, 2);
    REQUIRE(panel.openEditor());
    CHECK(panel.openEditor()->kind == ui::ModPage::EditorKind::List);
    CHECK(scrolled(ui::ScrollArea::List));
    CHECK(pressActs(panel, middleOf(TargetKind::Option, "Primary")));
    const auto& hand = std::get<model::Setting>(std::get<model::Group>(panel.mods()[0].frontPage.items[0]).items[2]);
    CHECK(std::get<std::string>(store.value(hand).value) == "primary");
    drawFrames(panel, 2);
    CHECK_FALSE(panel.openEditor());
}
