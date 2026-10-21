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

The menu opens by pressing in both thumbsticks and holding them, the gesture FRIK uses today for its own configuration (`primary longpress thumbstick 1.5 +offhand:thumbstick`). It is an ordinary CommonFramework input binding in `ModsConfigVR.toml`, so the player can change it.

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
│ Brightness: How bright the beam is. (Default: 1.00) [Discard]  [Save]   │
└─────────────────────────────────────────────────────────────────────────┘
```

- Navigation is on the left and selected mod content is on the right.
- The navigation is a tree of the mods. [The menu's front page](#the-menus-front-page), which is what the menu opens on, has no row in it: no mod is selected. The first breadcrumb, `MODS CONFIG VR`, goes to it, and is shown selected while it is open.
- Selecting a mod expands it to show the mod's pages under it, and shows the mod's front page. A config spec's page can have pages of its own, which it shows under it the same way when it is selected.
- The mod's front page has no row of its own in the tree: the mod's row is its row.
- The tree keeps its place and its width, with or without a selected mod.
- The player can select another mod at any time, without going to the front page first.
- The mods are one list. They are not grouped or labeled by where a mod's menu comes from, MCM or a config spec. A long list is grouped by category; see [Mod categories](#mod-categories).
- The title bar shows where the player is, as breadcrumbs: `MODS CONFIG VR » <mod> » <page>`, with a crumb for each page above the page, or `MODS CONFIG VR` alone on the menu's front page. The last crumb is where the player is, and each crumb before it can be pressed to go there: `MODS CONFIG VR` to the menu's front page, a mod to its front page, and a page to it. When the crumbs do not fit, the pages between the mod and the page become one `…`, which is not pressed, and then the mod's name is shortened with an ellipsis. The first crumb and the last are not shortened.
- The title bar has Help, Minimize and Close on its right, and the crumbs end before them. Help shows the [Help page](#the-help-page). Minimize and Close do what the wrist's buttons do: Minimize hides the panel as it is, and `Resume` on the wrist shows it again; Close closes the menu. The help line says what each does while the ray is on it.
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

- Only a small offhand control remains. Its two buttons are vrui buttons, pressed by touch. The panel is minimized with the first of them, `Minimize` until it is minimized, or with Minimize on the title bar.
- With FRIK, the control follows the forearm, so the arm never covers it. Without FRIK, it sits beside the offhand controller.
- `Resume` restores the previous Full or Focused context.
- `Close` enters the normal unsaved-changes flow when needed.
- Pending values and active previews remain alive while minimized.
- Minimized is not equivalent to Save or Close.

## The menu's front page

What the Full panel shows while no mod is selected. It is what the menu opens on: a menu that was closed does not keep its place, and one that was minimized does.

```text
┌─────────────────────────────────────────────────────────────────────────┐
│ MODS CONFIG VR                                       [?] [Minimize] [X] │
├───────────────────┬─────────────────────────────────────────────────────┤
│ Mods              │ Settings for the mods you have installed. Pick a    │
│                   │ mod on the left to see its settings.                │
│ ▸ Flashlight      │ Version 1.0.0                                       │
│ ▸ FRIK            │                                                     │
│ ▸ Holsters        │ Quick launch                                        │
│ ▸ Sim Settlements │ FRIK                                     [ Open ]   │
│                   │ Virtual Holsters                         [ Open ]   │
│                   │                                                     │
│                   │ Notices                                             │
│                   │ Sim Settlements is not available: its plugin        │
│                   │ SS2.esm is not loaded.                              │
├───────────────────┴─────────────────────────────────────────────────────┤
│ FRIK: Opens FRIK's own configuration in the world.                      │
└─────────────────────────────────────────────────────────────────────────┘
```

- The page has no row in the tree. The first breadcrumb, `MODS CONFIG VR`, is its entry, and is shown selected while the page is open. No mod is selected or expanded.
- The player comes back to it with that breadcrumb, and with Back from a mod's front page.
- It starts with a text: what the menu is, how to start, and the version of Mods Config VR.
- The [quick launch](#quick-launch) section and the [notices](#notices) follow, and, with Developer tools on, the [Developer](#developer-tools) section. A section with nothing in it is left out.
- Nothing is edited here, so the page has no editing session. The help line shows the help of the quick launch entry the ray is on.
- Back closes the menu; see [Back behavior](#back-behavior).

### Quick launch

An entry of the section opens a mod's own config UI, such as FRIK's.

- A row has the mod's name and an `Open` button, and the whole row is pressed.
- A mod registers its entry through the native API, with a help text that says what its UI is for, and the text of its button in place of `Open`. It needs no config spec for it, and a mod that has a config spec is in the tree as well.
- Until the native API is made, the menu adds one entry itself: FRIK, when FRIK is installed. The menu took the opening gesture from FRIK, so FRIK's config is opened from here.
- The entries are listed alphabetically, as the mods are.
- Pressing an entry runs a function of the mod's client, which minimizes the menu and opens the mod's UI; see [A mod's own UI](#a-mods-own-ui). Until the native API is made, the menu minimizes itself for FRIK's entry and does not come back by itself, and `Resume` on the wrist shows it again.

### Notices

A mod that the menu found and cannot show is not available:

- A plugin it requires is not loaded: a game plugin of an MCM config or of a config spec, or an F4SE plugin of a config spec.
- Its MCM config or its spec.json cannot be read. It is then named by its folder, as in MCM.
- Its spec.json needs a newer Mods Config VR, or another config spec has its `modName`.

The player is told in two places:

- The mod's row is in the tree, in its place by name, and is disabled: it is not selected, and has no pages. While the ray is on it, the help line says why, as `Sim Settlements: Not available: its plugin SS2.esm is not loaded.`
- The front page has a line for each such mod under `Notices`, with the same reason, so they are read in one place.

The reason is in the log too.

A mod with a saved change that takes effect only after the game is restarted has a line under `Notices` too, from its Save until the game is restarted: `Restart the game: Immersive Flashlight VR has 1 saved change that takes effect then.` These lines come first. See the design system's [RestartNotice](design-system/project/components/RestartNotice/README.md).

### Developer tools

For a config spec's author. `Developer tools` is a switch in Mods Config VR's own settings, off as shipped: while it is off none of this is drawn. The log has every diagnostic either way.

```text
▾ Developer ──────────────────────────────────────────────────
Show [3 of 12]          ◂ [ Needing attention ] ▸
Immersive Flashlight VR   [Live copy] ▲ 2 errors △ 5 warnings ▸
Showcase                                      △ 3 warnings ▸
Virtual Holsters                              ▲ Not loaded ▸
```

- The menu's front page has a Developer section after the notices. Its heading folds, and folded it says the errors and the warnings of the config specs it lists. It stays folded as the author leaves it.
- Its first row is a filter, a stepper that is written to the config at once: the config specs that need attention (an error, a warning, a live copy, or not loaded), all of them, or the one config spec named in the settings, with or without problems. The filter is in Mods Config VR's own settings too, with the dropdown that names that config spec.
- A row for each config spec the filter lists, alphabetical: its name, its live copy, and its errors and warnings, or that it has none, is not loaded or is not available. An MCM mod is never listed.
- A press on the row opens the config spec's report, `MODS CONFIG VR » Developer » <mod>`. Back, and the `Developer` crumb, go back to the front page.
- The report says where `spec.json` is read from, and has its live copy and its problems:
  - `Read the live copy` copies the config spec's `spec.json` to `My Games\Fallout4VR\Mods_Config\ModsConfigVR\Dev\<folder>\spec.json`, where there is none, and reads it in place of the installed file. The copy is read again each time it is saved, and Reload reads it at once. A read drops the mod's unsaved changes, and keeps the page that is open while it is still there. A copy that cannot be read is an error, and the mod keeps what it read last. Off reads the installed file again, and keeps the copy.
  - `Open` opens the copy in the desktop's editor for `.json`, and `Show` its folder.
  - The errors, then the warnings, each under a heading that folds. A problem is a row with its mark and message, and the help line says where it is: the page and the row, and the path in `spec.json`. A press on one that names a row goes to the row.

See the design system's [Developer](design-system/project/components/Developer/README.md), [FullPanelDeveloper](design-system/project/components/FullPanelDeveloper/README.md) and [FullPanelConfigSpecReport](design-system/project/components/FullPanelConfigSpecReport/README.md).

### What differs from MCM's

MCM's menu also opens on a page with no mod selected, and its mods are on the left. What differs:

- MCM's page has MCM's logo and version, and is gone once a mod is selected. Here the player comes back to the page, for the quick launch and the notices.
- MCM lists the mods in the order of its `sOrder` setting, and then by their folders. Here they are alphabetical by display name.
- MCM's list ends with two entries of its own, `MCM Settings` and `Hotkey manager`. Neither is listed here. The first holds where MCM is in the pause menu and the order of its list, and a mod's hotkeys are shown on the mod's own pages.
- MCM lists a mod that it cannot show as any other, with a page that says why. Here its row is disabled, and the reason is in the help line and in the notices.

MCM's Cancel closes its menu from the list of mods, as Back does here from the front page.

## The Help page

What the menu's controls do, with the buttons the player's config has for them. Help in the title bar shows it, over where the player is.

```text
┌─────────────────────────────────────────────────────────────────────────┐
│ MODS CONFIG VR » Help                                [?] [Minimize] [X] │
├───────────────────┬─────────────────────────────────────────────────────┤
│ Mods              │ How the menu is used, with the buttons your config  │
│                   │ has.                                                │
│ ▾ Flashlight      │ Controls ────────────────────────────────────────── │
│     Beam          │ Point and press: Point at the menu with either      │
│     Hand mount    │ hand, and pull the trigger on what the ray is on.   │
│ ▸ FRIK            │ Back: Press the grip briefly on the hand that       │
│ ▸ Holsters        │ points: one step back, and from the menu's front    │
│                   │ page the menu closes.                               │
├───────────────────┴─────────────────────────────────────────────────────┤
│ • 1 unsaved change                                  [Discard]  [Save]   │
└─────────────────────────────────────────────────────────────────────────┘
```

- The crumbs are `MODS CONFIG VR » Help`. The tree keeps the selected mod and page, and so does the editing session: the Help page is shown in place of the page, which is not left.
- Back goes back to the page it was shown over. A row of the tree or the first crumb goes there, as from any page.
- It is a text, then each control as `Name: text`, as the help line has a row's help: pointing and the trigger, the thumbstick's scroll and step, Back, moving the panel, Recenter, opening and closing the menu, and Minimize.
- A button and a binding are said as the config has them, in the sentence of the [InputBinding](design-system/project/components/InputBinding/README.md) control, as the controllers print their buttons: "Long press the thumbstick on the primary hand for 1.5 s, holding the thumbstick on the off hand". Back and the move are on the hand that points, either hand, so an A button is "the A or X button". A control the config turns off is left out.
- Nothing on it is pressed.

## Full-panel placement

The Full panel appears in front of the player at a comfortable distance and height. The reference for how it should feel is Fallout 4 VR's projected Pip-Boy.

The panel is world-locked: it is spawned in front of the player, level, and then stays where it is, without following the HMD. A panel that moves with the head also moves under the ray.

The panel is curved toward the player, so its sides are about as far away as its middle and nearly face the player.

The player can move the panel. They point at it and hold the grip of the hand that points, for longer than the short press that is Back, and the panel follows that hand, smoothly and kept level, until the grip is released. It then stays where it was left. The button can be changed, and moving can be turned off.

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
- Thumbstick up and down scrolls the list under the pointer. Left and right steps the slider under the pointer, once the ray is on its row for a short time, and the stepper under the pointer, one option for a push.
- A dropdown, popup, or confirmation opens inside the panel and stays within its edges, so the panel is the only surface the ray has to reach. A list too long for the panel scrolls inside its popup.
- Back follows the navigation hierarchy; see [Back behavior](#back-behavior). It is a short press of the grip of the hand that points at the panel, and the button can be changed.
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

Back is a short press of the grip on the hand that points at the panel, either hand. Holding the grip longer moves the panel, and is not Back. That wand is hidden from the game, so the game does not get the press. A press while no ray is on the panel is the game's: the player who looks away to try a change does not step back or close the menu with it.

Back resolves the most local temporary interaction first:

1. Cancel an active drag or text edit.
2. Close a popup, dropdown, keyboard, or confirmation.
3. From the Help page, go back to the page it was shown over.
4. While searching: from a page of a result, go to the results; from the results, empty the query and go back to where the player was.
5. From a page of a mod, go to the page it is under, or to the mod's front page.
6. From a mod's front page, go to the menu's front page.
7. From the menu's front page, close the menu.

Step 6 leaves the mod. When the mod has unsaved changes, the player gets the Save, Discard, and Cancel choices first, as when another mod is selected. So the menu's front page has nothing pending, and step 7 closes the menu without asking.

## Haptics

Haptic feedback should be restrained and meaningful:

- Light pulse when a press does something
- Distinct confirmation for Save or successful action
- Distinct error pulse for rejected input
- No pulse for every rendered frame or every tiny increment of a slider that is dragged. A step of a slider by the thumbstick gets one

The light pulse is given to the hand that points, in the frame its press does something: another page is shown, the Help page too, the menu is minimized or closed from the title bar, a quick launch entry is opened, a value is changed, the list of a dropdown is opened, Back goes a step back or closes the menu. A press that changes nothing gives none, as one on the row that is selected already. It is a pattern of the framework by its name, `Tick`, which can be changed and turned off.

The pattern is `sPressHaptic` in `[Pointer]` of `ModsConfigVR.toml`, which the menu's own page edits with the [Haptic control](controls.md#haptic).

## Help line

The help of what the ray is on is shown on the left of the footer, in the help line. Save and Discard stay to the right of it: they do not move or hide, and only the text of the help line changes.

The help line shows, in priority order:

1. Active validation error
2. The help of the row, option or control the ray is on, as `Name: help`: the row's text, a colon, and its help. The name is not repeated on a line of its own. A row whose [warning](schema.md#warnings) holds has the warning before its help.
3. The status of the editing session when the ray is not on a row: the unsaved changes, or that changes are saved as they are made.

The help is kept while the ray moves from row to row, through the gaps between them, and the status comes back when the ray leaves the rows or the open popup. Without that, the line would flicker as the ray sweeps the list.

The help line has room for two lines. Help that needs a third is drawn in a smaller size, and longer help ends in an ellipsis.

Help is one size and one color. An MCM config can have the tags of a [text row](schema.md#text-spacers-and-images) in a help, with or without `html`. They are removed, and a line break becomes a space, so the help keeps to its lines.

The help of a config spec's setting also says when its change takes effect, unless that is as it is made: after Save, or after the game is restarted. A setting that needs a restart has a mark on its row too. See [When a change takes effect](schema.md#when-a-change-takes-effect).

A config spec's mod and its pages can have a help of their own, which the help line shows while the ray is on their row in the tree. An MCM mod and its pages have none. A mod that is [not available](#notices) has the reason there instead: the help line says why while the ray is on its row. A page that explains itself at length does it with a text item at the top of its content, as MCM pages do, and the text scrolls with the rows.

Keeping the help in one place avoids layout movement and makes it discoverable without pop-up tooltips floating at uncomfortable depths. The Focused panel's footer is too narrow for it, so there the help is a two-line area at the top.

## Navigation behavior

- Mods are listed alphabetically by display name; page and setting order follow their JSON arrays.
- A mod that is not available is in the list too, as a disabled row; see [Notices](#notices).
- The mods are the top level of a tree, unless the list is grouped by category, and only the selected mod is expanded. Its pages are the rows under it, without the front page.
- A config spec's page that has pages of its own is expanded while it or one of its pages is selected, and its pages are the rows under it, a step further in. Each level is set in by the same small step, up to the third, so a deep tree still fits the tree's width. A page that has pages has an arrow, as a mod has, and the names of a level line up whether or not they have one. See the design system's [ModTree](design-system/project/components/ModTree/README.md).
- A mod's front page is its root `content`, in an MCM config and in a config spec alike, and its `pages` are the rows under it. A mod with no pages has no rows under it.
- A list longer than its area scrolls with the thumbstick while the ray is on it, and shows a scroll bar: the tree, a page's rows, and the list of a dropdown. About nine rows fit in the Full panel's tree, so most lists of mods scroll.
- The tree always keeps room for its scroll bar, so its rows do not narrow when the bar appears, as when a category or a mod is opened near the bottom.
- A row that is selected from outside the tree, from a breadcrumb, with Back, or when the panel is shown again, is scrolled into view. A category that is opened near the bottom scrolls to show its first mods.
- Search comes after the first release, with the localization; see [Search](#search).
- The selected mod and current page survive switches among Full, Focused, and Minimized states.

## Search

The tree's header is a search field, which finds a setting by its name in every mod. It comes after the first release, with the localization ([Mod work plan](mod-plan.md), M91). Until then the header says `Mods` and the count.

- Idle the field says what it searches: "Search 11 mods". A press on it shows the keyboard, as a text input does, and each key filters as it is typed.
- It finds the names as the player sees them: a mod's display name, a page's, a section's heading and a setting's text, of a config spec and of an MCM mod alike. Not the help. Each word of the query starts a word of the name, in any order, with case and accents ignored, so "hol siz" finds "Holster size".
- A setting that a condition hides is not found. One that a condition disables is found, and is disabled in the results.
- While the query has text, the tree has only the mods with a match, flat, each expanded to its pages that have a setting that matches, with how many. A mod whose name matches is there with its pages closed. The match is underlined in each name.
- The content is the results, with the crumbs `MODS CONFIG VR » Search`: the settings that match, under a heading for each mod and page. Each is the setting's own row, changed here as on its page, and Save saves it.
- A press on a row of the tree goes to that page, with the query kept, and the rows that match are underlined there. Back goes back to the results.
- Clear, in the field, empties the query and gives the tree back. Back from the results empties it too, and goes back to where the player was.
- In the Focused panel there is no search: it shows the page that is selected.

The look is the design system's [Search](design-system/project/components/Search/README.md).

## Mod categories

A mod is hard to find in a list of many mods, so a long list is grouped by category.

```text
│ Mods                 │
│                      │
│ ▸ Combat ───────── 6 │
│ ▾ Interface ────── 2 │
│   ▾ FallUI           │
│       Inventory      │
│       HUD            │
│   ▸ Mods Config VR   │
│ ▸ Settlements ──── 4 │
│ ▸ No category ──── 2 │
```

- With fewer than 30 mods the list is flat: the mods are the top level of the tree. With 30 or more, the categories are the top level and each mod is under its category. The number is a setting of Mods Config VR, and 0 groups a list of any length.
- A mod has one category or none, and categories do not nest.
- A mod's category is the first of these that it has: the player's choice, the mod's own `category`, and the one in the table of known mods that Mods Config VR ships. See [Category](schema.md#category).
- A mod with none of them is under `No category`, which is the last row.
- Categories are listed alphabetically, and so are the mods of each. A category with no mod is not shown.
- Pressing a category opens or closes it, and does not select it. More than one can be open, and the category of the selected mod is open.
- A category's row is set apart from a mod's: its name is in a muted color, followed by a line and the number of its mods.
- The category is not in the breadcrumbs, and Back does not stop at it.

### Changing a mod's category

The player can give a mod another category, to arrange the list as they like. Mods Config VR's own entry has a `Categories` page that the menu fills: a row for every mod, with a dropdown of category names.

- The dropdown has the names Mods Config VR ships and the names that are in use. `No category` is one of them, and takes a mod out of the category its author gave it. The last entry, `Type a new name`, opens the keyboard to type a name that is not in the list.
- A row is an ordinary setting of Mods Config VR's own config spec. It is saved with Save, Discard takes it back, and Reset gives the mod the category it has without the player's choice.
- The tree is grouped again after Save.
- The choices are kept in `Categories.toml` beside `ModsConfigVR.toml`, one key for each mod, named by its `modName`. A key whose text is empty is `No category`. `ModsConfigVR.toml` holds only the keys the mod ships.
- Only a choice that differs from the category the mod has without it is kept, and Save removes the key of any other. So a mod that the player did not move, or that was reset, follows its own category and the table when they change.
- The page is there while the list is flat too, so a list can be arranged before it is grouped.

A player who used [MCM Categorizer](https://www.nexusmods.com/fallout4/mods/66311) has arranged their MCM mods already. When its file `Data/MCM/Settings/MCMCategorizer.ini` is there, the page has an `Import from MCM Categorizer` button at its top, above the rows of the mods. It gives every mod that is in one of that mod's categories the category's name, as a pending choice: Save keeps them and Discard takes them back. The file is only read.

## MCM presentation

MCM-backed entries should feel integrated without concealing their different lifecycle:

- MCM-backed entries appear only when MCM is installed.
- They are in the same list as every other mod, with no group or badge of their own.
- State that changes are saved as they are made, which is what MCM does. It does not say that the mod acts on them at once: the menu does not know that.
- Do not offer a misleading Save contract for changes the original MCM has already saved.
- Preserve MCM-specific help, option state, and event behavior where possible.

What the menu shows for a mod follows what the mod supports, not where its menu comes from: changes that are saved as they are made, or a Save bar, with live preview where the mod has it. A config spec says when a change takes effect, for the mod and for any page, section or setting that differs.

## A mod's own UI

A mod may have a UI of its own, opened by a button such as `Open FRIK Calibration` or `Configure Holsters in World`, or by its quick launch entry.

1. The press runs a function of the mod's client. It minimizes the menu and opens its UI. The editing session stays, so a pending change stays pending.
2. The player uses the mod's UI.
3. As it closes, the mod shows the menu again, in the view and on the page it was, and the menu reads the mod's values again: a clean setting shows what the mod's UI saved.

The mod's UI is not drawn inside the menu's panel. A UI that does not open leaves the menu as it is, and the help line says why. One that never shows the menu again leaves it minimized, and `Resume` on the wrist shows it. A mod can also open the menu at its own settings, from its wrist button or its holotape. See [The mod's own UI](architecture.md#the-mods-own-ui).

FRIK's own configuration is the first case: once Mods Config VR takes over the gesture that opened it, FRIK's menu is reached this way.
