# For Mod Authors

How to give your mod a page in Mods Config FO4 VR: which way fits your mod, your first config spec, how it ships with your mod, and how you test it in the game.

Using the menu: [documentation](../README.md). Problems: [FAQ](../faq.md#for-mod-authors).

## Contents

- [Which Way Fits Your Mod](#which-way-fits-your-mod)
- [Your First Config Spec](#your-first-config-spec)
- [Shipping It](#shipping-it)
- [Testing It in the Game](#testing-it-in-the-game)
- [Next](#next)

## Which Way Fits Your Mod

| Your mod has | What you do | What players get |
| --- | --- | --- |
| An MCM menu | Nothing | Its MCM menu, as it is. A change is made as the player makes it, as in MCM's menu, with Keep and Revert. |
| Settings in INI or TOML files, or in its scripts' globals and properties | A config spec: one `spec.json`, and no code | Its page in the menu: VR controls, Save and Discard, values checked as they are typed, and when each change takes effect. |
| A DLL built on the [F4VR Common Framework](https://github.com/ArthurHub/F4VR-CommonFramework) | A config spec. The framework connects your DLL to the menu by itself | All of the above, and live preview: each change shows in the game as it is made, before Save. |
| A DLL of its own | A config spec, and the [native API](native-api.md) | Live preview, buttons that run your functions, options and states only your code knows, and your own UI opened from the menu. |

A config spec says what the page shows and where each value is kept. The menu reads and writes your mod's real files: there is no copy of your settings anywhere else. Your mod keeps reading its files as it does now, and a CommonFramework mod already reloads its TOML as it changes.

### Both an MCM menu and a config spec

A mod can ship both, with the config spec's `modName` the MCM config's `modName`. Mods Config FO4 VR lists the mod by its config spec, and MCM's own menu keeps showing the MCM config. So you keep MCM for the players who use it, and give the players of this menu its VR controls, Save and Discard, and live preview: the best of both. See [Keeping both](mcm.md#keeping-both), which has what both menus should share.

## Your First Config Spec

Start from the Showcase: an optional download on the Nexus page, whose config spec uses every feature. Its front page and its **General** page are a complete small config spec, and its **Detailed Examples** page has everything else.

An AI agent can write it for you, from your mod's files: [the prompt](ai-agent.md).

![The Showcase's General page](../assets/showcase/general.png)

1. Copy the Showcase's `Data\ModsConfigFO4\Config\ModsConfigFO4_Showcase\spec.json` to `Data\ModsConfigFO4\Config\<your mod>\spec.json`. The folder's name is yours to choose.
2. Delete the **Detailed Examples** page: the second entry of `pages`.
3. Set who it is:
   - `modName`: your mod's identity. For a CommonFramework mod it is the mod's own name, which the framework connects to the menu under, as `FRIK` or `ImmersiveFlashlightVR`.
   - `displayName`, the name in the menu's list, and `help`, what your mod is, shown while the player points at its name.
   - `pluginRequirements`, your game plugins, and `f4sePluginRequirements`, your F4SE plugin. While one isn't loaded, your mod is greyed out, with the reason.
4. Replace `files` with your file, and set `defaultFile` to its name. For a CommonFramework mod:

   ```json
   "files": {
     "config": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml", "pathFromClientState": "configFile" }
   },
   "defaultFile": "config",
   ```

   `root` is `myGames`, the game's `Documents\My Games\Fallout4VR`, or `data`, its `Data` folder. `pathFromClientState` makes the menu edit the file your mod loaded, wherever that is.
5. Set `applies`, when a change takes effect in your mod: `live` for a CommonFramework mod, which previews it, `save` for a mod that reads its file again as it changes, `restart` for one that reads it only as the game starts.
6. Make the front page yours: the banner's `texture`, a path under `Data\Textures`, the **About** text, and the URL of the **Help** button.
7. Make the General page yours: each row's `text`, `help` and `default`, and its `storage`, the section and key of your file:

   ```json
   {
     "type": "switcher",
     "valueType": "bool",
     "text": "Enabled",
     "help": "Turns the mod on and off.",
     "default": true,
     "storage": { "path": ["Main", "bEnabled"] }
   }
   ```

   Delete the rows you don't need, and copy more from the Detailed Examples, where each control is shown in all its forms: the [Showcase walkthrough](showcase.md) goes through them.

Every field is in the [config spec schema](../design/schema.md).

## Shipping It

The config spec ships in your mod's archive, beside the rest of your files:

```text
Data\
  F4SE\Plugins\MyMod.dll
  ModsConfigFO4\Config\MyMod\spec.json
  Textures\MyMod\banner.dds
```

- Mods Config FO4 VR is not a requirement of your mod: without it, `spec.json` is a file nobody reads.
- `minSchemaVersion` is `1`. From version 1.0 of Mods Config FO4 VR the schema only grows, so a config spec keeps working on every later version.
- Your settings file doesn't have to exist: the menu creates it on the first Save that changes one of its settings.
- Give your mod a `category`, as `"category": "Gameplay"`, so a long list shows it under that name. Use one of the twelve shipped names where one fits: [Category](../design/schema.md#category). An MCM config can have the same root field.

## Testing It in the Game

Turn on **Developer tools**: in the menu, pick **Mods Config FO4 VR**, then the **Developer** page.

- The menu's front page then has a **Developer** section, with each config spec's errors and warnings. Press your mod's row for its report: each problem, and where it is in `spec.json`. A press on a problem goes to its row.
- **Read the live copy**, in the report, copies your `spec.json` to `Documents\My Games\Fallout4VR\Mods_Config\ModsConfigFO4VR\Dev\<your mod>\spec.json` and reads that copy instead. It reads it again each time you save it, so you edit it on the desktop while the game runs, and see the change in the headset. **Edit on desktop** opens it in your editor. When it's right, copy it back into your mod.
- The log, `Documents\My Games\Fallout4VR\F4SE\ModsConfigFO4VR.log`, has every problem too.

The Showcase's **Mistakes** page has what a config spec can get wrong, each with the error the menu reports.

## Next

- **[The Showcase walkthrough](showcase.md):** each thing a config spec does, with the Showcase's JSON for it and a picture of the page it draws.
- **[Writing it with an AI agent](ai-agent.md):** a ready prompt for an agent that writes your config spec from your mod's files.
- **[MCM compatibility](mcm.md):** what your MCM menu gets as it is, what a config spec adds, and moving from MCM, or keeping both.
- **[The native API](native-api.md),** for DLL authors: what your DLL adds to its config spec, and how.
