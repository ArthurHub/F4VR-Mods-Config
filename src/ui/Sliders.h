#pragma once

#include "model/Definition.h"
#include "model/Store.h"

namespace mods_config::ui
{
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
     * The sliders of the page that is shown: how one is drawn, and how the player edits one. The trigger
     * drags a slider by its track, and the thumbstick steps the one whose row the ray is on.
     * It points into the page that is shown, so it is reset as another page is.
     */
    class Sliders
    {
    public:
        void startFrame(float thumbstickSideValue);
        float edit(const model::Setting& setting, float right, float top, const SliderRow& row, float value, bool& pulse);
        float draw(const model::Setting& setting, float right, float top, float value) const;
        void endFrame();
        bool cancelDrag(model::Store* store);
        void reset();

    private:
        int stepsOfPush(float push, float seconds);

        // the slider the trigger is held on since a press on its track, and none
        const model::Setting* _dragged = nullptr;

        // its value as the drag began, which Back gives it again
        float _dragStart = 0.0f;

        // from the middle of the grab to the ray, for a press on the grab itself: the grab does not jump
        float _grabOffset = 0.0f;

        // Back ended the drag: the rest of the press changes nothing
        bool _dragCancelled = false;

        // the row of the dragged slider was held in the frame that is drawn
        bool _dragHeld = false;

        // the slider whose row the ray is on, and for how long
        const model::Setting* _pointed = nullptr;
        float _pointedSeconds = 0.0f;

        // the row of a slider had the ray in the frame that is drawn
        bool _pointedSeen = false;

        // The push of the thumbstick: its side as -1 or 1, and 0 while it is not pushed, how long it is
        // held, and when it steps next
        int _pushSide = 0;
        float _pushSeconds = 0.0f;
        float _nextStepSeconds = 0.0f;

        // the thumbstick was pushed while the ray was on no slider, or came to another: it steps nothing
        // until it is let go
        bool _pushIgnored = false;

        // the steps the thumbstick moves the slider under the ray by in the frame that is drawn
        int _steps = 0;
    };
}
