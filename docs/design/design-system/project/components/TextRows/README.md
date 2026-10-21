# TextRows

The rows of a page that hold no value: a `text`, a `spacer` and an `image`. The ray does nothing on them, they have no hover look, and the help line keeps what it showed.

**Provides:** a text's string and whether it is `html`; a spacer's `numLines`; an image's texture path and its `width` and `height`.

## Text

- A paragraph in `t-body` `green-muted`, set in as a row is, a level (`space-3`), with its text `space-3` further, under the rows' names, and wrapped to the end of the control column. `space-1` above and below. `TextWrapped` between `PushTextWrapPos`.
- With `html`, the tags MCM configs use:
  - `<br>` a new line, `<p>` a paragraph with `space-2` after it, and its `align` `left`, `center` or `right`.
  - `<font size>` at 1.05 times its number, so 20, MCM's ordinary size, is the rows' 21 px. The font is rasterized at the sizes a page uses, as `t-title` is.
  - `<font color>` as given, `#RRGGBB`: the one place where a mod's own color is drawn on the panel.
  - `<b>` and `<i>` in Roboto's bold and italic. Both at once is bold.
  - `<u>` a 1 px line under its text, the spaces between its words included.
  - Any other tag is dropped and its text kept; entities are decoded.
- A text is not a warning the host draws: a mod's own warning is a text, as in MCM.

## Spacer

- Empty, a line of body text (the font's size, 21 px) for each of its `numLines`, one when it has none (`ModPage.cpp`). Nothing is drawn: the outline in the card is only to show it.

## Image

- A texture under `Data/Textures`, drawn at its `width` and `height` in layout px, or at the texture's own size when the config spec gives none, centered in the width of the rows, `space-1` above and below.
- One wider than the rows is scaled down to their width, keeping its aspect.
- Its colors are its own, as a swatch's are.
- A texture that cannot be loaded is a box of its size, with a 2 px dashed `frame-border` outline, that names it in `t-detail` `green-muted`: "Missing: MyMod/config/banner.dds". The log has the reason.

## Focused panel

The same, wrapped to the panel's width. An image wider than the panel is scaled to fit.

The behavior is in [Text, spacers and images](../../../../schema.md#text-spacers-and-images).
