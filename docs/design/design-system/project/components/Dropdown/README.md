# Dropdown

A choice from a longer list: a frame showing the current option that opens a popup list inside the panel.

**Provides:** options (stored value, label, description) and the current one.

- Closed: 392 x 48 in `track` with a `frame-border` outline, label left, a drawn down arrow right. Open: outline in `green`.
- Popup: `popup` background, 2 px `green` border, `radius-popup`, `space-2` padding, 48 px option rows.
- The current option is solid `green` with `on-green` text and a drawn check. Hover is `hover` with a `frame-border` outline.
- While the ray is on an option, the help line in the footer shows "Option: help" before it is picked.
- Trigger picks, the thumbstick scrolls, Back closes without changing anything. A list too long for the panel scrolls inside the popup, with a `scrollbar-size` position bar on its right.
- A pending choice shows its label in `amber`.
- An entry that does something other than pick, such as **Type a new name** in the category list, is last, set apart by a `rule` line and drawn in `green-muted`.

The framework already opens a combo's list inside a dedicated panel, upward when there is no room below.
