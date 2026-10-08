# FullPanelHome

The menu's front page: what the menu opens on, with no mod selected. A proposal for M11, which settles the page.

**Provides:** the quick launch entries (each a mod's own config UI, registered through the native API), and the notices the host has about mods it could not show.

- Breadcrumbs: `MODS CONFIG VR` alone, selected: solid `green` with `on-green` text, as a selected tree row. It is the page's only entry: pressing it, or Back from a mod's front page, lands here. No tree row is selected and no mod is expanded.
- A one-line text item first, in `green-muted`: what the menu is and how to start.
- **Quick launch**: one row per entry, its name on the left and an **Open** button (160 px) in the control column. The whole row is the target. FRIK is the first entry when FRIK is installed. The help line says what the mod's own UI does.
- **Notices**: one `green-muted` text line per mod that is listed but not available, or a package that was not loaded, with the reason. The section is left out when there is nothing to say.
- There is no editing session here, so Discard and Save are disabled; the help line shows help for the quick launch row under the ray.
