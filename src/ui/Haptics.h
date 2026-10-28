#pragma once

#include <optional>

#include <imgui.h>
#include <imgui_internal.h>

#include "Drawing.h"
#include "Theme.h"
#include "vrcf/VRControllersHaptic.h"

// A config spec's haptic, as the design system's Haptic has it: a dropdown whose frame and options show each pattern's
// shape, with a Play button after it.

namespace mods_config::ui
{
    // the frame of a haptic: a dropdown's box without the room of its Play button
    constexpr float HAPTIC_FRAME_WIDTH = DROPDOWN_WIDTH - theme::SPACE_2 - theme::CONTROL_HEIGHT;

    // the box of a pattern's shape
    constexpr float HAPTIC_SHAPE_WIDTH = 96.0f;
    constexpr float HAPTIC_SHAPE_HEIGHT = 24.0f;

    /**
     * Where the parts of a haptic that ends at the right are: its frame, and its Play button after it.
     */
    struct HapticBoxes
    {
        ImRect frame;
        ImRect play;

        HapticBoxes(float right, float top);
        bool onPlay(float x) const;
    };

    void drawHapticShape(const ImVec2& min, std::optional<vrcf::HapticPattern> pattern, ImU32 color, ImU32 baseColor);
    void drawPlayIcon(const ImVec2& center);
}
