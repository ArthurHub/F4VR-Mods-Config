## v1.1.0

- A setting another mod overrides, as ROCK sets FRIK's Pip-Boy button, has the warning mark, and the help line names the mod and the value in effect. A change of it is still saved, and applies once the override ends; it isn't previewed meanwhile.
- For mod authors: a native client tells the menu a setting's overrides with `addOverrides`, API version 2. A CommonFramework mod's config file needs no code, once the mod is built with the framework that answers them.

- For mod authors: `"omitDefault": true`, on a file, a page, a section or a setting, is for a file that keeps only the values a player changed: a missing key is its default and no warning, a Save writes a value only while it differs from its default, and Reset takes its key out, from a TOML or an INI file. A Save that leaves the file with no values deletes it.
- A Save writes a color, a transform or a binding that a mod's TOML file has as a `[section]` of its own. It failed before.
- A Save no longer logs a warning for each F4SE plugin of the config spec that has no listener of its message.
- For mod authors: the AI agent prompt asks its open decisions as numbered questions before it writes, points to FRIK's and Immersive Flashlight VR's config specs, and has a second prompt that connects a DLL to the menu. The guide suggests a page layout, and the native API's docs walk through live preview in a mod and choosing `live` or `save`.
- For mod authors: the native API's `closeMenu` closes the menu as the player's Exit does, asking first while there are unsaved changes, or dropping them with `closeMenu(true)`. It is API version 2.
- For mod authors: a section's `maxOn` limits how many of its switchers are on, for a choice of up to a number of things; `countsAs` makes a switcher take more than one place. At the limit, the switchers that are off are disabled, with the reason.
- For mod authors: a page with `contentFromClientCallback` gets its rows from the mod's DLL as it is shown, for rows that depend on the game right now, as the items the player carries. The native client gives them with `addPageContent`, API version 2.

## v1.0.1

- For mod authors: after a config spec is read again from its live copy, its mod's native client is told `PageShown` for the page still on screen, as after any other selection.
- For mod authors: a condition or an option's `sets` on a config spec's front page can name a setting on any page. It was reported as naming no setting, and left out.

## v1.0.0

The first release. From it, the config spec schema and the native API only grow: a config spec and a DLL built for 1.0 keep working on every later version.

## Development versions

### v0.14.0

- A page with unsaved changes has the pending dot in the tree, and a closed page shows the changes of its pages too.
- A config spec reads an INI value that has a comment after it, as `fTarget=8.5  # target`, and Save keeps the comment.
- An activation sphere's binding icon, the icons of the buttons that fire it, has rows under Visuals: when it shows, which bindings, its position and size, and its color.
- An activation sphere's top rows have Icons: None, Icon, Binding, or Icon and binding, which turns each on and puts the binding icon in the icon's place or beside it. Show icon is under Visuals.
- A config spec's stepper or dropdown can be a set of presets: each option sets other settings, as Quality sets Low, Medium or High on several rows, and the row shows the option whose values they have. An option's `alsoSets` are set by a pick too but don't decide the option, so a player tunes them and keeps it: an activation sphere's Icons keeps its option as the binding icon is moved or sized.
- For mod authors: a ready prompt for an AI agent that writes a mod's config spec from its files, and fixes it from the errors the menu reports.

### v0.13.0

- A list of 20 mods or more is grouped by category, from the mod's own `category` or the menu's table of known mods, with No category last. The number is on the Panel page.
- The Categories page of the menu's own settings moves a mod to another category, or a new one typed on the keyboard. Save keeps it in `Categories.toml`, and Reset gives the mod its own again.
- A config spec and an MCM config can name their mod's `category`.

### v0.12.0

- The tree's header is a search field: type a name and the tree shows the mods and pages with a setting that matches, with how many, and a page shows only its settings that match, marked `(filtered)`. Back shows the whole page, with the matches still underlined.

### v0.11.0

- The gesture that opens and closes the menu no longer leaves the game's favorites open.
- A config spec's condition and text can ask a setting of the game's INI, as the game's settings change it, with no DLL to answer it.
- A config spec keeps a color, a controller binding and a position in an INI file too, as a line a mod with no TOML reads.
- A mod's DLL is told which row you point at, and the section or activation sphere it is in, so it can show in the world what that row sets.
- An activation sphere's zone is under Position and size, which says its size is there. The drawn sphere's size has a row only where a config spec names it.

### v0.10.0

- An alpha for testing, with the Nexus page, the README, the docs and the FAQ.
- A mod's own UI hands its edits back to the menu, to Save with the rest.
- An edit a mod's UI hands back comes back to the mod marked `handedOver`, so the mod doesn't apply it twice.
- A color's editor has basic colors and the mod's presets.
- The menu works without FRIK, and without MCM.
- The help line says the help of the option you point at in an open list.
- An MCM page whose own plugins aren't loaded is greyed out with the reason, or hidden, as its config says.
- An MCM row's text and help can come from the game: a form's name or description, or a script's property.
- A config spec's setting, button and section can show a state of its mod's DLL in their text and help, as a text row can.
- The menu's own settings are polished, and its thumbstick bindings are read as the framework reads them.
- The menu's own Developer page has the VR UI dev layout, with a button that opens its file.
- A dragged `live` setting is previewed to its mod at most every 50 ms, and always the value it ends on.

### v0.9.0

- CommonFramework controls in config specs: colors with a picker, controller bindings with an editor, haptics with Play, positions and rotations with sliders or adjusted in the world from the wrist, activation spheres, and a choice of the files in a folder.
- An MCM mod has the `MCM` tag, and its changes are made as in MCM, with Keep and Revert.
- A config spec's button can stand out, be dangerous, and have an icon.
- Help stands out until it is first opened.

### v0.8.1

- The Help page tells how the menu works, with the buttons your config has.
- Restore and Exit on the wrist while the menu is minimized.
- The panel's curve is a setting.
- A loading screen and the main menu close the menu, and the panel stands down while a game menu takes the controllers.

### v0.8.0

- Live preview: a CommonFramework mod with a config spec shows a change as it is made, with no code of its own.

### v0.7.0

- Buttons run a mod's own functions, and a mod adds quick launch entries for its own UIs.
- FRIK's config opens from the menu's front page, and the menu comes back when it closes.

### v0.6.0

- Config specs edit FRIK's TOML, INI files and a script mod's globals, and a Save that fails changes no file.
- Developer tools: each config spec's problems on the front page, and a live copy of a `spec.json` that reloads as it is saved.

### v0.5.0

- Text typed on SteamVR's keyboard.
- Config specs: the menu's own TOML is edited from its config spec and saved, and the mod reloads its file.

### v0.4.0

- Every MCM feature is supported, shown read-only, or shown as unsupported with the reason.

### v0.3.0

- MCM values are edited: globals, ModSettings and script properties, and the mod reacts as it does to MCM.

### v0.2.0

- Every MCM mod is browsed in the panel: its pages, rows and images, with translated names.

### v0.1.0

- The project, from the F4VR Common Framework template.
