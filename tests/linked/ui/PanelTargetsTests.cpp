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
#include "ui/Targets.h"

using namespace mods_config;
using test_panel::drawFrames;
using test_panel::pressActs;

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
            controls::targets().setRecording(true);
        }

        Recording(const Recording&) = delete;
        Recording& operator=(const Recording&) = delete;

        ~Recording()
        {
            controls::targets().setRecording(false);
        }
    };

    const controls::Target* findTarget(const std::string_view kind, const std::string_view key)
    {
        const auto& targets = controls::targets().targets();
        const auto found = std::ranges::find_if(targets, [kind, key](const controls::Target& target) {
            return target.kind == kind && target.key == key;
        });
        return found != targets.end() ? &*found : nullptr;
    }

    /**
     * The middle of a target of the last frame, by its kind and key, where the ray presses it.
     */
    ImVec2 middleOf(const std::string_view kind, const std::string_view key)
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

TEST_CASE("Panel targets: a frame keeps nothing until recording is on")
{
    const test_panel::PanelContext context;
    ui::FullPanel panel;
    panel.setMods({ myMod() });
    drawFrames(panel, 2);
    CHECK(controls::targets().targets().empty());
}

TEST_CASE("Panel targets: the rows of the tree and the page, the title bar, the color editor and a dropdown's options are where a press on them acts")
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

    const auto scrolled = [](const std::string_view area) {
        return std::ranges::any_of(controls::targets().scrolls(), [area](const controls::ScrollTarget& scroll) {
            return scroll.area == area;
        });
    };
    CHECK(scrolled(controls::target::TREE_AREA));
    CHECK(scrolled(controls::target::PAGE_AREA));
    CHECK(findTarget(ui::target::TITLE_BUTTON, "Help"));
    CHECK(findTarget(ui::target::TITLE_BUTTON, "Close"));
    CHECK(findTarget(ui::target::CRUMB, "MODS CONFIG FO4"));

    // the mod's row selects it, and then its page's row is there, by the mod's name and the page's
    CHECK(pressActs(panel, middleOf(ui::target::TREE, "MyMod")));
    drawFrames(panel, 2);
    CHECK(panel.selection().mod == 0u);
    const auto& targets = controls::targets().targets();
    const auto page = std::ranges::find_if(targets, [](const controls::Target& target) {
        return target.kind == ui::target::TREE && target.path == std::vector<std::string>{ "Second" };
    });
    REQUIRE(page != targets.end());
    CHECK(page->key == "MyMod");

    // a setting's row by its id, under its section's heading
    CHECK(findTarget(ui::target::SECTION, "Look"));
    const auto& enabled = std::get<model::Setting>(std::get<model::Group>(panel.mods()[0].frontPage.items[0]).items[0]);
    CHECK(pressActs(panel, middleOf(ui::target::SETTING, "enabled")));
    CHECK(std::get<bool>(store.value(enabled).value));

    // the color's row opens its editor, whose channels and buttons are there while it is open
    CHECK(pressActs(panel, middleOf(ui::target::SETTING, "ray")));
    drawFrames(panel, 2);
    const auto* red = findTarget(ui::target::COLOR_CHANNEL, "Red");
    REQUIRE(red);
    const auto editor = panel.openEditor();
    REQUIRE(editor);
    CHECK(editor->kind == ui::ModPage::EditorKind::Color);
    CHECK(editor->setting->id == "ray");
    CHECK_FALSE(findTarget(ui::target::COLOR_CHANNEL, "Alpha"));
    CHECK(pressActs(panel, ImVec2(red->rect.Min.x + 1.0f, red->rect.GetCenter().y)));
    const auto& ray = std::get<model::Setting>(std::get<model::Group>(panel.mods()[0].frontPage.items[0]).items[1]);
    CHECK(std::get<std::string>(store.value(ray).value) == "{ rgb = [0, 255, 190] }");
    CHECK(pressActs(panel, middleOf(controls::target::BUTTON, "Cancel")));
    drawFrames(panel, 2);
    CHECK_FALSE(findTarget(ui::target::COLOR_CHANNEL, "Red"));
    CHECK_FALSE(panel.openEditor());

    // the dropdown's list has its options, and one picked is its value
    CHECK(pressActs(panel, middleOf(ui::target::SETTING, "hand")));
    drawFrames(panel, 2);
    REQUIRE(panel.openEditor());
    CHECK(panel.openEditor()->kind == ui::ModPage::EditorKind::List);
    CHECK(scrolled(controls::target::LIST_AREA));
    CHECK(pressActs(panel, middleOf(controls::target::OPTION, "Primary")));
    const auto& hand = std::get<model::Setting>(std::get<model::Group>(panel.mods()[0].frontPage.items[0]).items[2]);
    CHECK(std::get<std::string>(store.value(hand).value) == "primary");
    drawFrames(panel, 2);
    CHECK_FALSE(panel.openEditor());
}
