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

- **The controller holds:**
  - the request queue and its precedence;
  - the native API's requests (open, minimize, restore, read again);
  - the live copy actions and reloads;
  - the quick launch list;
  - the Developer and Help-opened writes to the config;
  - the panel's and stores' wiring.

  It owns the stores and the panel.
- **The port** covers the game side:
  - the panel in the world (open, close, minimize, restore);
  - the wrist buttons;
  - controller input and haptic pulses;
  - textures for images;
  - opening files and links in the shell;
  - FRIK;
  - the clock that times live copy checks.

  It is kept as small as the controller's real needs.
- **Two adapters.** The game adapter is in the mod. The recording adapter is in the tests.
- **Adjust in world.** Its controller reading and wrist column stay on the game side. The controller only starts and cancels it through the port.
- **Build.** The controller is added to the unit test build. The game adapter stays out, like the game access sources today.
- **The mod list and categories** move into the controller in [03](03-mod-list.md), which is blocked by this one. Until then the controller carries the category wiring as it is.

## Testing Decisions

- **Seam.** Tests drive the controller with the recording adapter of the game port, the real config spec store over a per-test folder, and the existing doubles for the game's globals, properties, INI settings, events and actions.
- **They cover:**
  - request precedence;
  - an open or restore request cancelling Adjust in world;
  - read again only for the shown mod;
  - a saved live copy being reloaded and replaced, with its unsaved changes dropped;
  - Overwrite, Open and Show on a live copy;
  - the quick launch list;
  - the Developer and Help-opened writes.
- **Prior art:** the config spec store tests (per-test folders and files), the native clients tests (recording callbacks), and the test file helpers.
- **e2e.** The e2e tests through devbench are run by hand, and the menu is tried in the headset before this is called done.

## Out of Scope

- The mod list and categories, which are [03](03-mod-list.md).
- Changing the menu's behaviour, the native API or the wrist buttons.
- Unit testing the game adapter.

## Further Notes

- Seventh in the suggested order, and the largest change.
- devbench reaches into the menu and the panel today. It moves to the controller's interface where that's a plain rename, and otherwise stays as it is.
- The glossary may gain "menu controller".
