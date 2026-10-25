# List

A list of texts, a CommonFramework `a` key: a frame that shows the entries, and opens an editor with a line for each entry.

**Provides:** the setting's name, its value (the TOML text of an array of strings, `["AddOnNode"]`), its default, its state, and whether its order means something (`valueOptions.ordered`, false when left out).

## The row

- The frame is 392 x 48, as wide as a dropdown's, in `track` with a `frame-border` outline. From the left: the entries in `t-body`, joined by commas, as many as fit and then an ellipsis; how many there are in `t-detail` `green-muted`; and the drawn down arrow. Then the Reset slot.
- An empty list says `None` in `green-muted`, with no count.
- A press anywhere on the row opens the editor.
- Pending: the entries in `amber`, the pending mark after the name, Reset shown.
- Invalid: a value the framework cannot read as an array of strings, as `"AddOnNode"`. The frame has a `red` outline, the file's text in `red`, no count, and the invalid mark after the name. The help line gives the framework's reason.

## The editor

A popup inside the content pane, 717 wide, placed as the Color editor is: under its row, or over it where the pane has no room under it, and then its header names the setting. `popup` background, a 2 px `green` border, `radius-popup`, `space-4` padding.

- **Header**, 48 px: the setting's name in `t-body` on the left, and on the right in `t-detail` `green-muted` how many entries it has: "18 entries", or "3 entries, in order" when the order means something.
- **Entries**, one line each, 48 px tall with `space-1` between:
  - The entry's text in a frame, `track` with a `frame-border` outline, `t-body`, ending in an ellipsis where it is long. A press on it opens the keyboard with its text, as a [text input](../../../../controls.md#text-input) does: Done keeps the new text and Cancel the old. An entry typed empty is removed.
  - Remove, an icon button (`X`), at the right. It removes the entry at once: Cancel, Discard and Reset bring it back, so it asks nothing.
  - When the order means something: the entry's number in `t-detail` `green-muted` on the left (`space-6`), and Up and Down, icon buttons before Remove. Up is disabled on the first entry and Down on the last. An unordered list has neither, and keeps its entries in the order the file has them.
  - An entry that is new or changed in this session is in `amber`.
  - Under the ray a line has the row's hover look, around its frame and its buttons.
  - Six lines show. A longer list scrolls inside the editor with the thumbstick, with a `scrollbar-size` bar on the right, as a dropdown's list does.
- **Footer**, under a `rule` line: Add (secondary, with a plus) on the left, Cancel (quiet) and Done (primary) on the right, 160 px each. Add puts an empty entry at the end, scrolls to it and opens the keyboard on it.
- Every change sets the pending value as it is made. Done, or a press beside the editor, closes it and keeps the list. Cancel and Back close it and give back the list it had when it opened.
- While the ray is on an entry, the help line has its text and what a press does: "0026D8|SS2Extended.esp: Press it to type it again, or Remove it. Added: it is saved with Save."

## Writing the value

- The list is written as the framework's array, `["AddOnNode", "LightAttachNode"]`, one line, each text in quotes with its `"` and `\` escaped. A file that had the array over several lines, as Immersive Flashlight's allow list, gets it on one line.
- Two entries with the same text are kept: the help line of the second says it is in the list already. It does not block Save.

## Focused panel

A list is shown and not edited, as a binding is: it is changed in the Full panel, where the keyboard has room.

The behavior is in [Controls](../../../../controls.md#list).
