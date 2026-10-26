# DescriptionArea

The help line: the left of the footer, which shows help for what the ray is on and the session status when the ray is elsewhere.

**Provides:** the current validation error, the setting, option or control under the ray with its help, and the session status (pending and invalid counts, or the last Save).

- It takes the footer's width left of Discard and Save, about 740 px. Discard and Save never move, and nothing in the help line is pressable.
- Format: "Name: help". The name and colon in `green`, the help in `green-muted`. The name is the row's, so a name cut off by an ellipsis in its row shows here in full.
- Two lines at `t-body`. Help that needs a third line is drawn at `t-detail` (three lines fit the 72 px footer); past three lines it ends in an ellipsis.
- Priority: an active validation error, in `red` with the warning triangle; then help for the setting, dropdown option, Reset, quick launch entry, unavailable mod or crumb under the ray; then the status ("1 unsaved change", "No unsaved changes", or why the last Save failed).
- Hold the help while the ray is anywhere over the rows, the gaps between them included, and switch back to the status only when the ray leaves the list or the open popup. Without that it flickers as the ray sweeps.
- Include the default where there is one, in brackets after the help ("(Default: 1.00)"). Reset's help names the value it resets to, and a pending setting's Discard the saved value it goes back to: "Discard the change, back to 1.00." A changed setting whose mod says when the change takes effect has its tag first, in `amber` ([TakesEffect](../TakesEffect/README.md)): the help does not say it.
- A page's own help is not here: it is the first item of the page's rows.
- The Focused panel keeps this at the top, in a `description-height` area on `plate`, because its footer has no room.

Don't use tooltips: the help line replaces them, at a fixed place and depth.
