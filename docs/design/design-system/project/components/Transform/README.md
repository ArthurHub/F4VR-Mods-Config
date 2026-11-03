# Transform

A row of position, rotation and scale, which opens an editor of sliders, and tunes it in the world from the wrist. Behavior is defined in [Controls](../../../../controls.md#transform); storage and the ranges in [Schema](../../../../schema.md#commonframework-values).

**Provides:** the setting's name, its value (the TOML text), its default, its state, the keys the value has, the parts that are on and their ranges.

## Row

- A 392 x 48 frame: two `t-detail` lines in tabular digits, then the drawn down arrow. The first is `pos` and the position, and `×1.5` after it when the scale is not 1; the second is `rot` and the three angles in degrees. `pos` and `rot` are `green-muted`, 30 px wide, so the numbers line up.
- A key the value leaves out is drawn as what it means, in `green-muted`: `0, 0, 0` for no move or no rotation, and no `×` for a scale of 1. A part the config spec turns off is left out of the row; with rotation off the row is the one line.
- Pending: the numbers in `amber`, the pending mark after the name. Invalid: a value the framework cannot read, its text as the file has it in `red`, a `red` outline, and the invalid mark after the name; the help line has the framework's reason.
- A press anywhere on the row opens the editor.

## Editor

A popup inside the content pane, 560 wide, under its row, over it, or as near as it fits, as the Color editor is placed. `popup` background, a 2 px `green` border, `radius-popup`, `space-4` padding.

- **Header**, 48 px: the setting's name in `t-body`.
- **Lines**, `space-1` apart, one for each axis of each part that is on, in this order: X, Y, Z, Heading, Roll, Attitude, Scale. No headings: the names say which part each is. A transform with rotation off has four lines, one of rotation only three.
  - A line is 48 px: its name in `t-body`, 96 px, then the [Slider](../Slider/README.md) with its step arrows, the 224 px track and its value box. Hover: the line's, the `hover` fill and the `frame-border` outline.
  - Its range and its step are the config spec's for the part or the axis, or the part's own: a position -50 to 50 by 1, a rotation -180° to 180° by 1°, a scale 0.1 to 3 by 0.1.
  - The value box has the step's decimals, or the value's own where they are finer, so a file's 20.72° is `20.72°` with a step of 1°. Degrees have `°`.
  - A value outside the range is shown as it is, with the grab at its end and that end's arrow disabled.
  - A changed value is amber, its fill and grab too.
- **Footer**, under a `rule` line: **Adjust in world** on the left, a button in the `important` style, the `accent` outline and text, with the `hand` icon before its text ([ButtonRow](../ButtonRow/README.md)); Cancel (quiet) and Done (primary) on the right.
- Every change sets the pending value as it is made, and a `live` transform is previewed: the mesh or the light moves as it is changed.
- Done, or a press beside the editor, closes it and keeps the value. Cancel and Back close it and give back the value it had when it opened.
- While the ray is on a line, the help line says what it moves, its range and step, and the default: "Y: Moves the flashlight mesh along Y, -20 to 20, by 0.01 a step. Default -5.86."

## Adjust in world

The panel is minimized, and the offhand wrist has a column of its own in place of Restore and Exit ([WristButtons](../WristButtons/README.md)). It is vrui, not ImGui: the framework's primitive panels in `F4VR_PANEL_STYLE` and `F4VR_BUTTON_STYLE`, as Immersive Flashlight's and FRIK's own config UIs are. From the top, 0.25 units apart:

- **Title**: `Mods Config: ` and the setting's name, plain text in `green`, capitals 0.65 units tall, not underlined and in no box (`UITextPanel` with no style of its own).
- **Values**: a box as wide as its content needs, with the values on one line, capitals 0.3 units tall, each with two decimals, also a zero, so the numbers do not jump as they change: the position, the rotation in degrees and the scale after `×`, each group `space-3` from the next and only the parts that are on. It changes as the object moves; a value that differs from when the tuning started is `amber`.
- **Controls**: a box as wide as its content needs, with 0.3 units of padding at its sides, capitals 0.35 units tall, as the footer of FRIK's Pip-Boy adjust screen, with each binding's icons three times as tall as the text (`appendBindingPrompt`, so a left-handed player's are the left hand's):
  - `ADJUST:` and the right and the left thumbstick, with a `/` between them, while position is on.
  - Then one line of the modifiers: `+ROTATION:` with the hold of the offhand grip, and `+SCALE:` with the hold of the offhand A, each its text and its icon, for the parts that are on.
  - With position off there is no `ADJUST:` line, so a modifier's line has its sticks after it: `ROTATION:` grip + both thumbsticks, as the offhand's X rolls it, and `SCALE:` A + the right thumbstick. Rotation is always with the grip.
  - The icons are the plain thumbstick's, with no arrow of a direction.
- **Cancel** and **Done**: the two wrist buttons, 3 x 2.5 units each, 0.5 apart, each its icon over its label, capitals 0.4 units tall: an X and `CANCEL`, a check and `DONE`. There are no bindings for them.

Done brings the panel back with the editor open and the tuned value pending; Cancel gives back the value from before Adjust in world. The button is enabled for a `live` transform whose mod's client is connected, and is disabled otherwise, with the reason in the help line; see [Controls](../../../../controls.md#transform).

## Writing the value

- A changed value is written with the keys the file had, and the key of the part that changed: `{ rot = [0.0, -5.0, 0.0] }` with a new Y is `{ pos = [0.0, 1.0, 0.0], rot = [0.0, -5.0, 0.0] }`. Numbers are written as the framework writes them, with `.0` on a whole number.
- A part that is off is written as the file has it.
- Reset sets the default as the config spec gives it.
