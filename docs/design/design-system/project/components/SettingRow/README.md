# SettingRow

One setting: a gutter mark, the name, and its control with a Reset slot, all one ray target.

**Provides:** the setting's name, control, value and state (ready, pending, invalid, disabled), its default, and an invalid message.

- Height `control-height`, pitch `row-pitch`. Columns: `space-6` gutter, the name, the `control-width` control column. The last 48 px of that column is the Reset slot, kept empty when Reset does not apply so controls line up.
- Make the whole row the target: a `Selectable` that spans it, with `SetNextItemAllowOverlap()`, and the control drawn over it.
- States as in the brand book: hover (`hover` + `frame-border` outline), focused (`green` ring), active (`pressed` + `glow`), pending (`amber` dot, value in `amber`), invalid (warning triangle, value in `red`, message), disabled (`BeginDisabled`).
- An invalid row keeps its height: the name moves up and the `t-detail` message sits under it in `red`.
- Reset appears when the value differs from the default and the setting has one. It is an icon button; the help line names what it resets to while the ray is on it.

- A package setting whose change takes effect only after the game is restarted (`applies: restart`) has a `t-detail` **Restart** mark after its name, outlined in `frame-border`. Its help says so too.

Don't clamp an out-of-range typed value: show it as invalid.
