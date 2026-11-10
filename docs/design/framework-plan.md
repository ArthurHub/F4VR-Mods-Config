# Framework work plan

The framework commits (F) that delivered [CommonFramework work required](framework-requirements.md), with the mod commits (M) that used them as the test bed. All done. Per-commit notes are in each framework subsystem's README and the framework's `0.5.0` changelog; the narrative this plan had is at `git show 6a0be38:docs/design/framework-plan.md`.

## Commits

### The test bed

| #   | Repo | Commit                                                                                   | Status |
| --- | ---- | ---------------------------------------------------------------------------------------- | ------ |
| M1  | mod  | Open the menu with the gesture taken from FRIK: one ImGui panel and the wrist buttons    | done   |

### Version 0.5.0

| #   | Repo      | Commit                                                                      | Status |
| --- | --------- | --------------------------------------------------------------------------- | ------ |
| F0  | framework | Version 0.5.0, the deprecated `PlayerNodes` and `getPlayerNodes()` removed  | done   |

### Hand-anchored UI (item 1)

| #   | Repo      | Commit                                                                      | Status    |
| --- | --------- | --------------------------------------------------------------------------- | --------- |
| F1  | framework | vrui: an element takes a rotation, and its whole tree turns with it         | done      |
| M2  | mod       | The panel tilted back, a wrist button for FRIK's config, the dev layout     | done      |
| F2  | framework | vrui: offhand controller presets, top and right, as on the primary wand     | done      |
| M3  | mod       | The wrist buttons on the offhand top preset                                 | done      |
| F3  | framework | vrui: wrist preset that follows the forearm bone, with controller fallback  | done      |

### Dev layout

Not an item of the requirements: the tool the placements above are tuned with.

| #   | Repo      | Commit                                                                      | Status    |
| --- | --------- | --------------------------------------------------------------------------- | --------- |
| F15 | framework | vrui: the dev layout is turned on by a debug flag and keeps its own file    | done      |
| M4  | mod       | The wrist buttons on the wrist preset, the dev layout by the framework flag | done      |

### Interactive ImGui panel (items 2 to 5)

| #   | Repo      | Commit                                                                                        | Status |
| --- | --------- | --------------------------------------------------------------------------------------------- | ------ |
| F4  | framework | imgui: opt-in pointer input from a wand ray and its trigger, one hand owning the pointer      | done      |
| F16 | framework | config: a color read from the INI                                                             | done      |
| F5  | framework | imgui: default pointer ray and hit mark                                                       | done      |
| M5  | mod       | The panel takes the pointer, with its look in the INI                                         | done      |
| F6  | framework | imgui: the wand that operates a panel is fully suppressed, with the press latch               | done      |
| F7  | framework | imgui: thumbstick scrolling                                                                   | done      |
| M6  | mod       | The menu's wand hidden from the game, the scroll speed in the INI                             | done      |

### Dedicated panel (item 6)

| #   | Repo      | Commit                                                                          | Status |
| --- | --------- | ------------------------------------------------------------------------------- | ------ |
| F8  | framework | imgui: the canvas window's zero padding and border are popped before content    | done      |
| F9  | framework | imgui: dedicated panel with a texture of its own, as ImGui's display            | done      |
| M7  | mod       | The panel is dedicated, with its combo at the bottom edge                       | done      |

### World-locked placement (item 7)

| #   | Repo      | Commit                                                                           | Status |
| --- | --------- | -------------------------------------------------------------------------------- | ------ |
| F17 | framework | vrui: a preset puts a root where the HMD is and leaves it in the world           | done   |
| F18 | framework | imgui: the mark keeps its size on the canvas, and the other hand's ray is drawn  | done   |
| M8  | mod       | The panel stays in the world, recentered by a gesture and when the player leaves | done   |

### Curved panel

Not an item of the requirements. The flat panel is as wide as its distance from the player, so its sides are further away than its middle and turned away from the player, and the text there reads smaller.

| #   | Repo      | Commit                                                              | Status    |
| --- | --------- | ------------------------------------------------------------------- | --------- |
| F19 | framework | imgui: a panel is curved toward the player, around a cylinder       | done      |
| M9  | mod       | The panel is curved around the player                               | done      |

### Moving the panel

Not an item of the requirements. The panel is spawned at one place in front of the player, and the player moves it to where it suits them.

| #   | Repo      | Commit                                                              | Status    |
| --- | --------- | ------------------------------------------------------------------- | --------- |
| F20 | framework | vrui: a root in the world is moved by the hand that points at it    | done      |
| M10 | mod       | The panel is moved with the grip, the button in the INI             | done      |

### Config (items 8 to 10)

These do not need the panel, so they can be moved earlier.

| #   | Repo      | Commit                                                                      | Status    |
| --- | --------- | --------------------------------------------------------------------------- | --------- |
| F10 | framework | config: loads are serialized                                                | done      |
| F11 | framework | config: the mod's own writes are recognized without the ignore flag         | done      |
| F12 | framework | config: several overrides in one reload, cached parsed INI, debug-level log | done      |
| F13 | framework | config: subscribers are notified after an override reload                   | done      |
| F14 | framework | config: each override has an owner                                          | done      |
| F21 | framework | config: the INI value and the set of several overrides are unit tested      | done      |

## Decisions that differ from the requirements

- The ray comes from the wand's UI node; the wand node itself points 59° above where the hand aims.
- Suppression hides the whole wand, per hand by its own ray, and a press outlives its canvas until the trigger is released (else the game sees a trigger already down).
- A dedicated panel is at most 4096 layout pixels a side: the largest D3D11 texture at the largest supersample.
- A world-locked root gets no transform: a preset puts it at the HMD and its own position and rotation place it from there.
- A set override re-parses the kept file text (SimpleIni can't copy a parsed file); no disk read.
- Config subscribers are called on the game thread at the start of the next frame, once for all loads since the last frame.

Items 11 to 13 (images, text entry with SteamVR's keyboard, the `ModBase` client) were made within the [mod's milestones](mod-plan.md).
