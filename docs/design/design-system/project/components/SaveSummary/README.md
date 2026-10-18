# SaveSummary

What Save shows when it is pressed while a changed value is invalid: every invalid setting of the mod, the first one that shows focused on its page, and a Reset for each.

**Provides:** the invalid settings of the session: each one's name, where it is (its page and sections), its message, and whether it shows now (a folded section, or a condition that hides or disables it).

- An invalid value that was already in the file, and was not changed, does not block Save; it is listed too, after the ones that block, as the summary names every invalid setting.
- Save is secondary while a value blocks it, and the footer's status says how many: the warning mark and "3 invalid values" in `red`. A press on it opens the summary; nothing is written.
- **At the same time** the page scrolls to the first invalid setting that shows on it, and that row takes the focus ring (`green`). When none shows on this page, the page stays as it is.
- **The summary** is a popup over the content pane, its bottom-right corner above Save: 640 wide, `popup` background, a 2 px `red` border, `radius-popup`, `space-4` padding. It is never over the footer, so Save and Discard stay where they are.
- **Header:** the warning mark and "3 values are not valid. Save is blocked until they are." in `t-body` `red`.
- **A line for each** invalid setting, in the order of the mod's pages: the warning mark; then the name in `t-body`, where it is in `t-detail` `green-muted`, and the message in `t-detail` `red`; then Reset, an icon button, at the right. Hover: `hover` with a `frame-border` outline.
  - Where it is: its page and the sections it is in, joined by `»` ("Weapons » Advanced"), and why it does not show, when it does not: ", folded", or ", hidden while Sling is off" with the condition's setting by its name.
  - A press on a line goes to the setting: its page, its folded sections opened, the row scrolled into view and focused, and the summary closed. A setting a condition hides cannot be shown: its name is in `green-muted`, a press on its line does nothing, and Reset is how it is fixed.
  - Reset sets the setting to its default. Its line goes, and the summary closes once no line is left, with Save primary again.
- More lines than fit scroll inside the summary, as a dropdown's list does.
- **Footer:** Close (quiet). Back, Close, or a press beside the summary closes it, and the values stay as they are.
- While the ray is on a line, the help line has the setting's help, as on its row.

The behavior is in [Help and validation feedback](../../../../controls.md#help-and-validation-feedback).
