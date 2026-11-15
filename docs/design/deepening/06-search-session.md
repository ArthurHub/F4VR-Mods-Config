# 06. A search session module behind the panel

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam B.

## Problem Statement

The panel keeps nine members of search state among its forty-odd: the query, its words, the results per mod, the match count, the mod searched with, the stale flag, whether game texts were read, the whole-page flag and the row pointed while filtered.

- When results go stale is spread out. Replacing a mod sets a flag. The selected mod is searched again because its conditions ask the store. MCM rows' game texts are read once by reaching for the MCM store by name.
- The text that says a match is hidden by a condition runs a search of its own.
- The filtered/whole state interacts with Back.

The search function itself is pure and well tested. The bugs would be in when results go stale, and only pixel-driven panel tests reach that.

## Solution

A search session module, `SearchSession` in `src/ui/`, private to the panel, owns the query, the results, when they go stale, and whether the page is filtered or whole. The panel draws what it returns.

## User Stories

1. As the maintainer, I want the search query, its results, when they go stale and whether the page is filtered owned by one search session module behind the panel, so that the panel's state shrinks by nine members.
2. As the maintainer, I want the search session to read the game texts of the mods' rows through each mod's own store, so that the panel does not reach for one specific store.
3. As the maintainer, I want "the mods are searched again after a config spec is read again" tested through the panel's interface, so that a stale search result after a live copy reload fails a test.
4. As the maintainer, I want "the selected mod is searched again because its conditions ask the store" tested, so that a row hidden by a condition is found or not found correctly.
5. As a player, I want Back while searching to show the whole page and keep the row I pointed at, as it does today, so that the change doesn't alter how search feels.
6. As a player, I want typing, clearing and Back on the search field to behave as today, so that the change is invisible to me.

## Implementation Decisions

- **What goes stale.**
  - A new mod list resets the panel, which resets the session: no query, nothing found.
  - A replaced mod (`modReplaced`) searches every mod again.
  - So does a change of selected mod, as the mod selected before was searched with its store.
  - The selected mod is searched in every update with its store, as its conditions ask it. Every other mod is searched with none, so its conditions hold.
- **Game texts.** As the first search after a reset starts, every available mod's store gets `readTexts`, through a `storeOf` the panel gives. There is no MCM check: a store whose rows take no texts does nothing, as the store session already relies on.
- **Filtered or whole.**
  - The session's `filtered()` is "the query has words and the page is not whole". The panel's `isFiltered` adds that neither Help nor a report is shown.
  - Back calls `showWhole`, which returns the row pointed while filtered. The panel focuses it, with its sections opened, or shows the page from its top.
  - Another page shown (`pageShown`) is filtered again and forgets the pointed row. So does a changed query.
- **The interface.**
  - `update(text, mods, selectedMod, storeOf)` runs before the panel draws, and says whether the query changed. The panel then scrolls the tree to its top, and shows the page whole or filtered again.
  - The session never touches the tree or the page. The panel builds the tree's and the page's search structs from what it exposes: the typed text, the query, what it found per mod, the match count, `filtered()`, `showsRows` and `hiddenByConditions`.
  - `hiddenByConditions` replaces the search of its own in the panel's text.
  - The panel exposes the session read-only as `search()`, as it does `helpLine()`.
- **What stays.**
  - The pure search function, its folding rules and match ranges stay as they are.
  - The search field's own copy of the query (the Escape trap) stays in the search field.
  - Whether the ray is on the search field stays in the panel, as it comes from the tree's draw.

## Testing Decisions

- **Seam.** Tests go through the panel's interface, in `SearchSessionTests.cpp`, with the existing test panel. They type with the search helper and read through `FullPanel::search()`. A recording store counts `readTexts` and answers the value a condition reads.
- **New tests cover:**
  - a new mod list empties the query and what it found;
  - results after a config spec is read again;
  - a row hidden by a condition, found while its mod is not selected and not while it is, and `hiddenByConditions` for its page;
  - game texts read once per reset, of every available mod's store;
  - Back showing the whole page with the query kept, until another page is shown or the query changes.
- **Prior art:** the existing search and panel tests. The frame, press, point and search helpers move into `TestPanel.h`, and the store session tests' recording store into `TestStore.h`, so both test files share them.
- **e2e.** The mod's devbench tool gains a `search` action, which types a query as the keyboard does, and reports the search in its state, the mods and the page. `tests/e2e/test_search.py` drives it on the showcases.
- **Regression net.** The existing search and panel tests pass unchanged. "The page whole again has the focus ring on the last row the ray was on" covers the pointed row Back keeps.

## Out of Scope

- Changing what search matches or how it ranks.
- Searching only the two mods a selection change affects: every mod is searched again, as today.
- The search field's drawing and keyboard.
