#include "Haptics.h"

#include <algorithm>

#include "model/Haptics.h"

namespace
{
    using namespace mods_config;

    // a pattern is drawn at this many pixels a second, or at less where it is longer than its box
    constexpr float SHAPE_PIXELS_PER_SECOND = 200.0f;

    // the base line under the keyframes, and the height of an intensity of 1 over it
    constexpr float SHAPE_BASE_HEIGHT = 2.0f;
    constexpr float SHAPE_FULL_HEIGHT = ui::HAPTIC_SHAPE_HEIGHT - SHAPE_BASE_HEIGHT;

    // a keyframe of intensity is at least this wide, so a tap shows
    constexpr float SHAPE_MIN_KEYFRAME_WIDTH = 2.0f;

    /**
     * The frame of a haptic: a dropdown's box without the room of its Play button.
     */
    float hapticFrameWidth()
    {
        return ui::dropdownWidth() - ui::look().space2 - ui::look().controlHeight;
    }
}

namespace mods_config::ui
{
    HapticBoxes::HapticBoxes(const float right, const float top)
        : frame(controls::controlBox(right - dropdownWidth(), top, hapticFrameWidth())),
          play(controls::controlBox(right - look().controlHeight, top, look().controlHeight))
    {}

    /**
     * Whether a place of the row is on the Play button. It is pressed a little beside it too, and in the gap between
     * it and the frame.
     */
    bool HapticBoxes::onPlay(const float x) const
    {
        return x >= frame.Max.x && x < play.Max.x + look().space1;
    }

    /**
     * The shape of a pattern in its box from its top left, as the design system's Haptic draws it: a base line the full
     * width, and over it a quad for each keyframe of intensity, from its start to its end intensity, where a keyframe
     * of none is a gap. It is drawn at 200 px a second, or at less so that a pattern longer than its box fits it: a tap
     * stays narrow and a rumble fills the box. None is the base line alone.
     */
    void drawHapticShape(const ImVec2& min, const std::optional<vrcf::HapticPattern> pattern, const ImU32 color, const ImU32 baseColor)
    {
        auto* drawList = ImGui::GetWindowDrawList();
        const float base = min.y + SHAPE_FULL_HEIGHT;
        drawList->AddRectFilled(ImVec2(min.x, base), ImVec2(min.x + HAPTIC_SHAPE_WIDTH, base + SHAPE_BASE_HEIGHT), baseColor);
        if (!pattern) {
            return;
        }
        const float length = model::hapticLength(*pattern);
        const float scale = length > 0.0f ? (std::min)(SHAPE_PIXELS_PER_SECOND, HAPTIC_SHAPE_WIDTH / length) : SHAPE_PIXELS_PER_SECOND;
        float time = 0.0f;
        for (const auto& segment : vrcf::VRControllersHaptic::getPattern(*pattern)) {
            const float left = min.x + time * scale;
            time += segment.duration;
            if (segment.startIntensity <= 0.0f && segment.endIntensity <= 0.0f) {
                continue;
            }
            const float right = (std::max)(min.x + time * scale, left + SHAPE_MIN_KEYFRAME_WIDTH);
            drawList->AddQuadFilled(ImVec2(left, base),
                ImVec2(left, base - segment.startIntensity * SHAPE_FULL_HEIGHT),
                ImVec2(right, base - segment.endIntensity * SHAPE_FULL_HEIGHT),
                ImVec2(right, base),
                color);
        }
    }
}
