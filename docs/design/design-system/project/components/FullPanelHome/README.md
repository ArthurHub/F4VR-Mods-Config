# FullPanelHome

The menu's front page: what the menu opens on, with no mod selected.

**Provides:** the version of Mods Config FO4 VR, the quick launch entries (each a mod's own config UI, registered through the native API), and the mods that are not available, each with its reason.

- Breadcrumbs: `MODS CONFIG FO4` alone, selected: solid `green` with `on-green` text, as a selected tree row. It is the page's only entry: pressing it, or Back from a mod's front page, lands here. No tree row is selected and no mod is expanded. Back on this page closes the menu.
- A text item first, in `green-muted`: what the menu is and how to start, then the version on a line of its own.
- **Quick launch**: one row per entry, alphabetical, its name on the left and an **Open** button (160 px) in the control column. The whole row is the target. The help line says what the mod's own UI does.
- **Notices**: one `green-muted` text line per mod that is not available, with the reason: the same reason the help line shows on the mod's disabled row in the tree.
- A section with nothing in it is left out, its heading too.
- There is no editing session here, so Discard and Save are disabled; the help line shows help for the quick launch row under the ray.
