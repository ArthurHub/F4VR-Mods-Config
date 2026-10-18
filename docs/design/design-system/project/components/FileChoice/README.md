# FileChoice

A choice of the files in a folder, MCM's `dropdownFiles`, and a package's: a dropdown whose options are the files that match a mask, with a thumbnail when they are textures, as Immersive Flashlight's gobos.

**Provides:** the folder (`path`, under `Data`), the `mask` (`*.dds`), the value, its default and its state, and the files the folder has when the page is shown.

## The row

- A dropdown's frame, 392 x 48: from the left, a texture's thumbnail, the file's name in `t-body` with its extension in `green-muted`, and the drawn down arrow. Then the Reset slot.
- **The thumbnail** is the texture itself, 32 x 32 on black, outlined in `frame-border`: a gobo shows how the light falls through it. A file that is not a texture (`.json`, `.nif`) has none, and its name starts at the left.
- A press anywhere on the row opens the list.
- Pending: the name in `amber`, the dot in the gutter, the thumbnail of the new file.
- **Not in the folder:** a value that names a file the folder does not have. The frame has a `red` outline and the name as the file has it in `red`, with no thumbnail, and the row's message is "Not in Gobos", the folder's last name. Reset sets the default; picking a file fixes it.
- **An empty folder**, or one that is missing: the row is disabled and says `No files`. The help line says where it looked: "No *.dds files in Textures\ImmersiveFlashlightVR\Gobos."

## The list

- A dropdown's popup ([Dropdown](../Dropdown/README.md)), 392 wide, under the row or over it, scrolling inside past five files, as a dropdown's does.
- At its top, in `t-detail` `green-muted` over a `rule` line: the folder on the left, and how many files on the right ("7 files"). It is not pressed.
- A line for each file, alphabetical, case ignored: the check of the current one, its thumbnail at 40 x 40, its name with its extension muted. The current line is solid `green`, its extension in `on-green` at 60%.
- While the ray is on a line, the help line has its name with its extension, and the folder.
- A pick sets the pending value and closes the list, as a dropdown's. A `live` setting shows it at once: a gobo on the light.

## The value written

- The pick is written in the form the value had: Immersive Flashlight's `'data\Textures\ImmersiveFlashlightVR\Gobos\6_Incandescent.DDS'` keeps its `data\Textures\…\` and its backslashes, with the new name, and a value that was a bare name stays a bare name. A file with no value yet takes the default's form.
- The name keeps the case the folder has.

## Thumbnails

They are the folder's textures, loaded when the list first opens and kept while the page is shown. One that cannot be loaded has an empty black square. A folder of many large textures loads them as their lines show, so the list opens at once.

The behavior is in [MCM support](../../../../mcm.md) (`dropdownFiles`), and M76 in the [Mod work plan](../../../../mod-plan-phase-2.md).
