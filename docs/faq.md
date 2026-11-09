# FAQ and Troubleshooting

Common questions and fixes. How the menu works: [documentation](README.md). Install and requirements: [main README](../README.md). Click a question to expand it.

## Contents

- [Getting Started](#getting-started)
- [Changes](#changes)
- [MCM Mods](#mcm-mods)
- [The Menu Itself](#the-menu-itself)
- [For Mod Authors](#for-mod-authors)

## Getting Started

<details>
  <summary>The menu doesn't open</summary>
&#10240;

Press in **both** thumbsticks and hold them for a second. If nothing happens, check that the mod is loaded: its log, `%USERPROFILE%\Documents\My Games\Fallout4VR\F4SE\ModsConfigFO4VR.log`, is written each time the game starts. No log means F4SE VR didn't load the plugin: check [F4SE VR](https://f4se.silverlock.org/) and the [VR Address Library](https://www.nexusmods.com/fallout4/mods/64879).

The gesture is `kOpenMenuBinding` in the menu's config file, in case it was changed there.

</details>

<details>
  <summary>FRIK's config doesn't open with the thumbsticks anymore</summary>
&#10240;

This menu takes over that gesture. Open FRIK's config from the menu's front page: **Quick launch**, **FRIK Configuration**, **Open**. The menu comes back when FRIK's config closes. FRIK's settings are also in the menu itself, under FRIK, with FRIK v0.80+.

</details>

<details>
  <summary>Do I need FRIK?</summary>
&#10240;

No. With FRIK, you press the wrist buttons with your fingertip and your hand points while it works the menu. Without it, the controller presses them.

</details>

<details>
  <summary>Do I need MCM?</summary>
&#10240;

Only for MCM mods. Without MCM VR the menu works, and shows only the mods that have a config spec. Keep MCM installed if you have MCM mods: this menu shows their MCM menus, and MCM still stores their settings and runs their hotkeys.

</details>

<details>
  <summary>A mod is greyed out in the list</summary>
&#10240;

The menu found it and can't show it. Point at it, and the help line says why, for example that a plugin it needs isn't loaded. The menu's front page lists every such mod under **Notices**, with the same reason.

</details>

<details>
  <summary>A mod I have isn't in the list at all</summary>
&#10240;

The menu lists the mods that have an MCM menu or a config spec. A mod with an MCM menu needs MCM VR installed. A mod with neither has no settings for the menu to show.

</details>

## Changes

<details>
  <summary>My changes are gone</summary>
&#10240;

A change of a mod with a config spec waits for **Save**. Discard, or leaving without saving, drops it, and the menu asks before you leave with changes. A mod with the `MCM` tag makes each change at once, as MCM does.

</details>

<details>
  <summary>I saved, but the mod doesn't act on it</summary>
&#10240;

Each setting says when it takes effect. One that needs a restart is listed on the menu's front page, under **Notices**, until you restart the game. A setting that says nothing about it is up to the mod: an MCM mod's settings never say, so check the mod's own page.

</details>

<details>
  <summary>What is "live"?</summary>
&#10240;

The mod applies the change as you make it, so you see it in the game before you Save. Discard, or leaving without saving, puts the old value back in the game too. Minimize the menu to look at it, and Restore it from your wrist.

</details>

<details>
  <summary>Why is Save greyed out?</summary>
&#10240;

Either nothing changed, or a value isn't valid. An invalid value is red, and the help line says why while you point at it. Fix it, or take it back with its undo arrow, and Save is available again.

</details>

<details>
  <summary>Does the game pause while the menu is open?</summary>
&#10240;

No. That's on purpose: you see a change in the world while the menu is open, and try another without closing it.

</details>

## MCM Mods

<details>
  <summary>Why do MCM mods have Keep and Revert instead of Save and Discard?</summary>
&#10240;

An MCM mod's change is made in the game as you make it, as MCM's own menu makes it, so scripts react as they always have. There is nothing left to save: **Keep** accepts your changes, and **Revert** sets them back, each as a change is made.

</details>

<details>
  <summary>How do I change an MCM mod's hotkey?</summary>
&#10240;

In MCM's own menu. The menu shows a mod's hotkeys, as MCM keeps them, but MCM binds and runs them.

</details>

<details>
  <summary>An MCM button asked nothing and ran at once</summary>
&#10240;

That's right for an MCM mod: its changes are made already, so its script reads the values you see. A button of a mod with a config spec that runs a script asks you to save first, as the script reads the saved values.

</details>

<details>
  <summary>A setting's help says its default twice</summary>
&#10240;

The menu adds the default to every setting's help, and some MCM mods write it in their own text too.

</details>

## The Menu Itself

<details>
  <summary>The menu is too small, too big, or too far away</summary>
&#10240;

Pick **Mods Config FO4 VR** in the list, then the **Panel** page: its scale, its size, its distance and height, its tilt and curve, and how much the background hides the world. Changes show as you make them.

</details>

<details>
  <summary>The menu is inside a wall, or behind me</summary>
&#10240;

Press in the offhand thumbstick and hold it to bring the menu in front of you. Walking away from it does the same by itself, and **Recenter distance** on the Panel page sets how far.

</details>

<details>
  <summary>The keyboard doesn't show when I edit a text</summary>
&#10240;

The menu uses SteamVR's keyboard. OpenComposite has none, so type with a real keyboard, or a keyboard overlay that types into the game's window.

</details>

<details>
  <summary>Back closes the menu when I didn't mean to</summary>
&#10240;

Back is a tap of the grip, and on the menu's front page it closes the menu. A longer hold of the grip moves the menu instead. Both are bindings on the **Controls** page of the menu's own settings, as is the opening gesture.

</details>

## For Mod Authors

<details>
  <summary>How do I give my mod a menu?</summary>
&#10240;

With a config spec: a `spec.json` installed with your mod under `Data\ModsConfigFO4\Config\<your mod>\`. It needs no code. Start from the Showcase download's config spec. See [For Mod Authors](for-mod-authors/README.md).

</details>

<details>
  <summary>My mod has an MCM menu. Do I have to change anything?</summary>
&#10240;

No: it's shown as it is. A config spec whose `modName` is your MCM config's `modName` replaces it in this menu, and MCM's own menu keeps the MCM config.

</details>

<details>
  <summary>My config spec doesn't show, or shows wrong</summary>
&#10240;

Turn on **Developer tools** on the Developer page of the menu's own settings. The menu's front page then lists each config spec's errors and warnings, and a press shows where each is. **Read the live copy** reads a copy of your `spec.json` that reloads as you save it, so you fix it while the game runs. The log has every problem too.

</details>
