# InputBinding

A CommonFramework controller binding: a frame that shows the binding as the framework's icon and as a sentence, and opens an editor of its parts.

**Provides:** the setting's name, its value (the TOML text of the binding's table), its default, its state, and whether it offers `suppress` (`valueOptions.suppress`, false when left out). The words, their aliases and what each type does are the framework's (`vrcf::parseInputBindingTable`, `docs/input-binding.md`); the control writes a table with `vrcf::inputBindingTomlText`.

## The sentence

A binding is said as one sentence, the same in the row, the editor and the help line. Hands are said as the config names them, and a button as the controller prints it: with `primary` and `offhand` resolved by the player's handedness, the A button of the left controller is X, as `vrui::bindingLabel` resolves it.

| Table                                                                                                       | Sentence                                                                        |
| ----------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------- |
| `{ hand = "offhand", type = "tap", button = "trigger", suppress = true }`                                    | Tap the trigger on the off hand                                                 |
| `{ hand = "offhand", type = "longpress", button = "a" }`                                                    | Long press the X button on the off hand                                         |
| `{ hand = "primary", type = "longpress", button = "thumbstick", duration = 1.5, modifier = { hand = "offhand", button = "thumbstick" } }` | Long press the thumbstick on the primary hand for 1.5 s, holding the thumbstick on the off hand |
| `{ hand = "primary", type = "press", button = "trigger", modifier = { button = "grip" } }`                  | Press the trigger on the primary hand, holding the grip                         |
| `{ hand = "offhand", type = "axis", direction = "left", threshold = 0.5 }`                                  | Push the thumbstick left on the off hand, past 0.50                             |
| `{ hand = "primary", type = "axis", direction = "updown" }`                                                 | Move the thumbstick up or down on the primary hand                              |
| `{ type = "none" }`                                                                                         | None                                                                            |

- The verb is the type: Tap, Press, Release, Long press, Double press, Hold, Touch, and Push for `axis`. The thumbstick moved rather than pushed to a side, a `direction` of `any`, `updown` or `rightleft`, is "Move the thumbstick", "… up or down", "… left or right".
- A `duration` is said only when the table has one: "for 1.5 s" (long press), "from 0.5 s" (hold), "presses within 0.3 s" (double press), "held no longer than 0.5 s" (release). A `threshold` likewise: "past 0.50".
- A modifier is ", holding the grip", with "on the off hand" where it names a hand.
- `suppress` is not in the sentence: the editor shows it, where the setting offers it.

## The row

- The frame is 392 x 48, as a dropdown's: the icon, the sentence in `t-body`, ending in an ellipsis where it is long, and the drawn down arrow. Then the Reset slot. A press anywhere on the row opens the editor.
- **The icon** is the framework's, `Textures\ModsConfigFO4VR\f4cf\bindings\`, as `vrui::bindingIconPaths` names it, so every binding has one: the button as the controller prints it, an L or R circle over its top left for the controller, and how it is worked as a solid mark over its bottom left, with none for a tap or a press: a clock for a long press, an arrow down onto a bar for a hold, 2× for a double press, an arrow up off a bar for a release, ripples for a touch. The thumbstick is a ring with a half-opaque rim and its knob with a shadow, and a push adds a smaller mark with its arrow over its bottom right; the stick moved has arrowheads around its rim instead, every way or along its axis; a push of the trigger or grip axis is that button's icon. The controller is `primary` and `offhand` resolved by the player's handedness, and a letter is the one the controller prints. A chord is the binding's icon, a `+` and the modifier's, as a press. The system button has none, and its sentence stands alone.
- The icon is drawn with `ImGui::Image` at 0.5 of its size: every icon is 80 px tall with its button in the middle, so 40 px, inside the 48 px line with room above and below it, so the thumbstick does not touch the frame, every button at one scale. It is tinted with the text's color: `green`, `amber` while pending.
- The full sentence is in the help line while the ray is on the row: "Open the menu: Long press the thumbstick on the primary hand for 1.5 s, holding the thumbstick on the off hand."
- Pending: the sentence and the icon in `amber`, the pending mark after the name.
- None: "None", with no icon.
- Invalid: a table the framework cannot read. No icon, the file's text in `red` with a `red` outline, and the invalid mark after the name. The help line gives the framework's reason: "'thumbstik' is not a button, one of trigger, grip, a, b, thumbstick, menu, system. Change the binding, or Reset it to Tap the trigger on the off hand."

## The editor

A popup inside the content pane, 717 x 458 with the line of Direction and 717 x 402 without it, placed as the Color editor is: under its row, or over it, or as near as fits, never over the title bar and the footer.

- **Header**, 48 px: the setting's name.
- **The binding**, a `plate` strip 64 px tall: the icons at 0.7 of their size, 56 px, and the full sentence, two `t-body` lines.
- **The parts**, two columns of four 48 px lines, `space-2` apart, each a `t-detail` `green-muted` label (96 px) and a 226 px control:

| Left column | Control | Right column | Control |
| --- | --- | --- | --- |
| Hand | Dropdown: None, then Primary hand, Off hand, Right hand, Left hand, Any hand | Duration, or Threshold for Push | Slider, track 114, value box `value-width` |
| Button | Dropdown: Trigger, Grip, A, B, Thumbstick, System, as the chosen hand's controller prints them | While holding | Dropdown: Nothing, then each button on the same hand and on the other |
| How | Dropdown: Tap, Press, Release, Long press, Double press, Hold, Touch, Push, of those the button has: Push for the thumbstick, and Pull for the trigger and the grip, and Touch for every button but System | Suppress from the game | Toggle, Off and On (`suppress`), where the setting offers it |
| Direction | Choice stepper: Up, Down, Left, Right, for a push of the thumbstick alone. The stick moved shows Any, Up/Down or Left/Right with both arrows disabled: it is the mod's, which reads the stick itself | | |

- The button is picked before how it is worked, and How offers what that button has. A button that does not have the chosen way makes it a Tap. A pull of the trigger or the grip is its axis pushed `right`, as the framework reads it, and has no Direction.
- None, the first option of Hand, turns the binding off, and a hand picked after it turns it on again, worked as it was before.
- A setting can take some hands, ways and buttons alone (`"valueOptions": { "hands": [...], "types": [...], "buttons": [...] }`): Hand, Button and How list those, and a button with no way the setting takes is left out. A button that does not have the chosen way, or a way the setting does not take, makes it a Tap where the setting takes one, or the first way it takes. A part the file has that the setting does not take is shown as it is, and the value is invalid: its icons and its sentence in `red`, in a `red` outline, with the invalid mark after the name, and the help line has the framework's reason and what the setting takes: "'primary' is not a hand this binding takes, one of any. Change the binding, or Reset it to Tap the grip on either hand."
- A part the chosen type does not have is disabled, and keeps its place: Duration for Tap, Press and Touch (and for Push it is Threshold), and every part but Hand for None. Its help line says why: "How: A binding that is off is not worked." Direction is there only for a push of the thumbstick: the editor grows a line for it, and has no empty line otherwise.
- **Suppress from the game** is there only for a setting that offers it: `"valueOptions": { "suppress": true }`, which an activation sphere's two bindings have, as only a consumer that applies suppression gives it a meaning. Elsewhere its place is empty, and a `suppress` the file has is kept as it is. Its label takes two lines.
- **Duration** runs from 0.1 to 3.0 s in steps of 0.05, from 0 for a hold and a release, whose default is 0, and **Threshold** from 0.10 to 1.00. At the type's default, 0.60 s for a long press, they write no key, and the help line says "Default". The thumbstick steps it while the ray is on its line, as it steps a setting's slider.
- **While holding**: "Nothing", then "Grip, same hand", "Grip, off hand" and so on for each button. The same hand writes a modifier with no `hand`; the other writes the other hand's word in the binding's own terms, `offhand` for `primary`, `left` for `right`.
- Each dropdown is the Dropdown's list, opened over the editor, with each option's help in the help line: "Long press: Hold the button past a time, 0.6 s unless Duration says."
- **Footer**, under a `rule` line: Cancel (quiet) and Done (primary) on the right.
- Every change sets the pending value as it is made, and a `live` binding is previewed. Done, or a press beside the editor, keeps it. Cancel and Back give back the binding it had when it opened.

## Icons in the previews

The previews draw the real icons, from `tools/binding_icons.json`, which `tools/gen_binding_icons.py` writes from the mod's copy of the framework's icons. The framework draws them, with its own `tools/gen_binding_icons.py`. A CSS mask stands in for ImGui's tint.

The behavior is in [Controls](../../../../controls.md#inputbinding).
