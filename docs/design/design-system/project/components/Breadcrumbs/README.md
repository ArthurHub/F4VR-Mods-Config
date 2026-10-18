# Breadcrumbs

Where the player is, in the title bar of the Full panel: `MODS CONFIG VR » <mod> » <page>`, with a crumb for every page above the page.

**Provides:** the selected mod and the path of pages to the selected page.

- `t-title`, each crumb `control-height` high, `»` between them in `green-muted`.
- The last crumb is where the player is, in `green`, and is not pressed. Each crumb before it is `green-muted` and is pressed to go there: `MODS CONFIG VR` to the menu's front page, the mod to its front page, a page to it. Under the ray a crumb has the row's hover look, and its text is `green`.
- On the menu's front page `MODS CONFIG VR` is the only crumb, and is selected: solid `green`, `on-green` text.
- On the Help page they are `MODS CONFIG VR » Help`, wherever it was shown from (`FullPanelHelp`).
- **When the crumbs do not fit** the title bar beside its buttons:
  1. The pages between the mod and the page become one `…`, which is not pressed: the tree and Back go there.
  2. If they still do not fit, the mod's name shortens with an ellipsis.
  The first crumb and the last are never shortened.
- Back goes the way of the crumbs: to the page above, then to the mod's front page, then to the menu's front page. From the Help page it goes back to the page Help was pressed on.

The behavior is in [Full](../../../../ui-and-vr-interaction.md#full) and [Back behavior](../../../../ui-and-vr-interaction.md#back-behavior).
