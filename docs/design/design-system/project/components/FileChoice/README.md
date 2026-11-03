# FileChoice

A choice of the files in a folder, MCM's `dropdownFiles`, and a config spec's `listFromFolder`: a dropdown whose options are the files that match a mask, as Immersive Flashlight's gobos.

**Provides:** the folder (`path`, from its root), the `mask` (`*.dds`), the text of the option that is no file (`noneText`), the value, its default and its state, and the files the folder has.

## The row

- A dropdown's frame, 392 x 48: from the left, the file's name in `t-body`, and the drawn down arrow. Then the Reset slot.
- **The name:** with no extension where the mask names one, as `*.dds`, which every file has. With any other mask, or none, its extension follows it in `green-muted`.
- A press anywhere on the row opens the list.
- Pending: the name in `amber`, the pending mark after the row's text.
- **No file:** the option that is no file shows its text, as `No gobo`.
- **Not in the folder:** a value of another folder, or of a file the folder does not have. The frame has a `red` outline and the file's name as the value has it in `red`, and the help line says where it points and where the folder is: "Invalid: Main.sWeapon is "data\Textures\Effects\Gobos\9_Laser.dds" in ImmersiveFlashlightVR.toml, which is not in Textures\ImmersiveFlashlightVR\Gobos." Reset sets the default; picking a file fixes it.
- **An empty folder**, or one that is missing: the row is disabled and says `No files`. The help line says where it looked: "Not available: no *.dds files in Textures\ImmersiveFlashlightVR\Gobos." A choice with an option that is no file has that option alone, and is not disabled.

## The list

- A dropdown's popup ([Dropdown](../Dropdown/README.md)), 392 wide, under the row or over it, scrolling inside past five files, as a dropdown's does.
- At its top, in `t-detail` `green-muted` over a `rule` line: the folder on the left, and how many files on the right ("7 files"). It is not pressed, and the files scroll under it.
- The option that is no file first, then a line for each file, alphabetical, case ignored: the check of the current one, and its name as the row shows it. The current line is solid `green`, a muted extension in `on-green` at 60%.
- While the ray is on a file, the help line has its name with its extension, and the folder: "2_Round_Rings.DDS: In Textures\ImmersiveFlashlightVR\Gobos."
- A pick sets the pending value and closes the list, as a dropdown's. A `live` setting shows it at once: a gobo on the light.

## The value written

- A config spec's pick is written in the form of the setting's default: Immersive Flashlight's `'data\Textures\ImmersiveFlashlightVR\Gobos\6_Incandescent.DDS'` gives every file its `data\Textures\…\` and its backslashes, with the file's name. A bare default gives bare names.
- The name keeps the case the folder has.
- A value that names a file in another way, as with `/`, another case or no `data\`, is that file, and stays as it is written until the player picks one.
- An MCM `dropdownFiles` writes the file's name, or its place, as its storage keeps it.

There are no thumbnails: a texture's setting is `live`, so a pick shows on the light.

The behavior is in [Options from a folder](../../../../schema.md#options-from-a-folder) and [MCM support](../../../../mcm.md) (`dropdownFiles`), and M76 in the [Mod work plan](../../../../mod-plan-phase-2.md).
