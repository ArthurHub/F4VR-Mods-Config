# Transform

A CommonFramework transform, a `t` key, `{ pos = [x, y, z], rot = [heading, roll, attitude], scale = s }` with each key optional: where a mesh, a light or a zone sits on its node. A frame that shows the numbers, and an editor with a stepper for each axis, which hands off to tuning in the world.

**Provides:** the setting's name, its value (the TOML text), its default, its state, and the keys the value has.

## The row

- The frame is 392 x 48: two `t-detail` lines in tabular digits, then the drawn down arrow. The first is `pos` and the position, and `×1.5` after it when the scale is not 1; the second is `rot` and the three angles in degrees. `pos` and `rot` are `green-muted`, 30 px wide, so the numbers line up.
- A key the value leaves out is drawn as what it means, in `green-muted`: `0, 0, 0` for no move or no rotation, and no `×` for a scale of 1.
- A press anywhere on the row opens the editor.
- Pending: the numbers in `amber`, the pending dot after the name. Invalid: a value the framework cannot read, its text as the file has it in `red`, a `red` outline, and the invalid mark after the name; the help line has the framework's reason.

## The editor

A popup inside the content pane, 717 wide, placed as the Color editor is. `popup` background, a 2 px `green` border, `radius-popup`, `space-4` padding.

- **Header**, 48 px: the setting's name in `t-body`, and on the right "Step" in `t-detail` `green-muted` and a choice of three, `Fine`, `Normal` and `Coarse`, the chosen one solid `green`. It sets the step of every stepper: 0.1, 1 or 10 for a position, 1°, 5° or 15° for an angle, 0.01, 0.05 or 0.1 for the scale. `Normal` when the editor opens; the choice is kept for the session.
- **Body**, two columns with a `t-detail` `green-muted` heading over a `rule` line each:
  - Position: X, Y and Z, then Scale.
  - Rotation: Heading, Roll and Attitude, the names the framework gives them.
  - A line is 48 px: its name in `t-detail` (72 px), minus, the value box, plus. The value box is as wide as fits, centered, two decimals at most, degrees with `°`. Hover: the line's.
  - A press on minus or plus steps it, and holding repeats faster. The thumbstick steps the line the ray is on, as a slider's.
  - A key the value leaves out shows its meaning in `green-muted` until it is changed.
  - A changed value is in `amber`.
- **Footer**, under a `rule` line: In the world (secondary) on the left, Cancel (quiet) and Done (primary) on the right.
- Every change sets the pending value as it is made, and a `live` transform is previewed: the mesh or the light moves as it is stepped.
- Done, or a press beside the editor, closes it and keeps the value. Cancel and Back close it and give back the value it had when it opened.
- While the ray is on a line, the help line says what it moves and by how much: "Y: Moves the flashlight mesh along Y by 0.1 a step. The thumbstick steps it."

## In the world

- In the world hands the transform to the framework's live tuning (`DebugAdjuster`), which moves the object with the controllers while the player looks at it. The menu minimizes, as for a quick launch entry, and the wrist has `Done` and `Cancel` for it.
- `Done` takes the tuned value as the pending value and brings the menu back with the editor open; `Cancel` gives back the value from before.
- Open: the framework's tuning works on its own mod's keys today. Tuning another mod's key, and handing back the value, is framework work; until then In the world is not shown.

## Writing the value

- A changed value is written with the keys the file had, and the key of the part that changed: `{ rot = [0.0, -5.0, 0.0] }` with a new Y is `{ pos = [0.0, 1.0, 0.0], rot = [0.0, -5.0, 0.0] }`. Numbers are written as the framework writes them, with `.0` on a whole number.
- Reset sets the default as the package gives it.

## Focused panel

The editor fills the Focused panel in one column, Position then Rotation, scrolling, with the step choice in its header. The Focused panel is the place to tune a transform while looking at it.

The behavior is in [Controls](../../../../controls.md#transform).
