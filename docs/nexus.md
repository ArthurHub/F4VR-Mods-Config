# Short description

A VR-first settings menu for Fallout 4 VR mods: every mod's settings in one place, changed in the headset. It stands in the world and never pauses the game, so you see and feel each change without closing it between tries. It shows the MCM menus you already have, and edits the settings of mods made for it.

# Full description

[size=3][b]Configure your Fallout 4 VR mods without leaving VR or pausing the game.[/b]

Mods Config FO4 VR puts your existing MCM menus and the settings of supported mods in one menu made for the headset. Change a setting, see the result in the game, and fine-tune it without closing and reopening the menu between tries.

Unlike MCM, the menu stands in the game world and never pauses the game. With native support for [b]FRIK (v0.80+)[/b] and [b]Immersive Flashlight VR (v1.4+)[/b], you can preview changes live, customize controller bindings, adjust positions in 3D, and more.[/size]

[center][img]https://raw.githubusercontent.com/ArthurHub/F4VR-Mods-Config/main/docs/assets/mods-config-front-page.jpg[/img][/center]

[size=4][b]Alpha Release[/b][/size]
[size=3]The menu is ready for testing, and I'd particularly appreciate feedback on how it feels to use in VR. Are the controls intuitive? Is the interface comfortable? Is anything confusing or awkward?

If you encounter any bugs, please report them on [url=https://github.com/ArthurHub/F4VR-Mods-Config/issues]GitHub[/url] or in the Bugs tab. Include your [b]ModsConfigFO4VR.log[/b] from [b]Documents\My Games\Fallout4VR\F4SE[/b].

Still planned: editing lists and hand poses, and translations for mods without a game plugin.[/size]

[size=4][b]Getting Started[/b][/size]
[list=1]
[*][size=3]Install the mod and its requirements.[/size]
[*][size=3]In VR, press and hold [b]both thumbsticks[/b] for one second to open the menu.[/size]
[*][size=3]Point at the menu with either hand and use the [b]trigger[/b] to interact. Use the thumbstick to scroll.[/size]
[*][size=3]Select a mod from the list and start configuring it.[/size]
[/list]
[size=3]The menu includes an in-game [b]? Help[/b] page showing your current controls. All bindings and the menu's appearance can be customized in [b]Mods Config FO4 VR[/b], found in the mod list.[/size]

[size=4][b]Installation[/b][/size]

[size=3][b]Requirements[/b][/size]
[list]
[*][size=3][url=https://f4se.silverlock.org/]F4SEVR[/url][/size]
[*][size=3][url=https://www.nexusmods.com/fallout4/mods/64879]VR Address Library[/url][/size]
[/list]

[size=3][b]Optional[/b][/size]
[list]
[*][size=3][url=https://www.nexusmods.com/fallout4/mods/21497?tab=files]MCM VR[/url] (under Optional Files) — to display your existing MCM menus.[/size]
[*][size=3][url=https://www.nexusmods.com/fallout4/mods/53464]FRIK[/url] — for fingertip interaction with wrist buttons and integration with FRIK's configuration interface.[/size]
[/list]

[size=3][b]Installation steps[/b][/size]
[list=1]
[*][size=3]Install using your mod manager, or extract directly into the Data folder.[/size]
[*][size=3]No ESP file; load order does not matter.[/size]
[*][size=3]If you use MCM VR, keep it installed. Mods Config displays its menus; it does not replace MCM.[/size]
[/list]
[size=3]Mods Config also works without MCM VR. Only MCM-based menus will be unavailable.[/size]

[size=4][b]Features[/b][/size]

[size=3][b]Manage your mods[/b][/size]
[list]
[*][size=3][b]Existing MCM menus:[/b] Displays your MCM menus with their pages, images, and translations. Buttons and presets run their original scripts, hotkeys are shown, and changes work as they do in MCM. Use Keep or Revert to accept or undo changes.[/size]
[*][size=3][b]Native mod configuration:[/b] Mods with a config spec can expose their INI, TOML, and script settings directly in VR. Config files retain their comments and layout when saved.[/size]
[*][size=3][b]Search:[/b] Type a name to find a setting in any mod. The list shows the mods and pages that match, and a page shows only its settings that match.[/size]
[*][size=3][b]Categories:[/b] A long mod list is grouped by category. Move a mod to another category, or type a new one, on the Categories page.[/size]
[*][size=3][b]Quick launch:[/b] Open a mod's own configuration interface, such as FRIK's, from the menu. Mods Config returns when that interface closes.[/size]
[*][size=3][b]Notices:[/b] A mod that can't be shown says why, such as a plugin it needs that isn't loaded, and a saved change that needs a restart tells you so.[/size]
[/list]

[size=3][b]Built for VR[/b][/size]
[list]
[*][size=3][b]World-space menu:[/b] A curved panel that stays in the game world without pausing the game. Move it, minimize it, or bring it back in front of you.[/size]
[*][size=3][b]VR-friendly controls:[/b] Sliders, switches, dropdowns, steppers, and buttons designed for controller pointing.[/size]
[*][size=3][b]Color picker:[/b] Choose colors directly, use basic colors, or select a mod's presets.[/size]
[*][size=3][b]Controller bindings:[/b] See bindings as button icons and readable descriptions, with an editor to change them.[/size]
[*][size=3][b]Haptic patterns:[/b] Preview each haptic pattern using its Play button.[/size]
[*][size=3][b]3D adjustments:[/b] Configure positions and rotations using sliders for each axis, or adjust them directly in the world from your wrist.[/size]
[*][size=3][b]Text input:[/b] Type on SteamVR's keyboard or a physical one. This includes MCM text fields, which MCM itself can't edit in VR.[/size]
[*][size=3][b]Customizable appearance:[/b] Adjust panel size, distance, height, tilt, curvature, opacity, and pointer appearance.[/size]
[*][size=3][b]FRIK integration:[/b] Use fingertip interaction for wrist controls when FRIK is installed. The menu also works without FRIK.[/size]
[/list]

[size=3][b]Live preview and safe editing[/b][/size]
[list]
[*][size=3][b]Live preview:[/b] Supported mods show changes in the game immediately, before you save. Minimize the menu to inspect the result.[/size]
[*][size=3][b]Save and Discard:[/b] For mods with a config spec, file changes are not written until Save. Discard restores the previous settings, including live-previewed values.[/size]
[*][size=3][b]Undo and Reset:[/b] Undo an individual setting or restore its default value.[/size]
[*][size=3][b]Validation:[/b] Invalid values are highlighted in red with an explanation. Save is disabled until they are fixed.[/size]
[*][size=3][b]Change notifications:[/b] Each setting indicates whether it takes effect immediately, on Save, or after a restart.[/size]
[*][size=3][b]Unsaved changes:[/b] Leaving a mod or closing the menu with pending changes asks what to do first.[/size]
[/list]

[size=4][b]Controls Reference[/b][/size]
[list]
[*][size=3][b]Open / Close:[/b] Press and hold both thumbsticks for one second. With FRIK installed, this takes over FRIK's configuration gesture.[/size]
[*][size=3][b]Point and select:[/b] Only one hand controls the menu at a time, indicated by the mark at the end of its ray. Use that hand's trigger to select and thumbstick to scroll. These controls are blocked from the game while pointing at the menu. Point away to use the trigger in-game without accidentally selecting anything.[/size]
[*][size=3][b]Back:[/b] Tap the pointing hand's grip to close a popup, cancel an edit, or go back one page. From Help, it returns to your previous page. From the front page, it closes the menu.[/size]
[*][size=3][b]Move:[/b] Point at the menu and hold the grip to grab it. Move your hand to reposition it, then release.[/size]
[*][size=3][b]Recenter:[/b] Press and hold the offhand thumbstick to bring the menu in front of you. It also returns automatically if you walk far enough away.[/size]
[*][size=3][b]Navigate pages:[/b] The breadcrumbs in the title bar show your current location. Select any earlier page to jump back, or [b]MODS CONFIG FO4[/b] to return to the front page.[/size]
[*][size=3][b]Minimize:[/b] Select [b]–[/b] in the title bar to hide the menu while inspecting changes in-game. Your current page and unsaved edits are preserved. Use Restore on your offhand wrist to bring it back and refresh values, or Exit to close the menu (with a warning if you have unsaved changes).[/size]
[*][size=3][b]Help:[/b] Select ? Help in the title bar for the controls matching your configuration. Contextual help appears at the bottom as you point at settings.[/size]
[/list]
[size=3]All controls are customizable under [b]Mods Config FO4 VR[/b] in the mod list.[/size]

[size=4][b]Mods with Native Configuration Support[/b][/size]
[list]
[*][size=3][url=https://www.nexusmods.com/fallout4/mods/53464]FRIK[/url] — v0.80+[/size]
[*][size=3][url=https://www.nexusmods.com/fallout4/mods/99342]Immersive Flashlight VR[/url] — v1.4+[/size]
[/list]
[size=3]Both show their settings in the menu with live preview, their controller bindings in the binding editor, and their positions adjustable in the world.

Other mods show up through their existing MCM menus, or through a config spec their author adds.[/size]

[size=4][b]For Mod Authors[/b][/size]
[size=3]Mods Config FO4 VR is designed to make VR-native configuration accessible to other mod developers.

There are several integration options:[/size]
[list]
[*][size=3][b]Config spec (no code):[/b] Add a JSON file to your mod to expose INI, TOML, and Papyrus global or property settings through the menu.[/size]
[*][size=3][b]F4VR CommonFramework:[/b] Mods using CommonFramework can get live preview support without additional code.[/size]
[*][size=3][b]Native API:[/b] A DLL can register for live previews, buttons that execute native functions, and custom configuration interfaces.[/size]
[*][size=3][b]MCM compatibility:[/b] Existing MCM menus work as they are. A config spec can also replace an MCM menu with more advanced controls.[/size]
[*][size=3][b]Showcase:[/b] The optional Showcase download includes an MCM menu and a config spec demonstrating the available features. Use it as a starting point.[/size]
[*][size=3][b]AI agent prompt:[/b] A [url=https://github.com/ArthurHub/F4VR-Mods-Config/blob/main/docs/for-mod-authors/ai-agent.md]ready prompt[/url] for an AI agent that writes your mod's config spec from its files.[/size]
[/list]
[size=3]See the [url=https://github.com/ArthurHub/F4VR-Mods-Config/blob/main/docs/for-mod-authors/README.md]Mod Author Documentation[/url] for implementation details.[/size]

[size=4][b]Configuration File[/b][/size]
[size=3]The menu's own settings are stored in [b]ModsConfigFO4VR.toml[/b]. The game reloads this file when it is saved, and you can edit these settings directly through Mods Config.

Location:[/size]
[code]%USERPROFILE%\Documents\My Games\Fallout4VR\Mods_Config\ModsConfigFO4VR\ModsConfigFO4VR.toml[/code]

[size=4][b]Documentation[/b][/size]
[list]
[*][size=3][url=https://github.com/ArthurHub/F4VR-Mods-Config/blob/main/docs/README.md]Documentation and User Guide[/url][/size]
[*][size=3][url=https://github.com/ArthurHub/F4VR-Mods-Config/blob/main/docs/faq.md]FAQ and Troubleshooting[/url][/size]
[*][size=3][url=https://github.com/ArthurHub/F4VR-Mods-Config/blob/main/docs/changelog.md]Changelog[/url][/size]
[*][size=3][url=https://github.com/ArthurHub/F4VR-Mods-Config/blob/main/docs/for-mod-authors/README.md]Mod Author Documentation[/url][/size]
[/list]

[size=4][b]Source Code[/b][/size]
[size=3][url=https://github.com/ArthurHub/F4VR-Mods-Config]GitHub — ArthurHub/F4VR-Mods-Config[/url] (GPL-3.0)[/size]

[size=4][b]Support[/b][/size]
[size=3]If you like my work, consider supporting my mod development on [url=https://patreon.com/theartofdev]Patreon[/url].[/size]

[size=4][b]Credits / Thanks[/b][/size]
[size=3]This mod builds on the work of the Fallout VR modding community and public code.[/size]
[list]
[*][size=3]Ryan, rsm, McKenzie, alandtse, devartificial, and other [url=https://github.com/alandtse/CommonLibVR/tree/vr]CommonLibVR[/url] contributors.[/size]
[*][size=3]Neanka, reg2k, and the [url=https://www.nexusmods.com/fallout4/mods/21497]Mod Configuration Menu[/url] team, whose menus this mod displays and whose design it follows.[/size]
[*][size=3]Omar Cornut and the contributors of [url=https://github.com/ocornut/imgui]Dear ImGui[/url], which draws the menu.[/size]
[*][size=3]RollingRock, alandtse, shizof, and CylonSurfer for FRIK, whose configuration gesture the menu uses.[/size]
[*][size=3]alandtse and RollingRock for [url=https://github.com/ArthurHub/devbench]devbench[/url], used to test the menu in the running game.[/size]
[/list]
