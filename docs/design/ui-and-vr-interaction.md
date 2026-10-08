# UI and VR Interaction

## Visual direction

Mods Config VR should feel like a large projected Pip-Boy interface rather than a desktop window floating in VR:

- Translucent panel material
- Strong outer border and section framing
- High contrast at headset resolution
- Large controls and generous spacing
- Clear depth and hover response without excessive animation
- Stable placement while the player is interacting

Most of the UI is one Dear ImGui panel placed in a CommonFramework vrui layout (`imgui::UIImGuiPanel`). A few controls that sit on the hand are vrui buttons. The framework work this needed is done; see the [Framework work plan](framework-plan.md).

## Opening the menu

The menu opens by pressing in both thumbsticks and holding them, the gesture FRIK uses today for its own configuration (`primary longpress thumbstick 1.5 +offhand:thumbstick`). It is an ordinary CommonFramework input binding in `ModsConfigVR.ini`, so the player can change it.

When FRIK is installed, Mods Config VR takes the gesture over. It turns off FRIK's `sOpenConfigButton` binding for the session through FRIK's config override API, and FRIK's configuration is then opened from the quick launch section of the menu's front page. FRIK can integrate more closely later.

The game is not paused while the menu is open, so a change that the mod acts on at once is seen as it is made.

FRIK is not required. With FRIK, the hand-anchored vrui buttons are pressed with the index fingertip and the hand takes a pointing pose. Without FRIK the controller itself presses them, and a small sphere marks the point.

## Three UI states

### Full

The primary browsing and editing state.

```text
┌─────────────────────────────────────────────────────────────────────────┐
│ MODS CONFIG VR » Immersive Flashlight VR             [?] [Minimize] [X] │
├───────────────────┬─────────────────────────────────────────────────────┤
│ Mods              │ General                                             │
│                   │ Enabled                    [ On ]                   │
│ ▾ Flashlight      │ Brightness       [-] [====o----] [1.25] [Reset]     │
│     Beam          │                                                     │
│     Hand mount    │ ▸ Advanced                                          │
│ ▸ FRIK            │                                                     │
│ ▸ Holsters        │                                                     │
│ ▸ Sim Settlements │                                                     │
│                   │                                                     │
│ Search / Filter   │                                                     │
├───────────────────┴─────────────────────────────────────────────────────┤
│ Brightness: How bright the beam is. Default 1.00.  [Discard]   [Save]   │
└─────────────────────────────────────────────────────────────────────────┘
```

- Navigation is on the left and selected mod content is on the right.
- The navigation is a tree of the mods. The menu's front page, which is what the menu opens on, has no row in it: no mod is selected, and the page has the quick launch links. The first breadcrumb, `MODS CONFIG VR`, goes to it, and is shown selected while it is open.
- A quick launch link opens a mod's own config UI. The mod registers for it through the native API, and needs no package for it.
- Selecting a mod expands it to show the mod's pages under it, and shows the mod's front page.
- The mod's front page has no row of its own in the tree: the mod's row is its row.
- The tree keeps its place and its width, with or without a selected mod.
- The player can select another mod at any time, without going to the front page first.
- The mods are one list. They are not grouped or labeled by where a mod's menu comes from, MCM or a package.
- The title bar shows where the player is, as breadcrumbs: `MODS CONFIG VR » <mod> » <page>`, or `MODS CONFIG VR` alone on the menu's front page. The last crumb is where the player is, and each crumb before it can be pressed to go there: `MODS CONFIG VR` to the menu's front page, a mod to its front page. When the crumbs do not fit, the mod's name is shortened with an ellipsis.
- The content pane has no heading of its own, so its height goes to settings.
- The footer has Save and Discard on the right and the help line on the left; see [Help line](#help-line).
- Save and Discard remain in stable locations.

### Focused

A smaller offhand-anchored view of only the selected mod or page.

```text
┌──────────────────────────────────────────┐
│ Immersive Flashlight VR       [Full] [—] │
├──────────────────────────────────────────┤
│ Brightness: Controls the flashlight beam │
│ intensity.                               │
├──────────────────────────────────────────┤
│ Brightness                               │
│ [-] [==========o------] [1.25] [Reset]   │
│                                          │
│                         [Discard] [Save] │
└──────────────────────────────────────────┘
```

- Anchored to the offhand in a readable orientation.
- Omits the global mod navigator.
- May use more compact presentations of the same controls.
- Retains help, validation, Save, Discard, Minimize, and return-to-Full controls.
- Its footer has no room for the help line, so the help is a two-line area at the top, in the same `Name: help` form.
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

The panel is world-locked: it is spawned in front of the player, level, and then stays where it is, without following the HMD. A panel that moves with the head also moves under the ray.

The panel is curved toward the player, so its sides are about as far away as its middle and nearly face the player.

The player can move the panel. They point at it and hold the grip of the hand that points, and the panel follows that hand, smoothly and kept level, until the grip is released. It then stays where it was left. The button can be changed, and moving can be turned off.

- Closing and reopening creates a fresh placement from the player's current position.
- Resuming Full mode from Minimized also creates a fresh placement.
- A simple `Recenter` action respawns the panel in front of the player, also after it was moved. It is a long press of the offhand thumbstick, and can be rebound.
- The panel is also respawned when the player walks away from it. The menu is not closed, as it can hold changes that are not saved.

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
3. From a page of a mod, go to the mod's front page.
4. From a mod's front page, go to the menu's front page.
5. From the menu's front page, request closing the interface. Whether Back does this last step is not decided.

If closing would abandon pending changes, the player receives Save, Discard, and Cancel choices.

## Haptics

Haptic feedback should be restrained and meaningful:

- Light pulse when a target becomes actively engaged
- Distinct confirmation for Save or successful action
- Distinct error pulse for rejected input
- No pulse for every rendered frame or every tiny slider increment

Exact haptic profiles can use the future CommonFramework `Haptic` control/configuration design.

## Help line

The help of what the ray is on is shown on the left of the footer, in the help line. Save and Discard stay to the right of it: they do not move or hide, and only the text of the help line changes.

The help line shows, in priority order:

1. Active validation error
2. The help of the row, option or control the ray is on, as `Name: help`: the row's text, a colon, and its help. The name is not repeated on a line of its own.
3. The status of the editing session when the ray is not on a row: the unsaved changes, or that changes are saved as they are made.

The help is kept while the ray moves from row to row, through the gaps between them, and the status comes back when the ray leaves the rows or the open popup. Without that, the line would flicker as the ray sweeps the list.

The help line has room for two lines. Help that needs a third is drawn in a smaller size, and longer help ends in an ellipsis.

The help of a package's setting also says when its change takes effect, unless that is as it is made: after Save, or after the game is restarted. A setting that needs a restart has a mark on its row too. See [When a change takes effect](schema.md#when-a-change-takes-effect).

A page or a mod has no help of its own in the help line. A page that explains itself does it with a text item at the top of its content, as MCM pages do, and the text scrolls with the rows.

Keeping the help in one place avoids layout movement and makes it discoverable without pop-up tooltips floating at uncomfortable depths. The Focused panel's footer is too narrow for it, so there the help is a two-line area at the top.

## Navigation behavior

- Mods are listed alphabetically by display name; page and setting order follow their JSON arrays.
- The mods are the top level of a tree, and only the selected mod is expanded. Its pages are the rows under it, without the front page.
- A mod's front page is its root `content`, in an MCM config and in a package alike, and its `pages` are the rows under it. A mod with no pages has no rows under it.
- Long lists need smooth thumbstick scrolling and a clear position indicator.
- Search comes after the first release; see the [Mod work plan](mod-plan.md).
- The selected mod and current page survive switches among Full, Focused, and Minimized states.

## MCM presentation

MCM-backed entries should feel integrated without concealing their different lifecycle:

- MCM-backed entries appear only when MCM is installed.
- They are in the same list as every other mod, with no group or badge of their own.
- State that changes are saved as they are made, which is what MCM does. It does not say that the mod acts on them at once: the menu does not know that.
- Do not offer a misleading Save contract for changes the original MCM has already saved.
- Preserve MCM-specific help, option state, and event behavior where possible.

What the menu shows for a mod follows what the mod supports, not where its menu comes from: changes that are saved as they are made, or a Save bar, with live preview where the mod has it. A package says when a change takes effect, for the mod and for any page, section or setting that differs.

## Bespoke UI handoff

A mod may expose an action such as `Open FRIK Calibration` or `Configure Holsters in World`.

Expected experience:

1. Mods Config VR records its current state and hides or suspends its interactive panel.
2. It transfers interaction ownership to the registered external UI.
3. The external mod reports completion or cancellation.
4. Mods Config VR restores the prior Full or Focused context and refreshes affected values.

The external UI is not embedded inside the Mods Config VR panel. Failure recovery and timeout behavior remain open design work.

FRIK's own configuration is the first case: once Mods Config VR takes over the gesture that opened it, FRIK's menu is reached this way.
