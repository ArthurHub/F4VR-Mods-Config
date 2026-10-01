# UI and VR Interaction

## Visual direction

Mods Config VR should feel like a large projected Pip-Boy interface rather than a desktop window floating in VR:

- Translucent panel material
- Strong outer border and section framing
- High contrast at headset resolution
- Large controls and generous spacing
- Clear depth and hover response without excessive animation
- Stable placement while the player is interacting

Dear ImGui, rendered through F4VR-CommonFramework's panel support, is the intended implementation stack.

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

- Only a small offhand control remains.
- `Resume` restores the previous Full or Focused context.
- `Close` enters the normal unsaved-changes flow when needed.
- Pending values and active previews remain alive while minimized.
- Minimized is not equivalent to Save or Close.

## Full-panel placement

When opened, the Full panel is spawned relative to the HMD at a comfortable distance and height, oriented primarily from horizontal head direction. It then becomes world-locked for the duration of use.

The panel does not continuously follow the HMD or either controller. This avoids movement while aiming and reduces discomfort.

For the initial design:

- There is no grab-to-move or free repositioning.
- Closing and reopening creates a fresh placement from the player's current position.
- Resuming Full mode from Minimized also creates a fresh comfortable placement.
- A simple `Recenter` recovery action may respawn the panel if its placement becomes unusable; this is not a general repositioning system.

This behavior should be tested against the projected Pip-Boy experience in Fallout 4 VR rather than accepted solely on paper.

## Pointing and activation

- Either controller can point at the interface.
- Only one controller owns focus/active interaction at a time.
- A preferred interaction hand may be configurable, but the other controller should still be usable where practical.
- A controller ray is the initial pointing mechanism.
- Trigger activates controls and performs drags.
- Thumbstick scrolls lists or adjusts the currently focused control where defined.
- Back follows the navigation hierarchy.
- Direct-touch interaction can be explored later.
- There is no fake desktop mouse cursor or operating-system mouse emulation.

Targets must be sized for natural controller jitter. Hover, focused, active, disabled, changed, and invalid states must be visually distinct.

## Back behavior

Back resolves the most local temporary interaction first:

1. Cancel an active drag or text edit.
2. Close a popup, dropdown, keyboard, or confirmation.
3. Leave a nested page or Advanced section context.
4. Clear the selected mod in Full mode.
5. Request closing the interface.

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

- Mod order follows the resolved package order; page and setting order follow their JSON arrays.
- The navigation area can collapse in width after selecting a mod, but remains usable.
- Category/group labels and badges must not consume so much width that names become unreadable.
- Long lists need smooth thumbstick scrolling and a clear position indicator.
- Search/filter is desirable, but its exact place in an initial release is not decided.
- The selected mod and current page survive switches among Full, Focused, and Minimized states.

## MCM presentation

MCM-backed entries should feel integrated without concealing their different lifecycle:

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
