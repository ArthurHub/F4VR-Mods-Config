# 01. Devbench reports and presses from the panel's views

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam B.

## Problem Statement

Devbench is how the e2e tests see the menu, so what it reports should be what the player sees. The setting row's look already comes from one view (`rowViewOf`) that both draw from. Three other panel rules are still decided twice, once in the panel and once in devbench:

- **A button's press.** The panel saves first only when the action reads saved values, the store keeps changes for a Save, and there are unsaved changes ([Buttons.cpp](../../../src/ui/Buttons.cpp) `asksToSaveFirst`). Devbench's `runButton` leaves out the store check. So for an MCM mod's script button with changes, devbench calls `McmStore::save()` (Keep, a Saved event, the pause-menu wait) and the panel runs the script at once, as the FullPanel test "a script button of an MCM mod runs at once" pins. Devbench's `runnable` and `enabled` of a button are also worked out apart from the panel's.
- **A setting's Reset.** The setting row shows Reset only when its control can show the default: not for a slider with no range, nor a default that is none of the options ([SettingRow.cpp](../../../src/ui/SettingRow.cpp) `defaultOf`). Devbench's `runReset` and its `default` field use the store's default as it is.
- **The breadcrumbs.** Devbench's `runBreadcrumb` works out the number of crumbs, which are links and where each goes. The title bar has its own version, with a Developer crumb over a report that devbench cannot press.

Devbench has no unit tests, and 35 of its 47 commits also touched `src/ui/`.

## Solution

The panel's views cover these rules too, and devbench reports and presses from them:

- a **button view**: whether the button runs, why not, whether it is enabled, and whether a press asks to save first;
- the **row view** gains the Reset value (the default, when the control can show it) and whether Reset is pressed now;
- a **breadcrumbs** list: each crumb's text, its look (link, current, the menu's selected) and where a press goes.

The button row, the setting row and the title bar draw from them. Devbench is left with turning them into JSON and reading its arguments.

## User Stories

1. As the maintainer, I want devbench to run a button as the panel does, so that an e2e test of an MCM script button checks what the player gets.
2. As the maintainer, I want one place that decides whether a button runs and saves first, so that a new action kind changes one function.
3. As the maintainer, I want devbench's Reset to exist exactly where the row's does, so that an e2e test can't reset a value the player can't.
4. As the maintainer, I want devbench to press every breadcrumb the title bar draws, the Developer crumb over a report too.
5. As the maintainer, I want the button rules tested without pressing pixels.

## Implementation Decisions

- **Where the views live.** Beside the row view in `src/ui/`, game-free and pure: `ButtonView.h` and `Breadcrumbs.h`. They take the item and its store (`Store`, as the button and Reset ask what only a store knows) or, for the crumbs, the mods, the selection and what is shown over the page.
- **Button view.** `buttonViewOf` holds `enabled` (its enabledWhen), the `reason` it is not run (an unknown action's, the store's `notRunnable`), `runs`, and `asksToSaveFirst`. Devbench's button row reports them as `enabled`, `notRunnable`, `runnable` and `savesFirst`, and `runButton` saves first exactly when the view says so; `withoutSaving` stays as Run without saving.
- **Reset.** `rowViewOf` takes the `Store` and gains `resetValue` (what `defaultOf` gave) and `resets`: Reset is pressed now, as the row is editable, not dirty (its slot is Discard then) and not at its default. Devbench keeps `default` as the store's default and adds `reset` and `resets`; `runReset` fails unless `resets`, and says why.
- **Breadcrumbs.** `breadcrumbsOf` lists every crumb with its text, look, target (a selection, or the Developer section) and whether the last says the page is filtered. The title bar keeps the layout: widths, separators, and the ellipsis of the pages that do not fit, which it works out from the widths. `FullPanel::breadcrumbs` gives the list and `FullPanel::pressCrumb` goes to a target, for the title bar and devbench alike. Devbench presses a crumb by its place in the list, a crumb the ellipsis stands for too, and gains `openReport` to show a report whose Developer crumb a test presses.
- **Devbench's row walks.** `addRows` and `findButton` read `shown` from the panel's `isShown`, in `PageRows.h`; they keep their own walk.

## Testing Decisions

- **New tests at the views' interfaces:** a button view test file (runs, reasons, enabled, save first for each action kind and a store that writes at once or keeps changes), Reset cases in the row view tests, and a breadcrumbs test file (the front page, a page, a nested page, a filtered page, Help and a report).
- **Moved tests.** FullPanel tests that only check a button's or crumb's rule through pixels move to the view tests: "says why a button is not run", "a script button of an MCM mod runs at once", "a page under a page has a breadcrumb for each page". Tests of the press, its dialog and the title bar's layout stay in the panel: "a breadcrumb before the last is pressed", the ellipsis, and "Save and run disabled while invalid", which is the dialog's. A panel test presses the Developer crumb.
- **e2e.** The button and breadcrumb e2e tests are run by hand; one is added for the Developer crumb, and the Reset test presses Reset only where the row has it: after a Save, not on a pending change.

## Out of Scope

- Devbench's own reading of a value from JSON (`valueFromJson`), which takes a table as its TOML text or INI line, as the e2e tests send it.
- The title bar's layout and look.
