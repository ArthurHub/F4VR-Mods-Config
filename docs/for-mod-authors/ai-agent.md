# Writing a Config Spec with an AI Agent

Ready prompts for an AI coding agent, such as Claude Code, Codex or Copilot: the first writes your mod's config spec, the second connects your DLL to the menu. The config spec is a new format, so each prompt points the agent at its docs and the Showcase before it writes anything.

Where to start: [For Mod Authors](README.md). Every field of a config spec: the [config spec schema](../design/schema.md). Each thing it does, with its JSON: the [Showcase walkthrough](showcase.md).

## Before You Start

- **Give the agent your mod's files:** its INI or TOML files, its scripts' source, its DLL's source, and its MCM config if it has one. It reads where each setting is kept and its default from them.
- **It asks first.** Before it writes anything, the agent lists its open decisions as numbered questions, each with the answer it recommends: which settings, which control for each kind, how the pages are laid out. Answer by number.
- **The agent can't run the game; you start it.** It asks you to load the config spec with **Read the live copy**. If the agent can reach your `Documents` folder, it then edits the live copy and reads the errors from the log by itself while the game runs; otherwise it asks you to paste them back.
- **Read what it wrote.** Check each `default` against your mod: it is what Reset sets. The agent lists the values it chose and didn't find.

## The Prompt

Copy the whole block into your agent, in your mod's folder:

````markdown
Write a config spec for my Fallout 4 VR mod: one `spec.json` that gives the mod a page in Mods Config FO4 VR, an in-game VR settings menu. The menu reads and writes the mod's real settings where the mod keeps them; the config spec says what the page shows and where each value is kept. It is a JSON format you have not seen before, so read its docs before writing anything.

## Read first

1. The author guide, with its "A Layout That Works": https://raw.githubusercontent.com/ArthurHub/F4VR-Mods-Config/main/docs/for-mod-authors/README.md
2. The schema, every field: https://raw.githubusercontent.com/ArthurHub/F4VR-Mods-Config/main/docs/design/schema.md
3. The Showcase config spec, a working example of every feature: https://raw.githubusercontent.com/ArthurHub/F4VR-Mods-Config/main/data/showcase/ModsConfigFO4/Config/ModsConfigFO4_Showcase/spec.json
   Its front page and General page are a complete small config spec, and its Detailed Examples page has the rest. Its Mistakes page is wrong on purpose: never copy from it.
4. Two real mods' config specs, for the layout the guide suggests: FRIK https://raw.githubusercontent.com/rollingrock/Fallout-4-VR-Body/main/data/mod/ModsConfigFO4/Config/FRIK/spec.json and Immersive Flashlight VR https://raw.githubusercontent.com/ArthurHub/F4VR-ImmersiveFlashlight/main/data/mod/ModsConfigFO4/Config/ImmersiveFlashlightVR/spec.json
   Copy their page layout, the About and Help front page, the folded Developer section, choices with a help per option. Their `{clientState:...}` texts and `CallNativeFunction` buttons need a DLL connected to the menu: leave them out unless the mod has one.
5. If the mod has an MCM menu (`Data\MCM\Config\<mod>\config.json`): https://raw.githubusercontent.com/ArthurHub/F4VR-Mods-Config/main/docs/for-mod-authors/config-spec-vs-mcm.md
6. If the mod has a DLL not built on the F4VR Common Framework: https://raw.githubusercontent.com/ArthurHub/F4VR-Mods-Config/main/docs/for-mod-authors/native-api.md
   Write the config spec first; connecting the DLL is a later step I decide on.

## Find the settings

Work from the mod's files and source, never from guesses.

- Find each setting a player should change: keys of its INI and TOML files, and its scripts' globals and properties. For each, note where it is kept (file, section and key; or plugin and form id, script and property), its type, its default, its range or options, and what it does in the game.
- `default` is the mod's own default, what Reset sets: take it from the shipped file or the code that reads the setting.
- Take ranges from the code (clamps, checks) or the shipped file's comments. Where there are none, choose a sensible range and tell me.
- The mod's own settings menu, or its MCM config, is the best map of how players see the settings: its labels, its lists' values and its ranges. Carry an MCM config over as the "Moving From MCM" table of Config Spec vs MCM says.

## Ask before writing

List the decisions you can't take from the code as numbered questions, each with the answer you recommend, and wait for my answers before you write the config spec. Ask all of them in one list:

- Which files the page edits, and which settings it leaves out.
- Each setting whose meaning, range or file you aren't sure of.
- The control for each kind of value, and any value the menu can't show as the mod keeps it (see below).
- How the pages and sections are laid out.
- `applies`, when the code doesn't tell you.

## Write it

Start from the Showcase's front page and General page, as the author guide's "Your First Config Spec" does, and copy controls from the Detailed Examples page.

- `minSchemaVersion` is `1`.
- `modName`: for a mod built on the F4VR Common Framework, the name the framework registers the mod under; for a mod with an MCM config, its MCM `modName`; otherwise a stable name of your choice. `f4sePluginRequirements` names an F4SE plugin as it registers, not by its DLL's file name.
- `applies`: `live` for a Common Framework mod, `save` for a mod that reads its file again as it changes (look for a file watcher or a reload call in its code), `restart` for one that reads it only as the game starts. Leave it out when the code doesn't tell you.
- Every row has `text`; give every setting a `help` in the player's terms: what it changes in the game, not the key's name. Every writable setting has a `default`.
- A file the mod keeps only changed values in, removing a key set back to its default, takes `"omitDefault": true` in `files`.
- A `dropdown` or `stepper` is for a closed set of values the mod writes as the options spell them: a value in the file that is none of its options is invalid, and blocks Save for the whole mod. If the mod's parser takes other spellings (any case, aliases), or the value has no closed list, use a `textinput` or ask me.
- A color the mod keeps as a value per channel (red, green, blue and maybe alpha keys) is a `cf.color` with `parts` naming a `slider` per channel, each with its own storage, default and range: the sliders' rows are hidden, and the color's row edits them.
- A value the menu has no control for, as an `[x, y, z]` array, can't be shown as it is. Tell me, and propose a change of the mod's file, as an array of a position turned into a `cf.transform`: changing the mod's format is my decision.
- Give the mod a `category`, one of the twelve shipped names where one fits.
- Use only fields the schema has: the menu ignores an unknown field, with a warning.
- Ids compare case-insensitively and are unique. A condition names a setting by its `id`, so give a short `id` to each setting a condition asks.
- A texture (the banner, an image) must be a file the mod ships under `Data\Textures`. Leave it out when there is none, and tell me.
- Keep the front page short; put each area of the mod on a page of its own, with sections inside.

Write it to `Data\ModsConfigFO4\Config\<mod>\spec.json` in the mod's files, and make sure the mod's packaging or install step ships it with the rest.

## Check it

You can't run the game; I test it, and you fix what the menu reports.

1. Ask me to install the mod with the config spec, turn on Developer tools (in the menu: Mods Config FO4 VR, Developer page), press the mod's row in the front page's Developer section, and press Read the live copy.
2. If you can reach `Documents\My Games\Fallout4VR`, work on your own while the game runs: edit the live copy, `Mods_Config\ModsConfigFO4VR\Dev\<mod>\spec.json`, which the game reads again each time it is saved, and read its problems at the end of `F4SE\ModsConfigFO4VR.log`, each with where it is in `spec.json`. Copy the live copy back to the mod's files when it is clean.
3. Otherwise, ask me to paste the report's errors and warnings, or the problems in the log, and fix each one in the mod's files.
4. Repeat until the report is clean, then ask me to check the page in the headset.

When you are done, list for me: the settings you left out and why, each default and range you chose rather than found, and what to check in the headset. If the mod's code has a list of its settings (a table of keys with their defaults), offer a test that checks the config spec against it, so a setting added later fails until it is placed on a page or listed as left out.
````

## Connecting Your DLL

For a mod with a DLL not built on the F4VR Common Framework, once the config spec works: a second prompt, in the same agent, connects the DLL to the menu, for live preview and the rest of the [native API](native-api.md). A CommonFramework mod is connected by the framework and doesn't need it.

````markdown
Connect my mod's DLL to Mods Config FO4 VR through its native API, for the config spec you wrote. Read first: https://raw.githubusercontent.com/ArthurHub/F4VR-Mods-Config/main/docs/for-mod-authors/native-api.md, and the client header it links to.

- Copy `ModsConfigFO4Api.h` into the mod as it is, and connect under the config spec's `modName` at F4SE's `kPostLoad`. The mod must work as it does when Mods Config FO4 VR isn't installed.
- Preview the `live` settings as "Previewing in Your Mod" says: the change handler records, the mod applies every recorded change on its own thread, over the file's values, without writing the file.
- Propose for each setting whether it is `live` or `save`, as "Live or Save" says, and ask me before you change `applies`.
- Add a `modVersion` state if the front page has a version line, and a function for each button the config spec has for the mod's own UI.
- If the mod remaps controller input, ask me how it behaves while the menu is open: the menu's pointer reads the controllers too.

When you are done, list what to check in the headset: each live setting as you drag it, Discard, and Save.
````
