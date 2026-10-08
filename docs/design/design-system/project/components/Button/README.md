# Button

A 48 px pressable: primary (Save), secondary (Discard), quiet (Cancel), and 48 x 48 icon buttons.

**Provides:** the label or drawn icon, and whether it is enabled.

- Height `control-height`, min width 112 (160 in the footer), `space-4` side padding, `radius-frame`.
- Primary: `green` fill, `on-green` text; hover `glow` fill; pressed `green` with a `glow` ring. One per view, only for Save.
- Secondary: transparent with a `frame-border` outline. Quiet: no outline until hovered.
- Hover `hover`, pressed `pressed` with a `glow` outline, focused a `green` ring, disabled `opacity-disabled`.
- For a primary, push `ImGuiCol_Button` = `green`, `ButtonHovered` = `glow`, `ButtonActive` = `green` and `Text` = `on-green` around `ImGui::Button`.
