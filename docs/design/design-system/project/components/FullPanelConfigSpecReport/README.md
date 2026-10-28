# FullPanelConfigSpecReport

A config spec's report: the file it is read from, its live copy, and its problems, errors first.

- The crumbs are `MODS CONFIG FO4 » Developer » <mod>`. `Developer` goes to the menu's front page, scrolled to its Developer section, and so does Back. No mod is selected in the tree, as on the front page.
- The page's help says where `spec.json` is read from: where it is installed, `Data\ModsConfigFO4\Config\<folder>\spec.json`, or its live copy, `My Games\Fallout4VR\Mods_Config\ModsConfigFO4VR\Dev\<folder>\spec.json`.

## Live copy

- **Read the live copy** is a switch. On copies the config spec's `spec.json` to its live copy, when there is none, and reads the copy in its place. The installed file is not changed. Off reads the installed file again, and keeps the copy, so turning it on again keeps the author's edits.
- The live copy is watched, and read again each time it is saved. Only `spec.json` is copied: the config spec's other files, its images and its translations are read from where they are installed.
- Under the switch, **Live spec.json** is a section in a section: a heading that folds, a level in, with its rule down its rows while it is open, as a [SectionHeader](../SectionHeader/README.md) in a section has. It is folded each time the menu opens. Its help is "Reload, overwrite, edit and reveal the live copy.". Its rows are [button rows](../ButtonRow/README.md), disabled while the switch is off, and their help then says the switch has to be on.
- **Reload** (`Reload`) reads the copy again now. Each read, by the button or by a save, is the result of the button: the info mark after its name until the ray leaves the row after being on it, and in the help line "Reloaded at 10:42:05." or "Reloaded at 10:42:05: 2 unsaved changes were dropped." A copy that cannot be read puts the invalid mark there instead, and the help line has, in `red` after the invalid mark, "Read at 10:42:31: the live copy cannot be read. The menu shows the copy it read last."
- **Re-copy the installed spec.json** (`Overwrite`) copies the installed `spec.json` over the copy, which loses the copy's edits, and reads it. While the installed file is newer than the copy, the row has the warning mark ([RowWarning](../RowWarning/README.md)), and the help line has "The installed spec.json is newer than the live copy." in `amber` before its help. Folded, the heading of Live spec.json says it at the end of its rule: the warning mark and `Installed is newer` in `amber`, `t-detail`, and its help line has the warning too.
- **Edit on desktop** (`Open`) opens the live copy in the desktop's editor for `.json`, and **Reveal in file explorer** (`Open`) opens its folder with the file selected. The live copy is the file to edit.
- A read drops the mod's unsaved changes, without asking: the author is the one changing its settings.
- The mod's row, its pages and the page that is open show the config spec as it was read, and that page stays open while it is still there.
- A live copy that cannot be read is an error at the top of the report, with its line and column in the help line. The mod keeps the config spec it read last, so the page the author is on stays.
- A config spec that is not loaded has the switch too, so its author can fix the copy while the game runs.

## Problems

- `2 errors`, then `5 warnings`: a heading that folds, as a [SectionHeader](../SectionHeader/README.md) that folds does, with its count in its text, and under it each problem's row ([Developer](../Developer/README.md)), in the order of `spec.json`. Both are open as the report opens, and stay as the author left them while the menu is open. A heading with none is left out, and a config spec with no problem says `No problems` after the page's help.
- Nothing is edited here: Discard and Save stay disabled. The switch and the buttons act at once.
