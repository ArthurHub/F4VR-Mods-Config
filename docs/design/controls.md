# Controls and Setting Types

This document defines the initial control vocabulary and interaction rules. The order the controls are built in is in the [Mod work plan](mod-plan.md).

## Separation of concerns

Every setting has four distinct concepts:

- **Type**: the value stored by the target mod. A config spec's `valueType`.
- **Control**: how Mods Config FO4 VR presents and edits that value. A config spec's `type`, as in an MCM config.
- **Storage**: where the value is read from and written to. A config spec's `storage`.
- **Application policy**: when a change takes effect in the running mod: as it is made, after Save, after a restart, or not known. A config spec's `applies`.

Keeping these separate prevents the file format, widget, and runtime behavior from becoming unnecessarily coupled.

## Core stored types

The declarative model should support these scalar types:

- `bool`
- `int`
- `float`
- `string`
- `color`

`int` is a signed 32-bit integer and `float` is a 32-bit float.

A choice does not need a separate storage type. Its options may store typed boolean, integer, floating-point, or string values.

`vec2` and `vec3` are intentionally excluded for now. Complex CommonFramework values are represented by dedicated controls rather than generic vectors.

## Core interactive controls

### Switcher

Primary use: boolean settings.

- The entire row is a large target.
- The current state is shown in both shape/text and color.
- Trigger toggles the value.
- Reset restores the declared default.

### Slider

Primary use: bounded integer and floating-point settings.

The row:

```text
[◂][=============o-------][▸]  [ 1.25 ]  [ Reset ]
```

- Trigger-drag gives continuous preview when the setting supports it.
- Releasing the trigger ends the control interaction; it does not Save the page.
- Back during a drag restores the value from the beginning of that drag.
- Thumbstick left/right changes the slider the ray is on by the declared step, once the ray is on its row for a short time. A slider is not selected first.
- Holding the thumbstick may accelerate after an initial repeat delay.
- A thin arrow at each end of the track steps it by the declared step too, so a step is seen and pressed without the thumbstick, and holding it repeats as the thumbstick does. The arrow of the end the value is at is disabled.
- The value box only shows the value: it is not typed.
- The value has the decimals, the unit or the percentage of its `display`, and stays visible. A value too wide for its box is drawn smaller, and then without its unit, never shortened; see the design system's [NumberDisplay](design-system/project/components/NumberDisplay/README.md).
- Initial schema support is linear. Logarithmic or curved mappings can be considered later.
- Haptics should mark meaningful interaction boundaries, not fire on every tiny value change.

### Dropdown and stepper

Primary use: one value selected from a finite set.

They are two types, as in MCM:

- `dropdown`: a popup list
- `stepper`: previous and next


Choice behavior:

- Each option has a typed stored value, a text, and an optional short text and help.
- The current option has a strong visual marker.
- Popup rows use large VR targets.
- Trigger selects; thumbstick scrolls; Back closes without changing the selection. So does a press beside the list, which does nothing else.
- A press anywhere on the row of a dropdown opens its list.
- The buttons of a stepper stop at the first and the last option. A press on the rest of its row goes to the option after, and from the last to the first.
- Thumbstick left/right steps the stepper the ray is on, one option for a push.
- The help line may show the pointed option's help before it is selected.

### Choice of files

Primary use: a file of a folder, MCM's `dropdownFiles`, as Immersive Flashlight's gobo textures.

- A dropdown whose options are the files of a folder under `Data` that match a mask, alphabetical, read when the page is shown. A texture has its thumbnail in the row and in the list.
- The pick is written in the form the value had: a path keeps its folder and its separators, and a bare name stays a bare name.
- A value that names a file the folder does not have is invalid. A folder with no file that matches disables the row, and the help line says where it looked.

The look is the design system's [FileChoice](design-system/project/components/FileChoice/README.md).

### Text input

Primary use: short strings, and numbers that are typed. A `textinput` whose `valueType` is int or float takes a number, as MCM's `textinputInt` and `textinputFloat` do, and is typed on the same keyboard. Its limits are the `min` and `max` of its `valueOptions`.

The keyboard is SteamVR's, which the game types its own names with, and it is shown while the field is edited. A physical keyboard types into the field too, and so does a keyboard overlay that types into the game's window. A runtime with no keyboard, as OpenComposite, has those only. The design is the framework's [VR keyboard](../../external/F4VR-CommonFramework/docs/tech/vr-keyboard.md).

- `Done` validates and commits the edit to the pending setting value.
- `Cancel`, the keyboard's close button, restores the value from before text editing began.
- Invalid temporary text is not sent as a preview; the last valid preview remains active.

Multiline text, password fields, text selection tools, and speech input are not part of the initial design.

The look is the design system's [TextInput](design-system/project/components/TextInput/README.md): the field shows the text and a pencil, and while it is typed a caret and the count of characters.

### Color

Primary use: a CommonFramework color, `{ rgb = [r, g, b], alpha = a }`, as a light's or a pointer's. MCM's menu has no color control.

- The row shows the color as a swatch, with its alpha over a checkerboard, and as its numbers: "190, 255, 190, alpha 200". A press on the row opens the editor.
- The editor is a popup in the content pane: a picker for the hue, saturation and brightness, dragged with the trigger, and a slider for each of red, green, blue and alpha. The thumbstick steps the slider the ray is on by 1, which is how a color is set exactly.
- It shows the color it had when it opened beside the color as it is, and a press on the first goes back to it.
- Each change sets the pending value as it is made, and a `live` color is previewed as it is dragged. Done, or a press beside the editor, keeps it. Cancel and Back give back the color it had when it opened.
- The value shown is the one the framework reads: the keys the value has, over the default's channels. A changed value is written as whole numbers, with the keys it had and the key of the part that changed, so `{ alpha = 60 }` with a new hue is `{ rgb = [255, 190, 120], alpha = 60 }`.
- A value the framework cannot read is invalid, shown as the file has it, with the framework's reason in the help line.
- A color whose opacity means nothing, as a light's, is RGB only: `"valueOptions": { "alpha": false }`. Its swatch is opaque, its numbers have no alpha, its editor has no Alpha slider, and only `rgb` is written. An `alpha` the file has is kept as it is.

The look is the design system's [Color](design-system/project/components/Color/README.md).

### Button

Primary use: an operation that does not directly edit a stored value.

- A button runs what an MCM button runs, such as a function of a script, a console command, or a function that the mod's native client registered, by its name and with typed parameters if needed.
- Its button says `Run`, or the button text its config spec gives it.
- A config spec can give it a `style`: `important`, its outline, text and icon in the accent color, so a button that matters is not missed, or `dangerous`, in red, which always asks first. It can give it an `icon` too, one of the menu's drawn icons, before its text; see [Buttons](schema.md#buttons).
- Potentially destructive actions require explicit confirmation. A config spec's `confirmation` opens a dialog first, and a `dangerous` one is red.
- While its action runs, the button says `Running` and is not pressed again. Its result, when the action gives one, is in the help line while the ray is on the row, with a mark after its name: an info mark until the player has read it, and the invalid mark while it failed.
- Cancelling an action that runs, and its progress, remain open design work.

The look is the design system's [ButtonRow](design-system/project/components/ButtonRow/README.md).

### Read-only

Primary use: a value the mod writes for the player to read, as a measured height.

- A setting that is read-only shows its value in its control, and its whole row is disabled, as an MCM hotkey's is: a press does nothing, and the row has no look of one.
- The help line says first that it is read-only: `(Read-only)`.
- It has no Reset.

The look is the design system's [ReadOnly](design-system/project/components/ReadOnly/README.md).

## Layout and informational elements

These elements do not directly store values:

- Section heading, with the rows of its section
- Explanatory text
- Spacer
- Image or preview asset

A config spec's section can hold sections, and can fold. A press on the heading of a section that folds hides its rows and shows them again, and its heading has a mark of which it is. A folded section with an invalid value or an unsaved change inside it marks its heading too, with how many of each. A section in a section is set in with its rows, with a line from under its heading down to its last row. See [Sections](schema.md#sections), and the design system's [SectionHeader](design-system/project/components/SectionHeader/README.md) for the look.

Detailed settings are a folded section right after the settings they belong to, so they do not overwhelm ordinary players.

A text, a spacer and an image are drawn as the design system's [TextRows](design-system/project/components/TextRows/README.md): a text in the muted color with MCM's tags, a spacer of one row or more, and an image at its size, scaled down to the rows' width when wider.

## CommonFramework-specific controls

The following exact CommonFramework concepts are reserved as first-class controls:

- `Haptic`
- `Color`
- `InputBinding`
- `List`
- `HandPose`
- `Transform`
- `ActivationSphere`

The framework's TOML config already fixes how each is stored, and the type prefix of a key says which it is:

| Control            | Key prefix | Stored as                                                                                                                                                                                                                                       |
| ------------------ | ---------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Haptic`           | `s`        | A string: the name of a pattern of the framework's library, or `"none"`. `sEntryHaptic = "Tick"`                                                                                                                                               |
| `Color`            | `c`        | An inline table, `{ rgb = [r, g, b], alpha = a }`, each a whole number in 0..255. Either key can be left out and keeps the default's channels, so `{ alpha = 60 }` changes only the opacity                                                   |
| `InputBinding`     | `k`        | An inline table, `{ hand = "offhand", type = "tap", button = "grip" }`, with `duration`, `suppress` and `modifier = { button, hand }` optional. `{ hand, type = "axis", direction, axis, threshold }` for a thumbstick or an axis, and `{ type = "none" }` for none |
| `List`             | `a`        | An array of strings, `["AddOnNode", "LightAttachNode"]`. `aHeadLightDenyList = ["0316D4\|Fallout4.esm"]`                                                                                                                                      |
| `HandPose`         | `h`        | An inline table, `{ thumb = [prox, mid, dist, splay], index, middle, ring, pinky, palm = [pitch, yaw] }`, each key optional and a finger or the palm left out being zeros                                                                    |
| `Transform`        | `t`        | An inline table, `{ pos = [x, y, z], rot = [heading, roll, attitude], scale = s }`, the rotation in degrees, each key optional                                                                                                                 |
| `ActivationSphere` |            | A whole section: the zone `tZone` and an optional power-armor zone `tZonePA`, `kPrimaryBinding` and `kSecondaryBinding`, `sEntryHaptic`, `sPrimaryHaptic` and `sSecondaryHaptic`, and the keys of its sphere and its icon                  |

A table is one value in one key. In the settings model its value is a string, the TOML text of the table (`{ rgb = [255, 51, 51], alpha = 40 }`), which its control reads and writes, and which the store writes into the file as the inline table, not as a quoted string. So its setting is dirty, previewed and saved as a whole, as any other. The framework checks every key and word of a table and names the one that is wrong, and a table it cannot read is an invalid value with that reason.

The Haptic, InputBinding, List, HandPose, Transform and ActivationSphere controls are designed below, and Color above. [Open questions](open-questions.md#commonframework-specific-controls) lists what is left of them.

These should not be flattened into `vec2` or `vec3`, because their meaning, validation, and editing affordances are richer than a tuple of numbers.

### Haptic

Primary use: the pattern a mod plays on a controller when something happens, as an activation sphere's `sEntryHaptic`, or the menu's own `sPressHaptic`.

- The value is one of None and the 18 patterns of the framework's library (`vrcf::HapticPattern`), in the library's order. The menu knows the list, with a text and a help for each; a config spec names only the setting's key and its default.
- It is a dropdown. Each option shows its pattern's shape, drawn from the framework's keyframes (`VRControllersHaptic::getPattern`), so patterns can be told apart before one is felt.
- A pattern is felt and not seen, so the control plays it on the hand that points: once when an option is picked, at each step of the stepper, and when the Play button beside the control is pressed. A press that plays a pattern gets no press pulse of the menu: the pattern is its feedback.
- Pointing at an option plays nothing, as hover has no haptic. The help line shows the option's help: how it feels, its length, and what it is for.
- Reset sets the default and does not play it. Play plays it.
- A text in the file is matched as the framework reads it: case-insensitive, with spaces, underscores and hyphens ignored, so `double click` is Double click, and `off` and an empty text are None. A changed value is written as the library names it, `DoubleClick`, or `none`.
- A name the library does not have is invalid, shown as the file has it. The framework plays nothing for it, and the help says so.
- A `live` haptic is previewed as any setting: the mod plays the pending pattern when its event happens.

The look is the design system's [Haptic](design-system/project/components/Haptic/README.md).

### InputBinding

Primary use: a controller binding of a mod, as an activation sphere's `kPrimaryBinding`, or the menu's own `kOpenMenuBinding`.

- A binding is shown as an icon of its button and as one sentence, "Long press the X button on the off hand", with the hand as the config names it and the button as the controller prints it. Every binding has the framework's icon of the button and how it is pressed: the button alone for a tap or a press, with a mark for the rest (a clock for a long press, an arrow down onto a bar for a hold, 2× for a double press, an arrow up off a bar for a release, ripples for a touch), and the thumbstick with its direction's arrow for a push. A chord shows both buttons' icons, and only the system button has none.
- A press on the row opens the editor, a popup in the content pane. It builds the binding from its parts: Hand, with None first for a binding that is off, Button, How (the type, of those the button has), Direction for a push of the thumbstick, Duration or Threshold, While holding (the modifier), and Suppress from the game (`suppress`) where the setting offers it. A part the chosen type does not have is disabled and keeps its place, so nothing moves as the type changes.
- A setting can take some hands, ways and buttons alone, with `hands`, `types` and `buttons` in its `valueOptions`: the editor offers those, and a binding of another is invalid, with what the setting takes. It keeps a binding from what does not work where it is used, as a Back on the trigger, which presses the menu's rows.
- `suppress` is offered only by a setting with `"valueOptions": { "suppress": true }`, as an activation sphere's two bindings: suppression means something only where a consumer applies it. Elsewhere the part is not there, and a `suppress` the file has is kept as it is.
- Each change sets the pending value as it is made, and a `live` binding is previewed. Done, or a press beside the editor, keeps it. Cancel and Back give back the binding it had when it opened.
- A key the type does not have is not written, and a Duration or Threshold at the type's default is left out, so the table stays as short as the framework's own examples.
- A table the framework cannot read is invalid, shown as the file has it, with the framework's reason in the help line.
- Another binding setting of the same mod that is the same press is named in the help line. It does not block Save. Bindings of two mods are not checked against each other: FRIK, Immersive Flashlight and this menu each use the thumbsticks in their own context.

The look is the design system's [InputBinding](design-system/project/components/InputBinding/README.md).

### List

Primary use: a list of texts a mod reads, as Immersive Flashlight's keywords, its allow and deny lists of headgear, and the names of the nodes it looks for.

- The row shows the entries, as many as fit, and how many there are: "ArmorBodyPartHead, ObjectTypeAr… 4". An empty list says `None`.
- A press on the row opens the editor, a popup in the content pane, with a line for each entry. A press on an entry opens the keyboard with its text, as a text input does, and an entry typed empty is removed. Remove takes an entry out at once. Add puts an empty entry at the end and opens the keyboard on it. A list longer than six entries scrolls inside the editor.
- A list whose order means something, as nodes searched in priority order, has `"valueOptions": { "ordered": true }`: its entries are numbered and have Up and Down. Otherwise the entries keep the order the file has.
- An entry that is new or changed is marked pending. Each change sets the pending value as it is made. Done, or a press beside the editor, keeps it. Cancel and Back give back the list it had when it opened.
- The list is written as one line, `["AddOnNode", "LightAttachNode"]`. A text that is in the list twice is named in the help line, and does not block Save.
- A value the framework cannot read as an array of strings is invalid, shown as the file has it, with the framework's reason in the help line.

The look is the design system's [List](design-system/project/components/List/README.md).

### Hand pose

Primary use: how a hand holds something, as Immersive Flashlight's four grip poses, given to FRIK through its API.

- The row shows a bar for each finger, thumb to pinky, as tall as the finger is straight, and the palm's pitch and yaw: "palm 15°, 0°".
- A press on the row opens the editor, a popup in the content pane: the fingers and the palm on the left, and the joints of the one selected on the right, as sliders with their numbers. A finger has Base, Middle, Tip and Spread, from −1 to 2 (Spread −1 to 1), 0 bent and 1 straight. The palm has Pitch and Yaw, from −90° to 90°. The thumbstick steps the joint the ray is on by 0.01 or 1°.
- While the editor is open, the player's hand that the setting names takes the pose as it changes, through FRIK, so the pose is set by looking at it. It is `"valueOptions": { "hand": "offhand" }` when left out, or `"primary"`. Without FRIK the editor sets the numbers only.
- It shows the pose it had when it opened beside the pose as it is, and a press on the first goes back to it. Done, or a press beside the editor, keeps it. Cancel and Back give back the pose it had when it opened.
- A changed pose is written as the framework writes one, every finger and the palm.
- A value the framework cannot read is invalid, shown as the file has it, with the framework's reason in the help line.

The look is the design system's [HandPose](design-system/project/components/HandPose/README.md).

### Transform

Primary use: where a mesh, a light or a zone sits on its node, as Immersive Flashlight's 13 transforms.

- The row shows the position and the scale on one line and the rotation on the next. A key the value leaves out is shown as what it means, muted: no move, no rotation, a scale of 1.
- A press on the row opens the editor, a popup in the content pane: a stepper for X, Y, Z and Scale, and for Heading, Roll and Attitude, and a step size of Fine, Normal or Coarse for all of them. The thumbstick steps the line the ray is on.
- A `live` transform moves as it is stepped. Done, or a press beside the editor, keeps it. Cancel and Back give back the value it had when it opened.
- In the world hands it to the framework's live tuning, with the menu minimized and Done and Cancel on the wrist, and brings the tuned value back as the pending value. It needs the framework to tune another mod's key, and is not shown until it can.
- A changed value is written with the keys the file had and the key that changed.

The look is the design system's [Transform](design-system/project/components/Transform/README.md).

### ActivationSphere

Primary use: a zone the hand reaches into and what a press there does, a whole section of a CommonFramework mod's TOML.

- The section is drawn as one group under a heading with the sphere's mark: its zone and its power-armor zone as transforms, its two bindings and three haptics under Actions, and its icon and sphere in a section that folds, folded by default.
- Each key is its own setting, with the control of its kind, and is dirty, previewed and saved alone.
- A power-armor zone that the file does not have says `As Zone`, and a change writes it.
- While the ray is on the group, its sphere is lit in the world, so the player sees which one the rows are of. It needs the client to be told which group the ray is in, beside the page events.

The look is the design system's [ActivationSphere](design-system/project/components/ActivationSphere/README.md).

## Help and validation feedback

The panel reserves a stable place for the help of the item the ray is on: the help line on the left of its footer. See [Help line](ui-and-vr-interaction.md#help-line). The same place presents validation errors so content does not jump around.

Additional feedback:

- An invalid row has the invalid mark after its name and its value in red, and the help line has the reason while the ray is on it.
- A row whose warning holds has a warning mark, and the help line shows the warning before its help. A warning does not block Save. Its mark is a hollow amber triangle after the name and its pending dot. While the value is invalid, the invalid mark takes its place. See [Warnings](schema.md#warnings), and the design system's [RowWarning](design-system/project/components/RowWarning/README.md).
- Save is disabled while a dirty setting is invalid, and the footer's status says how many there are. An invalid value that was already in the file does not block it.
- A folded section's heading says how many invalid values it hides.
- Validation must not rely on color alone.

## Defaults and reset

Every editable declarative setting must declare its default beside the setting definition. A read-only setting has no Reset, and needs none.

- Each setting can be reset independently.
- A page or section may expose Reset All when unambiguous.
- Reset changes the pending value and participates in preview like any other edit.
- Reset is not persisted until Save.
- Discard restores the values from the start of the editing session, including any values changed through Reset.
- While a setting's change is pending, its row has Discard in the place of Reset, which takes back that change alone, to the setting's saved value. Reset is back once the change is discarded.

## MCM controls

The MCM bridge should preserve the existing MCM control vocabulary and behavior as faithfully as practical instead of translating every control into a more powerful native concept. MCM's controls are not marked as MCM's, and their changes are made as MCM's menu makes them, with the Save bar, which keeps them or sets them back.
