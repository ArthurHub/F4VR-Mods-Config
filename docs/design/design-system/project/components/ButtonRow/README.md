# ButtonRow

A `button` item: a row that runs an action, a function of a script, a console command, or a function of the mod's native client, and the confirmation a package can ask for first.

**Provides:** the row's text and help, its `buttonText`, whether it can run (a `CallNativeFunction` needs the mod's client), its `confirmation`, and the result of its last run.

## The row

- A setting row with no gutter mark and no Reset: the name, and in the control column a secondary button, 160 x 48, right-aligned where the controls end. The Reset slot is kept empty, so it lines up with the other rows.
- The button says `Run`, or the package's `buttonText`. A press anywhere on the row runs it, as a press on a toggle's row toggles it.
- Hover: the row's hover look. The button is only drawn, as the Open of a quick launch row is: the row is what is pressed.
- **Running:** while the action has not returned, the button is `pressed` with a `glow` outline, and says `Running` after a drawn arc that turns (`PathArcTo`, 20 px, a turn a second). A press on it does nothing. A Papyrus call returns when the script's function does; a console command at once.
- **The result**, when the action gives one, is a `t-detail` line under the name, as an invalid row's message is: in `green-muted` when it worked ("Done: 1.78 m"), and in `red` with the warning mark in the gutter when it failed ("Stand still, then try again"). It stays until the page is left. An action that gives no message shows nothing, and the light pulse says it ran.
- **Disabled:** a `CallNativeFunction` whose mod has no client loaded, or a condition that disables it. The help line says why: "Needs Immersive Flashlight VR's DLL, which is not loaded."
- A warning shows its mark after the name; see [RowWarning](../RowWarning/README.md).

## The confirmation

With a `confirmation`, a press opens it first, as the unsaved-changes dialog is drawn ([ConfirmDialog](../ConfirmDialog/README.md)): 576 px wide, centered on the panel, the panel dimmed with `scrim`.

- Its `title` in `t-title`, its `message` in `t-body`.
- Cancel (quiet) and the confirming button, which says `confirm`, or `buttonText`, or `Run`. It is primary.
- `dangerous`: the dialog's frame and the confirming button are `red`, the button's text `on-red`. Hover is the `red` fill with a 2 px `red` ring outside. The ray lands on Cancel first: the dialog opens with Cancel focused.
- Back is Cancel. A press beside the dialog does nothing, so a stray press does not run it.

The behavior is in [Buttons](../../../../schema.md#buttons).
