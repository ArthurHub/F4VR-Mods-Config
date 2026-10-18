# FullPanel

The main browsing and editing view: title bar, mod tree, content pane and footer on one curved 1152 x 720 px panel in front of the player, here on a mod's front page.

**Provides:** the mod list (definitions), the selected mod and page, the store of the selected mod (values, states, pending count), and whether that store saves or applies at once.

- Draw on `glass` with a `border-panel` edge in `green` and `radius-panel` corners, inset by `panel-padding`. These are `UIPanelStyle` fields on the `UIImGuiPanel`.
- Title bar: the breadcrumbs in `t-title` on the left, `MODS CONFIG VR » Immersive Flashlight VR` on a front page and `… » Beam` on a page; Help (`?`), Minimize and Close as 48 px icon buttons on the right. A `rule` line under it.
- The buttons are a secondary `Button`'s frame, `space-2` apart and `space-2` from the bar's right, and the crumbs end `space-2` before them. `?` is in `t-body`; Minimize and Close are drawn (see Iconography). Help shows the Help page (`FullPanelHelp`). Minimize hides the panel and Close closes the menu, as the wrist's buttons do (`WristButtons`). The help line says what each does while the ray is on it: "Minimize: Hides the menu, as you left it. Resume on your offhand wrist shows it again."
- Breadcrumbs: the last crumb in `green`, the others in `green-muted`, `»` (U+00BB) between them. Each crumb but the last is a 48 px target: `MODS CONFIG VR` clears the selection, as Back does; the mod opens its front page. When they do not fit, the mod's name shortens with an ellipsis first.
- Body: `ModTree` at `nav-width`, its rows scrolling under its header, a `space-4` gap, then the content pane: the rows in a child window that scrolls with the thumbstick, the page's own help as their first item in `green-muted`.
- Footer: Discard and Save on the right (`SaveBar`; for an MCM mod, the line "Changes are saved as they are made" instead), and on the left the help line (`DescriptionArea`): help for what the ray is on, else the status.
- The tree keeps its place and width. Selecting a mod opens the mod's front page, its root `content`, shown here; a page opens from a row under the mod (`FullPanelPage`). The menu's front page (`FullPanelHome`), which the menu opens on, is reached from the first breadcrumb.
- Back goes from the Help page to the page it was shown over, from a page to its mod's front page, and from a mod's front page to the menu's front page. On the menu's front page it closes the menu.
- Keep the panel world-locked and level. Do not follow the head.

Don't add a heading or a row of tabs above the content: the breadcrumbs and the tree already show where the player is, and the height belongs to settings.
