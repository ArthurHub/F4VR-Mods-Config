# SettingRow

One setting: the name with its marks after it, and its control with a Reset slot, all one ray target.

**Provides:** the setting's name, control, value and state (ready, pending, invalid, disabled), its default, and why a value is invalid.

- Height `control-height`, pitch `row-pitch`. A row under a heading is set in a level, `space-3`, from it, so its frame starts under the heading's text. A row under no heading is at the content's left, where a heading's row starts, so its frame lines up with a folding heading's and its name with a heading's text. Every row ends where the others do. Columns: the name, `space-3` in from the row's left inside its frame, then the `control-width` control column. The last 48 px of that column is the Reset slot, kept empty for a setting with no default so controls line up.
- The row's marks follow its name, `space-2` after it and apart, in this order: the Restart tag, the pending dot, then the warning mark or the invalid mark. Most rows have none, so the name has the row's width. The name shortens with an ellipsis before the marks do, so the marks always show: draw the name clipped to its room less the marks' width, then each mark after it.
- Make the whole row the target: a `Selectable` that spans it, with `SetNextItemAllowOverlap()`, and the control drawn over it.
- States as in the brand book: hover (`hover` + `frame-border` outline), focused (`green` ring), active (`pressed` + `glow`), pending (`amber` dot after the name, value in `amber`), invalid (the invalid mark after the name, value in `red`), disabled (`BeginDisabled`).
- An invalid row is drawn as any other, with its marks: why it is invalid is in the help line while the ray is on it, as its help is.
- Reset is there for a setting that has a default, and is disabled (`BeginDisabled`) while the value is the default. It is an icon button; the help line names what it resets to while the ray is on it.

- A config spec setting whose change takes effect only after the game is restarted (`applies: restart`) has a `t-detail` **Restart** mark after its name, outlined in `frame-border`. Its help says so too.

- A setting with a warning that holds has a hollow `amber` mark after its name and its pending dot; see [RowWarning](../RowWarning/README.md). A read-only setting is a disabled row with no Reset; see [ReadOnly](../ReadOnly/README.md). An MCM hotkey is laid out as a setting's row, with its key in a box; see [Hotkey](../Hotkey/README.md). A control the menu does not have is its text and its type; see [UnsupportedControl](../UnsupportedControl/README.md).

Don't clamp an out-of-range value: show it as invalid.
