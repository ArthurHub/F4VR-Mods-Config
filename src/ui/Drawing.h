#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "Theme.h"
#include "imgui/controls/Buttons.h"
#include "imgui/controls/Controls.h"
#include "imgui/controls/Dialogs.h"
#include "imgui/controls/Frames.h"
#include "imgui/controls/Icons.h"
#include "imgui/controls/Marks.h"
#include "imgui/controls/Rows.h"
#include "imgui/controls/Text.h"
#include "model/ConfigDefinition.h"
#include "model/Search.h"

namespace mods_config
{
    // the framework's controls, which the panel is drawn with
    namespace controls = f4cf::imgui::controls;
}

// What the parts of the panel draw with, in the look of the design system, beside the framework's controls.

namespace mods_config::ui
{
    /**
     * The theme that is set, which the panel is drawn in: the design system's (`theme::designTheme()`), with what
     * devbench's theme action sets over it. Its sizes are read as the panel draws, not `Theme.h`'s constants.
     */
    inline const f4cf::imgui::Theme& look()
    {
        return controls::get().theme();
    }

    /**
     * The theme's colors, packed for ImGui.
     */
    inline const controls::ControlsColors& colors()
    {
        return controls::get().colors();
    }

    /**
     * How the value of a setting looks: as it is saved, pending a Save in amber, or invalid in red.
     */
    enum class ValueLook : std::uint8_t
    {
        Saved,
        Pending,
        Invalid,
    };

    // the box of a stepper's option, and a slider's track with its step arrows, in the panel's pixels: not a token
    // of the design system
    constexpr float TRACK_WIDTH = 280.0f;

    // the box of the mark of when a pending change takes effect, which is drawn in place of the dot
    constexpr float APPLIES_MARK_WIDTH = 16.0f;

    // the width of an activation sphere's mark, as the design system has it
    constexpr float SPHERE_MARK_SIZE = 20.0f;

    float dropdownWidth();
    controls::Tone toneOf(ValueLook look);
    ImU32 valueColor(ValueLook look);
    float mcmScale();
    void drawPageText(std::string_view text);
    float drawTextRuns(float left, float right, float top, float height, ImU32 color, std::span<const model::TextRun> runs);
    float drawTextLines(std::span<const model::TextLine> lines, const ImVec2& pos, float width, ImU32 color);
    void sphereHeading(std::string_view text, float right, std::span<const model::TextRange> found = {});
    float appliesTagWidth(std::string_view word);
    void drawAppliesTag(float left, float middle, model::Applies applies, std::string_view word);
    float restartMarkWidth();
    void drawRestartMark(float left, float middle);
    float pendingMarkWidth(model::Applies applies);
    void drawAppliesMark(model::Applies applies, const ImVec2& center);
}

namespace mods_config
{
    // the theme's reads, for the panel's code outside ui:: too, as the helpers of a part in an anonymous namespace
    using ui::colors;
    using ui::look;
}
