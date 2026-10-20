# Developer

Developer tools, for a package's author: a switch and a filter in Mods Config VR's own settings, a Developer section on the menu's front page with a row for each package it lists, and each package's report of its problems.

**Provides:** each package's name, its errors and warnings (the message, the page and the row it is on, and the path in `config.json`, or the line and column of a file that cannot be read), whether it is read from its live copy, and when that was last read.

## Turning it on, and which packages

On Mods Config VR's own page, saved to `ModsConfigVR.toml`:

- **Developer tools**, a switch, off as shipped. While it is off nothing here is drawn, so a player never sees it. The log has every problem either way.
- **Show packages**, a stepper: `Needing attention` (as shipped), `All`, or `Only one`.
  - `Needing attention`: a package with an error or a warning, one read from its live copy, and one that is not loaded.
  - `All`: every package the menu found, those not available too.
  - `Only one`: the package of **Package** alone, with or without problems, for the one the author is working on.
- **Package**, a dropdown of the packages the menu found, alphabetical: it stores a package's `modName` and shows its name. Its options come from the menu itself, with `listFromClientCallback`. It is disabled while Show packages is not `Only one`.

An MCM mod is never listed.

## The Developer section

On the menu's front page, after Notices; see [FullPanelDeveloper](../FullPanelDeveloper/README.md).

- Its heading folds, as a [SectionHeader](../SectionHeader/README.md) that folds does: its arrow, `Developer` in `green-muted`, and a heading's rule. Folded, the rule ends with what it hides, in `t-detail`: the invalid mark and `2 errors` in `red`, then the warning mark and `8 warnings` in `amber`, counted over the packages it lists. Whether it is folded is kept with the filter, so it stays folded.
- Its first row is the filter: `Show`, with how many of the packages it lists in a `t-detail` tag (`3 of 12`), and a choice stepper of Show packages. Its options are `Needing attention`, `All`, and the name of the Package, for `Only one`. A press writes it to the config at once, as a preference of the view: the front page has no editing session, and nothing waits for Save. With no Package chosen, the stepper skips that option.

## A package's row

- Laid out as a setting's row: the mod's name, and in the control column, right-aligned:
  - the `Live copy` tag while it is read from its live copy, `t-detail` outlined in `frame-border`, as the Restart mark is;
  - its errors: the invalid mark and `2 errors` in `red`;
  - its warnings: the warning mark ([RowWarning](../RowWarning/README.md)'s hollow `amber` triangle) and `5 warnings` in `amber`;
  - or `No problems` in `green-muted`. A package that cannot be read has the invalid mark and `Not loaded` in `red`, and one whose requirements are missing `Not available` in `green-muted`.
- The Reset slot has a drawn 14 px arrow: a press anywhere on the row opens the package's report ([FullPanelPackageReport](../FullPanelPackageReport/README.md)). It has the hover look of any row that is pressed.
- The help line says the same in words, then "Opens its report.". For a package that cannot be read it is the reason, in `red` with the invalid mark, as an error is.

## A problem

A row of the report, as any other row: one line.

- `control-height` tall, across the content: the mark in the `space-6` gutter, and the message in `t-body` `green`, which shortens with an ellipsis.
- The mark is the invalid mark for an error, the warning mark for a warning, and the info mark for something the menu did, as a reload.
  - An **error** leaves something out: the whole package, when `config.json` cannot be read, or a part of it, as the second of two settings with one id, or a setting with no storage.
  - A **warning** is read but likely not meant: an unknown field (a typo, or a field of a newer schema), a condition that names no setting, a `$` token with no translation and no fallback.
  - The message says what the menu did with it: "it is not read", "the row is left out", "the row is shown as if it had none".
- While the ray is on it, the help line has all of it: the name of the row it is about, or `config.json` for the file, then the message, then where it is: the page and the row (`Beam › Beam angle`) and the path in `config.json` (`pages[0].content[4]`), or the line and column. Only a file that cannot be read has the help line in `red` with the invalid mark, as a validation error has it: any other problem is told as a row's help is.
- A problem that names a row that is drawn has an arrow on the right: a press opens the mod at that page, with the row scrolled into view and focused. Back comes back to the report. One that names no row, or a row that was left out, has no arrow, and a press does nothing.
- Each problem is a line in the log too, with the package's name first.
