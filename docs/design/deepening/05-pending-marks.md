# 05. One walk for a page's pending marks

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam A.

## Problem Statement

The option set helpers in the model are four small pure functions, unit tested. The rule that matters is how a page's pending changes are counted and marked, and that is written out by callers:

- The section headings count hidden pending and invalid rows with their own recursive walk.
- The tree's unsaved marks count with a near-identical walk in the unsaved pages module.
- Both carry the same skip for a choice whose options set other settings.

When the rule changes, as it did for `alsoSets`, both walks must change together. The headings' walk is reached only through render tests.

## Solution

The unsaved pages module grows into the one walk over a page's rows. It decides which rows are visible and which are skipped, and returns the counts and marks. The section headings and the tree ask it, and their own walks go.

## User Stories

1. As the maintainer, I want one walk over a page's rows that decides which pending changes are counted, so that the section headings, the tree's unsaved marks and the change counts agree.
2. As the maintainer, I want the walk to own the rule that a choice whose options set other settings is skipped in the counts, so that the rule is written once.
3. As the maintainer, I want the walk to own which rows are visible, so that a hidden row's pending change is counted the same way in a heading and in the tree.
4. As the maintainer, I want the counting rule tested once in the model, so that it is not reached only through render tests.
5. As a player, I want the headings' counts and the tree's unsaved marks to show what they show today, so that the change is invisible to me.

## Implementation Decisions

- **The walk.** The unsaved pages module in the model becomes the one walk over a page's rows. It decides visibility (conditions, folded or hidden sections), skips a choice whose options set other settings, and returns each section's and page's pending and invalid counts.
- **Callers.** The section headings and the tree's unsaved marks ask it. Their own recursive walks are deleted.
- **What stays in the store.** How the config spec store derives an option-set choice's value, dirty state, default and Discard from the settings it sets. The four option set helpers stay as they are.
- **Its store dependency.** The walk asks the store only for what it reads. Once [08](08-store-read-interface.md) lands, it takes the read part.

## Testing Decisions

- **Seam.** Tests go through the walk's interface in the model: pages with nested sections, hidden rows, invalid rows, and an option-set choice with its set settings and `alsoSets`.
- **Prior art:** the existing unsaved pages and option sets tests.
- **Regression net.** The existing panel and render tests pass unchanged. The headings' counts are now checked by the walk's tests, not only by renders.

## Out of Scope

- Changing what counts as pending or invalid.
- The option-set rules inside the config spec store.

## Further Notes

- Third in the suggested order. It does not depend on another change.
