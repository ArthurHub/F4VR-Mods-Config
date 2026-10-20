# Color

A CommonFramework color: a frame that shows the color as a swatch and as its numbers, and opens an editor with a picker and a slider for each channel.

**Provides:** the setting's name, its value (the TOML text of `{ rgb = [r, g, b], alpha = a }`, either key optional), its default, its state, and whether it has an alpha (`valueOptions.alpha`, true when left out). The control reads the color the framework would read: the keys the value has, over the default's channels.

## RGB only

A color whose opacity means nothing, as a light's, is RGB only: `"valueOptions": { "alpha": false }`. Then:

- The swatch is the color opaque, with no checkerboard half, and the numbers are "255, 214, 170".
- The editor has Red, Green and Blue only, so it is 56 px shorter: 717 x 358. Its Before and Now swatches are opaque, and the text under the channels is `{ rgb = [255, 214, 170] }`.
- Only `rgb` is written. An `alpha` that the file has is kept as it is, and not shown.

## The row

- The frame is 392 x 48, as wide as a dropdown's, in `track` with a `frame-border` outline. From the left: the swatch, the numbers, and the drawn down arrow. Then the Reset slot.
- **The swatch** is 64 x 32, outlined in `frame-border`. Its left half is the color opaque. Its right half is the color at its alpha over a checkerboard of 8 px squares, `rule` on `glass`, so the opacity shows. It is the only place, with the editor, where a color other than the panel's is drawn: it shows the color it edits.
- **The numbers** are `t-body`, as the file writes them: "190, 255, 190, alpha 200".
- A press anywhere on the row opens the editor.
- Pending: the numbers in `amber`, the pending dot after the name, the swatch in the new color.
- Invalid: a value the framework cannot read, as `{ rgb = [300, 0, 0] }`. The frame has no swatch and a `red` outline, the numbers are the file's text in `red`, ending in an ellipsis where it is long, and the invalid mark is after the name. The help line gives the framework's reason: "Ray color: rgb has a number that is not a whole number in 0..255. Pick a color, or Reset it to 190, 255, 190, alpha 200."

## The editor

A popup inside the content pane, 717 x 414: `popup` background, a 2 px `green` border, `radius-popup`, `space-4` padding. It is under its row, or over it where the pane has no room under it, as a dropdown's list is, and where the pane has room for neither, as near the row as fits: then it covers the row, so its header names the setting. It is never over the title bar and the footer.

- **Header**, 48 px: the setting's name in `t-body` on the left. On the right, "Before" and "Now" in `t-detail` `green-muted`, each followed by a 96 x 40 swatch drawn as the row's. Before is the color when the editor opened, and a press on it goes back to it. Now is the color as it is.
- **Picker**, on the left: ImGui's `ColorPicker4` at width 293, with `NoAlpha`, `PickerHueBar`, `NoSidePreview`, `NoSmallPreview`, `NoInputs` and `NoLabel`. With our frame height of 45 its hue bar is 45 px wide, and its square is 240 x 240 (293 − 45 − `space-2`). The trigger drags in the square for saturation and brightness, and in the bar for the hue. ImGui draws its cursors in white and black, which is right on any color.
- **Channels**, on the right, 368 wide: Red, Green, Blue and Alpha, one line each, 48 px tall with `space-2` between. A line is the channel's name in `t-detail` (56 px), its track (192 x 48), and its value box (`value-width`, right-aligned whole number).
  - A track is a gradient of that channel from 0 to 255 with the other channels as they are (`AddRectFilledMultiColor`), outlined in `frame-border`. Alpha's is the color from transparent to opaque over the checkerboard.
  - The grab is hollow, 20 x 40: a 2 px `green` ring around a 2 px `on-green` ring, so it shows on any color. While dragged its outer ring is `glow`.
  - The thumbstick steps the channel the ray is on by 1, and faster while it is held, as a slider's. This is how a color is set exactly.
- Under the channels, in `t-detail` `green-muted`: the text that will be written, `{ rgb = [255, 190, 120], alpha = 200 }`.
- **Footer**, under a `rule` line: Cancel (quiet) and Done (primary) on the right, 160 px each.
- Every change sets the pending value as it is made, and a `live` color is previewed as it is dragged. Done, or a press beside the editor, closes it and keeps the color. Cancel and Back close it and give back the color it had when it opened.
- While the ray is on a part of it, the help line says what it does: "Green: The green of the color, 0 to 255. The thumbstick steps it by 1." "Before: The color when the editor opened. Press it to go back to it."

## Writing the value

- A changed color is written as whole numbers, with the keys the file had, and a key added for the part that changed: `{ alpha = 60 }` with a new hue is written `{ rgb = [255, 190, 120], alpha = 60 }`.
- Reset sets the default as the package gives it.

## Focused panel

The editor has no picker there: it fills the Focused panel, 576 x 432, with its header, the channels with the track as wide as fits, and its footer. The thumbstick on the channels is how a color is set on the hand.

## Mods Config VR's own pointer

`cRayColor`, `cMarkColor` and `cMarkBorderColor` are previewed on the pointer the player is using. An alpha near 0 hides the ray or the mark; Back gives the color back without the ray.

The behavior is in [Controls](../../../../controls.md#color).
