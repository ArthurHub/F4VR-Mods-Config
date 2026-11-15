# 03. A mod list module that owns the mods and their categories

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam B, behind the panel.

## Problem Statement

The mods and their categories are split between the menu and the panel.

- **Two copies of the category state.** The menu holds the category table, the player's choices and a "categories saved" flag. The panel holds the table again, the minimum number of mods to group from, the choices and the groups.
- **Manual wiring in the menu.** The menu refills the rows of its own Categories page on each of three reload paths. It reads the offered choices from the concrete config spec store, and detects a Save of the categories by matching a file id.
- **The panel's share.** It owns the mods' order, remapping the selection, the report and a pending leave when a mod is replaced, and the live copy read status, all in one long `replaceMod`.

The Categories page touched 21 files, and grouping the tree touched the config, the menu, the mod, the panel and the tree.

## Solution

One game-free mod list module, owned by the panel, holds the mods, their order, the category state, the groups, the live copy reads, and refills the Categories page. The menu reads and drives it through the panel and keeps no copy of its own.

## User Stories

1. As the maintainer, I want the mods, their order and replacing one owned by one module, so that a reload cannot leave the mods, their groups and the Categories page out of step.
2. As the maintainer, I want the category table, the player's category choices and the grouping of the tree held once, so that the menu and the panel never hold two copies that can drift.
3. As the maintainer, I want the rows of the menu's own Categories page refilled by the module that owns the mod list, so that no reload path can forget to refill them.
4. As a player, I want the tree regrouped as soon as I save a new category for a mod, so that the menu shows my choice, as it does today.
5. As the maintainer, I want any Save the menu's own client hears to read the categories again, not one matched by a file id, so that changing the own config spec's files cannot silently break regrouping.
6. As the maintainer, I want the categories offered on the Categories page to include pending choices read through the read part of the Store interface, so that the mod list does not reach into a concrete store.
7. As the maintainer, I want "reload, then rows refilled and tree regrouped" tested through the panel, so that this path is covered by a unit test.
8. As a player, I want the tree's grouping, category rows and their open state to look as today, so that the change is invisible to me.

## Implementation Decisions

- **The mod list** is `src/ui/ModList.{h,cpp}`, beside the store session: a private part of the panel, game-free and drawing nothing. It is in `ui/` as the live copy reads it holds are the report's type, with the ray's state. The menu reaches it through `FullPanel::modList()`.
- **It owns:**
  - the mods and their order of display names;
  - the category table, the player's category choices and the minimum number of mods to group from;
  - the groups by category, made again as any of these change;
  - the last read of each config spec's live copy, and whether its installed spec.json is newer;
  - the menu's own mod, and refilling its Categories page. The menu tells it the name once, with `setOwnMod(Version::PROJECT)` at startup; the own mod is the config spec of that name, matched without case.
- **Refilling the Categories page.** Setting the mods, setting the category table, and replacing the own mod each refill the rows of the own mod's Categories page, so no reload path refills them. Replacing any other mod does not: the store session holds the selected mod, and rebuilding the own mod's page while its session is open would pull its rows from under the mod page and the search. The table is set once at startup, before a mod is selected, which the setter's doc comment states, as it rebuilds the page too.
- **Replacing a mod** is split by whose state it is. The panel's `replaceMod` returns at once for a folder the mod list does not list, so no session ends for a mod that is not put in. Otherwise it counts the selected mod's dropped changes, ends the store session, and calls `std::optional<std::vector<std::size_t>> replace(model::Mod, std::size_t dropped, std::string_view time)`. The mod list puts the mod in, sorts again, regroups, refills the Categories page if it is the own mod, records the "Reloaded at" read with the dropped count, and returns the new place of each old place, none for a folder it does not list. The panel remaps its selection, report, Back's report and pending leave from those places, and starts the session again.
- **Live copy reads.** A live copy that is not read, a stale installed spec.json, and the last read of a folder go to the mod list directly: `modList().liveCopyNotRead`, `setLiveCopyStale` and `liveCopyRead`, with no pass-throughs on the panel. The menu, devbench and the tests call them there.
- **The panel's interface.** `setMods` and `replaceMod` stay, as they do panel work too. The category setters leave the panel: callers set the table, the choices and the minimum on the mod list. `FullPanel::mods()` and `ConfigMenu::mods()` stay, as reads of the mod list, so devbench, the quick launch list, read again and the Developer options do not change. `ConfigMenu::setMods` keeps its table parameter and passes it on, so the mod does not change.
- **Access to the mods.** The mod list gives `const std::vector<model::Mod>& mods() const` for reads. Only the panel, its friend, gets `model::Mod& mod(std::size_t place)` for the store session and the vector itself for the search, which reads the game's texts into the mods. Only `setMods` and `replace` add, remove or order mods, so the order and the groups cannot drift from the mods.
- **Offered categories.** The mod list gives the options of a mod's category row, from the mods, the table, the choices and the pending choices of the own mod, read through a `model::StoreReader&` the caller passes: the menu passes its config spec store. The mod list knows no store. The menu registers them on its own native API client as today: the registration stays out of the mod list.
- **Detecting a Save.** The menu keeps its own client's Save subscription. A client hears only the Saves of its own config spec, so any Save it hears sets the flag, and the frame update reads the choices again. A Save of another page of the own mod reads them too, which is one small file and a regroup. The file's path stays in the menu, as only the menu reads the file and the path is built on the mod's config folder. This wiring is in the menu until [04](04-menu-controller.md) moves it into the controller.
- **No copies left.** The menu drops its category table and its choices, and keeps only the flag its Save subscription sets for the frame update. The panel drops its own category state and live copy reads.
- **What stays.** The category functions in the model stay as they are, as the mod list's implementation. The tree's own open state of category rows stays in the tree. The native API, the config spec schema, `Categories.toml` and the devbench endpoints do not change.

## Testing Decisions

- **Seam.** Tests go through the panel's interface and its `modList()`, with the existing test panel and store doubles. Pending choices use a `StoreReader` double that answers `isDirty` and `value`: the store's own tests cover its pending changes.
- **They cover:**
  - grouping from the minimum number of mods;
  - the Categories page refilled after setting the mods, replacing the own mod, and setting the table, and not after replacing another mod;
  - a replaced mod keeping the selection, the report and a pending leave;
  - the live copy reads after a replace and a failed read;
  - pending choices offered on the Categories page.
- **Not here.** "A Save of the own mod regroups the tree" needs the menu, which links the game: [04](04-menu-controller.md) tests it through the controller.
- **Prior art:** the existing categories tests, the config spec store tests, and the panel's tests.
- **Regression net.** The existing categories and panel tests pass unchanged, except the calls that move to `modList()`: the category setters and the live copy reads.

## Out of Scope

- Changing how a mod's category is decided, or the shipped table.
- The tree's drawing.
- Moving the Save wiring out of the menu, which is [04](04-menu-controller.md).

## Further Notes

- Before [04](04-menu-controller.md) in the suggested order, so the menu that 04 splits is smaller.
- The glossary gains "mod list": the mods the menu lists, in order, with their categories and live copy reads, owned by the panel.
