# UnsupportedControl

A row of a control the menu does not have: its text and its type, so the player sees that the mod has something there and why it cannot be changed.

**Provides:** the row's text, and the type its config gives it, or none.

- Laid out as a setting's row: the text, and the type in `t-body` `green-muted` where a control ends, right-aligned before the Reset slot, which is kept empty.
- The type is written as the config writes it (`positioner`, `keyinput`, `customClipLoader`), and is `unknown` for a row with no type.
- The whole row is always disabled (`BeginDisabled`, `opacity-disabled`): there is nothing to press, and a press has no look.
- While the ray is on it, it has the hover look at full strength, `hover` with a `frame-border` outline, drawn before `BeginDisabled`, so its help is seen to be its, as a [hotkey](../Hotkey/README.md)'s row has.
- Its help says the menu has no such control, with the type, in place of the row's own help: "Holster position: Not available: the menu has no positioner control." A row with no type: "Advanced options: Not available: the row has no type of control."
- It keeps its place among the rows and is shown by its conditions as any row is. It holds no value and is not counted by Save.

It is drawn for an MCM row that MCM's menu runs Flash for (`positioner`, `keyinput`, `customClipLoader`), a type MCM's menu does not have, and a row of a package that the menu does not draw yet. The behavior is in [MCM support](../../../../mcm.md#rows).
