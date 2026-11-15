# 06. A search session module behind the panel

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam B.

## Problem Statement

The panel keeps nine members of search state among its forty-odd: the query, its words, the results per mod, the match count, the mod searched with, the stale flag, whether game texts were read, the whole-page flag and the row pointed while filtered.

- When results go stale is spread out. Setting the mods and replacing one set a flag. The selected mod is searched again because its conditions ask the store. MCM rows' game texts are read once by reaching for the MCM store directly.
- The filtered/whole state interacts with Back.

The search function itself is pure and well tested. The bugs would be in when results go stale, and only pixel-driven panel tests reach that.

## Solution

A search session module, private to the panel, owns the query, the results, when they go stale, and whether the page is filtered or whole. The panel draws what it returns.

## User Stories

1. As the maintainer, I want the search query, its results, when they go stale and whether the page is filtered owned by one search session module behind the panel, so that the panel's state shrinks by nine members.
2. As the maintainer, I want the search session to read the game texts of MCM mods' rows through the store it is given, so that the panel does not reach for one specific store.
3. As the maintainer, I want "the mods are searched again after a reload or a new mod list" tested through the panel's interface, so that a stale search result after a live copy reload fails a test.
4. As the maintainer, I want "the selected mod is searched again because its conditions ask the store" tested, so that a row hidden by a condition is found or not found correctly.
5. As a player, I want Back while searching to show the whole page and keep the row I pointed at, as it does today, so that the change doesn't alter how search feels.
6. As a player, I want typing, clearing and Back on the search field to behave as today, so that the change is invisible to me.

## Implementation Decisions

- **The module.** A search session module, private to the panel, owns:
  - the query and its words;
  - the results per mod and the match count;
  - when the results go stale (a new mod list, a replaced mod);
  - searching the selected mod again because its conditions ask the store;
  - reading MCM rows' game texts once as a search starts;
  - the filtered/whole state of the page, and the row pointed while filtered.
- **Store access.** It is given the stores through the Store interface. It does not reach for the MCM store by name.
- **What stays.** The pure search function, its folding rules and match ranges stay as they are. The search field's own copy of the query (the Escape trap) stays in the search field.
- **The panel.** It forwards the query, Back and pointing to the session, and draws the tree and page from what the session returns.

## Testing Decisions

- **Seam.** Tests go through the panel's interface, with the existing test panel and stores.
- **New tests cover:**
  - results after a new mod list;
  - results after a config spec is replaced;
  - the selected mod's condition-hidden rows;
  - MCM game texts read once;
  - Back showing the whole page and keeping the pointed row.

  They assert through the panel's queries, not pixels.
- **Prior art:** the existing search and panel tests.
- **Regression net.** The existing search and panel tests pass unchanged.

## Out of Scope

- Changing what search matches or how it ranks.
- The search field's drawing and keyboard.

## Further Notes

- Fourth in the suggested order. It does not depend on another change, though it is simpler after [02](02-store-session.md).
- The glossary may gain "search session".
