# 07. One row view for the panel and devbench, and Back owned by the mod page

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam B.

## Problem Statement

Two things about a page's rows are owned in the wrong place.

- **Back.** The mod page has seven one-line cancel and close pass-throughs, one per open editor or list. The panel chains them in the right order in its Back. Each new editor adds a member to the row context, a cancel method on the mod page, and a term in the panel's Back.
- **How a row looks.** Whether a row is enabled, whether its warning shows (hidden while invalid), and whether its tag shows (only while dirty) is computed in the setting row's state, and again in devbench's report of a page's rows. devbench is not unit tested, so the two can drift unnoticed, and an e2e test that checks a row's state may check something the player doesn't see.

## Solution

The mod page owns its own Back step. One game-free row view is computed from the setting and its store: the setting row draws it, and devbench reports it.

## User Stories

1. As the maintainer, I want the mod page to own its Back step (cancel a confirmation, cancel a drag, close a list, cancel an editor, in its own order), so that the panel calls one Back and a new editor adds no term to the panel.
2. As the maintainer, I want how a row looks (enabled, the warning hidden while invalid, the tag shown only while dirty) computed in one game-free row view, so that the panel and devbench cannot disagree.
3. As an e2e test author, I want devbench to report the row view the panel draws, so that an e2e test that checks a row's state checks what the player sees.
4. As an AI agent adding an editor, I want Back handled inside the mod page, so that I don't have to edit the panel.
5. As a player, I want Back to close an open list or editor before leaving the page, as it does today, so that the change is invisible to me.

## Implementation Decisions

- **Back.** The mod page has one Back step, which says whether it used Back. Its order is today's: a button's confirmation, a slider's drag, a dropdown's list, a text input's edit, then the binding, color and transform editors. The panel calls it once, between ending the search field's edit and leaving Help. The seven cancel and close pass-throughs on the mod page go.
- **Row view.** How a setting row looks (enabled, value look, warning shown, tag shown, the reasons) is one game-free row view, computed from the setting and its store.
- **Who uses the row view.** The setting row draws from it. devbench reports it, and its own copy of these rules goes.
- **Unchanged.** The row's drawing and controls.

## Testing Decisions

- **Seam.** Back is tested through the panel's interface, with the existing test panel. The row view is a pure function of the setting and its store, so it is tested at its own interface: the panel has no query of its rows as drawn, and both callers read the one function.
- **Back:** the existing panel test of each kind of open editor or list, then Back, is the regression net: each asserts that the editor closed, the value was restored or kept as today, and the page stayed.
- **Row view:** enabled, editable, look, warning, mark and tag for valid, invalid, dirty, unavailable, condition-disabled and read-only rows, through `rowViewOf`.
- **Prior art:** the existing panel tests.
- **e2e:** devbench's rows report keeps its existing e2e checks, run by hand.

## Out of Scope

- Changing what Back does in any state.
- Changing how a row is drawn.
- Unit testing devbench itself.

## Further Notes

- Fifth in the suggested order. It does not depend on another change.
- The glossary may gain "row view".
