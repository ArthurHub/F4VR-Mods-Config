# WristButtons

The vrui buttons on the offhand wrist that stay while the panel is shown or minimized: Minimize or Resume, and Close.

**Provides:** whether the menu is minimized.

- `vrui::UIButtonPanel`, 3 x 2.2 units each, `WRIST_BUTTON_GAP` 0.2 apart, in `F4VR_BUTTON_STYLE`: `glass-widget`, `border-widget` in `green`, `radius-widget`.
- Top text `CLICK` (`setTopText`), label in capitals (`setMiddleText`). The Minimize button's label changes to `RESUME` while minimized.
- With FRIK they are pressed with the index fingertip. How they are pressed without FRIK is an open question in the design.

These are not ImGui: their text is drawn by `render::TextFont` in the same Roboto Medium.
