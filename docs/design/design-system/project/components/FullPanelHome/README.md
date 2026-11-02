# FullPanelHome

The menu's front page: what the menu opens on, with no mod selected.

**Provides:** the version of Mods Config FO4 VR, the quick launch entries (each a mod's own config UI, registered through the native API, with its help, its `buttonText`, and why it does not run), the mods whose saved changes wait for a restart, and the mods that are not available, each with its reason.

- Breadcrumbs: `MODS CONFIG FO4` alone, selected: solid `green` with `on-green` text, as a selected tree row. It is the page's only entry: pressing it, or Back from a mod's front page, lands here. No tree row is selected and no mod is expanded. Back on this page closes the menu.
- The title bar's Help is `? Help` here, as on any page until the player first opens the Help page, which is most often on this one ([FullPanel](../FullPanel/README.md)).
- A text item first, in `green-muted`: what the menu is and how to start, then the version on a line of its own.
- **Quick launch** and **Notices** are headings that fold, as a [SectionHeader](../SectionHeader/README.md) that folds is, with the hover look of a row while the ray is on them. Both are open each time the menu opens. Their help is in the help line:
  - Quick launch: "Mods with a configuration of their own, which opens from here."
  - Notices: "What needs your attention: a restart of the game, or a mod that is not available."
- **Quick launch**: one row per entry, the entries of the mods' native clients, sorted as the mods are. The name on the left, and in the control column a button (160 px) that says the entry's `buttonText`, or `Open`. The whole row is the target, as a [button row](../ButtonRow/README.md)'s is. The help line has the entry's help.
  - An entry whose function does not run, as one of a client that is cut off, is disabled, and its help is the reason.
  - What a press did is in the help line while the ray is on the row: its message in `green` after the info mark, before the entry's help, or in `red` after the invalid mark when it failed, as a button row's result.
- **Notices**: first a [RestartNotice](../RestartNotice/README.md) line per mod whose saved changes wait for a restart, then one `green-muted` text line per mod that is not available, with the reason: the same reason the help line shows on the mod's disabled row in the tree.
- A section with nothing in it is left out, its heading too.
- There is no editing session here, so Discard and Save are disabled; the help line shows help for the quick launch row under the ray.
