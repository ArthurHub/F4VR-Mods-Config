# Mods Config FO4 VR

## Language

**Transform**:
Position, rotation and uniform scale relative to an attachment.

**Grab handle**:
A temporary sphere for moving a transform by hand during live adjustment.

**Setting value rules**:
What a value of a setting type is: whether two values are the same, its canonical text, why it is invalid, how a config spec's JSON writes it, and whether the type is a choice or kept as a table. One entry point answers them by setting type (`model/SettingValues.h`).

**Mod list**:
The mods the menu lists, in order of their display names, with their categories, the groups of the tree, and the last read of each config spec's live copy. Owned by the panel (`ui/ModList.h`), which also refills the Categories page of the menu's own config spec from it.

**Runtime**:
One build of the game's executable: 1.10.163, 1.10.984 or 1.11.x of flat Fallout 4, or 1.2.72 of Fallout 4 VR.
_Avoid_: game version, edition (OG, NG, AE)

**Flat Fallout 4**:
`Fallout4.exe`, on any of its runtimes, as opposed to Fallout 4 VR.
_Avoid_: non-VR, flatscreen, pancake

**Flat build / VR build**:
The two DLLs of the mod made from one source: `ModsConfigFO4.dll` for flat Fallout 4, `ModsConfigFO4VR.dll` for Fallout 4 VR.

**Menu controller**:
The config menu's game-free decisions: what a press, a native client's request and a live copy's save do in the frame update, and in which order. It owns the stores and the panel, and asks the game side of everything else through its port, `MenuPort`, which `GameMenu` implements in the mod (`menu/MenuController.h`).
