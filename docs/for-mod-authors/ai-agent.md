# Writing a Config Spec with an AI Agent

A ready prompt for an AI coding agent, such as Claude Code, Codex or Copilot, that writes your mod's config spec. The config spec is a new format, so the prompt points the agent at its docs and the Showcase before it writes anything.

Where to start: [For Mod Authors](README.md). Every field of a config spec: the [config spec schema](../design/schema.md).

## Before You Start

- **Give the agent your mod's files:** its INI or TOML files, its scripts' source, its DLL's source, and its MCM config if it has one. It reads where each setting is kept and its default from them.
- **The agent can't run the game; you test it.** It asks you to load the config spec with **Read the live copy** and to paste back the errors the menu reports. If the agent can reach your `Documents` folder, it edits the live copy itself, and you see each change in the headset as it saves.
- **Read what it wrote.** Check each `default` against your mod: it is what Reset sets. The agent lists the values it chose and didn't find.

## The Prompt

Copy the whole block into your agent, in your mod's folder:

````markdown
Write a config spec for my Fallout 4 VR mod: one `spec.json` that gives the mod a page in Mods Config FO4 VR, an in-game VR settings menu. The menu reads and writes the mod's real settings where the mod keeps them; the config spec says what the page shows and where each value is kept. It is a JSON format you have not seen before, so read its docs before writing anything.

## Read first

1. The author guide: https://raw.githubusercontent.com/ArthurHub/F4VR-Mods-Config/main/docs/for-mod-authors/README.md
2. The schema, every field: https://raw.githubusercontent.com/ArthurHub/F4VR-Mods-Config/main/docs/design/schema.md
3. The Showcase config spec, a working example of every feature: https://raw.githubusercontent.com/ArthurHub/F4VR-Mods-Config/main/data/showcase/ModsConfigFO4/Config/ModsConfigFO4_Showcase/spec.json
   Its front page and General page are a complete small config spec, and its Detailed Examples page has the rest. Its Mistakes page is wrong on purpose: never copy from it.
4. If the mod has an MCM menu (`Data\MCM\Config\<mod>\config.json`): https://raw.githubusercontent.com/ArthurHub/F4VR-Mods-Config/main/docs/for-mod-authors/mcm.md
5. If the mod has a DLL not built on the F4VR Common Framework: https://raw.githubusercontent.com/ArthurHub/F4VR-Mods-Config/main/docs/for-mod-authors/native-api.md
   Write the config spec first; the native API is a later step I decide on.

## Find the settings

Work from the mod's files and source, never from guesses.

- Find each setting a player should change: keys of its INI and TOML files, and its scripts' globals and properties. For each, note where it is kept (file, section and key; or plugin and form id, script and property), its type, its default, its range or options, and what it does in the game.
- `default` is the mod's own default, what Reset sets: take it from the shipped file or the code that reads the setting.
- Take ranges from the code (clamps, checks) or the shipped file's comments. Where there are none, choose a sensible range and tell me.
- An MCM config is the best map of the settings: carry it over as the MCM guide's "Moving to a Config Spec" table says.
- Ask me before you guess: a setting's meaning, range or file, or whether players should change it at all.

## Write it

Start from the Showcase's front page and General page, as the author guide's "Your First Config Spec" does, and copy controls from the Detailed Examples page.

- `minSchemaVersion` is `1`.
- `modName`: for a mod built on the F4VR Common Framework, the name the framework registers the mod under; for a mod with an MCM config, its MCM `modName`; otherwise a stable name of your choice. `f4sePluginRequirements` names an F4SE plugin as it registers, not by its DLL's file name.
- `applies`: `live` for a Common Framework mod, `save` for a mod that reads its file again as it changes, `restart` for one that reads it only as the game starts. Leave it out when the code doesn't tell you.
- Every row has `text`; give every setting a `help` in the player's terms: what it changes in the game, not the key's name. Every writable setting has a `default`.
- Give the mod a `category`, one of the twelve shipped names where one fits.
- Use only fields the schema has: the menu ignores an unknown field, with a warning.
- Ids compare case-insensitively and are unique. A condition names a setting by its `id`, so give an `id` to each setting a condition asks.
- A texture (the banner, an image) must be a file the mod ships under `Data\Textures`. Leave it out when there is none, and tell me.
- Keep the front page short; put each area of the mod on a page of its own, with sections inside.

Write it to `Data\ModsConfigFO4\Config\<mod>\spec.json` in the mod's files.

## Check it

You can't run the game; I test it, and you fix what the menu reports.

1. Ask me to install the mod with the config spec, turn on Developer tools (in the menu: Mods Config FO4 VR, Developer page), press the mod's row in the front page's Developer section, and press Read the live copy.
2. Ask me to paste the report's errors and warnings, or the problems in `Documents\My Games\Fallout4VR\F4SE\ModsConfigFO4VR.log`. Each names where it is in `spec.json`.
3. Fix each one. If you can reach `Documents\My Games\Fallout4VR\Mods_Config\ModsConfigFO4VR\Dev\<mod>\spec.json`, the live copy, edit it there: the game reads it again each time it is saved. Copy it back to the mod's files when it is clean.
4. Repeat until the report is clean.

When you are done, list for me: the settings you left out and why, each default and range you chose rather than found, and what to check in the headset.
````

## Next

- **[The Showcase walkthrough](showcase.md):** each thing a config spec does, with the JSON the agent copies from.
- **[Testing It in the Game](README.md#testing-it-in-the-game):** the Developer report and the live copy the prompt uses.
