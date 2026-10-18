# ReadOnly

A setting with `readOnly`: a value the mod writes for the player to read, as a measured height.

**Provides:** the row, its control, and the value its storage has, or none.

- The row is drawn as any row of its control, with the value it has, and its name and control are disabled (`BeginDisabled`, `opacity-disabled`): a press does nothing, and the row has no pressed look. While the ray is on it, it has the hover look at full strength, drawn before `BeginDisabled`, so its help is seen to be its, as an MCM [hotkey](../Hotkey/README.md)'s row has.
- It has no Reset, and needs no default. The Reset slot is kept empty, so its control lines up with the others.
- While its storage has no value, its control shows none: a slider's track with no fill and no grab and an empty value box, a toggle with neither side on, an empty text box.
- The ray on it still shows its help, with `(Read-only)` first: "Measured height: (Read-only) The height FRIK measured at the last calibration."
- It is never pending or invalid, and is not counted by Save.
- A value it has from the file is shown as it is, even outside the control's range: the slider's grab stops at its end, and the value box has the number.

The behavior is in [Read-only](../../../../controls.md#read-only).
