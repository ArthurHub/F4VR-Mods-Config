# Haptic

A pattern of the framework's haptic library, picked from a dropdown that draws each pattern's shape, with a Play button to feel it on the hand.

**Provides:** the setting's name, its value (a pattern name or `none`), its default and its state. The options are the menu's: None and the 18 patterns of `vrcf::HapticPattern`, in the library's order, with the text and help in the table below and the keyframes from `VRControllersHaptic::getPattern`.

- The control column holds, from the left: the frame (336 x 48), the Play button (48 x 48), and the Reset slot. The frame is the Dropdown's: `track` with a `frame-border` outline, the label on the left, the pattern's shape, and the drawn down arrow on the right.
- **The shape** is 96 x 24, `space-3` before the arrow. A pattern is drawn at 200 px a second, or at less so that one longer than 0.48 s fits the 96 px: a tap stays narrow and a rumble fills the box, and the length is in the help. A 2 px base line in `rule` runs the full width. Each keyframe of intensity is a quad over it, `AddQuadFilled` from (x0, base) up to its start and end intensities and back to (x1, base), where an intensity of 1 is 22 px. A keyframe of no intensity is a gap. A keyframe is at least 2 px wide. None is the base line alone.
- The shape is drawn in the value's color: `green`, `amber` while pending, `on-green` on the current option's solid fill, where the base line is `on-green` at 40%. An invalid value has no shape.
- **Play** is an icon button: a 5 px dot with two arcs on each side, 2.5 px strokes in the 24 px box (`AddCircleFilled`, `PathArcTo`). A press plays the value's pattern on the hand that points. It is disabled (`BeginDisabled`) for None and for an invalid value. While the ray is on it, the help line reads "Press haptic: Plays Tick on the hand that points."
- **The list** is the Dropdown's popup, as wide as the frame, five options at most, scrolling from the current option. Each option is the check, its text, and its shape right-aligned. Picking an option plays it once on the hand that points, and sets the pending value.
- Pointing at an option plays nothing. The help line shows the option's help from the table.
- A press that plays a pattern gives no press pulse of the menu. Reset does not play.
- Pending: the label and the shape in `amber`, and the pending mark after the name.
- Invalid: a name the library does not have. The label is the text as the file has it, in `red` with a `red` outline, and the invalid mark after the name. The help line reads "Tik is not a haptic pattern, so none plays. Pick one, or Reset it to Tick."

| Stored        | Text         | Length | Help                                                         |
| ------------- | ------------ | ------ | ------------------------------------------------------------ |
| `none`        | None         |        | No pulse.                                                    |
| `Tick`        | Tick         | 0.02 s | The lightest single tap. For a change of selection.          |
| `Click`       | Click        | 0.03 s | A crisp single tap. For a press.                             |
| `DoubleClick` | Double click | 0.11 s | Two quick taps. For turning something on, or a change of mode. |
| `TripleClick` | Triple click | 0.22 s | Three quick taps. For a rare action.                         |
| `Success`     | Success      | 0.18 s | A light tap, then a strong one. For something that worked.   |
| `Warning`     | Warning      | 0.31 s | A strong tap, then a weak one. For something that needs attention. |
| `Error`       | Error        | 0.38 s | Two short buzzes, then a long strong one. For something that failed. |
| `Notification`| Notification | 0.40 s | A long buzz, then two light taps. For something that happened elsewhere. |
| `Start`       | Start        | 0.60 s | A rise into a hold. For something that began.                |
| `Stop`        | Stop         | 0.60 s | A hold, then a fall. For something that ended.               |
| `RampUp`      | Ramp up      | 0.60 s | A smooth rise from nothing to full. For charging or filling. |
| `RampDown`    | Ramp down    | 0.60 s | A smooth fall from full to nothing. For draining or turning off. |
| `Heartbeat1`  | Heartbeat 1  | 0.24 s | One heartbeat.                                               |
| `Heartbeat2`  | Heartbeat 2  | 0.88 s | Two heartbeats.                                              |
| `Heartbeat3`  | Heartbeat 3  | 1.52 s | Three heartbeats.                                            |
| `Buzz`        | Buzz         | 0.25 s | A short, even rumble.                                        |
| `MidBuzz`     | Mid buzz     | 0.50 s | An even rumble.                                              |
| `LongBuzz`    | Long buzz    | 0.70 s | A long, stronger rumble.                                     |

The help line shows an option as its text, its help and its length: "Double click: Two quick taps. For turning something on, or a change of mode. 0.11 s."

The behavior is in [Controls](../../../../controls.md#haptic).
