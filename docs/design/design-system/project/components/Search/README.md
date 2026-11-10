# Search

The search of the mods and their settings: the tree's header is the field, and a query filters the tree and the page that is shown.

**Provides:** the query; the mods and pages with a setting that matches, how many on each page, and the mods whose name matches; the rows that match of the page that is shown.

## The field

- It is the tree's header: 40 px high, the width of the rows, in `track` with a `frame-border` outline and `radius-frame`. A drawn magnifier (20 px), then the text.
- Idle it says what it searches, in `t-detail` `green-muted`: "Search 11 mods". Hover: `hover`, its text `green`.
- A press on it shows SteamVR's keyboard, as a [text input](../../../../controls.md#text-input) does. Each key filters as it is typed: the keyboard's text reaches the field as character events. Done and the keyboard's close button both keep the query.
- Typing: a `green` outline, the query in `t-body` `green` with a caret, then how many settings match in `t-detail` `green-muted`, then Clear.
- With a query and the keyboard closed: the query in `green`, the count and Clear, with the `frame-border` outline.
- **Clear** is a 32 px icon button (`X`) at the right of the field. It empties the query and gives the tree back. Hover: `hover` with a `frame-border` outline.

## What it finds

- The names as the player sees them, in their language: a mod's display name, a page's, a section's heading, and the text of a setting, a button and a hotkey. Not the help, and not a text row.
- Each word of the query starts a word of the name, in any order, with case and accents ignored: "hol siz" finds "Holster size", and "ster" finds nothing. A word of Chinese, Japanese or Korean matches anywhere in the name.
- A word can match the name of the mod, the page or the section a row is in, with one word matching the row's own: "frik height" finds FRIK's Height.
- The most specific match wins: a section, a page or a mod whose name matches shows all that is under it only when nothing under it matches.
- The settings of MCM mods too, by the text of their rows.
- A setting of the selected mod that a condition hides is not found. Another mod's conditions hold, as its values are not read.

## The tree while searching

- It has the mods with a match, alphabetical, flat even where the list is grouped by category, each expanded to its pages that have a match. A page's row has how many on the right, in `t-detail` `green-muted`, before the `MCM` tag, and a mod's row how many on its front page.
- A mod found by its name alone is there closed, with how many in all, until it is selected.
- Where a word of the query starts a word of a row's name, it is underlined: a 2 px line in the text's color under those letters, at the bottom of the text, which stops where a shortened name ends in its ellipsis.
- A press on a row goes to that page, with the query kept, and the tree stays filtered. The row is selected as any page's, and the page has its rows that match.
- Nothing matches: the tree says "No mod matches" in `t-body` `green-muted`.

## The page while searching

- The page that is shown has only its rows that match, a setting, a button and a hotkey, in the sections they are in. A section that folds is open, with a heading that does not fold. A text row, a spacer and an image are not there.
- Its crumb says so after its name, in `disabled`: `Body stow (filtered)`. With the ray on no row, the help line says "Only the settings that match are shown. Back shows the whole page.", unless the mod's session has changes to tell.
- Each row is the setting's own, with its control, its pending state and its Reset: a value is changed there as on the whole page, and Save saves it. The match is underlined in its name, and in a section's heading, as in the tree. A row whose label is formatted, as some MCM rows are, has no underline.
- Where the page has none of them, as the menu's front page, a line at the top in `green-muted` in place of the rows: "7 settings match “hol”. Pick a page in the tree to change them."
- Nothing matches: "Nothing matches “holz”. Search finds the names of mods, pages, sections and settings."
- A page whose rows that match are all hidden by a condition, which the tree does not have: "What matches “intens” on this page is hidden by the values of other settings. Back shows the whole page."

## Back

- Back shows the page whole, with the query kept and the tree filtered, and the focus ring on the last row the ray was on, in its sections opened. The rows that match keep their underline, and the crumb has no `(filtered)`. The next Back goes back as with no query.
- Clear empties the query and shows the page whole the same way, and so does Back while the ray is on the field.
- A new query filters the page again, from its top.

The behavior is in [Search](../../../../ui-and-vr-interaction.md#search).
