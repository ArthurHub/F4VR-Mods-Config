# ButtonRow

A `button` item: a row that runs an action, a function of a script, a console command, or a function of the mod's native client, and the confirmation a package can ask for first.

**Provides:** the row's text and help, its `buttonText`, whether it can run (a `CallNativeFunction` needs the mod's client), its `confirmation`, and the result of its last run.

## The row

- A setting row with no gutter mark and no Reset: the name, and in the control column a secondary button, 160 x 48, right-aligned where the controls end. The Reset slot is kept empty, so it lines up with the other rows.
- The button says `Run`, or the package's `buttonText`. A press anywhere on the row runs it, as a press on a toggle's row toggles it.
- Hover: the row's hover look. The button is only drawn, as the Open of a quick launch row is: the row is what is pressed.
- **Running:** while the action has not returned, the button is `pressed` with a `glow` outline, and says `Running` after a drawn arc that turns (`PathArcTo`, 20 px, a turn a second). A press on it does nothing. A Papyrus call returns when the script's function does; a console command at once.
- **The result**, when the action gives one, is a mark in the gutter and its text in the help line, as an invalid setting's reason is. Nothing is written under the name.
  - **Done:** the info mark in the gutter, 20 px: a 2 px `green` circle with an `i` in `green`, drawn with `AddCircle`, `AddRectFilled` and `AddCircleFilled`. It says a result is there that the player has not read: it goes when the ray leaves the row after being on it with the result there, so a result that comes back while the ray is on the row is read at once. While the ray is on the row, the help line has the info mark and the result in `green` before the row's help: "Calibrate height: ⓘ Done: 1.78 m. Measures your height and updates the calibration."
  - **Failed:** the invalid mark in the gutter, a filled `red` triangle, which stays until the row runs again or the page is left. While the ray is on the row, the help line is the failure in `red` with the triangle, as an invalid setting's reason is: "Calibrate height: Stand still, then try again."
  - The result stays in the help line until the row runs again or the page is left, after its mark is gone. An action that gives no message shows nothing, and the light pulse says it ran.
- **Disabled:** a `CallNativeFunction` whose mod has no client loaded, or a condition that disables it. The help line says why: "Needs Immersive Flashlight VR's DLL, which is not loaded."
- A warning shows its mark after the name; see [RowWarning](../RowWarning/README.md).

## The confirmation

With a `confirmation`, a press opens it first, as the unsaved-changes dialog is drawn ([ConfirmDialog](../ConfirmDialog/README.md)): 576 px wide, centered on the panel, the panel dimmed with `scrim`.

- Its `title` in `t-title`, its `message` in `t-body`.
- Cancel (quiet) and the confirming button, which says `confirm`, or `buttonText`, or `Run`. It is primary.
- `dangerous`: the dialog's frame and the confirming button are `red`, the button's text `on-red`. Hover is the `red` fill with a 2 px `red` ring outside. The ray lands on Cancel first: the dialog opens with Cancel focused.
- Back is Cancel. A press beside the dialog does nothing, so a stray press does not run it.

The behavior is in [Buttons](../../../../schema.md#buttons).
