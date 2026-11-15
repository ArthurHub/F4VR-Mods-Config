# 05. One walk for a page's pending marks

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam A.

## Problem Statement

Two walks over a page's rows count its pending changes:

- The section headings count a folded section's invalid and unsaved rows with their own recursive walk, in the panel.
- The tree's unsaved marks count with a near-identical walk in the unsaved pages module.
- Both carry the same filter: a setting whose row is shown, and not a choice whose options set other settings.

The headings' walk is reached only through panel and render tests.

## Solution

The headings' walk moves into the unsaved pages module beside the tree's, as `foldedMarks`, and both share one filter. They stay two counts, as they count different things.

## User Stories

1. As the maintainer, I want the rows a count asks of decided in one place, so that the section headings and the tree's unsaved marks skip the same rows.
2. As the maintainer, I want the headings' counts tested in the model, so that they are not reached only through the panel.
3. As a player, I want the headings' counts and the tree's unsaved marks to show what they show today, so that the change is invisible to me.

## Implementation Decisions

- **Two counts, one filter.** The tree counts unsaved changes: dirty and available, an invalid change included. A folded heading counts its rows' marks: invalid, saved or not, or else unsaved. The two numbers differ on purpose, so they stay two functions over one filter: a setting, not a choice whose options set other settings, whose condition holds.
- **What each reads.** The tree counts the selected mod's pages every frame, and an MCM store reads the game for a value or a condition. So the tree asks whether a setting is dirty before its condition and its value. The headings read the value of each shown row, as they count only a folded section on screen.
- **Callers.** The section headings call `foldedMarks`. Their own walk is deleted. The tree keeps calling `unsavedChanges`.
- **What stays.** The option set helpers, and how the config spec store derives an option-set choice's value, dirty state, default and Discard.

## Testing Decisions

- **Seam.** Tests go through `foldedMarks` and `unsavedChanges` in the model: nested sections, hidden and unavailable rows, invalid rows saved or not, and a choice that sets other settings.
- **Prior art:** the existing unsaved pages tests.
- **Regression net.** The existing panel and render tests pass unchanged.

## Out of Scope

- Changing what counts as pending or invalid.
- The option-set rules inside the config spec store.
