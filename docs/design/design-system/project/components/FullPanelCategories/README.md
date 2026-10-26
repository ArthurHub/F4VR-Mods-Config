# FullPanelCategories

The `Categories` page of Mods Config FO4 VR's own entry, where the player gives a mod another category. The tree is shown grouped, with the page selected under `Interface` » `Mods Config FO4`.

**Provides:** every mod with its current category and the category it has without the player's choice; the shipped names and the names in use; whether MCM Categorizer's settings file is there.

- Breadcrumbs: `MODS CONFIG FO4 » Mods Config FO4 » Categories`. The category is not a crumb.
- A one-line text item first: what the page does.
- **Import from MCM Categorizer**, shown only when its settings file is found: a row with an **Import** button. Pressing it sets every mod in one of that mod's categories as a pending choice, so Save keeps them and Discard takes them back.
- **Mods**: a row per mod, alphabetical, with a `Dropdown` of category names. Each row is an ordinary setting: a changed one is pending (amber dot and value, Discard in the Reset slot), Save writes it, Discard takes it back, and Reset gives the mod the category it has without the player's choice.
- The dropdown lists the shipped names and the names in use, then `No category`, then **Type a new name**, set apart by a rule and in `green-muted`, which opens the keyboard. The list scrolls inside its popup, with a position bar.
- The help line names what a row's Reset would give and where it comes from (the mod's own `category` or the table of known mods).
- The tree is grouped again after Save.
