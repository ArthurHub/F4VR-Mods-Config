# ButtonRow

A `button` item: a row that runs an action, a function of a script, a console command, or a function of the mod's native client, and the confirmation a config spec can ask for first.

**Provides:** the row's text and help, its `buttonText`, its `style` and `icon`, whether it can run (a `CallNativeFunction` needs the mod's client), its `confirmation`, and the result of its last run.

## The row

- A setting row with no Reset: the name, and in the control column a secondary button, 160 x 48, right-aligned where the controls end. The Reset slot is kept empty, so it lines up with the other rows.
- The button says `Run`, or the config spec's `buttonText`. A press anywhere on the row runs it, as a press on a toggle's row toggles it.
- Hover: the row's hover look. The button is only drawn, as the Open of a quick launch row is: the row is what is pressed.
- **Running:** while the action has not returned, the button is `pressed` with a `glow` outline, and says `Running` after a drawn arc that turns (`PathArcTo`, 20 px, a turn a second). A press on it does nothing. A Papyrus call returns when the script's function does; a console command at once.
- **The result**, when the action gives one, is a mark after the name and its text in the help line, as an invalid setting's reason is. Nothing is written under the name.
  - **Done:** the info mark after the name, 20 px: a 2 px `green` circle with an `i` in `green`, drawn with `AddCircle`, `AddRectFilled` and `AddCircleFilled`. It says a result is there that the player has not read: it goes when the ray leaves the row after being on it with the result there, so a result that comes back while the ray is on the row is read at once. While the ray is on the row, the help line has the info mark and the result in `green` before the row's help: "Calibrate height: ⓘ Done: 1.78 m. Measures your height and updates the calibration."
  - **Failed:** the invalid mark after the name, a filled `red` triangle, which stays until the row runs again or the page is left. While the ray is on the row, the help line is the failure in `red` with the triangle, as an invalid setting's reason is: "Calibrate height: Stand still, then try again."
  - The result stays in the help line until the row runs again or the page is left, after its mark is gone. An action that gives no message shows nothing, and the light pulse says it ran.
- **Disabled:** a `CallNativeFunction` whose mod has no client loaded, or a condition that disables it. The help line says why: "Needs Immersive Flashlight VR's DLL, which is not loaded."
- A warning shows its mark after the name; see [RowWarning](../RowWarning/README.md).

## Style and icon

A config spec can make a button stand out, with `style`, and give it a mark, with `icon` ([Buttons](../../../../schema.md#buttons)). An MCM button has neither.

- **No style:** as above, a `frame-border` outline and a `green` label.
- **`important`:** the outline, the label and the icon in `accent`, the only use of that color, so it reads as no state. Not filled: Save stays the one filled button of the view. Hover, running and the result are as with no style, and the outline stays `accent` while it runs. Pressed, its ring is `accent`.
- **`dangerous`:** the outline, the label and the icon in `red`, and a press always asks first, with the red confirmation below. A `confirmation` that is `dangerous` makes the button `dangerous`.
- Disabled: as any button, at `opacity-disabled`, in the color of its style.
- **The icon** is drawn, 20 px, in the color of the label, `space-2` before it: a 2 px stroke with round ends and joins, or a fill. The set: `play`, `stop`, `bolt`, `target`, `check`, `window`, `external`, `link`, `gear`, `import`, `export`, `copy`, `eye`, `hand`, `sound`, `pin`, `refresh` and `trash`. An `OpenURL` button with none has `link`. Running replaces the icon with the turning arc.
  - It avoids the shapes the menu uses for something else: the disk of Save's mark, the undo arrow of Discard, and Reset's single arc. `refresh` is two arrows that chase each other.
- Keep `important` to the one or two buttons a player must find on the page: when every button is important, none stands out.

## The confirmation

With a `confirmation`, a press opens it first, as the unsaved-changes dialog is drawn ([ConfirmDialog](../ConfirmDialog/README.md)): 576 px wide, centered on the panel, the panel dimmed with `scrim`.

- Its `title` in `t-title`, its `message` in `t-body`.
- Cancel (quiet) and the confirming button, which says `confirm`, or `buttonText`, or `Run`. It is primary.
- `dangerous`: the dialog's frame and the confirming button are `red`, the button's text `on-red`. Hover is the `red` fill with a 2 px `red` ring outside. The ray lands on Cancel first: the dialog opens with Cancel focused.
- Back is Cancel. A press beside the dialog does nothing, so a stray press does not run it.

## Unsaved changes first

A script reads the saved values, so while its mod has unsaved changes a press on a `CallFunction` or a `CallGlobalFunction` asks first, in the same dialog: its title is the confirmation's, or `Unsaved changes`, and it says how many changes there are and that the button's script reads the saved values. An MCM mod's button does not ask: its changes are made as the player makes them.

- Cancel (quiet), `Run without saving` (secondary), and `Save and run`, which is primary, or `red` for a dangerous confirmation. It stands for the confirming button, so a button never asks twice.
- `Save and run` is disabled while a change is invalid, as Save is, and the text asks to fix or discard it. `Run without saving` stays enabled.
- The button's help ends with "A press asks to save the unsaved changes first." while there are changes. A console command, a link and a function of the mod's DLL do not ask.

The behavior is in [Buttons](../../../../schema.md#buttons).
