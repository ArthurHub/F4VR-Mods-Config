#include "Sliders.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "Drawing.h"
#include "Theme.h"

namespace
{
    using namespace mods_config;

    // The sizes of a slider that are not tokens of the design system, in the panel's pixels
    constexpr float FILL_HEIGHT = 8.0f;
    constexpr float GRAB_HEIGHT = 40.0f;

    // The thumbstick steps a slider from this far off its center, of 1 for a full push. It is far, so a push
    // that scrolls the page and is a little to a side steps nothing.
    constexpr float STEP_PUSH = 0.6f;
    // The seconds the ray is on the row of a slider before the thumbstick steps it, so a ray that crosses
    // the row steps nothing
    constexpr float STEP_POINTED_DELAY = 0.3f;
    // A push that is held steps again after the delay, then at the interval, and at the fast one once it is
    // held for long: in seconds
    constexpr float STEP_REPEAT_DELAY = 0.4f;
    constexpr float STEP_REPEAT_INTERVAL = 0.12f;
    constexpr float STEP_FAST_AFTER = 1.5f;
    constexpr float STEP_FAST_INTERVAL = 0.03f;
    // the steps a slider with no step of its own has from its least to its most
    constexpr float STEPS_WITH_NO_STEP = 100.0f;

    /**
     * Where the parts of a slider that ends at the right are: its track, the box of its value after it, and
     * the two ends the middle of its grab moves between, so the grab is whole inside the track at both.
     */
    struct Boxes
    {
        ImRect track;
        ImRect valueBox;
        float grabLeast = 0.0f;
        float grabMost = 0.0f;

        Boxes(const float right, const float top)
            : track(ui::controlBox(right - theme::VALUE_WIDTH - theme::SPACE_2 - ui::TRACK_WIDTH, top, ui::TRACK_WIDTH)),
              valueBox(ui::controlBox(right - theme::VALUE_WIDTH, top, theme::VALUE_WIDTH)),
              grabLeast(track.Min.x + theme::BORDER_FRAME + theme::GRAB_WIDTH * 0.5f),
              grabMost(track.Max.x - theme::BORDER_FRAME - theme::GRAB_WIDTH * 0.5f)
        {}

        /**
         * The middle of the grab at a value of the slider. A value outside the range is at its end.
         */
        float grabAt(const model::ValueOptions& options, const float value) const
        {
            const float fraction = options.max > options.min ? std::clamp((value - options.min) / (options.max - options.min), 0.0f, 1.0f) : 0.0f;
            return std::lerp(grabLeast, grabMost, fraction);
        }

        /**
         * The value of the slider with the middle of its grab at a place, in its range and not on a step.
         */
        float valueAt(const model::ValueOptions& options, const float grab) const
        {
            return std::lerp(options.min, options.max, std::clamp((grab - grabLeast) / (grabMost - grabLeast), 0.0f, 1.0f));
        }
    };

    /**
     * A value of a slider on the step that is nearest to it, counted from the least, and in the range. A
     * slider with no step has any value of its range.
     */
    float onStep(const model::ValueOptions& options, const float value)
    {
        double stepped = value;
        if (options.step > 0.0f) {
            stepped = options.min + std::round((static_cast<double>(value) - options.min) / options.step) * options.step;
        }
        return std::clamp(static_cast<float>(stepped), options.min, options.max);
    }
}

namespace mods_config::ui
{
    /**
     * A value of a slider as its text, with as many decimals as the slider's step has, and three at most.
     */
    std::string sliderValueText(const float value, const float step)
    {
        int decimals = 0;
        for (float scaled = std::abs(step); decimals < 3 && std::abs(scaled - std::round(scaled)) > 0.001f; scaled *= 10.0f) {
            ++decimals;
        }
        return std::format("{:.{}f}", value, decimals);
    }

    /**
     * Starts a frame of the page, before its rows. The thumbstick's value is how far the one of the hand
     * that points is pushed to a side, from -1 at the left to 1 at the right.
     */
    void Sliders::startFrame(const float thumbstickSideValue)
    {
        _dragHeld = false;
        _pointedSeen = false;
        _steps = stepsOfPush(thumbstickSideValue, ImGui::GetIO().DeltaTime);
    }

    /**
     * The value a slider that ends at the right shows in this frame: the one the player gives it, and its
     * own where they give none. A pull of the trigger on the track takes hold of the grab, which follows
     * the ray until the trigger is let go, also beside the track. A pull on the grab itself does not move
     * it. The value of a drag is to be written as the trigger is let go, and not before: a store can write
     * a file for each. After Back the drag gives nothing.
     * The thumbstick steps the slider whose row the ray is on, once it is there for a short time, and each
     * step is to be written.
     * Tells whether the hand gets its pulse: the pull took hold of the slider, or the thumbstick stepped it.
     */
    float Sliders::edit(const model::Setting& setting, const float right, const float top, const SliderRow& row, const float value, bool& write, bool& pulse)
    {
        const auto& options = setting.valueOptions;
        const auto& io = ImGui::GetIO();
        const Boxes boxes(right, top);
        if (row.started && boxes.track.Contains(io.MousePos)) {
            const float grab = boxes.grabAt(options, value);
            _dragged = &setting;
            _dragValue = value;
            _grabOffset = std::abs(io.MousePos.x - grab) <= theme::GRAB_WIDTH * 0.5f ? io.MousePos.x - grab : 0.0f;
            _dragCancelled = false;
            pulse = true;
        }
        // by the frame before, as the steps of this frame are
        const bool wasPointed = row.pointed && _pointed == &setting;
        if (row.pointed) {
            _pointedSeen = true;
            if (_pointed == &setting) {
                _pointedSeconds += io.DeltaTime;
            } else {
                // a push that is held as the ray comes here is not for this slider
                _pointed = &setting;
                _pointedSeconds = 0.0f;
                _pushIgnored = _pushSide != 0;
            }
        }
        if (_dragged == &setting) {
            if (_dragCancelled) {
                _dragHeld = row.held;
                return value;
            }
            if (!row.held) {
                write = _dragValue != value;
                return _dragValue;
            }
            _dragHeld = true;
            _dragValue = onStep(options, boxes.valueAt(options, io.MousePos.x - _grabOffset));
            return _dragValue;
        }
        if (wasPointed && _steps != 0) {
            const float step = options.step > 0.0f ? options.step : (options.max - options.min) / STEPS_WITH_NO_STEP;
            const float stepped = onStep(options, value + static_cast<float>(_steps) * step);
            write = stepped != value;
            pulse = pulse || write;
            return stepped;
        }
        return value;
    }

    /**
     * A slider that ends at the right: its track, with a fill from the least to the grab, and the box of its
     * value. While it is dragged the grab and the outline of the track are in the color of the pointer's
     * mark. Returns its left edge.
     */
    float Sliders::draw(const model::Setting& setting, const float right, const float top, const float value) const
    {
        const auto& options = setting.valueOptions;
        const Boxes boxes(right, top);
        const auto& track = boxes.track;
        const bool dragged = _dragged == &setting && _dragHeld && !_dragCancelled;
        drawFrame(track);
        if (dragged) {
            drawOutline(track.Min, track.Max, theme::color::GLOW);
        }

        const float grab = boxes.grabAt(options, value);
        const float middle = track.GetCenter().y;
        const auto green = ImGui::GetColorU32(theme::color::GREEN);
        auto* drawList = ImGui::GetWindowDrawList();
        if (grab > boxes.grabLeast) {
            drawList->AddRectFilled(ImVec2(boxes.grabLeast, middle - FILL_HEIGHT * 0.5f), ImVec2(grab, middle + FILL_HEIGHT * 0.5f), green, theme::RADIUS_GRAB);
        }
        drawList->AddRectFilled(ImVec2(grab - theme::GRAB_WIDTH * 0.5f, middle - GRAB_HEIGHT * 0.5f),
            ImVec2(grab + theme::GRAB_WIDTH * 0.5f, middle + GRAB_HEIGHT * 0.5f),
            dragged ? ImGui::GetColorU32(theme::color::GLOW) : green,
            theme::RADIUS_GRAB);

        drawFrame(boxes.valueBox);
        drawTextIn(boxes.valueBox, sliderValueText(value, options.step), theme::color::GREEN, 1.0f, theme::SPACE_3);
        return track.Min.x;
    }

    /**
     * Ends a frame of the page, after its rows: a slider whose row was not held is not dragged any more, and
     * one whose row the ray has left is not the one the thumbstick steps.
     */
    void Sliders::endFrame()
    {
        if (!_dragHeld) {
            _dragged = nullptr;
        }
        if (!_pointedSeen) {
            _pointed = nullptr;
        }
    }

    /**
     * Ends the drag of a slider with nothing written, for Back: the slider has the value it had as the drag
     * began. The trigger is still held then, and the rest of its press changes nothing.
     * Returns false while no slider is dragged.
     */
    bool Sliders::cancelDrag()
    {
        if (!_dragged || _dragCancelled) {
            return false;
        }
        _dragCancelled = true;
        return true;
    }

    /**
     * Forgets the slider that is edited, as another page is shown: it is a row of the page before.
     */
    void Sliders::reset()
    {
        *this = {};
    }

    /**
     * The steps the thumbstick moves the slider under the ray by in this frame: one to the side it is pushed
     * to, and one again while it is held, after a delay and then faster. None until the ray is on the row
     * of the slider for a short time: a push in that time steps as the time is over. A push that began
     * with the ray on no slider, or on another, steps nothing until the thumbstick is let go.
     */
    int Sliders::stepsOfPush(const float push, const float seconds)
    {
        const int side = push >= STEP_PUSH ? 1 : push <= -STEP_PUSH ? -1 : 0;
        if (side != _pushSide) {
            _pushSide = side;
            _pushSeconds = 0.0f;
            _nextStepSeconds = 0.0f;
            _pushIgnored = side != 0 && !_pointed;
        }
        if (side == 0 || _pushIgnored) {
            return 0;
        }
        if (!_pointed) {
            _pushIgnored = true;
            return 0;
        }
        if (_pointedSeconds < STEP_POINTED_DELAY) {
            return 0;
        }
        if (_nextStepSeconds == 0.0f) {
            _nextStepSeconds = STEP_REPEAT_DELAY;
            return side;
        }
        _pushSeconds += seconds;
        if (_pushSeconds < _nextStepSeconds) {
            return 0;
        }
        _nextStepSeconds += _pushSeconds < STEP_FAST_AFTER ? STEP_REPEAT_INTERVAL : STEP_FAST_INTERVAL;
        return side;
    }
}
