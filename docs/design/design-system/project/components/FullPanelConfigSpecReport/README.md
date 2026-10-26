# FullPanelConfigSpecReport

A config spec's report: the file it is read from, its live copy, and its problems, errors first.

- The crumbs are `MODS CONFIG FO4 » Developer » <mod>`. `Developer` goes to the menu's front page, scrolled to its Developer section, and so does Back. No mod is selected in the tree, as on the front page.
- The page's help says where `spec.json` is read from: where it is installed, `Data\ModsConfigFO4\Config\<folder>\spec.json`, or its live copy, `My Games\Fallout4VR\Mods_Config\ModsConfigFO4VR\Dev\<folder>\spec.json`.

## Live copy

- **Read the live copy** is a switch. On copies the config spec's `spec.json` to its live copy, when there is none, and reads the copy in its place. The installed file is not changed. Off reads the installed file again, and keeps the copy, so turning it on again keeps the author's edits.
- The live copy is watched, and read again each time it is saved. Only `spec.json` is copied: the config spec's other files, its images and its translations are read from where they are installed.
- **Reload** is a [button row](../ButtonRow/README.md) that reads it again now. Each read, by the button or by a save, is the result of the button: the info mark after its name until it is read, and in the help line "Reloaded at 10:42:05: 2 unsaved changes were dropped."
- **Edit spec.json** (`Open`) opens the live copy in Windows' editor for `.json`, and **Its folder** (`Show`) opens its folder in Explorer, with the file selected. Both are on the desktop, and are disabled while the switch is off: the live copy is the file to edit.
- A read drops the mod's unsaved changes, without asking: the author is the one changing its settings.
- The mod's row, its pages and the page that is open show the config spec as it was read, and that page stays open while it is still there.
- A live copy that cannot be read is an error at the top of the report, with its line and column in the help line. The mod keeps the config spec it read last, so the page the author is on stays.
- A config spec that is not loaded has the switch too, so its author can fix the copy while the game runs.

## Problems

- `2 errors`, then `5 warnings`: a heading that folds, as a [SectionHeader](../SectionHeader/README.md) that folds does, with its count in its text, and under it each problem's row ([Developer](../Developer/README.md)), in the order of `spec.json`. Both are open as the report opens, and stay as the author left them while the menu is open. A heading with none is left out, and a config spec with no problem says `No problems` after the page's help.
- Nothing is edited here: Discard and Save stay disabled. The switch and the buttons act at once.
