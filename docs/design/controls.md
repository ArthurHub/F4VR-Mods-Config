# Controls and Setting Types

The control vocabulary and its interaction rules. Build order: [Mod work plan](mod-plan.md). Each control's look is its design system component, linked at its end.

## Separation of concerns

A setting has four separate parts, so file format, widget and runtime behavior stay uncoupled:

| Part | Meaning | Config spec field |
| --- | --- | --- |
| Type | The value the mod stores | `valueType` |
| Control | How the menu shows and edits it (MCM's names) | `type` |
| Storage | Where it is read and written | `storage` |
| Application policy | When the mod acts on a change: as made, after Save, after restart, or unknown | `applies` |

## Core stored types

`bool`, `int` (signed 32-bit), `float` (32-bit), `string`, `color`. A choice has no type of its own: its options store typed values. No `vec2` / `vec3`: complex CommonFramework values have dedicated controls.

## Core interactive controls

### Switcher

For bools. The whole row is the target; the state shows in shape/text and color; trigger toggles; Reset gives the default.

### Slider

For bounded ints and floats.

```text
[◂][=============o-------][▸]  [ 1.25 ]  [ Reset ]
```

- Trigger-drag previews continuously where the setting supports it. Release ends the interaction; it doesn't Save. Back during a drag restores the value from its start.
- Thumbstick left/right steps the slider the ray is on by its step, once the ray has been on the row a short time; no selecting first. Holding may accelerate after a repeat delay.
- The thin arrow at each end steps it too and repeats when held; the arrow of the end the value is at is disabled.
- The value box only shows the value (not typed), with the `display` decimals, unit or percentage. Too wide: drawn smaller, then without its unit, never shortened ([NumberDisplay](design-system/project/components/NumberDisplay/README.md)).
- Linear only; log or curved mappings are later work.
- Haptics mark meaningful boundaries, not every tiny change.

### Dropdown and stepper

One value from a finite set, as in MCM: `dropdown` (popup list) and `stepper` (previous/next).

- Each option has a typed value, a text, and an optional short text and help. The current option is strongly marked; popup rows are large targets.
- In the list: trigger selects, thumbstick scrolls, Back or a press beside the list closes it unchanged (that press does nothing else).
- A press anywhere on a dropdown's row opens its list.
- A stepper's buttons stop at the first and last option; a press on the rest of its row goes to the next option, wrapping from last to first. Thumbstick left/right steps it one option per push.
- While the list is open, the help line shows the help of the option the ray is on (a config spec option's `help`), else the row's.

### Choice of files

A file of a folder: MCM's `dropdownFiles`, as Immersive Flashlight's gobo textures. Schema: [Options from a folder](schema.md#options-from-a-folder). Look: [FileChoice](design-system/project/components/FileChoice/README.md).

- A dropdown of the folder's files matching a mask, alphabetical, read on selection and on rereading values. A file shows its name without the extension when the mask names one, else with the extension muted. The folder and file count head the list.
- A pick is written in the form of the default: its folder and separators with the file's name. A value naming the same file differently (`/`, case, no `data\`) is that file, and clean.
- A value of another folder or a missing file is invalid. A folder with no matching file disables the row, and the help line says where it looked; one whose choice has an option that is no file has that option alone.

### Text input

Short strings, and typed numbers: a `textinput` with `valueType` int or float takes a number (MCM's `textinputInt` / `textinputFloat`), limited by `valueOptions` `min` / `max`.

- The keyboard is SteamVR's, shown while the field is edited; a physical keyboard and an overlay that types into the game window work too, and are all a runtime without one (OpenComposite) has. See the framework's [VR keyboard](../../external/F4VR-CommonFramework/docs/tech/vr-keyboard.md).
- `Done` validates and commits to the pending value. `Cancel` (the keyboard's close) restores the value from before editing.
- Invalid temporary text is never previewed; the last valid preview stays.
- No multiline, password, selection tools or speech input.

Look: [TextInput](design-system/project/components/TextInput/README.md): the text and a pencil; while typing, a caret and the character count.

### Color

A CommonFramework color, `{ rgb = [r, g, b], alpha = a }`. MCM has no color control. Look: [Color](design-system/project/components/Color/README.md).

- The row shows a swatch (alpha over a checkerboard) and the numbers: "190, 255, 190, alpha 200". A press opens the editor.
- The editor is a popup in the content pane: a hue/saturation/brightness picker (trigger-drag) and R, G, B, A sliders; the thumbstick steps the slider the ray is on by 1.
- It shows the opening color beside the current one; a press on the first goes back to it.
- Swatches set a color with a press: the five basics in the footer (white, black, red, green, blue) and the config spec's presets under the picker. A preset without alpha keeps the color's ([CommonFramework values](schema.md#commonframework-values)).
- Each change sets the pending value; a `live` color previews while dragged. Done or a press beside keeps it; Cancel and Back restore the opening color.
- The shown value is what the framework reads: the value's keys over the default's channels. A change is written as whole numbers with the keys it had plus the changed part's: `{ alpha = 60 }` with a new hue is `{ rgb = [255, 190, 120], alpha = 60 }`.
- An unreadable value is invalid, shown as in the file, with the framework's reason in the help line.
- `"valueOptions": { "alpha": false }` makes it RGB only (a light's): opaque swatch, no alpha in the numbers, no Alpha slider, only `rgb` written; a file's `alpha` is kept.

### Button

An operation that doesn't edit a stored value. Schema: [Buttons](schema.md#buttons). Look: [ButtonRow](design-system/project/components/ButtonRow/README.md).

- Runs what an MCM button runs (a script function, a console command) or a function the mod's native client registered, by name, with typed parameters.
- Says `Run`, or the config spec's button text, after an optional `icon` from the menu's drawn icons.
- `style`: `important` (outline, text and icon in the accent color) or `dangerous` (red, always asks first).
- A destructive action asks first: `confirmation` opens a dialog.
- While running it says `Running` and can't be pressed. Its result, if any, is in the help line while the ray is on the row, with a mark after its name: info until read, invalid while it failed.
- Cancel and progress for a running action are open design work.

### Read-only

A value the mod writes for the player to read, as a measured height. Look: [ReadOnly](design-system/project/components/ReadOnly/README.md).

- Shows its value in its control; the whole row is disabled as an MCM hotkey's: a press does nothing and nothing looks pressable.
- The help line starts with `(Read-only)`. No Reset.

## Layout and informational elements

Items that store nothing: a section heading with its rows, text, spacer, image.

- A config spec's section can nest and fold. A press on a folding heading hides or shows its rows, and the heading marks which. A folded section counts its invalid values and unsaved changes on its heading. A nested section is indented with its rows, with a line from under its heading to its last row. See [Sections](schema.md#sections) and [SectionHeader](design-system/project/components/SectionHeader/README.md).
- Detailed settings go in a folded section right after the settings they belong to.
- Text, spacer and image are [TextRows](design-system/project/components/TextRows/README.md): text in the muted color with MCM's tags; a spacer of one row or more; an image at its size, scaled down to the rows' width.

## CommonFramework-specific controls

First-class controls for the framework's own value types, never flattened into tuples: their meaning, validation and editing are richer. The framework's TOML fixes the storage, and the key prefix says which:

| Control | Prefix | Stored as |
| --- | --- | --- |
| `Haptic` | `s` | A pattern name of the framework's library, or `"none"`: `sEntryHaptic = "Tick"` |
| `Color` | `c` | `{ rgb = [r, g, b], alpha = a }`, whole numbers 0..255; a missing key keeps the default's channels |
| `InputBinding` | `k` | `{ hand = "offhand", type = "tap", button = "grip" }`, optional `duration`, `suppress`, `modifier = { button, hand }`; `{ hand, type = "axis", direction, axis, threshold }` for a stick or axis; `{ type = "none" }` |
| `List` | `a` | An array of strings: `aHeadLightDenyList = ["0316D4\|Fallout4.esm"]` |
| `HandPose` | `h` | `{ thumb = [prox, mid, dist, splay], index, middle, ring, pinky, palm = [pitch, yaw] }`, each optional, a missing one zeros |
| `Transform` | `t` | `{ pos = [x, y, z], rot = [heading, roll, attitude], scale = s }`, degrees, each optional |
| `ActivationSphere` | | A whole section: `tZone`, optional `tZonePA`, `kPrimaryBinding`, `kSecondaryBinding`, `sEntryHaptic`, `sPrimaryHaptic`, `sSecondaryHaptic`, and its sphere and icon keys |

A table is one value in one key. In the model it is a string holding the table's TOML text (`{ rgb = [255, 51, 51], alpha = 40 }`), which the control reads and writes and the store writes as an inline table, not a quoted string; it is dirty, previewed and saved as a whole. The framework names any wrong key or word, and an unreadable table is invalid with that reason (shown as in the file, the reason in the help line). That holds for every control below.

The editors of Color, InputBinding, List, HandPose and Transform are popups in the content pane: Done or a press beside keeps the value, Cancel and Back restore the value from when it opened. What's left to design: [Open questions](open-questions.md#commonframework-specific-controls).

### Haptic

A controller pattern, as `sEntryHaptic` or the menu's own `sPressHaptic`. Look: [Haptic](design-system/project/components/Haptic/README.md).

- One of None and the library's 18 patterns (`vrcf::HapticPattern`), in library order. The menu knows the list with a text and help for each; a config spec names only the key and default.
- A dropdown whose options show the pattern's shape from its keyframes (`VRControllersHaptic::getPattern`).
- It plays on the pointing hand when an option is picked, at each stepper step, and with the Play button beside it; such a press gets no menu press pulse. Pointing at an option plays nothing; its help says how it feels, its length and its use.
- Reset sets the default without playing it.
- File text matches as the framework reads it: case-insensitive, ignoring spaces, underscores and hyphens (`double click` is Double click; `off` and empty are None). A change is written in library spelling, `DoubleClick`, or `none`.
- An unknown name is invalid; the help says the framework plays nothing for it.
- A `live` haptic previews like any setting: the mod plays the pending pattern on its event.

### InputBinding

A controller binding, as `kPrimaryBinding` or the menu's `kOpenMenuBinding`. Look: [InputBinding](design-system/project/components/InputBinding/README.md).

- Shown as the button's icon and one sentence, "Long press the X button on the off hand": the hand as the config names it, the button as the controller prints it. The icon marks how it's pressed: none for tap/press, a clock (long press), arrow down onto a bar (hold), 2× (double), arrow up off a bar (release), ripples (touch); a thumbstick push has its direction arrow. A chord shows both buttons; only the system button has no icon.
- The editor builds it from parts: Hand (None first, for off), Button, How (types the button has), Direction (thumbstick push), Duration or Threshold, While holding (modifier), and Suppress from the game where offered. A part the type lacks is disabled in place, so nothing moves.
- `valueOptions` `hands`, `types`, `buttons` limit what's offered; a binding outside them is invalid, naming what's allowed. This keeps out what can't work there, as Back on the trigger, which presses rows.
- `suppress` shows only with `"valueOptions": { "suppress": true }` (an activation sphere's two bindings): it means something only where a consumer applies it. Elsewhere a file's `suppress` is kept.
- Each change sets the pending value; a `live` binding is previewed.
- Keys the type lacks aren't written, and Duration or Threshold at the type's default is left out, so the table stays as short as the framework's examples.

### List

Texts a mod reads, as Immersive Flashlight's keywords, headgear allow/deny lists and node names. Look: [List](design-system/project/components/List/README.md).

- The row shows as many entries as fit and the count: "ArmorBodyPartHead, ObjectTypeAr… 4"; empty says `None`.
- The editor has a line per entry. A press on one opens the keyboard on it; typed empty removes it. Remove removes at once. Add appends an empty entry and opens the keyboard. Over six entries scroll inside the editor.
- `"valueOptions": { "ordered": true }` (a priority order) numbers entries and adds Up and Down; otherwise the file's order is kept.
- New or changed entries are marked pending; each change sets the pending value.
- Written on one line: `["AddOnNode", "LightAttachNode"]`. A duplicate is named in the help line and doesn't block Save.

### Hand pose

How a hand holds something, as Immersive Flashlight's four grip poses, given to FRIK through its API. Look: [HandPose](design-system/project/components/HandPose/README.md).

- The row shows a bar per finger, thumb to pinky, as tall as the finger is straight, and the palm: "palm 15°, 0°".
- The editor: fingers and palm on the left, the selected one's joints on the right as sliders with numbers. A finger has Base, Middle, Tip (−1..2, 0 bent, 1 straight) and Spread (−1..1); the palm Pitch and Yaw (−90°..90°). The thumbstick steps by 0.01 or 1°.
- While open, the hand the setting names (`"valueOptions": { "hand": "offhand" }` by default, or `"primary"`) takes the pose through FRIK as it changes. Without FRIK it sets the numbers only.
- It shows the opening pose beside the current one; a press on the first goes back to it.
- A change is written as the framework writes a pose: every finger and the palm.

### Transform

Where a mesh, light or zone sits on its node, as Immersive Flashlight's 13 transforms. Look: [Transform](design-system/project/components/Transform/README.md).

- The row: position and scale on one line, rotation on the next. A missing key shows what it means, muted (no move, no rotation, scale 1). A part the config spec turns off is left out.
- The editor: one slider per axis of each enabled part (X, Y, Z, Heading, Roll, Attitude, Scale), a line each, no headings. Ranges and steps come from the config spec's part or axis, else the part's own ([CommonFramework values](schema.md#commonframework-values)). They are the row slider, with step arrows and thumbstick stepping.
- A value keeps decimals finer than its step (a file's 20.72° isn't shown as 21°). A value out of range shows as is with the grab at the end; a step or drag brings it in.
- A `live` transform moves as changed. Done keeps the value pending; Save writes it.
- **Adjust in world**, in the editor footer (`important` style, hand icon), tunes it with the controllers while the player looks at the object. The panel minimizes, and the wrist shows, in place of Restore and Exit: `Mods Config: <setting>`, its values live on one line, the controls of the enabled parts, and Cancel and Done.
  - Controls are the framework's `DebugAdjuster`: right stick moves X and Y, left stick Z; holding the offhand grip makes both sticks rotate (left X rolls); holding the offhand A makes the right stick scale. A disabled part doesn't move and its control isn't shown.
  - All controller input is taken from the game while tuning. Each change is the pending value, previewed as in the editor.
  - Done returns to the panel with the editor open and the value pending. Cancel restores the value from when tuning started, keeping earlier editor changes.
  - A game menu opening ends tuning as Cancel. A loading screen or the main menu closes the menu and discards the session.
  - Rotation turns as the matrix does, so one stick can change all three angles. Slider ranges don't limit tuning; a value past one shows as is. Scale stays above 0.
  - Enabled while the transform is `live` and its mod's client is connected (nothing moves otherwise); else disabled with the reason in the help line.
- Grabbing the object by hand is next: [Open questions](open-questions.md#commonframework-specific-controls).
- A change is written with the file's keys plus the changed one.

### ActivationSphere

A zone the hand reaches into and what a press there does: a whole TOML section. Look: [ActivationSphere](design-system/project/components/ActivationSphere/README.md).

- One group under a heading with the sphere's mark. First the most changed: primary binding (Activate), entry haptic, Show icon. Then three folding sections, folded each time the page shows: Position and size (zone and power-armor zone; scale is size), Activation (primary haptic, secondary binding and its haptic), Visuals (icon and sphere and how each shows).
- The config spec names each row (else its default name) and can hide any but the primary binding: a hidden key has no row and keeps the file's value. A section with no rows is left out. See [CommonFramework values](schema.md#commonframework-values).
- Each key is its own setting with its kind's control: dirty, previewed and saved alone.
- A binding set to None hides its haptic; an icon or sphere set to Never disables its rows.
- The power-armor zone has a row only while the file has its key or the config spec gives it a default (else the framework uses the zone and there's nothing to tune); with no default it has no Reset.
- Its dropdowns read words as the framework does: `off` is Never, `Cyan_Subtle` is Cyan, subtle.
- Icon and sphere mesh/texture paths are text inputs until the [choice of files](#choice-of-files) is a config spec control.
- While the ray is on the group, its sphere is lit in the world and the heading's mark has a halo. This needs the client told which item the ray is on, beside the page events.

## Help and validation feedback

Help and validation share one stable place, the [help line](ui-and-vr-interaction.md#help-line), so content doesn't jump.

- An invalid row has the invalid mark after its name and its value in red; the help line has the reason.
- A row whose warning holds has a hollow amber triangle after its name and pending dot, and the help line shows the warning before the help. It doesn't block Save; an invalid mark replaces it. See [Warnings](schema.md#warnings), [RowWarning](design-system/project/components/RowWarning/README.md).
- Save is disabled while a dirty setting is invalid, and the footer status counts them. An invalid value already in the file doesn't block it.
- A folded section's heading counts the invalid values it hides.
- Validation never relies on color alone.

## Defaults and reset

Every editable config spec setting declares its default; a read-only one has no Reset.

- Each setting resets on its own; a page or section may offer Reset All when unambiguous.
- Reset changes the pending value, previews like any edit, and is saved only by Save. Discard restores the session's starting values, Reset changes included.
- While a change is pending, the row has Discard in place of Reset, taking back that change alone to the saved value; Reset returns once it's discarded.

## MCM controls

The MCM bridge keeps MCM's control vocabulary and behavior as faithfully as practical rather than upgrading controls. MCM rows aren't marked as MCM's, and their changes are made as MCM's menu makes them, with the Save bar keeping them or setting them back.
