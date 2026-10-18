# Search

The search of the mods and their settings: the tree's header is the field, a query filters the tree, and the settings that match are a page of their own. It comes with the localization, after the first release (M91); until then the header says `Mods` and the count.

**Provides:** the query; the mods and pages with a setting that matches, how many on each page, and the mods whose name matches; the settings that match.

## The field

- It is the tree's header: 40 px high, the width of the rows, in `track` with a `frame-border` outline and `radius-frame`. A drawn magnifier (20 px), then the text.
- Idle it says what it searches, in `t-detail` `green-muted`: "Search 11 mods". Hover: `hover`, its text `green`.
- A press on it shows SteamVR's keyboard, as a [text input](../../../../controls.md#text-input) does. Each key filters as it is typed: the keyboard's text reaches the field as character events.
- Typing: a `green` outline, the query in `t-body` `green` with a caret, then how many settings match in `t-detail` `green-muted`, then Clear.
- With a query and the keyboard closed: the query in `green`, the count and Clear, with the `frame-border` outline.
- **Clear** is a 32 px icon button (`X`) at the right of the field. It empties the query and gives the tree back. Hover: `hover` with a `frame-border` outline.

## What it finds

- The names as the player sees them, in their language: a mod's display name, a page's, a section's heading, and a setting's text. Not the help.
- Each word of the query starts a word of the name, in any order, with case and accents ignored: "hol siz" finds "Holster size", and "ster" finds nothing.
- The settings of MCM mods too, by the text of their rows.
- A setting a condition hides is not found. One that a condition disables is found, and shown disabled.

## The tree while searching

- It has the mods with a match, alphabetical, flat even where the list is grouped by category, each expanded to its pages that have a setting that matches. A page's row has how many on the right, in `t-detail` `green-muted`, and a mod's row how many on its front page.
- A mod whose name matches is there with all its pages, closed.
- Where a word of the query starts a word of a row's name, it is underlined: a 2 px line in the text's color under those letters (`AddLine` from the start of the match to its end, at the bottom of the text).
- A press on a row goes to that page, with the query kept: its rows that match are underlined, and the tree stays filtered. The row is selected as any page's.
- Nothing matches: the tree says "No mod matches" in `t-body` `green-muted`.

## The results

While the query has text and no page of a result is selected, the content is the results: a page with the crumbs `MODS CONFIG VR » Search`.

- A line at the top in `green-muted`: "7 settings match “hol”. Change them here, or go to their page in the tree."
- The settings that match, under a heading for each page, `<mod> » <page>`, or the mod's name for its front page, in the order of the tree. Each is the setting's own row, with its control, its pending state and its Reset: a value is changed here as on its page, and Save saves it.
- The underline marks the match in each row's name.
- Nothing matches: "Nothing matches “holz”. Search finds the names of mods, pages, sections and settings."

## Back

- From a page reached from the results, Back goes to the results.
- From the results, Back empties the query and goes back to where the player was when they started typing.
- Clear empties the query and stays on the page that is shown; from the results, it goes back to where the player was.

The behavior is in [Search](../../../../ui-and-vr-interaction.md#search).
