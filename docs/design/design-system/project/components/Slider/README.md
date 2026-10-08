# Slider

A bounded int or float: a track to drag, a value box, and Reset.

**Provides:** value, min, max, step, units, decimals, state.

- Track 280 x 48 in `track` with a `frame-border` outline. An 8 px fill from the minimum to the grab in `green`; a `grab-width` x 40 grab in `green`. ImGui's `SliderFloat` draws the frame and grab; the fill is an extra `AddRectFilled`.
- Value box `value-width` x 48, right-aligned tabular digits with units (`40°`). Pressing it opens the numeric keypad once that exists.
- Dragging: grab and track outline in `glow`. Back during a drag restores the value from its start.
- Pending: fill, grab and value in `amber`. Invalid: in `red`, with a `red` value outline.
- The thumbstick steps by the declared step while the slider is focused, and repeats faster when held.

The design's suggested row starts with a minus button. It is left out here to give the track 280 px; the thumbstick and the value box cover precise entry. Add minus and plus at the ends if the headset shows they are missed.
