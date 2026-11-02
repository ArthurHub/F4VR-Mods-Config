# Slider

A bounded int or float: a step arrow on each side of a track to drag, a value box, and Reset.

**Provides:** value, min, max, step, units, decimals, state.

- **Step arrows**, one at each end of the track, `space-1` from it: 24 x 48, a `frame-border` outline and `radius-frame`, with a drawn 10 px triangle in `green`, pointing left and right. They are thin on purpose: they nudge the slider beside them, and are not the stepper's 48 px buttons.
  - A press steps the value by the declared step, as the thumbstick does, and holding repeats, faster while held. Each step gives the light pulse.
  - Under the ray: `hover` fill and a `green` outline. Pressed: `pressed` fill and a `glow` outline.
  - At the minimum, the left arrow is disabled, and at the maximum the right one, at `opacity-disabled`.
  - The hit box is the arrow's full 48 px height, and ImGui's `TouchExtraPadding` adds `space-1` around it, so it is about 32 x 56 to the ray.
- Track 224 x 48 in `track` with a `frame-border` outline: the 280 px it had, less the arrows and their gaps, so the control column keeps its width. An 8 px fill from the minimum to the grab in `green`; a `grab-width` x 40 grab in `green`. ImGui's `SliderFloat` draws the frame and grab; the fill is an extra `AddRectFilled`.
- Value box `value-width` x 48, right-aligned tabular digits with units (`40°`). It only shows the value, which is not typed.
- Dragging: grab and track outline in `glow`. Back during a drag restores the value from its start.
- Pending: fill, grab and value in `amber`. Invalid: in `red`, with a `red` value outline.
- The thumbstick steps by the declared step while the ray is on the row, after a short time there, and repeats faster when held, as the arrows do. A slider has no focused state.
- A read-only slider shows its arrows disabled, with the rest of its row.
