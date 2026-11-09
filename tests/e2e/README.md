# End-to-end tests

Python tests that drive the running game through devbench's REST port, as the player's presses drive the menu, and check what the menu did in the menu, in the files, in the game and in the mods. They are run by hand, as CI has no game.

## What they need

- The game running, with [devbench](https://github.com/ArthurHub/devbench) installed, of a build that has its `files` tool.
- The MCM test set (the MCM mods of the author's Mad God Overhaul list), both showcases, FRIK and Immersive Flashlight installed. A run fails at its start with what is missing.
- The game at the main menu, where the tests start a new game in QASmoke with `coc qasmoke`, so nothing they do reaches a save. A game already in QASmoke is used as it is, and any other is refused.
- `pip install -r tests/e2e/requirements.txt`

## Running them

```powershell
pytest tests/e2e                      # all of them
pytest tests/e2e -m showcase          # one area: sweep, showcase, dialog, mcm, live, navigation, rows, session, perf
pytest tests/e2e -m sweep -k FallUI   # the sweep of one mod
pytest tests/e2e -m sweep -rA         # the sweep, with the settings it skipped
```

devbench's port is the one its `runtime.json` has, or `--devbench-port`.

## The report

A run ends with the link of its report, `build/e2e/<date>-<time>/index.html`: the totals, each failure with its message, each test's result and time, the sweep of each mod with the settings it skipped, failed and knew to fail, and the game's dialogs the run closed. Each test that had the menu open leaves a picture of the panel as it ended, taken with the mod's devbench `snapshot`, beside the report, and the report shows them. The perf tests add the cost per frame of each state they held, as the mod's `perf` action's table, and every site of each in `perf.json` beside the report.

A setting whose failure is an open issue is in `KNOWN` in `test_sweep.py`, with the issue's link: the sweep lists it as known, and does not fail on it. Take it out when its issue is closed.

## The files they change

Before a run changes anything, it backs up every file a Save can write: MCM's `Data\MCM\Settings`, the config specs' folders in `Data\ModsConfigFO4\Config`, and `My Games\Fallout4VR\Mods_Config` and `FRIK_Config`. It reads them through devbench, so it sees them as the game does, MO2's virtual files included. The backup is `tests/e2e/.backup/manifest.json`, written in full before the run starts. After the run it is put back: a file that differs is written back, a file the run created is deleted, and the manifest is removed.

A run that a crash ended leaves the manifest, and the next run puts it back before it starts. `pytest tests/e2e --e2e-restore` puts it back and runs nothing; the game has to be running for either.

## What they test

| File                      | What                                                                                                                                                             |
| ------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `test_sweep.py`           | Each mod and page of the test set, the showcases and the config specs. Each setting the player can change is changed, saved, checked in its file, then set back. |
| `test_showcase.py`        | Each section of the [showcase walkthrough](../../docs/for-mod-authors/showcase.md): what it tells the player to do, and what it says happens.                    |
| `test_dialog.py`          | The dialog that asks about unsaved changes, as a mod is left and the menu is closed: Cancel, Discard and Save.                                                    |
| `test_mcm_keep_revert.py` | An MCM change made as it is made, in MCM's file and in the game, and the Keep and Revert of the Save bar and the dialog.                                          |
| `test_live_preview.py`    | FRIK's and Immersive Flashlight's live preview, in the mod through its devbench tool, and the menu's own client handing an edit back.                             |
| `test_navigation.py`      | Back one step at a time, the breadcrumbs of the title bar, the Help page, and Minimize and Restore, with the dialog that asks as a mod with changes is left. |
| `test_rows.py`            | Reset, Run without saving and Save and run, a confirmation, an MCM button, a client's state in a text, an MCM row's text from the game, its images and its page's plugin requirements, FallUI's translated texts, and a config spec's report. |
| `test_session.py`         | A loading screen closes the menu without asking, and discards a config spec's pending change with its preview. |
| `test_perf.py`            | The menu's cost per frame, each state held 10 s: closed, minimized, the front page, the page with the most rows of the test set and of the showcase, the dialog asking, and FRIK's live preview. A report, with no check yet. |

`devbench.py` is the client of devbench, `menu.py` the menu, `stored.py` a value in its file and in the game, `backup.py` the backup, and `report.py` the report.
