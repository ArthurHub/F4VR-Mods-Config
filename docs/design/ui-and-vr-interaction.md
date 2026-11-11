# UI and VR Interaction

## Visual direction

A large projected Pip-Boy interface, not a desktop window in VR: translucent panel, strong border and section framing, high contrast at headset resolution, large controls with generous spacing, clear depth and hover response without much animation, stable placement while interacting.

Most of the UI is one Dear ImGui panel in a CommonFramework vrui layout (`imgui::UIImGuiPanel`); the few hand controls are vrui buttons.

## Opening the menu

- The gesture is both thumbsticks pressed and held, FRIK's own (`primary longpress thumbstick 1.5 +offhand:thumbstick`): an ordinary input binding in `ModsConfigFO4VR.toml`, rebindable.
- With FRIK installed, the menu takes it over: FRIK's `sOpenConfigButton` is turned off for the session through FRIK's config override API, and FRIK's configuration opens from the front page's quick launch.
- The game isn't paused: a `live` change is seen as made, a saved one as the mod acts on it.
- FRIK is optional. With FRIK, hand-anchored vrui buttons are pressed with the index fingertip in a pointing pose; without, the controller presses them and a small sphere marks the point.

## Two UI states

### Full

The browsing and editing state.

```text
┌─────────────────────────────────────────────────────────────────────────┐
│ MODS CONFIG FO4 » Immersive Flashlight VR            [?] [Minimize] [X] │
├───────────────────┬─────────────────────────────────────────────────────┤
│ Search 4 mods     │ General                                             │
│                   │ Enabled                    [ On ]                   │
│ ▾ Flashlight      │ Brightness       [-] [====o----] [1.25] [Reset]     │
│     Beam          │                                                     │
│     Hand mount    │ ▸ Advanced                                          │
│ ▸ FRIK            │                                                     │
│ ▸ Holsters        │                                                     │
│ ▸ Sim Settlements │                                                     │
├───────────────────┴─────────────────────────────────────────────────────┤
│ Brightness: How bright the beam is. (Default: 1.00) [Discard]  [Save]   │
└─────────────────────────────────────────────────────────────────────────┘
```

- The tree of mods is on the left, the selected content on the right. The tree keeps its place and width, with or without a selection; another mod can be selected at any time. See [Navigation behavior](#navigation-behavior).
- Mods are one list, not grouped or labeled by MCM vs config spec; a long list is grouped by [category](#mod-categories).
- **Breadcrumbs** in the title bar: `MODS CONFIG FO4 » <mod> » <page>`, a crumb per ancestor page, or `MODS CONFIG FO4` alone on the [front page](#the-menus-front-page). The last crumb is where the player is; each earlier one goes there when pressed. When they don't fit, the pages between mod and page become one unpressable `…`, then the mod's name is ellipsized; the first and last crumbs never shorten.
- **Title bar buttons** right of the crumbs: Help (the [Help page](#the-help-page)), Minimize, Close; the help line explains each. Help is `? Help` in the accent color until first opened, then `?`: `bHighlightHelp` in `[ModsConfigFO4VR]`, on as shipped, written off when the Help page first shows.
- The content pane has no heading, so its height goes to settings.
- The footer has the [help line](#help-line) on the left and Discard and Save fixed on the right.

### Minimized

The panel is hidden while the player checks the effect of changes.

```text
inner offhand forearm

     MODS CONFIG
┌──────────┬──────────┐
│ Restore  │  Exit    │
└──────────┴──────────┘
```

- Only while minimized does the offhand wrist have buttons: `MODS CONFIG` over `Restore` and `Exit`, vrui buttons pressed by touch. With FRIK they follow the forearm so the arm never covers them; without, they sit beside the offhand controller.
- `Restore`, or the opening gesture, shows the panel as it was, rereading the shown mod's values (the mod may have changed them).
- `Exit` closes the menu, asking first while the shown mod has unsaved changes.
- Pending values and previews stay alive; minimizing is neither Save nor Exit.

## The menu's front page

What the Full panel shows with no mod selected, and what the menu opens on: a closed menu doesn't keep its place, a minimized one does.

```text
┌─────────────────────────────────────────────────────────────────────────┐
│ MODS CONFIG FO4                                      [?] [Minimize] [X] │
├───────────────────┬─────────────────────────────────────────────────────┤
│ Search 4 mods     │ Settings for the mods you have installed. Pick a    │
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

- No tree row: the `MODS CONFIG FO4` crumb is its entry, shown selected while open. No mod is selected or expanded. Reached by that crumb or Back from a mod's front page; Back here closes the menu.
- A text (what the menu is, how to start, the version), then [quick launch](#quick-launch), [notices](#notices) and, with Developer tools on, [Developer](#developer-tools). Empty sections are left out.
- Nothing is edited, so no editing session; the help line shows the quick launch entry's help.

### Quick launch

An entry opens a mod's own config UI, such as FRIK's.

- A row is the mod's name and an `Open` button; the whole row is pressed.
- A mod registers entries through the native API, one per UI, each with a help text and optional button text. It needs no config spec; a mod with one is also in the tree.
- FRIK adds its entry, as the menu took its gesture. For a FRIK without the native API, the menu adds one while FRIK is installed; it minimizes the menu, which doesn't return by itself (`Restore` brings it back).
- Entries are alphabetical.
- A press runs a function of the mod's client, which minimizes the menu and opens the UI; see [A mod's own UI](#a-mods-own-ui).

### Notices

A mod the menu found but can't show is **not available** when:

- a required plugin isn't loaded (a game plugin of an MCM config or config spec, or a config spec's F4SE plugin);
- its MCM config or spec.json can't be read (then it's named by its folder, as in MCM);
- its spec.json needs a newer Mods Config FO4 VR, or another config spec has its `modName`.

Its tree row is in its place by name, disabled (not selectable, no pages), and the help line gives the reason: `Sim Settlements: Not available: its plugin SS2.esm is not loaded.` The front page lists each under `Notices` with the same reason. The log has it too.

A mod with a saved change that takes effect after a restart has a notice from its Save until the restart, listed first: `Restart the game: Immersive Flashlight VR has 1 saved change that takes effect then.` See [RestartNotice](design-system/project/components/RestartNotice/README.md).

### Developer tools

For config spec authors. `Developer tools` is a switch in the mod's own settings, off as shipped; off, none of this is drawn. The log has every diagnostic either way.

```text
▾ Developer ──────────────────────────────────────────────────
Show [3 of 12]          ◂ [ Needing attention ] ▸
Immersive Flashlight VR   [Live copy] ▲ 2 errors △ 5 warnings ▸
Showcase                                      △ 3 warnings ▸
Virtual Holsters                              ▲ Not loaded ▸
```

- A Developer section after the notices. Its heading folds (and stays as the author leaves it); folded, it counts the listed config specs' errors and warnings.
- First row: a filter stepper, written to the config at once: those needing attention (error, warning, live copy, not loaded), all, or the one named in the settings. The filter and that name's dropdown are in the mod's own settings too.
- A row per listed config spec, alphabetical: name, live copy, errors and warnings, or none / not loaded / not available. MCM mods are never listed.
- A press opens its report, `MODS CONFIG FO4 » Developer » <mod>`; Back or the `Developer` crumb returns to the front page.
- The report says where `spec.json` is read from, and has:
  - `Read the live copy`: copies `spec.json` to `My Games\Fallout4VR\Mods_Config\ModsConfigFO4VR\Dev\<folder>\spec.json` if absent and reads that instead. The copy is reread on each save; Reload rereads at once. A read drops the mod's unsaved changes and keeps the open page if it still exists. An unreadable copy is an error and the mod keeps its last read. Off reads the installed file again and keeps the copy.
  - Under it, `Live spec.json` folds (folded each time the menu opens): `Reload`; `Re-copy the installed spec.json` (warns while the installed one is newer); `Edit on desktop` (the desktop's `.json` editor); `Reveal in file explorer`. Folded, it says `Installed is newer` while so.
  - Errors, then warnings, each under a folding heading. A problem row has its mark and message; the help line gives the page, row and `spec.json` path. A press on one naming a row goes to it.

See [Developer](design-system/project/components/Developer/README.md), [FullPanelDeveloper](design-system/project/components/FullPanelDeveloper/README.md), [FullPanelConfigSpecReport](design-system/project/components/FullPanelConfigSpecReport/README.md).

### What differs from MCM's

MCM's menu also opens on a no-mod page with the mods on the left. Differences:

- MCM's page (logo, version) is gone once a mod is selected; here the player returns to it for quick launch and notices.
- MCM orders by its `sOrder`, then folder; here alphabetical by display name.
- MCM's `MCM Settings` and `Hotkey manager` aren't listed: the first holds MCM's pause-menu place and list order; hotkeys are on each mod's pages.
- MCM lists a mod it can't show like any other, with a page saying why; here the row is disabled with the reason in the help line and notices.
- MCM's Cancel from the mod list is Back from the front page here.

## The Help page

How this menu works its own way, with the buttons the player's config has (not what every VR player knows, as pointing and scrolling). Help in the title bar shows it over where the player is.

```text
┌─────────────────────────────────────────────────────────────────────────┐
│ MODS CONFIG FO4 » Help                               [?] [Minimize] [X] │
├───────────────────┬─────────────────────────────────────────────────────┤
│ Search 4 mods     │ How the menu works, with the buttons your config    │
│                   │ has.                                                │
│ ▾ Flashlight      │ Navigation ──────────────────────────────────────── │
│     Beam          │ Pointing at the menu: That hand's trigger and       │
│     Hand mount    │ thumbstick work the menu, and the game does not get │
│ ▸ FRIK            │ them.                                               │
│ ▸ Holsters        │ Back: Tap the grip on either hand while pointing at │
│                   │ the menu.                                           │
├───────────────────┴─────────────────────────────────────────────────────┤
│ • 1 unsaved change                                  [Discard]  [Save]   │
└─────────────────────────────────────────────────────────────────────────┘
```

- Crumbs: `MODS CONFIG FO4 » Help`. The selected mod, page and editing session stay: Help shows in place of the page without leaving it. Back returns to that page; a tree row or the first crumb goes there as from any page.
- A text, then five sections, lines as `Name: text`; none folds, the page scrolls. Nothing on it is pressed.
  - **Navigation**: opening and closing (which also restores a minimized menu); the pointing hand works the menu and the game doesn't get its trigger and thumbstick; Back; moving the panel; Recenter (also automatic); Minimize, with Restore and Exit on the wrist.
  - **Search**: finds a setting by the start of each word of its name, any order; a page opened while searching shows only matches, underlined, crumb marked `(filtered)`, and Back shows it whole; Clear, or Back on the field, empties it.
  - **Changes**: nothing is written until Save, per mod, and leaving with changes asks; the undo arrow and Discard; Reset; a script button asks to save first; each when-it-takes-effect mark. `Live` needs saying: the change is seen in the game before Save, and Discard or leaving unsaved puts the old value back in the game too.
  - **Marks**: amber and red values, the warning mark, a button's result, a greyed row (the help line says why).
  - **MCM**, always shown: the `MCM` tag marks these mods; a change is made at once, as in MCM's menu, Keep accepts and Revert sets back, so a script button doesn't ask to save; hotkeys are only shown; a mod that can't be shown is greyed, with why.
- Buttons and bindings are named as the config has them and the controllers print them, shorter than the [InputBinding](design-system/project/components/InputBinding/README.md) sentence: no time or threshold; a both-hands chord of one button is one press, "Long press both thumbsticks". Back and move are on the pointing hand, either, so "the A or X button". A control the config turns off is left out.

## Full-panel placement

- The panel spawns in front of the player at a comfortable distance and height, level, like the projected Pip-Boy, and is world-locked: it doesn't follow the HMD (a panel moving with the head moves under the ray).
- It is curved so its sides are about as far as its middle and nearly face the player.
- **Move:** point and hold the grip longer than Back's tap; the panel follows that hand, smoothly and level, until release, and stays. It is the framework's world move binding (`UIManager::setWorldMoveBinding`), on any hand, rebindable (also to an immediate press) or off.
- Opening, and restoring from Minimized, place it fresh from the player's position.
- **Recenter** (long press of the offhand thumbstick, rebindable) respawns it in front, also after a move. Walking away respawns it too, without closing the menu, which may hold unsaved changes.
- Open: whether world geometry hides it, and placement when a wall is nearer than its distance.

## Pointing and activation

- A controller ray operates the panel, ending in a mark where it meets it. No fake desktop cursor or OS mouse emulation.
- Either controller can point; one owns interaction at a time, and the other's ray is fainter, without a mark. A preferred hand may be configurable, with the other still usable.
- Trigger activates and drags.
- Thumbstick up/down scrolls the list under the ray; left/right steps the slider under it (after a short dwell on its row) or the stepper (one option per push).
- Dropdowns, popups and confirmations open inside the panel, within its edges; a long list scrolls inside its popup.
- Back is a tap of the pointing hand's grip, rebindable; see [Back behavior](#back-behavior).
- Direct touch on the ImGui panel is later work.
- Targets are sized for controller jitter. Hover, focused, active, disabled, changed and invalid are visually distinct.

## Input while the menu is open

The game keeps running, so input is taken only while the panel is operated:

- While a ray points at the panel, that hand's trigger and thumbstick drive the UI and are hidden from the game (no firing, moving or turning).
- A press that starts on the panel stays with the UI until released, even if the ray drifts off.
- A trigger already held when the ray reaches the panel is ignored until released.
- With no ray on the panel, the controls are the game's. Nothing else is withheld: the player points away to move, turn or fire to try a change, then points back. The cost: a pull that just misses the panel reaches the game.

## Back behavior

Back is a tap of the grip of the hand pointing at the panel, either hand: an any-hand binding that fires only on the pointing hand (`VRControllers.check(binding, hand)`); a single-hand binding fires only while that hand points. A longer hold moves the panel instead: a long press that fires is never a tap. The pointing wand is hidden from the game; with no ray on the panel the press is the game's, so looking away to try a change never steps back.

Back resolves the most local interaction first:

1. Cancel an active drag or text edit.
2. Close a popup, dropdown, keyboard or confirmation.
3. From the Help page, return to the page under it.
4. While searching, show a filtered page whole, with the focus ring on the last row the ray was on.
5. From a mod's page, go to its parent page or the mod's front page.
6. From a mod's front page, go to the menu's front page. This leaves the mod: with unsaved changes it first asks Save / Discard / Cancel, as selecting another mod does.
7. From the menu's front page, close the menu (nothing can be pending there, so no question).

## Haptics

Restrained and meaningful: a light pulse when a press does something, distinct pulses for Save or a successful action and for rejected input, none per frame or per slider increment while dragging (a thumbstick step gets one).

The light pulse goes to the pointing hand in the frame its press acts: a page is shown (Help too), the menu is minimized or closed from the title bar, a quick launch entry opens, a value changes, a dropdown opens, Back steps back or closes. A press that changes nothing (on the already-selected row) gives none. It is the framework pattern `Tick` by name: `sPressHaptic` in `[Pointer]` of `ModsConfigFO4VR.toml`, edited on the menu's own page with the [Haptic control](controls.md#haptic), changeable or off.

## Help line

The left of the footer; Save and Discard stay fixed to its right, and only its text changes. This one place avoids layout movement and tooltips at uncomfortable depths.

Priority:

1. An active validation error.
2. The help of the row, option or control the ray is on, as `Name: help` (the name not repeated on its own line). A row whose [warning](schema.md#warnings) holds puts the warning first.
3. With the ray on no row, the session's status: the unsaved changes or the last Save.

- The help holds while the ray crosses gaps between rows; the status returns when the ray leaves the rows or the open popup. Otherwise it would flicker during a sweep.
- Two lines of room; help needing a third is drawn smaller, longer help ends in an ellipsis.
- One size and color. Tags of a [text row](schema.md#text-spacers-and-images) in an MCM help are stripped, with or without `html`, and line breaks become spaces.
- **When it takes effect:** while a change waits for Save, the help line adds an amber mark of its `applies` (as made, after Save, after restart), appearing as the setting is changed, so browsing shows help alone. `unknown` (every MCM setting) has none. The row's pending mark is that same amber mark, or the plain pending mark for `unknown`; counts of pending changes (a folded heading, the status, the unsaved-changes dialog) use the plain mark. See [When a change takes effect](schema.md#when-a-change-takes-effect), [TakesEffect](design-system/project/components/TakesEffect/README.md).
- A script button of a mod with unsaved changes says in its help that a press asks to save first ([Buttons](schema.md#buttons)).
- A config spec's mod and pages can have help, shown while the ray is on their tree row; MCM mods and pages have none. A [not available](#notices) mod shows its reason there. A page that explains itself at length uses a text item at the top of its content, scrolling with the rows, as MCM pages do.

## Navigation behavior

- Mods are alphabetical by display name; pages and settings follow their JSON order. A not-available mod is a disabled row ([Notices](#notices)).
- Mods are the top level unless grouped by category; only the selected mod is expanded, its pages under it (not the front page, which is the mod's own row). A mod's front page is its root `content`, in MCM and config spec alike, and its `pages` are the rows under it; no pages, no rows.
- A config spec page with subpages is expanded while it or a subpage is selected, its pages one step further in. Each level is indented by the same small step up to the third. A page with subpages has an arrow, as a mod does, and names of a level line up with or without one. See [ModTree](design-system/project/components/ModTree/README.md).
- A list longer than its area scrolls with the thumbstick while the ray is on it, with a scroll bar: tree, page rows, dropdown lists. About nine rows fit in the tree. The tree always reserves its scroll bar's room, so rows don't narrow when it appears.
- A row selected from outside the tree (crumb, Back, panel shown again) is scrolled into view; a category opened near the bottom scrolls to show its first mods, and the selected mod's category, opened again, to show the selection.
- The tree's header is the [search field](#search).
- The selected mod and page survive minimize and restore.

## Search

The tree's header is a search field that finds a setting by name across all mods.

- Idle, it says what it searches: "Search 11 mods". A press shows the keyboard as a text input does, filtering per key. Done and the keyboard's close keep the query; closing the menu empties it.
- It matches the names the player sees: mod display name, page, section heading, and the text of a setting, button or hotkey, config spec and MCM alike (an MCM row's text from the game too); not help or text rows. Each query word must start a word of the name, any order, ignoring case and accents: "hol siz" finds "Holster size". A Chinese, Japanese or Korean word matches anywhere.
- A word can match the row's mod, page or section name, as long as one word matches the row's own: "frik height" finds FRIK's Height.
- The most specific match wins: a matching section, page or mod shows everything under it only when nothing under it matches.
- In the selected mod, a setting hidden by a condition isn't found. Other mods' values aren't read, so their conditions hold and their hidden settings are found.
- With a query, the tree is flat: only mods with a match, each expanded to its matching pages, with counts on the right (a mod's row counts its front page). A mod found only by name is closed, with its total, until selected. Matches are underlined.
- The shown page has only its matching rows, in their sections, opened, matches underlined. Each is the real row, edited and saved as on the whole page. A press on a tree page goes there, keeping the query.
- The crumb is `<page> (filtered)`, `(filtered)` dimmer; with the ray on no row the help line says "Only the settings that match are shown. Back shows the whole page.", unless the session has changes to report.
- Where the shown page has no matches (as the front page), the content says "7 settings match “hol”. Pick a page in the tree to change them.", or that nothing matches. A page whose matching rows are all condition-hidden says so.
- Back shows the page whole, query kept, focus ring on the last row the ray was on, its sections opened; matches stay underlined. The next Back acts as with no query.
- Clear, or Back with the ray on the field, empties the query, restores the tree and shows the page whole the same way. A new query filters again.

Look: [Search](design-system/project/components/Search/README.md).

## Mod categories

A long list of mods is grouped by category.

```text
│ Mods                 │
│                      │
│ ▸ Combat ───────── 6 │
│ ▾ Interface ────── 2 │
│   ▾ FallUI           │
│       Inventory      │
│       HUD            │
│   ▸ Mods Config FO4  │
│ ▸ Settlements ──── 4 │
│ ▸ No category ──── 2 │
```

- Under 20 mods the list is flat; at 20 or more, categories are the top level. The threshold counts every mod and is a setting; 0 always groups.
- A mod has one category or none; categories don't nest. Its category is the first it has of: the player's choice, its own `category`, the shipped table of known mods ([Category](schema.md#category)). Otherwise it's under `No category`, the last row. A not-available mod is under its category too.
- Categories and their mods are alphabetical; an empty category isn't shown.
- A press opens or closes a category (no selection). Several can be open; the selected mod's is open. Which ones are open is kept until the game closes, in no file.
- A category row has its name muted, a line, and its mod count.
- Categories aren't in the crumbs, and Back doesn't stop at them.

### Changing a mod's category

The mod's own entry has a `Categories` page that the menu fills: a row per mod with a dropdown of category names. It is there while the list is flat too.

- The dropdown has every name, the shipped ones and those in use, alphabetical, then `No category` (removes the author's category); its last entry, `Type a new name`, opens the keyboard in the dropdown's box.
- A typed name is trimmed and at most 24 characters, so it fits a tree row; an empty one sets nothing. A name in the list, in any case, is that one: its spelling is kept, and `No category` gives no category. A typed name is in every row's list at once, while it waits for Save.
- A row is an ordinary setting of the mod's own config spec: Save saves, Discard takes back, Reset gives the category the mod has without the player's choice. The tree regroups after Save.
- Choices are in `Categories.toml` beside `ModsConfigFO4VR.toml`, one key per mod named by its `modName`; empty text is `No category`. `ModsConfigFO4VR.toml` holds only the mod's shipped keys.
- Only a choice that differs from the mod's own category is kept; Save removes any other key, so an unmoved or reset mod follows its own category and the table as they change. A mod that is no longer installed keeps its key.
- Search doesn't find the page's rows by the mods' names: a mod's name finds the mod.
- Later ([M99](mod-plan-phase-3.md#15-advanced-features)): when [MCM Categorizer](https://www.nexusmods.com/fallout4/mods/66311)'s `Data/MCM/Settings/MCMCategorizer.ini` exists, the page has an `Import from MCM Categorizer` button at the top. It gives each mod in one of its categories that name as a pending choice (Save keeps, Discard takes back). The file is only read.

## MCM presentation

- MCM mods appear only with MCM installed, in the same list as every other mod. Their rows have the `MCM` tag, as their changes are made at once where a config spec's wait for Save.
- Changes are made as in MCM's menu, with the Save bar's buttons named Keep (accept) and Revert (set back); the status and the leave-mod dialog say the same. When the mod acts on a change is unknown, and the help line says so.
- MCM help, option state and event behavior are kept where possible; events and actions run as a change is made and as Revert sets it back.

What the menu shows follows what the mod supports, not where its menu comes from: the Save bar for every mod, live preview where the mod has it. A config spec can state when a change takes effect, for the mod and any page, section or setting that differs.

## A mod's own UI

A mod may have its own UI, opened by a button (`Open FRIK Calibration`, `Configure Holsters in World`) or its quick launch entry. FRIK's configuration is the first case.

1. The press runs a function of the mod's client, which minimizes the menu and opens the UI. The editing session stays, pending changes too.
2. The player uses the mod's UI.
3. On close, the mod shows the menu again in its view and page, and the menu rereads the mod's values, so clean settings show what the UI saved.

The mod's UI is never drawn inside the panel. A UI that fails to open leaves the menu as it is, with the reason in the help line. One that never shows the menu leaves it minimized; `Restore` shows it. A mod can also open the menu at its own settings, from its wrist button or holotape. See [The mod's own UI](architecture.md#the-mods-own-ui).
