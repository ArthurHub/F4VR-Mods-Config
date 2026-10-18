# RowWarning

A setting's or a button's warning: a value that works and has a cost, or a button whose action needs a save to be loaded again. It shows while its condition holds, and blocks nothing.

**Provides:** the row, and the text of the first of its `warnings` whose `when` holds on the pending values, or none.

- **The mark** is a hollow triangle after the name, 20 x 18: a 2 px `amber` outline with a `!` in `amber`, `space-2` after the name, or after the Restart mark when the row has one. Drawn with `AddTriangle` and two `AddRectFilled`.
- It is not the invalid mark: that one is a filled `red` triangle, in the gutter, and blocks Save. The gutter keeps the pending dot and the invalid mark, so a row can be pending and warned at once.
- The name shortens with an ellipsis before the marks do: the marks always show.
- An invalid row shows its invalid mark and message, and no warning mark: what is wrong comes first.
- **The help line** has the warning before the row's help, the mark and its text in `amber`: "Player height: ⚠ Above 1.4 m your view can be in the ceiling. Adjusts the estimated player height."
- Save stays as it is: a warning is not counted in the footer's status, and Save is not blocked.
- A folded section does not count warnings in its heading: only invalid values and unsaved changes.
- The Focused panel shows the mark after the name the same way, and the warning in its help area.

The behavior is in [Warnings](../../../../schema.md#warnings).
