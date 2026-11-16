# 06. Where the panel is, and its Back order

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam B.

## Problem Statement

[FullPanel.cpp](../../../src/ui/FullPanel.cpp) is the most changed UI file. Where the panel is lives in five fields: `_selection`, `_helpShown`, `_report`, `_returnToReport` and `_leaving`. Eight methods move them: `select`, `back`, `showHelp`, `openReport`, `leaveReport`, `showProblem`, `leave` and `close`. Each new place the panel can be (Help, a report, a problem's row) adds a field or a case to several of them.

## Solution

A navigation module private to the panel owns where the panel is and the Back order: a press, Back or a selection gives the next place. The panel does what showing a place needs: the store session, the search session, the page's reset, the unsaved-changes dialog.

## User Stories

1. As the maintainer, I want where the panel is and what Back does in one module, so that a new place is one case.

## Implementation Decisions

- **Private to the panel**, as the store and search sessions are, and tested through the panel.
- **Speculative.** The panel's queries already let tests skip pixels for these paths, so the gain is locality only. Before writing it, check whether the side effects left in the panel make the module shallow; if they do, close this change as not worth it.

## Testing Decisions

- **Regression net.** The panel's selection, Help, report and Back tests pass unchanged.
- **New tests** through the panel only where a Back order isn't covered today.

## Out of Scope

- The unsaved-changes dialog and the store session, which stay as they are.
