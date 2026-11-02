# WristButtons

The vrui buttons on the offhand wrist while the panel is minimized: Restore and Exit, under the menu's title. While the panel is shown, the wrist has none: Minimize is on the title bar.

**Provides:** whether the menu is minimized, and whether the mod that is shown has unsaved changes.

- A row of two `vrui::UIButtonPanel`, 3 x 2.5 units each, `WRIST_BUTTON_GAP` 0.5 apart, in `F4VR_BUTTON_STYLE`: `glass-widget`, `border-widget` in `green`, `radius-widget`.
- Each is its icon over its label in capitals (`setImage`, `setBottomText`), the text 0.5 units tall: `RESTORE`, whose icon is `vrui\restore.dds` of the mod's textures, written by `tools/gen_wrist_icons.py`, and `EXIT`, `vrui\exit.DDS`.
- Over them, 0.5 units apart, the title `MODS CONFIG`, 0.75 units tall and underlined, in `green` (`UITextPanel`), so the player knows whose buttons they are.
- **Restore** shows the panel again, as it was, with the shown mod's values read again. The opening gesture does the same while the menu is minimized.
- **Exit** closes the menu: while the mod that is shown has unsaved changes, the panel is shown with the unsaved-changes dialog first ([ConfirmDialog](../ConfirmDialog/README.md)).
- While a transform is adjusted in the world, the wrist has the transform's own column in place of these: its name, its values, its controls, and Cancel and Done; see [Transform](../Transform/README.md#adjust-in-world).
- With FRIK they are pressed with the index fingertip. How they are pressed without FRIK is an open question in the design.

These are not ImGui: their text is drawn by `render::TextFont` in the same Roboto Medium.
