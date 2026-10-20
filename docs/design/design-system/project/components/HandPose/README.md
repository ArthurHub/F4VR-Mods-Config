# HandPose

A hand pose, a CommonFramework `h` key: a frame that shows how each finger bends, and opens an editor of the joints of each finger and the palm, which the player sees on their own hand.

**Provides:** the setting's name, its value (the TOML text of `{ thumb = [prox, mid, dist, splay], index, middle, ring, pinky, palm = [pitch, yaw] }`, each key optional), its default, its state, and the hand it is shown on (`valueOptions.hand`: `primary` or `offhand`, `offhand` when left out).

## The row

- The frame is 392 x 48, in `track` with a `frame-border` outline. From the left: the glyph, the palm as text, and the drawn down arrow. Then the Reset slot.
- **The glyph** is 104 x 32, outlined in `frame-border`: a `green` bar for each finger, thumb to pinky, as tall as the finger is straight. A finger's height is the mean of its three joints, 0 bent and 1 straight, clamped to 0..1. It tells an open hand from a fist at a glance; the numbers are in the editor.
- **The palm** is `t-body`: "palm 15°, 0°", its pitch and yaw.
- A press anywhere on the row opens the editor.
- Pending: the bars and the text in `amber`, the dot in the gutter, Reset shown.
- Invalid: a value the framework cannot read as a hand pose, as a finger with two numbers. The frame has no glyph and a `red` outline, the file's text in `red` with an ellipsis, and the invalid mark in the gutter. The help line gives the framework's reason: "thumb is not an array of four numbers, [prox, mid, dist, splay]".

## The editor

A popup inside the content pane, 717 wide, placed as the Color editor is. `popup` background, a 2 px `green` border, `radius-popup`, `space-4` padding.

- **Header**, 48 px: the setting's name in `t-body`. On the right, "Before" and "Now" in `t-detail` `green-muted`, each followed by the glyph at 120 x 40. Before is the pose when the editor opened, and a press on it goes back to it.
- **Fingers**, on the left, 216 wide: Thumb, Index, Middle, Ring, Pinky and Palm, a 48 px row each with `space-1` between, the name in `t-body` and, for a finger, a 56 x 8 bar of how straight it is. The selected one is solid `green` with `on-green` text, as a tree row. Hover: `hover` with a `frame-border` outline. A finger changed in this session has its name and bar in `amber`. Thumb is selected when the editor opens.
- **Joints**, on the right: the selected finger's Base, Middle, Tip and Spread (`prox`, `mid`, `dist`, `splay`), or the palm's Pitch and Yaw. A line each, 48 px with `space-2` between: the name in `t-detail` (64 px), a slider's track, and its value box (`value-width`). The fill runs from a 2 px `rule` mark at 0 to the grab, so a negative value fills left.
  - Base, Middle and Tip go from −1.00 to 2.00, Spread from −1.00 to 1.00, with two decimals. Pitch and Yaw go from −90° to 90°, whole degrees. The poses mods ship go past 0 and 1 (Immersive Flashlight's pinky base is −0.80), so the track goes past them too. A value from the file outside the track keeps its number, with the grab at the end.
  - Under the tracks in `t-detail` `green-muted`, each under its value: "0 bent" and "1 straight", or "−90°", "0°" and "90°".
  - The thumbstick steps the joint the ray is on by 0.01, or 1°, and faster while it is held, as a slider's.
  - A value changed in this session is in `amber`.
- **Footer**, under a `rule` line: Cancel (quiet) and Done (primary) on the right, 160 px each.
- Every change sets the pending value as it is made.
- **The player's hand shows the pose.** While the editor is open, the hand the setting names takes the pose as it is changed, through FRIK's `setHandPose` with the menu's tag, which is over a mod's own. The hand is let go when the editor closes. The panel stays where it is, and the ray is on the other hand. A pose for power armor is shown on the hand as it is, in power armor or not.
- Without FRIK there is no hand to show it on: the editor sets the numbers only, and the help line says so: "Without FRIK the pose is not shown on your hand."
- Done, or a press beside the editor, closes it and keeps the pose. Cancel and Back close it and give back the pose it had when it opened.
- While the ray is on a part of it, the help line says what it does: "Base: How the base joint of the index finger bends: 0 is bent, 1 is straight. Your off hand shows the pose."

## Writing the value

- A changed pose is written as the framework writes it, every finger thumb to pinky and then the palm, each number the shortest text that reads back as its float: `{ thumb = [0.48, 1.14, 0.97, 0.06], …, palm = [15.0, 0.0] }` (`config::handPoseTomlText`). A finger the file left out is written as zeros once the pose is changed.
- Reset sets the default as the package gives it.

## Focused panel

A hand pose is shown and not edited: the Focused panel is on the hand that would take the pose. It is changed in the Full panel.

The behavior is in [Controls](../../../../controls.md#hand-pose).
