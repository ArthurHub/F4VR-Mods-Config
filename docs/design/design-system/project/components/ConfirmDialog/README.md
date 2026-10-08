# ConfirmDialog

The modal that asks what to do with unsaved changes before leaving a mod or closing the menu: Save, Discard or Cancel.

**Provides:** the mod's name, the pending count, and what the player was about to do.

- 576 px wide, centered on the panel, on `popup` with a 2 px `green` border and `radius-popup`; `space-6` padding. The panel behind is dimmed with `scrim` (`ModalWindowDimBg`).
- Title in `t-title`, one sentence in `t-body` that names the mod and the next step.
- Buttons right-aligned: Cancel (quiet), Discard (secondary), Save (primary). Back is Cancel.
- `ImGui::BeginPopupModal` with `AlwaysAutoResize`, positioned with `SetNextWindowPos` at the panel center.
