# 03. A mod list module that owns the mods and their categories

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam C, inside the menu controller. Blocked by [04](04-menu-controller.md).

## Problem Statement

The mods and their categories are split between the menu and the panel.

- **Two copies of the category state.** The menu holds the category table, the player's choices and a "categories saved" flag. The panel holds the table again, the minimum number of mods to group from, the choices and the groups.
- **Manual wiring in the menu.** The menu refills the rows of its own Categories page on each of three reload paths. It reads the offered choices from the concrete config spec store, and detects a Save of the categories by matching a file name.
- **The panel's share.** It owns the mods' order, remapping the selection, the report and a pending leave when a mod is replaced, and the live copy read status.

The Categories page touched 21 files, and grouping the tree touched the config, the menu, the mod, the panel and the tree.

## Solution

One game-free mod list module, inside the menu controller, owns the mods, their order, replacing one, the category state, grouping, and refilling the Categories page. The panel reads the mods and groups from it.

## User Stories

1. As the maintainer, I want the mods, their order and replacing one owned by one module, so that a reload cannot leave the panel's list and the menu's list out of step.
2. As the maintainer, I want the category table, the player's category choices and the grouping of the tree held once, so that the menu and the panel never hold two copies that can drift.
3. As the maintainer, I want the rows of the menu's own Categories page refilled by the module that owns the mod list, so that no reload path can forget to refill them.
4. As a player, I want the tree regrouped as soon as I save a new category for a mod, so that the menu shows my choice, as it does today.
5. As the maintainer, I want a Save of the categories detected through the store's Save, not by matching a file name, so that renaming the file cannot silently break regrouping.
6. As the maintainer, I want the categories offered on the Categories page to include pending choices through the Store interface, so that the mod list does not reach into a concrete store.
7. As the maintainer, I want "reload, then rows refilled and tree regrouped" tested through the menu controller, so that this path is covered by a unit test.
8. As a player, I want the tree's grouping, category rows and their open state to look as today, so that the change is invisible to me.

## Implementation Decisions

- **The mod list owns:**
  - the mods and their order;
  - replacing one after a reload, keeping the selection, the report and a pending leave pointing at the right mod;
  - the live copy read status;
  - the category table, the player's category choices and the minimum number of mods to group from;
  - grouping by category;
  - refilling the rows of the menu's own Categories page.
- **Where it lives.** Inside the menu controller, tested through it.
- **No copies left.** The menu and the panel stop holding their own copies of the category state. The panel reads the groups and the mods from the mod list.
- **Detecting a Save.** A Save of the menu's own config spec that wrote the categories is detected through the store's Save, not by matching a file name.
- **Pending choices** on the Categories page are read through the Store interface.
- **What stays.** The category functions in the model stay as they are, as the mod list's implementation. The tree's own open state of category rows stays in the tree.

## Testing Decisions

- **Seam.** Tests go through the menu controller's interface with its recording adapter, the real config spec store over a per-test folder, and a temporary `Categories.toml`.
- **They cover:**
  - grouping from the minimum number of mods;
  - a Save of a category regrouping the tree;
  - the Categories page refilled after a reload;
  - a replaced mod keeping the selection;
  - pending choices offered on the Categories page.
- **Prior art:** the existing categories tests, the config spec store tests, and the menu controller's tests.
- **Regression net.** The existing categories and panel tests pass unchanged.

## Out of Scope

- Changing how a mod's category is decided, or the shipped table.
- The tree's drawing.

## Further Notes

- Last in the suggested order.
- The glossary may gain "mod list".
