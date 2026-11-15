# 02. A store session module that orders the store's lifecycle calls

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam B.

## Problem Statement

The Store interface has 33 virtual methods. Eight of them are lifecycle notifications: open, close, page shown, read texts, show page, show pointed, start frame and read again. The panel calls them in the right order by hand at seven call sites. On top of that, the menu sends its events (opened, closed, frame update while closed) to each concrete store and to the native clients at its own call sites.

One path probably gets the order wrong. When a config spec is reloaded from its live copy, the panel opens the store and reads texts, but never tells it which page is shown. The config spec store sends PageLeft on close and records the page only when it is shown, so after a reload the native client is probably never told PageShown for the page still on screen. This is unconfirmed.

Each new selection path has to remember the whole order, and the only tests that reach it press pixels.

## Solution

A store session module, private to the panel, holds which mod and page are shown and which item is pointed. It tells the store once and in order for each real change. The panel only says what is shown, what is pointed, and that a frame starts. The menu's events reach every store and the native clients from one place.

## User Stories

1. As the maintainer, I want the panel to say only which mod and page are shown and which row is pointed, so that the order in which a store hears about its session is kept in one place.
2. As the maintainer, I want the store told about a session change once for each real transition, so that a store never hears `open` without `showPage`, or `showPointed` repeated for the same row.
3. As a native API client mod author, I want my mod told which page is shown after the menu reloads a config spec from its live copy, so that my mod's preview matches the page on screen.
4. As the maintainer, I want the store session order tested with a recording store through the panel's interface, so that a missing or repeated notification fails a test without pressing pixels.
5. As the maintainer, I want the reload of a config spec covered by that order test, so that the suspected missing `showPage` after a reload is confirmed and, if real, stays fixed.
6. As the maintainer, I want the menu's events (opened, closed, minimized) sent to the stores and to the native clients from one place, so that adding a menu event does not mean editing each store's call site.
7. As an AI agent adding a way to change the selection, I want one module to call, so that I cannot get the store's notification order wrong.
8. As a player, I want selecting, paging, pointing, Back and closing the menu to behave as today, so that the change is invisible to me.

## Implementation Decisions

- **The module.** A store session module, private to the panel, owns the call order of the store's lifecycle notifications: close, open, page shown, read texts, show page, show pointed, start frame.
- **What the panel tells it:** what is shown (mod and page), what is pointed, and that a frame starts. Every path that changes the selection goes through it: select, Back, a reload of a config spec, a new mod list, closing the menu, and the unsaved-changes dialog's answers.
- **On change only.** Show pointed is sent only when the pointed item changes.
- **After a reload.** When a config spec is reloaded from its live copy, the store is told the page that is shown, as after any other selection. If the test shows this is already sent by another path, nothing changes and the test pins the order.
- **Menu events.** Opened, closed and minimized, and the frame update while the menu is closed, are sent to every store and to the native clients from one place, not at each call site.
- **The Store interface itself does not change here.** Narrowing it is [08](08-store-read-interface.md).

## Testing Decisions

- **Seam.** Tests go through the panel's interface, with the existing test panel and a store double that records each call it gets, in order.
- **New tests cover the calls for:**
  - selecting a mod, changing page and Back;
  - pointing at a row, and pointing at the same row again;
  - a frame;
  - closing the menu;
  - a new mod list;
  - the dialog's Save, Discard and Cancel;
  - a reload of a config spec while its mod is shown, which is the suspected missing PageShown.
- **Prior art:** the existing panel tests and test panel setup. The recording double follows the native clients tests' recording callbacks.
- **Regression net.** The existing panel, config spec store and native clients tests pass unchanged.

## Out of Scope

- Splitting or narrowing the Store interface.
- Changing what a store does on each notification.
- Search state, which is [06](06-search-session.md).

## Further Notes

- Second in the suggested order. [08](08-store-read-interface.md) is blocked by this one.
- If the reload test confirms the missing PageShown, say so in the commit message as the bug it fixes.
