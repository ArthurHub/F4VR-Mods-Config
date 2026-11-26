# End-to-end tests

pytest tests that drive the running game through devbench's REST port and check the result in the menu, the files, the game and the mods. Run by hand; CI has no game. They run on devbench's Python client and its pytest plugin ([devbench-client](https://github.com/ArthurHub/devbench/tree/main/clients/python)), configured by `DEVBENCH` in `conftest.py`: the plugin enters the game, checks the builds, backs up the files and writes the report; the conftest adds the mods the menu must list and the menu fixture.

## Requirements

- The game running with [devbench](https://github.com/ArthurHub/devbench), at the main menu or with a game loaded (or closed, with a launch command: see Running). From the main menu the run starts a new game with `coc CTest`, an empty test cell, so nothing reaches a save; a game already loaded is used in whatever cell it is, and a run there can change that save's world, as the time of day. The session test leaves with a `coc CTest`.
- The mod deployed from this checkout: its version must be the one in `CMakeLists.txt`. FRIK and Immersive Flashlight need their devbench `config` action.
- Installed: SS2, FallUI and XDI from the MCM test set (see [mod-plan.md](../../docs/design/mod-plan.md#the-mcm-test-set)), both showcases, FRIK, Immersive Flashlight. A run fails at start listing what's missing. The rest of the test set is swept and measured when installed, and skipped otherwise.
- `pip install -r tests/e2e/requirements.txt`, which pins devbench-client by commit. Working on devbench too: `pip install -e <devbench>/clients/python`.

## Running

```powershell
pytest tests/e2e                      # all but perf, the sweep one setting of each kind
pytest tests/e2e -m full              # everything: perf, and the whole sweep of every mod
pytest tests/e2e -m showcase          # one area: sweep, showcase, dialog, mcm, live, controllers, keyboard, navigation, rows, session, search
pytest tests/e2e -m perf              # perf, which runs only when asked for (or test_perf.py is named)
pytest tests/e2e -m sweep -k FallUI   # one mod's whole sweep
pytest tests/e2e -m sweep -rA         # with the skipped settings
pytest tests/e2e --e2e-restore        # only restore a crashed run's backup
```

The devbench port comes from its `runtime.json`, or `--devbench-port`. Unattended, from a closed game to a report: set the launch command (your MO2 setup, never in the conftest) and quit at the end:

```powershell
$env:DEVBENCH_LAUNCH = '"C:\Games\MO2\ModOrganizer.exe" -p Default run -e "Fallout 4 VR (F4SEVR)"'
pytest tests/e2e --devbench-quit
```

## Backup

Before changing anything, a run backs up every file a Save can write (`Data\MCM\Settings`, `Data\ModsConfigFO4\Config`, `My Games\Fallout4VR\Mods_Config`, `FRIK_Config`; `DEVBENCH.backup`), read through devbench so MO2's virtual files are included, into `tests/e2e/.backup/manifest.json`. After the run, changed files are restored and created ones deleted. A crashed run leaves the manifest, and the next run (or `--e2e-restore`) restores it first; the game must be running.

## Report

`build/e2e/<date>-<time>/index.html`: totals, the builds checked, failures, per-test results and times by area, each mod's sweep (skipped, failed, known), the perf tables (`perf.json` alongside), dialogs the run closed, and a picture of the panel at the end of each test. A failed test also gets the open menus, the lines `ModsConfigFO4VR.log` gained during it, and a picture of the game's lit frame.

A setting whose failure is an open issue goes in `KNOWN` in `test_sweep.py` with the issue link; remove it when the issue closes.

## Tests

| File | Covers |
| --- | --- |
| `test_sweep.py` | Every changeable setting of the test set, showcases and config specs: change, save, check the file, set back. `-m sweep`, `-m full` or naming the file sweeps every setting of every mod; any other run sweeps one setting of each kind (type, options, where it is kept, whether its file leaves its default out) of each mod of `SUBSET`, the showcases, config specs, SS2, FallUI and XDI. One row of the Categories page is swept, as its rows are one setting made per mod. |
| `test_showcase.py` | Each section of the [showcase walkthrough](../../docs/for-mod-authors/showcase.md). |
| `test_dialog.py` | The unsaved-changes dialog on leaving a mod and closing the menu. |
| `test_mcm_keep_revert.py` | MCM's immediate writes, Keep and Revert. |
| `test_live_preview.py` | FRIK's and Immersive Flashlight's live preview; the menu's own client handing an edit back. |
| `test_controllers.py` | The menu worked with the controllers, which devbench's input tool holds: the opening gesture, the trigger on tree and page rows, the help line under the ray, the color editor with Back on the grip, Back only with the ray on the panel, the thumbstick scrolling the page and the tree and stepping a slider and a stepper, the stick turning the player only off the panel, and a left-handed player's primary hand (skipped with ROCK loaded, which holds `bLeftHandedMode:VR` off). The headset and controllers are held while a test runs, so the physical ones do nothing then. |
| `test_keyboard.py` | The framework's keyboard: shown by a press on a text input or the search field, typed with the ray and through its devbench action, Done and Back on the grip keeping the text and stepping no page, the caret keys and a pull on the line moving the caret, the symbols and accents pages, Enter doing nothing on a one-line field, a number field dropping a letter, and the search filtering per key. |
| `test_navigation.py` | Back, breadcrumbs, Help, Minimize/Restore. |
| `test_rows.py` | Reset, Run without saving / Save and run, confirmations, MCM buttons, client state in text, MCM game texts, images and plugin requirements, FallUI translations, the config spec report. |
| `test_session.py` | A loading screen closing the menu and discarding pending changes and previews. |
| `test_search.py` | The search field: what a query finds in the mods and on the page, Back showing a filtered page whole, a row a condition hides, an MCM row found by its text from the game, the query emptied as the menu opens. |
| `test_perf.py` | Per-frame cost of held states (10 s each, fails over budget at p95) and the worst frame of one-off actions (no check). Only run when asked for: `-m perf`, right after game start to catch the session's first open. |

Helpers: `menu.py` (the menu through the mod's devbench tool), `stored.py` (a value in its file), `controllers.py` (the primary hand's ray aimed at a target of the panel, as the framework's devbench `targets` action tells them, by the kinds of [`src/ui/Targets.h`](../../src/ui/Targets.h) and the framework's `button`, `option` and `search`, or at a key of the keyboard, as its `keyboard` action tells them, and its presses). devbench-client gives the rest: the client, events, console, files, waits and the plugin's fixtures (`game`, `mod`, `events`, `report`).
