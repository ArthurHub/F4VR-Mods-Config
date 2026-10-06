# UI and VR Interaction

## Visual direction

Mods Config VR should feel like a large projected Pip-Boy interface rather than a desktop window floating in VR:

- Translucent panel material
- Strong outer border and section framing
- High contrast at headset resolution
- Large controls and generous spacing
- Clear depth and hover response without excessive animation
- Stable placement while the player is interacting

Most of the UI is one Dear ImGui panel placed in a CommonFramework vrui layout (`imgui::UIImGuiPanel`). A few controls that sit on the hand are vrui buttons. The framework's ImGui panels cannot be pointed at or clicked yet; [CommonFramework work required](framework-requirements.md) lists what has to be built first.

## Opening the menu

The menu opens by pressing in both thumbsticks and holding them, the gesture FRIK uses today for its own configuration (`primary longpress thumbstick 1.5 +offhand:thumbstick`). It is an ordinary CommonFramework input binding in `ModsConfigVR.ini`, so the player can change it.

When FRIK is installed, Mods Config VR takes the gesture over. It turns off FRIK's `sOpenConfigButton` binding for the session through FRIK's config override API, and FRIK's configuration is then opened from FRIK's entry in Mods Config VR. FRIK can integrate more closely later.

The game is not paused while the menu is open, so the effect of a change is seen as it is made.

FRIK is not required. With FRIK, the hand-anchored vrui buttons are pressed with the index fingertip and the hand takes a pointing pose. How those buttons are pressed without FRIK is an open question.

## Three UI states

### Full

The primary browsing and editing state.

```text
┌─────────────────────────────────────────────────────────────────────────┐
│ MODS CONFIG VR                                      [?] [Minimize] [X] │
├───────────────────┬─────────────────────────────────────────────────────┤
│ Mods              │ Immersive Flashlight VR                            │
│                   │ Point at a setting to see its description here.    │
│ ● Flashlight      ├─────────────────────────────────────────────────────┤
│   FRIK            │ General                                             │
│   Holsters        │ Enabled                    [ On ]                    │
│   MCM Mods        │ Brightness       [-] [====o----] [1.25] [Reset]    │
│                   │                                                     │
│ Search / Filter   │ ▸ Advanced                                          │
│                   │                                                     │
│                   │                                                     │
├───────────────────┴─────────────────────────────────────────────────────┤
│                                      [Discard]              [Save]      │
└─────────────────────────────────────────────────────────────────────────┘
```

- Navigation is on the left and selected mod content is on the right.
- With no selected mod, navigation expands and can show richer summaries.
- Selecting a mod contracts navigation but does not hide it.
- The player can clear the selection to restore full-width navigation.
- The player can navigate to another mod without first clearing the current selection.
- MCM-backed mods may appear in a distinct group or carry a badge, whichever proves visually cleaner.
- A fixed top content area shows the description or error for the pointed/focused setting.
- Save and Discard remain in stable locations.

### Focused

A smaller offhand-anchored view of only the selected mod or page.

```text
┌──────────────────────────────────────────┐
│ Immersive Flashlight VR       [Full] [—]│
├──────────────────────────────────────────┤
│ Brightness                              │
│ Controls the flashlight beam intensity. │
│                                         │
│ [-] [==========o------] [1.25] [Reset] │
│                                         │
│                         [Discard] [Save] │
└──────────────────────────────────────────┘
```

- Anchored to the offhand in a readable orientation.
- Omits the global mod navigator.
- May use more compact presentations of the same controls.
- Retains descriptions, validation, Save, Discard, Minimize, and return-to-Full controls.
- Pending changes are the same editing session as Full mode.

### Minimized

The large interface is hidden while the player checks the effect of changes.

```text
inner offhand forearm / behind thumb

┌──────────┬─────────┐
│ Resume   │ Close   │
└──────────┴─────────┘
```

- Only a small offhand control remains. Its two buttons are vrui buttons, pressed by touch.
- With FRIK, the control follows the forearm, so the arm never covers it. Without FRIK, it sits beside the offhand controller.
- `Resume` restores the previous Full or Focused context.
- `Close` enters the normal unsaved-changes flow when needed.
- Pending values and active previews remain alive while minimized.
- Minimized is not equivalent to Save or Close.

## Full-panel placement

The Full panel appears in front of the player at a comfortable distance and height. The reference for how it should feel is Fallout 4 VR's projected Pip-Boy.

The panel is world-locked: it is spawned in front of the player, level, and then stays where it is, without following the HMD or either controller. A panel that moves with the head also moves under the ray.

The panel is curved toward the player, so its sides are about as far away as its middle and nearly face the player.

- Closing and reopening creates a fresh placement from the player's current position.
- Resuming Full mode from Minimized also creates a fresh placement.
- A simple `Recenter` action respawns the panel if its placement becomes unusable; this is not a general repositioning system. It is a long press of the offhand thumbstick, and can be rebound.
- The panel is also respawned when the player walks away from it. The menu is not closed, as it can hold changes that are not saved.

There is no grab-to-move or free repositioning.

Whether world geometry hides the panel, and where the panel goes when a wall is closer than its distance, are open questions.

## Pointing and activation

- The ImGui panel is operated with a controller ray, which ends in a mark where it meets the panel.
- Either controller can point at the interface.
- Only one controller owns focus/active interaction at a time. The ray of the other one is drawn fainter and without a mark.
- A preferred interaction hand may be configurable, but the other controller should still be usable where practical.
- Trigger activates controls and performs drags.
- Thumbstick scrolls the list under the pointer, or adjusts the currently focused control where defined.
- A dropdown, popup, or confirmation opens inside the panel and stays within its edges, so the panel is the only surface the ray has to reach. A list too long for the panel scrolls inside its popup.
- Back follows the navigation hierarchy; see [Back behavior](#back-behavior). Which controller button it uses is not decided.
- Direct touch on the ImGui panel can be explored later.
- There is no fake desktop mouse cursor or operating-system mouse emulation.

Targets must be sized for natural controller jitter. Hover, focused, active, disabled, changed, and invalid states must be visually distinct.

## Input while the menu is open

The game keeps running, so input is taken from it only while the player is operating the panel:

- While a ray points at the panel, that hand's trigger and thumbstick drive the UI and are hidden from the game. The trigger does not fire a weapon, and the thumbstick does not move or turn the player.
- A press that starts on the panel stays with the UI until it is released, even if the ray drifts off the panel.
- A trigger that is already held when the ray reaches the panel is ignored until it is released.
- When no ray points at the panel, the controls belong to the game.

Nothing more is withheld while the menu is open. The player can point away from the panel to move, turn, or use a weapon and try a change, then point back and keep editing. The cost is that a trigger pull that just misses the panel reaches the game.

## Back behavior

Back resolves the most local temporary interaction first:

1. Cancel an active drag or text edit.
2. Close a popup, dropdown, keyboard, or confirmation.
3. Clear the selected mod and return to the full-width mod list.
4. Request closing the interface.

Pages are a flat list inside a mod, so there is no page level to back out of.

If closing would abandon pending changes, the player receives Save, Discard, and Cancel choices.

## Haptics

Haptic feedback should be restrained and meaningful:

- Light pulse when a target becomes actively engaged
- Distinct confirmation for Save or successful action
- Distinct error pulse for rejected input
- No pulse for every rendered frame or every tiny slider increment

Exact haptic profiles can use the future CommonFramework `Haptic` control/configuration design.

## Description and help area

The current direction is a fixed description area near the top of the content pane. It should show, in priority order:

1. Active validation error
2. Description for the currently pointed/focused control or option
3. Page or mod-level help when no setting is focused

Keeping the area fixed avoids layout movement and makes help discoverable without pop-up tooltips floating at uncomfortable depths.

## Navigation behavior

- Mods are listed alphabetically by display name; page and setting order follow their JSON arrays.
- With no mod selected, the mod list uses the full width. Selecting a mod narrows the list to give the mod's settings more room, and the list remains usable. Back returns to the full-width list.
- Category/group labels and badges must not consume so much width that names become unreadable.
- Long lists need smooth thumbstick scrolling and a clear position indicator.
- Search/filter is desirable, but its exact place in an initial release is not decided.
- The selected mod and current page survive switches among Full, Focused, and Minimized states.

## MCM presentation

MCM-backed entries should feel integrated without concealing their different lifecycle:

- MCM-backed entries appear only when MCM is installed.
- Show a small `MCM` or equivalent compatibility badge if it remains legible.
- State that changes apply immediately when that is the original MCM behavior.
- Do not offer a misleading Save contract for changes the original MCM has already applied.
- Preserve MCM-specific help, option state, and event behavior where possible.

The exact grouping and visual treatment should be settled through headset mockups.

## Bespoke UI handoff

A mod may expose an action such as `Open FRIK Calibration` or `Configure Holsters in World`.

Expected experience:

1. Mods Config VR records its current state and hides or suspends its interactive panel.
2. It transfers interaction ownership to the registered external UI.
3. The external mod reports completion or cancellation.
4. Mods Config VR restores the prior Full or Focused context and refreshes affected values.

The external UI is not embedded inside the Mods Config VR panel. Failure recovery and timeout behavior remain open design work.

FRIK's own configuration is the first case: once Mods Config VR takes over the gesture that opened it, FRIK's menu is reached this way.
