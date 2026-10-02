# Framework work plan

The commits that deliver the work in [CommonFramework work required](framework-requirements.md), in the order they are made. That document says what each item needs and when it is done; this one tracks progress.

## How the work is done

- One commit at a time. Each is reviewed, and usually tried in the game, before it is committed.
- Framework commits are made in the submodule at `external/F4VR-CommonFramework`, on its `0.5.0` branch. Mod commits are made in this repository on `main`.
- New framework work is listed under `0.5.0` in the framework's changelog. The `0.4.1` section stays as it is.
- This mod is the test bed. Its first commit opens one ImGui panel, and every framework commit is tried on that panel. When a framework commit adds something the panel can use, a mod commit moves the submodule and uses it.
- A framework commit also updates its subsystem's README, and adds unit tests where the logic does not depend on the game.
- The devbench bridge (`devbench-fo4vr`) is set up for this repository, so values can be read and config overrides driven in the running game.

Status is one of `todo`, `in review`, or `done`.

## Commits

### The test bed

| #   | Repo | Commit                                                                                   | Status |
| --- | ---- | ---------------------------------------------------------------------------------------- | ------ |
| M1  | mod  | Open the menu with the gesture taken from FRIK: one ImGui panel and the wrist buttons    | done   |

M1 is the first real change in the mod:

- The opening gesture is a binding in `ModsConfigVR.ini`. With FRIK installed, FRIK's `sOpenConfigButton` is overridden through FRIK's API, which needs FRIK's API header in the mod.
- The gesture opens one `UIImGuiPanel` attached to the HMD. Until the real content exists it holds one of each kind of control the menu uses: a button, a checkbox, a slider, a long list, a combo, and a confirmation dialog.
- Two vrui buttons are attached to the offhand controller as the framework places them today, and are shown with the panel. They are the test case for item 1. Close closes the menu. The other one hides and shows the panel (Minimize and Resume), because nothing on the panel can be pressed yet.
- Devbench actions open and close the menu, and its state says whether the menu is open or minimized.

### Version 0.5.0

| #   | Repo      | Commit                                                                      | Status |
| --- | --------- | --------------------------------------------------------------------------- | ------ |
| F0  | framework | Version 0.5.0, the deprecated `PlayerNodes` and `getPlayerNodes()` removed  | done   |

F0 opens the `0.5.0` section of the changelog.

### Hand-anchored UI (item 1)

| #   | Repo      | Commit                                                                      | Status    |
| --- | --------- | --------------------------------------------------------------------------- | --------- |
| F1  | framework | vrui: an element takes a rotation, and its whole tree turns with it         | done      |
| M2  | mod       | The panel tilted back, a wrist button for FRIK's config, the dev layout     | done      |
| F2  | framework | vrui: offhand controller preset, mirrored in left-handed mode               | todo      |
| F3  | framework | vrui: wrist preset that follows the forearm bone, with controller fallback  | todo      |

F1 adds the rotation to the dev layout as `Rot`, so it is tuned live in `[VRUI_DevLayout]`.

M2 moves the submodule to F1 and uses it:

- The panel is larger, stands further away and lower, and is tilted back by 10 degrees.
- With FRIK installed, a third wrist button minimizes the menu and opens FRIK's own config, which lost its gesture to this menu.
- A devbench action, `devlayout`, turns the dev layout on for the open menu.

F2 needs one reading in the game first: which way the offhand's nodes face.

### Interactive ImGui panel (items 2 to 5)

| #   | Repo      | Commit                                                                                        | Status |
| --- | --------- | --------------------------------------------------------------------------------------------- | ------ |
| F4  | framework | imgui: opt-in pointer input from a wand ray and its trigger, one hand owning the pointer      | todo   |
| F5  | framework | imgui: default pointer ray and hit mark                                                       | todo   |
| F6  | framework | imgui: the wand that operates a panel is fully suppressed, with the press latch               | todo   |
| F7  | framework | imgui: thumbstick scrolling                                                                   | todo   |

### Dedicated panel (item 6)

| #   | Repo      | Commit                                                                          | Status |
| --- | --------- | ------------------------------------------------------------------------------- | ------ |
| F8  | framework | imgui: the canvas window's zero padding and border are popped before content    | todo   |
| F9  | framework | imgui: dedicated panel with a texture of its own, as ImGui's display            | todo   |

### Config (items 8 to 10)

These do not need the panel, so they can be moved earlier.

| #   | Repo      | Commit                                                                      | Status |
| --- | --------- | --------------------------------------------------------------------------- | ------ |
| F10 | framework | config: loads are serialized                                                | todo   |
| F11 | framework | config: the mod's own writes are recognized without the ignore flag         | todo   |
| F12 | framework | config: several overrides in one reload, cached parsed INI, debug-level log | todo   |
| F13 | framework | config: subscribers are notified after an override reload                   | todo   |
| F14 | framework | config: each override has an owner                                          | todo   |

F14 reaches FRIK's API only when FRIK is built against a framework that has it.

## Where the plan differs from the requirements

- **Pointer visuals come before suppression** (F5 before F6). Pointing cannot be judged in the headset without a visible ray.
- **Suppression is of the whole wand.** While a wand operates a panel, all of its buttons and axes are hidden from the game, instead of the trigger and the scrolling thumbstick only. F7 then only feeds the thumbstick to ImGui as the wheel.

## Not in this plan

- **Item 7, world-locked placement.** Decided after the Full panel has been tried on the HMD with F9.
- **Item 11, the Mods Config VR client in `ModBase`.** It needs the host's native API, which does not exist yet.
