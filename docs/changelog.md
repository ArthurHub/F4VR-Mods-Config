## Development versions

Not released: each is a milestone of the [work plan](design/mod-plan.md).

### v0.10.0

- An alpha for testing, with the Nexus page, the README, the docs and the FAQ.
- A mod's own UI hands its edits back to the menu, to Save with the rest.
- A color's editor has basic colors and the mod's presets.
- The menu works without FRIK, and without MCM.
- The help line says the help of the option you point at in an open list.
- The menu's own settings are polished, and its thumbstick bindings are read as the framework reads them.

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
