# Mods Config FO4 VR documentation

A VR-first settings menu for Fallout 4 VR mods: every mod's settings in one place, changed in the headset. The menu shows the MCM menus you already have, and edits the config files of mods that ship a config spec for it.

Install and requirements: [main README](../README.md). Problems: [FAQ](faq.md). Release notes: [Changelog](changelog.md).

## Contents

- [Quick Start](#quick-start)
- [Editing a Mod's Settings](#editing-a-mods-settings)
- [Mod Categories](#mod-categories)
- [The Menu's Own Settings](#the-menus-own-settings)
- [For Players](#for-players)
- [For Mod Authors](#for-mod-authors)
- [Design](#design)

## Quick Start

- **Open / Close:** Hold both thumbsticks down to open or close the menu. This also restores a minimized menu.
- **Point and select:** Only one hand controls the menu at a time, indicated by the mark at the end of its ray. Use that hand's trigger to select and thumbstick to scroll. These controls are blocked from the game while pointing at the menu. Point away to use the trigger in-game without accidentally selecting anything. [Switching hands](faq.md#the-menu-itself).
- **Back:** Tap the pointing hand's grip to close a popup, cancel an edit, or go back one page. From Help, it returns to your previous page. From the front page, it closes the menu.
- **Move the menu:** Point at the menu and hold the grip to grab it. Move your hand to reposition it, then release.
- **Recenter:** Hold the offhand thumbstick down to bring the menu in front of you. The menu also returns automatically if you move too far away.
- **Navigate pages:** The breadcrumbs in the title bar show your current location. Select any earlier page to jump back, or **MODS CONFIG FO4** to return to the front page.
- **Minimize:** Select **–** in the title bar to hide the menu while inspecting changes in-game. Your current page and unsaved edits are preserved. Use **Restore** on your offhand wrist to bring it back and refresh values, or **Exit** to close the menu (with a warning if you have unsaved changes).
- **Help:** Select **? Help** in the title bar for instructions and controller mappings based on your configuration. The bottom of the menu also displays contextual help for whatever you're pointing at.

![The Help page](assets/help-page.jpg)

## Editing a Mod's Settings

Pick a mod in the list on the left. Its front page opens, and its pages are under it in the list.

- **Save and Discard:** a change of a mod with a config spec waits for **Save**, which writes it to the mod's file. **Discard** drops your changes. Leaving a mod or closing the menu with changes asks first.
- **Undo one change:** a changed value is amber, and the arrow after it takes back that change alone.
- **Reset:** sets the mod's default. The help line names it.
- **A value that isn't valid** is red, with the reason in the help line. Save waits until it's fixed.
- **When a change takes effect:** each setting says it. A live one shows in the game as you change it, before you Save, and Discard puts the old value back in the game too. Others take effect on Save, or after a restart, which the front page then reminds you of.
- **MCM mods** have the `MCM` tag in the list. As in MCM's own menu, a change is made in the game as you make it: **Keep** accepts it and **Revert** sets it back.

The menu does not pause the game, on purpose: live changes show in the world immediately, with the menu still open, so you can try another without closing it. Settings marked **Save** or **Restart** still wait for that step.

## Mod Categories

With 20 mods or more, the list is grouped by category: Gameplay, Interface, Settlements and so on, with **No category** last. A press on a category opens or closes it. A mod is in the category its author gave it, or the one the menu knows for it.

To move a mod, pick **Mods Config FO4 VR**, then the **Categories** page: each mod has a dropdown of the categories. **Type a new name**, last in the list, makes a category of your own. Save keeps your choices, and **Reset** puts a mod back in its own category. Change from how many mods the list is grouped with **Min mods to group by category** on the **Panel** page: 0 always groups it.

## The Menu's Own Settings

The menu is in its own list: pick **Mods Config FO4 VR**. Its changes apply as you make them, and Save keeps them.

| Page | What's there |
| --- | --- |
| **Panel** | How big the menu is and where it stands: its scale, its distance and how high it is, the background's opacity, its width and height, the mod list's width, the scroll speed, its tilt and curve, how far you walk before it comes back in front of you, how smoothly it follows your hand as you move it, and from how many mods the list is grouped by category. |
| **Controls** | The bindings that open the menu, go back, move it and bring it in front of you, each changed with its editor, and the haptic a press gives. |
| **Pointer** | The ray's color, width, length and fade, the mark where it meets the menu, and where the ray starts on your hand. |
| **Categories** | The category of each mod, with a dropdown per mod. See [Mod Categories](#mod-categories). |
| **Developer** | For a config spec's author: **Developer tools**, which lists each config spec's problems on the menu's front page, and the log's level. |

![The Controls page](assets/mods-config-controls-bindings.jpg)

They are kept in a file the running game reloads as you save it, so you can edit it on the desktop too:

- Config: `%USERPROFILE%\Documents\My Games\Fallout4VR\Mods_Config\ModsConfigFO4VR\ModsConfigFO4VR.toml`
- Your categories: `Categories.toml` beside it, one line per mod you moved.
- Log: `%USERPROFILE%\Documents\My Games\Fallout4VR\F4SE\ModsConfigFO4VR.log`

## For Players

- The in-game **Help** page, from **? Help** in the title bar.
- The [FAQ and Troubleshooting](faq.md).
- The [Changelog](changelog.md).
- [The menu's own settings](#the-menus-own-settings), and its config file and log.

## For Mod Authors

A config spec gives your mod a menu with no code: one `spec.json`, installed with your mod under `Data\ModsConfigFO4\Config\<your mod>\`, which reads and writes your INI or TOML files and your scripts' globals and properties. A DLL can also register with the native API, for live preview, buttons that run its functions, and its own UI opened from the menu.

Start at [For Mod Authors](for-mod-authors/README.md): which way fits your mod, your first config spec from the Showcase, how it ships, and how you test it in the game. An AI agent can write the config spec, with [the prompt](for-mod-authors/ai-agent.md) for it.

## Design

How the menu is designed and built, and the plan of its work: [docs/design](design/README.md).
