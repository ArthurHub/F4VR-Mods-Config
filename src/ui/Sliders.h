#pragma once

#include <optional>
#include <string>

#include "Drawing.h"
#include "model/ConfigDefinition.h"

namespace mods_config::ui
{
    /**
     * The text of a slider's value as the box of its value shows it, and its size.
     */
    struct ValueBoxText
    {
        std::string text;
        float fontSize = 0.0f;

        // false for a number whose unit the box has no room for, which is drawn alone
        bool withUnit = true;
    };

    ValueBoxText valueBoxText(const model::Setting& setting, float value);

    /**
     * What the row of a slider tells of the pointer in the frame that is drawn.
     */
    struct SliderRow
    {
        // the ray is on the row
        bool pointed = false;

        // the trigger was pulled on the row in this frame, and is held since a pull on it
        bool started = false;
        bool held = false;
    };

    /**
     * When a press that is held steps a slider, the thumbstick's and a step arrow's alike.
     */
    struct StepRepeat
    {
        bool step(float seconds);

        // the press stepped as it began, how long it is held since, and when it steps next
        bool begun = false;
        float heldSeconds = 0.0f;
        float nextStepSeconds = 0.0f;
    };

    /**
     * The sliders of the page that is shown: how one is drawn, and how the player edits one. The trigger
     * drags a slider by its track and steps it by the arrow at each end of the track, and the thumbstick
     * steps the one whose row the ray is on. A slider that is dragged keeps its value here, and it is
     * written as the trigger is let go.
     * It points into the page that is shown, so it is reset as another page is.
     */
    class Sliders
    {
    public:
        void startFrame(float thumbstickSideValue);
        float edit(const model::Setting& setting, float right, float top, const SliderRow& row, float value, bool& write, bool& pulse);
        float draw(const model::Setting& setting, float right, float top, const SliderRow& row, std::optional<float> value, ValueLook look) const;
        void endFrame();
        bool cancelDrag();
        void reset();

    private:
        int stepsOfPush(float push, float seconds);

        // the slider the trigger is held on since a press on its track, and none
        const model::Setting* _dragged = nullptr;

        // the value the drag gave it, which is written as the trigger is let go
        float _dragValue = 0.0f;

        // from the middle of the grab to the ray, for a press on the grab itself: the grab does not jump
        float _grabOffset = 0.0f;

        // Back ended the drag: the rest of the press changes nothing
        bool _dragCancelled = false;

        // the row of the dragged slider was held in the frame that is drawn
        bool _dragHeld = false;

        // the slider whose step arrow the trigger is held on since a press on it, that arrow's side as -1 or
        // 1, and its steps
        const model::Setting* _arrowHeld = nullptr;
        int _arrowSide = 0;
        StepRepeat _arrowRepeat;

        // the slider whose row the ray is on, and for how long
        const model::Setting* _pointed = nullptr;
        float _pointedSeconds = 0.0f;

        // the row of a slider had the ray in the frame that is drawn
        bool _pointedSeen = false;

        // The push of the thumbstick: its side as -1 or 1, and 0 while it is not pushed, and its steps
        int _pushSide = 0;
        StepRepeat _pushRepeat;

        // the thumbstick was pushed while the ray was on no slider, or came to another: it steps nothing
        // until it is let go
        bool _pushIgnored = false;

        // the steps the thumbstick moves the slider under the ray by in the frame that is drawn
        int _steps = 0;
    };
}
