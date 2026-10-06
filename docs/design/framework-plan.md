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
| F2  | framework | vrui: offhand controller presets, top and right, as on the primary wand     | done      |
| M3  | mod       | The wrist buttons on the offhand top preset                                 | done      |
| F3  | framework | vrui: wrist preset that follows the forearm bone, with controller fallback  | done      |

F1 adds the rotation to the dev layout as `Rot`, so it is tuned live in the dev layout's file.

M2 moves the submodule to F1 and uses it:

- The panel is larger, stands further away and lower, and is tilted back by 10 degrees.
- With FRIK installed, a third wrist button minimizes the menu and opens FRIK's own config, which lost its gesture to this menu.

F2 attaches to `secondaryUIOffsetNode`. It has the same local transform as `primaryUIAttachNode`, no offset and the same tilt, so the offhand presets need no rotation. In left-handed mode the row sits a little to the left of the hand: the hand's placement on the controller is not mirrored there, which is left for FRIK to fix.

### Dev layout

Not an item of the requirements: the tool the placements above are tuned with.

| #   | Repo      | Commit                                                                      | Status    |
| --- | --------- | --------------------------------------------------------------------------- | --------- |
| F15 | framework | vrui: the dev layout is turned on by a debug flag and keeps its own file    | done      |
| M4  | mod       | The wrist buttons on the wrist preset, the dev layout by the framework flag | done      |

F15 turns the dev layout on with `bVRUIDevLayout` in `[Debug]`, which is not in the shipped INI, and devbench's `set` turns it on for a session. Its lines are in `ModsConfigVR_DevLayout.ini` beside the INI, a section for each attached root, and no longer in the mod's INI or in `ConfigBase`.

M4 moves the submodule to F15 and uses what the framework brings since M3:

- The wrist buttons lie on the wrist preset of F3, are taller, and say CLICK above their name.
- The mod's vrui adapter is removed: the framework finds the fingertip and points the hand by itself.
- The `devlayout` devbench action is removed: the dev layout is turned on by its flag.
- The panel is wider and taller, and tilted back by 5 degrees.

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

F4 places the ray from the wand's UI node (`primaryUIAttachNode`, `secondaryUIOffsetNode`). Its +Y is the way the hand aims a weapon: the node a weapon hangs on and the game's `SecondaryAimNode` are turned the same. The ray points 18 degrees above that and 5 degrees inward, tuned in the game, and is mirrored for the left hand.

F16 is not an item of the requirements: `ConfigBase::getColorValue`, which M5 reads the pointer's colors with.

F5 draws the pointer while it is on a canvas: the owner's ray, up to a set length and fading at both ends, and a mark with a border on the canvas. Where the ray is on the hand and the whole look are one `PointerStyle`, whose defaults are the default pointer. The framework has no INI keys for it.

M5 moves the submodule to F5 and uses what the framework brings since M4:

- The panel is interactive.
- The pointer is set in the `[Pointer]` section of the mod's INI: where the ray is on the hand, the color, width, longest length and fade of the ray, and the color, size and border of the mark.
- The pointer and the values of the placeholder controls are in the devbench state.

F6 hides the whole wand of a hand from the game and from other mods while its ray is on an interactive canvas, for each hand by its own ray. The mod has nothing to call.

- A press that began on a canvas stays on that canvas until the trigger is released. The pointer follows the ray on the canvas's plane, up to a canvas's size past its edges.
- The wand of a press stays hidden until the release, also after its canvas is hidden. The press ends with the trigger near its rest: the game also takes a pull from how far the trigger is pulled.
- A hand whose trigger is already down when its ray comes to a canvas has no pointer, and its wand is not hidden, until the trigger is released.

F7 makes the thumbstick of the hand that owns the pointer ImGui's mouse wheel, so it scrolls what the pointer is on. The speed follows how far the thumbstick is pushed, up to `PointerStyle::scrollSpeed` at a full push, in lines of text a second. ImGui's input trickling is turned off: with it, a wheel given in the same frame as a moved pointer waits a frame, and the two pile up.

M6 moves the submodule to F7 and uses what the framework brings since M5:

- The wand that points at the panel is hidden from the game, with nothing for the mod to do.
- The list of the placeholder controls scrolls with the thumbstick, and `fScrollSpeed` in `[Pointer]` sets how fast.

### Dedicated panel (item 6)

| #   | Repo      | Commit                                                                          | Status |
| --- | --------- | ------------------------------------------------------------------------------- | ------ |
| F8  | framework | imgui: the canvas window's zero padding and border are popped before content    | done      |
| F9  | framework | imgui: dedicated panel with a texture of its own, as ImGui's display            | done      |
| M7  | mod       | The panel is dedicated, with its combo at the bottom edge                       | done      |

F8 pops the canvas window's zero padding and border right after the window begins. What the content opens then has ImGui's own: a combo's list, a popup, a dialog, and a bordered child window, whose content moves in by that padding.

F9 adds `setDedicated` to `UIImGuiPanel` and to `Canvas`:

- A dedicated panel has a texture of its own size in place of a part of the atlas, up to 4096 layout pixels a side, which is about 85 vrui units.
- While it is shown, the ImGui frame holds that panel alone and ImGui's display is the panel.
- The atlas texture is created when a canvas is first drawn into it, so a mod with only a dedicated panel does not have it.

M7 moves the submodule to F9 and uses it:

- The panel is dedicated, so its combo's list and its dialog open inside it.
- The panel is 20 by 15 units at a scale of 6, where it was 38 by 30 at a scale of 3: about the same size in the world, with everything on it twice as large.
- The combo of the placeholder controls is at the panel's bottom edge, where its list has to open upward.

### World-locked placement (item 7)

| #   | Repo      | Commit                                                                           | Status |
| --- | --------- | -------------------------------------------------------------------------------- | ------ |
| F17 | framework | vrui: a preset puts a root where the HMD is and leaves it in the world           | done   |
| F18 | framework | imgui: the mark keeps its size on the canvas, and the other hand's ray is drawn  | done   |
| M8  | mod       | The panel stays in the world, recentered by a gesture and when the player leaves | done   |

F17 adds `attachPresetToWorldAtHMD` and `recenterWorldElement` to `UIManager`:

- The root is put where the HMD is and stays there while the player walks and turns. Its own position and rotation apply from that place as they do on the HMD's node.
- `recenterWorldElement` puts the root where the HMD is now.
- The root stays attached to the HMD's node. Every frame the manager gives it a base transform that moves it back to where the node was, the way the wrist preset moves a root onto the forearm.
- Given a distance, the preset recenters the root by itself when the player walks further than that from it. A hidden root is left where it is.

F18 is not an item of the requirements. It changes how the pointer is drawn, which showed once the player could walk away from the panel:

- The mark's size is set in the world and is the same at any distance, so the mark keeps its size on the canvas. Scaled to look the same size in the view, it grew on the panel as the player stepped back.
- While both rays are on a canvas, the ray of the hand that does not own the pointer is drawn at 0.3 of the opacity and with no mark. With nothing drawn for it, that hand looked broken.

M8 moves the submodule to F18 and uses both:

- The panel is attached with the world preset, with the position and tilt it had on the HMD.
- Resume puts the panel in front of the player again. So does closing and opening the menu, which creates the panel again.
- `sRecenterBinding` does it while the panel is shown: a long press of the offhand thumbstick by default. It does not fire while the button of the open gesture is held too, so closing the menu with both thumbsticks does not move the panel first.
- The preset is given `fRecenterDistance`, 350 units by default, so the framework puts the panel in front of the player when they walk away from it, and not while the menu is minimized. The menu is not closed, as it can hold changes that are not saved.
- `fMarkSize` and `fMarkBorderWidth` are 0.45 and 0.08 by default.

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

- **The ray comes from the wand's UI node**, not from `primaryWandNode` or `secondaryWandNode`. The wand's own node points 59 degrees above where the hand aims.
- **Pointer visuals come before suppression** (F5 before F6). Pointing cannot be judged in the headset without a visible ray.
- **Suppression is of the whole wand.** While a wand operates a panel, all of its buttons and axes are hidden from the game, instead of the trigger and the scrolling thumbstick only. F7 then only feeds the thumbstick to ImGui as the wheel.
- **Each hand is suppressed by its own ray**, not the pointer's hand only. A press of the other hand on a canvas takes the pointer over, so its wand has to be hidden before the press.
- **A press outlives its canvas.** When a canvas is hidden or destroyed in the middle of a press, the wand stays hidden from the game until the trigger is released, instead of being given back at once. The game would otherwise get a trigger that is already down.
- **A dedicated panel has a largest size**, 4096 layout pixels a side, instead of keeping its pixels at any size. At the largest supersample factor that is the largest texture D3D11 can have.
- **A world-locked root is given no transform.** A preset puts it where the HMD is, and the root's own position and rotation place it from there, instead of a helper that computes a world transform for the mod to pass. The HMD's node is that place already: at the head, level, and facing the way the head faces.

## Not in this plan

- **Item 11, the Mods Config VR client in `ModBase`.** It needs the host's native API, which does not exist yet.
