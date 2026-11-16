# 02. One walk of pages and rows in the config spec loader

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam C.

## Problem Statement

The loader reads a config spec's pages and rows in three passes, as conditions and ids need every setting read first:

- `loadItems` / `loadPage` read the rows into items;
- `loadConditions` / `loadPageConditions` give them their conditions and warnings, and read option sets;
- `loadItemIds` / `loadPageItemIds` give sections and buttons their ids.

Each pass writes its own recursion over pages and `content`, and `loadConfigSpec` writes the front page and pages loop for each. Every walk repeats the same steps: `readRow` (an activation sphere is the section made of it), `intoRow` (a made row's own path), the row's name and its place for the problem places. They share an unstated rule: the items are the rows of the content, one for each, in order. The comment on `spheres` still says "both readings", but there are three.

Work that needs the whole spec goes into whichever walk is handy: option sets are read in `loadConditions`. The loader is the most changed source file (3575 lines), and a new pass, or a fix to problem places, has to be made in each walk.

## Solution

One walk inside the loader recurses the front page, the pages and their content once per pass: it resolves made sections, keeps the reading's place and row name, and calls the pass at each row with the row's JSON and its item. The three passes become what they do at a row.

## User Stories

1. As the maintainer, I want one walk over a config spec's rows, so that a new pass that needs every setting read is one function, not a fourth recursion.
2. As the maintainer, I want problem places set in one place, so that a problem on a made sphere row or a nested page is placed the same in every pass.
3. As a mod author, I want my config spec's errors and warnings reported exactly as today.

## Implementation Decisions

- **Kept inside the loader.** The walk is private to `ConfigSpecLoader.cpp`, not a new file or a public interface.
- **The passes stay three.** Conditions and ids still run after every setting is read; the walk only removes the copied recursion.
- **Option sets.** Whether option sets get a pass of their own or stay in the conditions pass is settled when the walk is written; the order of their problems must not change.
- **Made sections first (optional).** Whether `cf.activationSphere` rows are turned into their sections before any pass, so that no pass calls `readRow`, is decided as the walk is written.

## Testing Decisions

- **Regression net.** The loader's 91 tests pass unchanged, including every assertion on a problem's place and order.
- **New test.** One test of problem places across a sphere's made rows and a nested page, in every pass.

## Out of Scope

- Splitting the loader by schema area.
- Changing what the loader reads or reports.
