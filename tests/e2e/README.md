# End-to-end tests

pytest tests that drive the running game through devbench's REST port and check the result in the menu, the files, the game and the mods. Run by hand; CI has no game.

## Requirements

- The game running with [devbench](https://github.com/ArthurHub/devbench) (a build with its `files` tool), at the main menu or already in QASmoke. The tests `coc qasmoke` so nothing reaches a save; any other loaded game is refused.
- Installed: the MCM test set (see [mod-plan.md](../../docs/design/mod-plan.md#the-mcm-test-set)), both showcases, FRIK, Immersive Flashlight. A run fails at start listing what's missing.
- `pip install -r tests/e2e/requirements.txt`

## Running

```powershell
pytest tests/e2e                      # all
pytest tests/e2e -m showcase          # one area: sweep, showcase, dialog, mcm, live, navigation, rows, session, perf
pytest tests/e2e -m sweep -k FallUI   # one mod's sweep
pytest tests/e2e -m sweep -rA         # with the skipped settings
pytest tests/e2e --e2e-restore        # only restore a crashed run's backup
```

The devbench port comes from its `runtime.json`, or `--devbench-port`.

## Backup

Before changing anything, a run backs up every file a Save can write (`Data\MCM\Settings`, `Data\ModsConfigFO4\Config`, `My Games\Fallout4VR\Mods_Config`, `FRIK_Config`), read through devbench so MO2's virtual files are included, into `tests/e2e/.backup/manifest.json`. After the run, changed files are restored and created ones deleted. A crashed run leaves the manifest, and the next run (or `--e2e-restore`) restores it first; the game must be running.

## Report

`build/e2e/<date>-<time>/index.html`: totals, failures, per-test results and times, each mod's sweep (skipped, failed, known), dialogs the run closed, a panel snapshot per test, and the perf tables (`perf.json` alongside).

A setting whose failure is an open issue goes in `KNOWN` in `test_sweep.py` with the issue link; remove it when the issue closes.

## Tests

| File | Covers |
| --- | --- |
| `test_sweep.py` | Every changeable setting of the test set, showcases and config specs: change, save, check the file, set back. |
| `test_showcase.py` | Each section of the [showcase walkthrough](../../docs/for-mod-authors/showcase.md). |
| `test_dialog.py` | The unsaved-changes dialog on leaving a mod and closing the menu. |
| `test_mcm_keep_revert.py` | MCM's immediate writes, Keep and Revert. |
| `test_live_preview.py` | FRIK's and Immersive Flashlight's live preview; the menu's own client handing an edit back. |
| `test_navigation.py` | Back, breadcrumbs, Help, Minimize/Restore. |
| `test_rows.py` | Reset, Run without saving / Save and run, confirmations, MCM buttons, client state in text, MCM game texts, images and plugin requirements, FallUI translations, the config spec report. |
| `test_session.py` | A loading screen closing the menu and discarding pending changes and previews. |
| `test_perf.py` | Per-frame cost of held states (10 s each, fails over budget at p95) and the worst frame of one-off actions (no check). Run `-m perf` right after game start to catch the session's first open. |

Helpers: `devbench.py` (client), `menu.py`, `stored.py` (a value in its file and the game), `backup.py`, `report.py`.
