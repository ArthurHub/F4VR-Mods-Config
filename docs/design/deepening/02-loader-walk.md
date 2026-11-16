# 02. The config spec loader reads every page before any condition

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam C.

## Problem Statement

The loader reads a config spec's pages and rows in three passes, as conditions and ids need every setting read first:

- `loadItems` / `loadPage` read the rows into items;
- `loadConditions` / `loadPageConditions` give them their conditions and warnings, and read option sets;
- `loadItemIds` / `loadPageItemIds` give sections and buttons their ids.

`loadConfigSpec` runs the conditions of the front page before it reads the pages. A front-page `visibleWhen`, `enabledWhen` or option `sets` that names a setting on a page is reported as naming no setting, and left out.

## Solution

`loadConfigSpec` reads the front page and every page, then runs the conditions of the front page and the pages, then the ids. A front-page condition or option set can name a setting on any page.

## User Stories

1. As a mod author, I want a condition or an option's `sets` on the front page to name a setting on any page.
2. As a mod author, I want every other error and warning of my config spec reported exactly as today.

## Implementation Decisions

- **Only the order changes.** The three passes and their recursions stay as they are; `loadConfigSpec` moves the front page's `loadConditions` after the pages' `loadPage`.
- **Problem order holds.** Every problem is sorted by its path (`isBefore`), so the front page's condition problems keep their place among the pages'.
- **The `spheres` comment** names the three readings of the rows.

## Testing Decisions

- **Regression net.** The loader's 91 tests pass unchanged, including every assertion on a problem's place and order.
- **New test.** A front-page `visibleWhen` and a front-page option's `sets` that name a setting on a page are read, with no problem. Written first, and seen to fail before the fix.

## Out of Scope

- One walk of the pages and rows shared by the passes. It pays when a fourth pass over the loaded items is needed; until then each pass keeps its own recursion.
- Splitting the loader by schema area.
- Changing anything else the loader reads or reports.
