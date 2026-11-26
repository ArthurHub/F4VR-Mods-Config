#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include <imgui.h>

#include "TestPanel.h"
#include "TestRender.h"
#include "imgui/controls/Labelled.h"
#include "imgui/controls/Panes.h"
#include "imgui/controls/Rows.h"
#include "imgui/controls/Targets.h"

// The framework's controls with their label, drawn in this mod's theme on a page the panel's size and saved as
// images, to look at them before a panel has them in the headset. They check nothing, so they are not run with the
// other tests. Run them by their tag, as the panel's render tests:
//     ModsConfigFO4VR_linked_tests "[render]"
// Each says where its image is, a .bmp file in the build folder.

using namespace mods_config;
using test_panel::POINTER_AWAY;

namespace
{
    constexpr std::array<std::string_view, 3> QUALITY{ "Low", "Medium", "High" };
    constexpr std::array<std::string_view, 4> COLORS{ "Green", "Amber", "Blue", "White" };

    // the rows of a page: one of each labelled control
    constexpr std::size_t ROW_COUNT = 7;
    using RowLooks = std::array<controls::Labelled, ROW_COUNT>;

    /**
     * The values the rows edit, which last from frame to frame.
     */
    struct Values
    {
        bool on = true;
        float distance = 2.5f;
        std::size_t quality = 1;
        std::size_t color = 0;
        std::string name = "Preacher";
    };

    /**
     * A page of one of each labelled control under a heading, each row in its look.
     */
    void drawRows(Values& values, const std::string_view heading, const RowLooks& looks, const std::string_view dropdownLabel = "Beam color")
    {
        controls::sectionHeading(heading, ImGui::GetCursorScreenPos().x + controls::rowWidth());
        controls::labelledSwitcher("Enabled", values.on, looks[0]);
        controls::labelledSlider("Beam distance", values.distance, { .min = 0.0f, .max = 10.0f, .step = 0.5f }, { .decimals = 1, .unit = " m" }, looks[1]);
        controls::labelledStepper("Quality", values.quality, QUALITY, looks[2]);
        controls::labelledDropdown(dropdownLabel, values.color, COLORS, looks[3]);
        controls::labelledText("Name", values.name, {}, looks[4]);
        controls::labelledButton("Cache", "Clear", looks[5], controls::Icon::Trash);
        controls::labelledValue("Version", "1.2.0", looks[6]);
    }

    /**
     * Every row in one look.
     */
    RowLooks allIn(const controls::Labelled& look)
    {
        RowLooks looks;
        looks.fill(look);
        return looks;
    }

    /**
     * The row of a label in the last frame, in the panel's pixels.
     */
    ImRect rowOf(const std::string_view label)
    {
        for (const auto& kept : controls::targets().targets()) {
            if (kept.kind == controls::target::ROW && kept.key == label) {
                return kept.rect;
            }
        }
        return {};
    }

    /**
     * The ray on a row, on its label, away from its control.
     */
    ImVec2 onLabel(const std::string_view label)
    {
        const auto row = rowOf(label);
        return { row.Min.x + 120.0f, row.GetCenter().y };
    }

    /**
     * Draws rows on the panel's page with the ray at a place and saves them as an image. A few frames are drawn first,
     * which ImGui needs to find what the ray is on.
     */
    std::string renderToFile(const std::string& name, const std::function<void()>& rows, const ImVec2 pointer)
    {
        for (int i = 0; i < 5; ++i) {
            test_panel::drawContentFrame(rows, pointer);
        }
        return test_render::saveRender(name);
    }

    /**
     * Keeps the rows' targets while it lasts, which the tests find a row by.
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
}

TEST_CASE("Controls render: the controls with their label at rest, disabled, with marks, and in the warning and error tones", "[.render]")
{
    const test_panel::PanelContext context;
    Values values;
    const auto render = [&values](const std::string& name, const std::string_view heading, const RowLooks& looks, const std::string_view dropdownLabel = "Beam color") {
        WARN("Saved " << renderToFile(
                 name,
                 [&] {
                     drawRows(values, heading, looks, dropdownLabel);
                 },
                 POINTER_AWAY));
    };
    render("controls-at-rest", "At rest", allIn({}));
    render("controls-disabled", "Disabled", allIn({ .disabled = true }));
    // a label too long for its row is cut before its mark
    render("controls-marks",
        "With a mark",
        { controls::Labelled{ .mark = controls::Mark::Warning },
            controls::Labelled{ .mark = controls::Mark::Error },
            controls::Labelled{ .mark = controls::Mark::Info },
            controls::Labelled{ .mark = controls::Mark::Warning },
            controls::Labelled{ .mark = controls::Mark::Error },
            controls::Labelled{ .mark = controls::Mark::Info },
            controls::Labelled{ .mark = controls::Mark::Warning } },
        "The color of the beam of the flashlight, with a label long enough that it is cut with an ellipsis before its mark");
    render("controls-warning", "In the warning tone", allIn({ .mark = controls::Mark::Warning, .tone = controls::Tone::Warning }));
    render("controls-error", "In the error tone", allIn({ .mark = controls::Mark::Error, .tone = controls::Tone::Error }));
}

TEST_CASE("Controls render: each control with its label under the ray", "[.render]")
{
    const test_panel::PanelContext context;
    const Recording recording;
    Values values;
    const auto rows = [&values] {
        drawRows(values, "Under the ray", allIn({}));
    };
    test_panel::drawContentFrame(rows, POINTER_AWAY);
    const std::array<std::pair<std::string_view, std::string_view>, 6> pointed{ {
        { "Enabled", "controls-under-ray-switcher" },
        { "Beam distance", "controls-under-ray-slider" },
        { "Quality", "controls-under-ray-stepper" },
        { "Beam color", "controls-under-ray-dropdown" },
        { "Name", "controls-under-ray-text" },
        { "Cache", "controls-under-ray-button" },
    } };
    for (const auto& [label, name] : pointed) {
        WARN("Saved " << renderToFile(std::string(name), rows, onLabel(label)));
    }
}

TEST_CASE("Controls render: a dropdown with its label, its list open and the ray on an option", "[.render]")
{
    const test_panel::PanelContext context;
    const Recording recording;
    Values values;
    const auto rows = [&values] {
        drawRows(values, "A list open", allIn({}));
    };
    test_panel::drawContentFrame(rows, POINTER_AWAY);
    // a press on the row opens the list, and the ray goes to an option of it
    const auto onDropdown = onLabel("Beam color");
    for (const bool pressed : { false, false, true, false }) {
        test_panel::drawContentFrame(rows, onDropdown, pressed);
    }
    ImVec2 onOption = POINTER_AWAY;
    for (const auto& kept : controls::targets().targets()) {
        if (kept.kind == controls::target::OPTION && kept.key == "Blue") {
            onOption = kept.rect.GetCenter();
        }
    }
    CHECK(onOption.x > 0.0f);
    WARN("Saved " << renderToFile("controls-dropdown-open", rows, onOption));
}

TEST_CASE("Controls render: a content pane of 10,000 rows with their label, clipped to those shown and scrolled to its middle", "[.render]")
{
    const test_panel::PanelContext context;
    constexpr std::size_t COUNT = 10000;
    const auto rows = [] {
        controls::sectionHeading("10,000 rows, scrolled to the middle", ImGui::GetCursorScreenPos().x + controls::rowWidth());
        // the pane's scroll is set in every frame: the first has no rows to scroll yet
        if (controls::beginContentPane("rows", ImGui::GetContentRegionAvail().y, static_cast<float>(COUNT / 2) * theme::ROW_PITCH)) {
            controls::clippedRows(COUNT, [](const std::size_t index) {
                const auto label = std::format("Item {}", index);
                if (index % 2 == 0) {
                    // every other switcher is on, and a press is not kept: the rows are only looked at
                    bool value = index % 4 == 0;
                    controls::labelledSwitcher(label, value);
                } else {
                    controls::labelledValue(label, std::format("{:08X}", 0x01000000 + index));
                }
            });
        }
        controls::endContentPane();
    };
    WARN("Saved " << renderToFile("controls-clipped-rows", rows, POINTER_AWAY));
}
