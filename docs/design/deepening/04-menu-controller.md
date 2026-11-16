# 04. A game-free menu controller behind a port

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam C, the only new seam.

## Problem Statement

The menu decides a lot of game-free things in code that links the game, so no unit test reaches them. It is built of the game's UI manager, the framework's VR input, the shell, FRIK and GPU textures, and is left out of the unit test build. Today these rules are checked only by e2e tests run by hand and in the headset.

- **Requests.** Nine request flags and optionals are combined in its frame update:
  - an open or restore request from a native client cancels Adjust in world;
  - read again applies only if the shown mod's name matches, in any case;
  - opening FRIK takes precedence over close, which takes precedence over minimize.
- **Live copy reloads.** Reloading a config spec from its live copy, re-reading an installed one, and checking for changed copies are game-free logic.
- **The rest.** The quick launch list, and the Developer and Help-opened writes, are game-free logic too.

## Solution

The menu splits into a game-free menu controller and the game side. The controller makes the decisions. The game side sits behind one small port, with a game adapter in the mod and a recording adapter in the tests.

## User Stories

1. As the maintainer, I want the menu's handling of the native API's requests in a game-free controller, so that its rules are unit tested.
2. As the maintainer, I want the precedence of close requests (open FRIK before close before minimize) tested, so that a change to one request cannot break another.
3. As the maintainer, I want "an open or restore request cancels Adjust in world, restoring the start value" tested, so that a client's request never leaves a transform half tuned.
4. As the maintainer, I want "read again applies only to the mod that is shown, by name in any case" tested, so that a client's read-again request for another mod does nothing.
5. As a mod author, I want my live copy's save to reload my config spec as it does today, so that editing spec.json in the game keeps working.
6. As the maintainer, I want the live copy reload pipeline tested through the menu controller, so that "a saved live copy is read again, replaced in the list, its report updated and its unsaved changes dropped" is covered by a unit test.
7. As the maintainer, I want the quick launch list built in the controller, so that which entries the front page offers is tested without the game.
8. As the maintainer, I want the game side of the menu (the panel in the world, the wrist buttons, the controllers, textures, the shell, FRIK) behind one port, so that the controller's tests use a recording adapter and the mod uses the game adapter.
9. As an AI agent changing how the menu reacts to a request, I want the rule in game-free code with tests next to it, so that I can verify my change without the headset.
10. As a player, I want opening, minimizing, closing, the wrist buttons, haptic pulses and Adjust in world to behave as today, so that the change is invisible to me.
11. As a native API client mod author, I want my requests to open, minimize, restore and read again to behave as today, so that my mod needs no change.

## Implementation Decisions

- **Shape.** `ModsConfigFO4` holds the controller, `menu::MenuController` in `src/menu/`, and the game adapter, `GameMenu` in `src/GameMenu.*`, which replaces `ConfigMenu`. `g_modsConfig.menu()` returns the controller, so the native API and devbench keep their calls.
- **The controller holds:**
  - the request queue and its precedence;
  - the native API's requests (open, minimize, restore, read again);
  - the live copy actions and reloads;
  - the quick launch list;
  - the Developer and Help-opened writes to the config;
  - the panel's and stores' wiring.

  It owns the stores, the native clients and the panel.
- **Setup.** The controller's constructor takes a `MenuSetup`: MCM's folder, the Data and My Games roots, the config specs' folder, the live copies' folder, the categories file and the plugin checks. `GameMenu::setup()` gives the game's, and a test gives its own folders.
- **The port** is `menu::MenuPort`, an abstract class, kept as small as the controller's real needs. It covers:
  - the panel in the world: open, close, shown or hidden, recentred, its look from the config;
  - the wrist buttons, shown or hidden;
  - Back on the pointing hand, the thumbstick's side push, handedness, whether a game menu takes input;
  - a haptic pattern on the pointing hand, which is also the config's pulse;
  - Adjust in world: start, active, a frame of it, cancel, stop;
  - the panel's image drawer, and dropping the textures of replaced pixels;
  - opening a file, and showing it in Explorer;
  - FRIK: installed, its config open, opening it;
  - the Developer and Help-opened writes of the config;
  - the clock, one `now()` on the system clock, which times the live copy checks and gives a read's time of day.
- **Callbacks.** The controller hands its draw, Restore and Exit handlers to the port as it opens the panel. The game side never holds the controller.
- **Two adapters.** The game adapter is in the mod. The recording adapter, `RecordingMenuPort` in `tests/linked/TestMenuPort.h`, records each call as text, in order, and answers each question from a field the test sets.
- **The config.** The controller reads `g_config` directly, and `Config.cpp` is in the unit test build. Its two writes go through the port, so no test writes the player's config file.
- **Adjust in world.** `AdjustInWorld` stays on the game side, owned by the adapter. The controller keeps its rules: it starts only while the menu is open and no game menu takes input, a game menu cancels it, its end shows the panel again with the transform's editor, and no live copy is read while it lasts.
- **Precedence as it is.** Opening FRIK's config goes before a close asked in the same frame, which is not dropped: the menu closes in the next frame. A close goes before a minimize. The tests pin this.
- **What stays in the mod.** The opening gesture, the recenter binding and closing the menu as a loading screen or the main menu opens stay in `ModsConfigFO4`. They are game input that calls the controller.
- **Build.** The controller is added to the unit test build. The game adapter stays out, like the game access sources today.
- **The mod list and categories** are the panel's, from [03](03-mod-list.md). The controller carries the menu's Save subscription for them: after a Save of its own mod it reads the player's choices again into the mod list, which regroups the tree.

## Testing Decisions

- **Seam.** Tests in `tests/linked/menu/` drive the controller with the recording adapter of the game port, the real stores over a per-test folder, and the existing doubles for the game's globals, properties, INI settings, events and actions.
- **They cover:**
  - request precedence;
  - an open or restore request cancelling Adjust in world, and a game menu cancelling it;
  - read again only for the shown mod;
  - a client's open request by name and page id in any case, and its refusal for a mod or page the menu does not have;
  - minimize ignored while closed, and Restore only while minimized, reading the values again;
  - Back stepping back with the pulse first, and closing from the front page;
  - Exit with unsaved changes showing the minimized panel for the dialog, which refuses a client's pending value;
  - a saved live copy being reloaded and replaced, with its unsaved changes dropped;
  - Overwrite, Open and Show on a live copy;
  - the quick launch list;
  - the Developer and Help-opened writes;
  - the Developer config spec options;
  - a Save of the menu's own mod regrouping the tree.
- **No drawing.** The controller's tests don't draw the panel. The draw handler's feedback (the pattern a press played, or else the pulse) is covered by the panel's own tests of `hapticPlayed` and `pressActed`, devbench and the headset. A press on Adjust in world needs a drawn panel, so its start is covered the same way. A press on a live copy's row is driven through the panel's `pressLiveCopy`, as `openReport` drives its report.
- **Prior art:** the config spec store tests (per-test folders and files), the native clients tests (recording callbacks), and the test file helpers.
- **e2e.** The e2e tests through devbench are run by hand, and the menu is tried in the headset before this is called done.

## Out of Scope

- The mod list and categories themselves, which are [03](03-mod-list.md).
- Changing the menu's behaviour, the native API or the wrist buttons.
- Unit testing the game adapter.

## Further Notes

- Last in the suggested order, and the largest change.
- devbench reaches the menu through `g_modsConfig.menu()`, which is the controller, with the same calls.
- The glossary may gain "menu controller".
