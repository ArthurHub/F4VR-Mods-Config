"""The menu's cost per frame in each state it can be held in: the mod's perf sites over a window of HOLD_SECONDS, read
through its devbench tool. A report and no check: each scenario's table goes into the run's report, and every site of
each into perf.json beside it.

The ray on the panel, SteamVR's keyboard and an open editor are not held here, as devbench does not drive them yet.
"""

import time

import pytest

from conftest import CONFIG_SPEC_SHOWCASE, TEST_SET
from devbench import wait_until
from menu import Menu, page_paths
from test_live_preview import CASES, setting_of
from test_sweep import candidates

pytestmark = pytest.mark.perf

# how long each state is held, and how long it settles before, as a page draws its images on its first frames
HOLD_SECONDS = 10
SETTLE_SECONDS = 1


def measure(menu, report, scenario, during=None):
    """Hold the state the menu is in: settle, reset the perf sites, then hold it HOLD_SECONDS, calling during each
    time round if given, and keep what the sites recorded in the report."""
    menu.state()
    time.sleep(SETTLE_SECONDS)
    menu.call("perf", reset=True)
    deadline = time.monotonic() + HOLD_SECONDS
    while time.monotonic() < deadline:
        if during:
            during()
        else:
            time.sleep(0.1)
    text = menu.call("perf", format="text")
    sites = menu.call("perf", format="flat", reset=True)
    report.add_perf(scenario, text["text"], sites)


def rows_of(menu, mod, page):
    menu.select(mod, page)
    return len(menu.rows())


@pytest.fixture(scope="module")
def heaviest(game):
    """The page with the most rows of the MCM test set, and of the config spec showcase."""
    menu = Menu(game)
    menu.close_all()
    menu.open()
    mods = {mod["name"]: mod for mod in menu.mods()}
    found = {}
    for key, names in (("mcm", TEST_SET), ("showcase", [CONFIG_SPEC_SHOWCASE])):
        pages = [(name, path) for name in names for path in page_paths(mods[name]["pages"])]
        found[key] = max(pages, key=lambda page: rows_of(menu, *page))
    menu.close_all()
    return found


def test_closed(menu, report):
    menu.close_all()
    assert not menu.state()["menu"]["open"]
    measure(menu, report, "Closed")


def test_minimized(menu, report):
    menu.call("minimize")
    assert menu.state()["menu"]["minimized"]
    measure(menu, report, "Minimized")


def test_front_page(menu, report):
    menu.call("select")
    measure(menu, report, "Front page")


@pytest.mark.parametrize("key", ["mcm", "showcase"])
def test_heaviest_page(menu, report, heaviest, key):
    mod, page = heaviest[key]
    menu.select(mod, page)
    measure(menu, report, f"Heaviest page: {mod} / {' / '.join(page)} ({len(menu.rows())} rows)")


def test_dialog(menu, report):
    menu.select(CONFIG_SPEC_SHOWCASE, ("Detailed Examples", "Switchers"))
    row = menu.setting("No help")
    menu.set(row["id"], not row["value"])
    assert menu.call("close", ask=True)["asking"]
    measure(menu, report, "The dialog asking")
    menu.answer("discard")


def test_live_preview(menu, report):
    """FRIK previewing a live setting that is dragged: changed as fast as devbench takes it, between two values that are
    not its saved one, as a set back to that is a Discard, which is told at once."""
    mod, _, page, path = CASES["FRIK"]
    menu.select(mod, page)
    row = setting_of(menu, path)
    values = list(candidates(row))[:2]
    assert len(values) == 2, f"{row['id']} has no two values to drag between"
    changes = 0

    def change():
        nonlocal changes
        changes += 1
        menu.call("setValue", id=row["id"], value=values[changes % 2])

    measure(menu, report, "FRIK's live preview, dragged", during=change)
    menu.discard()
    wait_until(lambda: not menu.state()["session"]["dirty"], 3, what="the change discarded")
    report.notes.append(f"FRIK's live preview: {changes} changes in {HOLD_SECONDS} s.")
