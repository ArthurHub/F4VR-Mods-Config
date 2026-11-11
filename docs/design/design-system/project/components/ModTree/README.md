# ModTree

The navigation: every mod in one alphabetical list, with the selected mod expanded to its pages, and a config spec's page that has pages of its own expanded to them.

**Provides:** each mod's display name, page names (with the pages under each, and whether each is shown and enabled) and availability; what is selected: a mod's front page, a page, or nothing while the menu's front page is open.

- Fixed at `nav-width` in a `plate` pane, in the same place with or without a selected mod.
- A `t-detail` `green-muted` header says `Mods` and the count. The menu's front page has no row of its own: the first breadcrumb, `MODS CONFIG FO4`, is its entry, and is selected while it is open. No tree row is selected then.
- Rows are `control-height` tall, `t-body`.
- A mod's own row is its front page: it has no separate page row. Only the selected mod is expanded.
- Selected: solid `green`, `on-green` text, ending in a tip inside the row that points at the content: it starts 10 px from the row's right edge and points at the edge's middle. Draw the bar and its tip as one shape, so no seam shows.
- Hover: `hover` with a `frame-border` outline.
- An MCM mod has the `MCM` tag at the right of its row, as its change is made in the game as the player makes it, where a config spec's waits for Save: its text 10 px in `green`, 1 px inside a `border-frame` outline in `rule`, 8 px from the row's right edge, inside the row, so its hover look has it. On the selected bar the text and the outline are `on-green`, and the tip starts over the tag. The name ends in an ellipsis `space-2` before it, so the tags of the rows line up. While the ray is on the row the help line says "An MCM menu: a change is made in the game as you make it. Keep accepts it, and Revert sets it back." A config spec's mod has no tag.
- Not available (a required plugin is not loaded, or the config cannot be read): disabled at `opacity-disabled`, in its place by name. It is not selected and has no pages. The reason is in the help line while the ray is on the row ("Sim Settlements 2: Not available: its plugin SS2.esm is not loaded.") and in the menu front page's notices.

## Levels

- Each level is set in by `space-4`, up to the third: a mod's pages are a level in, a config spec's page's pages a level further. A level past the third is set in as the third, so a name keeps its room.
- Every row keeps a 16 px column for a drawn 14 px arrow before its name, and the name starts 6 px after it, so the names of a level line up whether or not they have one.
- A mod always has its arrow: down while its pages are the rows under it, right otherwise. A mod with no pages has nothing to open, so its arrow stays right when it is selected.
- A page has an arrow only when it has pages of its own: down while it or one of its pages is selected, and its pages are the rows under it, and right otherwise. Of the selected mod's pages, only the ones on the way to the selected page are expanded.
- While the ray is on a page, the help line shows the page's help.

## Pages with conditions

A config spec's page can have a `visibleWhen` and an `enabledWhen`, which go down to the pages under it.

- A page whose `visibleWhen` does not hold is not in the tree, with the pages under it.
- A page whose `enabledWhen` does not hold is in its place, disabled: its name at `opacity-disabled`, with its arrow, closed, when it has pages. It is not pressed, and its pages are not shown. Under the ray it has a row's hover look, and the help line says "This page is disabled." before the page's help.
- An MCM page whose own `pluginRequirements` are not all loaded is disabled the same way, and the help line says "Not available:" and the reason, as for a mod, before its `messageIfMissingReqs`. One with `hideIfMissingReqs` is not in the tree.
- When the page that is shown comes to be hidden or disabled, as a setting changes, the menu shows the nearest page above it that is not, or the mod's front page. Its unsaved changes stay, and so does the Help page.

## Categories

From `iCategoriesMinMods` mods (20 as shipped; 0 groups any list) the categories are the top level of the tree and each mod is under its category.

- A category row is `control-height` tall, in `green-muted`: a drawn arrow, the name, a `rule` line to the count, and the count in `t-detail`. The rule and the muted text set it apart from a mod, which is `green` with no rule.
- Pressing it opens or closes it, and several can be open. It is never selected: it has no page. Hover is `hover` with a `frame-border` outline; the text stays `green-muted`, as a heading's does.
- Mods under a category are a level in, and their pages a level more.
- Categories are alphabetical, and so are their mods. `No category` is the last row. A category with no mod is not shown.
- The category of the selected mod is open. The category is not a breadcrumb, and Back does not stop at it.
- Which categories are open is kept until the game closes.

## Scrolling

About nine rows fit in the Full panel's tree, so most lists scroll: a flat list of up to 19 mods, twelve closed categories, or one category opened.

- The `Mods` header stays. The rows scroll under it with the thumbstick while the ray is on the tree.
- While the rows are longer than their area, a `scrollbar-size` bar shows on the right of the pane, styled as the content pane's: `track`, with a `frame-border` grab.
- Its room is always kept. The pane's right padding is 24 px: `space-1`, `scrollbar-size`, `space-1`. Rows are a fixed 304 px wide (`nav-width` minus both paddings), so nothing moves when the bar appears or goes.
- In ImGui: the rows in a child window, each drawn at that fixed width rather than `GetContentRegionAvail().x`, which narrows by `ScrollbarSize` when the bar appears. Not `ImGuiWindowFlags_AlwaysVerticalScrollbar`: it draws a full-length grab when nothing scrolls.
- A row selected from outside the tree (a breadcrumb, Back, the panel shown again) is scrolled into view: `SetScrollHereY()` on it when it is outside the visible rows. A category opened near the bottom scrolls to show its first mods, and the selected mod's category, opened again, to show the selection.

Don't group or color mods by where their menu comes from (MCM or a config spec): the `MCM` tag is the one mark, as an MCM change is made at once.
