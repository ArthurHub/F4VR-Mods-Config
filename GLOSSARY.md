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
