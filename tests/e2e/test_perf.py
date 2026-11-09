"""The menu's cost per frame in each state it can be held in: the mod's perf sites over a window of HOLD_SECONDS, read
through its devbench tool. Each scenario's table goes into the run's report, and every site of each into perf.json
beside it. A scenario fails when the mod's whole frame takes more than its budget at p95.

`test_one_off` reports the worst frame of each action that is done once, as showing a page or a Save, with no check: a
hitch there is a single frame, which no p95 shows.

The ray on the panel, SteamVR's keyboard and an open editor are not held here, as devbench does not drive them yet.
"""

import time

import pytest

from conftest import CONFIG_SPEC_SHOWCASE, CONFIG_SPECS, MCM_SHOWCASE, TEST_SET
from devbench import wait_until
from menu import Menu, page_paths
from test_live_preview import CASES, setting_of
from test_sweep import candidates

pytestmark = pytest.mark.perf

# how long each state is held, and how long it settles before, as a page draws its images on its first frames
HOLD_SECONDS = 10
SETTLE_SECONDS = 1

# the mod's whole frame on the game thread, the site whose p95 a budget is of
FRAME_SITE = "ModBase::onFrameUpdateSafe"

# the budget of each state, in ms of the frame's p95: a little over what QASmoke measured, so a change that costs more
# fails; a budget that fails in a busier run is raised alone
CLOSED_MS = 0.03
MINIMIZED_MS = 0.06
OPEN_MS = 0.20
DRAGGED_MS = 0.45

# the frames a one-off action is given to show its cost in: a page is drawn the frame after it is shown
ONE_OFF_FRAMES = 30


def measure(menu, report, scenario, budget, during=None):
    """Hold the state the menu is in: settle, reset the perf sites, then hold it HOLD_SECONDS, calling during each
    time round if given, keep what the sites recorded in the report, and check the frame's p95 against budget."""
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
    p95 = sites["sites"][FRAME_SITE]["p95Ms"]
    assert p95 <= budget, f"{scenario}: the frame's p95 is {p95:.3f} ms, over its budget of {budget} ms"


def worst_frame(menu, action):
    """The perf sites while an action runs and ONE_OFF_FRAMES pass after it: reset, act, wait, read."""
    menu.state()
    time.sleep(0.3)
    menu.call("perf", reset=True)
    action()
    time.sleep(ONE_OFF_FRAMES / 90)
    return menu.call("perf", format="flat", reset=True)["sites"]


def test_one_off(game, report):
    """The worst frame of each action that is done once rather than held: opening, minimizing, the Help page, every
    mod and page shown, a Save and an MCM change. A report and no check. First, so on a game that just started the
    first open and the first minimize of the session are the ones measured: run `pytest tests/e2e -m perf` then."""
    menu = Menu(game)
    menu.close_all()
    lines = []

    def one(name, action):
        sites = worst_frame(menu, action)
        frame = sites.get(FRAME_SITE, {}).get("maxMs", 0.0)
        slow = sorted(((key, site["maxMs"]) for key, site in sites.items() if site["maxMs"] >= 0.5 and key != FRAME_SITE), key=lambda each: -each[1])[:3]
        lines.append((frame, name, ", ".join(f"{key} {ms:.2f}" for key, ms in slow)))

    one("open", menu.open)
    one("minimize", lambda: menu.call("minimize"))
    one("restore", lambda: menu.call("minimize", minimized=False))
    one("Help page", lambda: menu.call("help"))
    one("Help page left", lambda: menu.call("help", shown=False))
    mods = {mod["name"]: mod for mod in menu.mods()}
    for name in [*TEST_SET, MCM_SHOWCASE, CONFIG_SPEC_SHOWCASE, *CONFIG_SPECS]:
        one(f"show {name}", lambda name=name: menu.call("select", mod=name))
        for path in page_paths(mods[name]["pages"]):
            one(f"show {name} / {' / '.join(path)}", lambda name=name, path=path: menu.call("select", mod=name, page=path))

    menu.select(CONFIG_SPEC_SHOWCASE, ("Detailed Examples", "Switchers"))
    row = menu.setting("No help")
    menu.set(row["id"], not row["value"])
    one(f"Save of {CONFIG_SPEC_SHOWCASE}", menu.save)
    menu.set(row["id"], row["value"])
    menu.save()
    mod, _, page, path = CASES["FRIK"]
    menu.select(mod, page)
    row = setting_of(menu, path)
    menu.set(row["id"], next(candidates(row)))
    one("Save of FRIK", menu.save)
    menu.set(row["id"], row["value"])
    menu.save()
    menu.select(MCM_SHOWCASE, ("Sections and spacers",))
    switch = next(each for each in menu.rows() if each["kind"] == "setting" and each["type"] == "switcher" and each["enabled"])
    one("MCM change", lambda: menu.set(switch["id"], not switch["value"]))
    one("MCM Revert", menu.discard)
    one("close", lambda: menu.call("close"))

    lines.sort(key=lambda line: -line[0])
    text = "the mod's worst frame in ms, each action, slowest first, with its sites over 0.5 ms\n" + "\n".join(
        f"{frame:8.2f}  {name}{f'   ({slow})' if slow else ''}" for frame, name, slow in lines
    )
    report.add_perf("One-off actions", text, {"actions": [{"name": name, "frameMaxMs": frame, "slow": slow} for frame, name, slow in lines]})


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
    measure(menu, report, "Closed", CLOSED_MS)


def test_minimized(menu, report):
    menu.call("minimize")
    assert menu.state()["menu"]["minimized"]
    measure(menu, report, "Minimized", MINIMIZED_MS)


def test_front_page(menu, report):
    menu.call("select")
    measure(menu, report, "Front page", OPEN_MS)


@pytest.mark.parametrize("key", ["mcm", "showcase"])
def test_heaviest_page(menu, report, heaviest, key):
    mod, page = heaviest[key]
    menu.select(mod, page)
    measure(menu, report, f"Heaviest page: {mod} / {' / '.join(page)} ({len(menu.rows())} rows)", OPEN_MS)


def test_dialog(menu, report):
    menu.select(CONFIG_SPEC_SHOWCASE, ("Detailed Examples", "Switchers"))
    row = menu.setting("No help")
    menu.set(row["id"], not row["value"])
    assert menu.call("close", ask=True)["asking"]
    measure(menu, report, "The dialog asking", OPEN_MS)
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

    try:
        measure(menu, report, "FRIK's live preview, dragged", DRAGGED_MS, during=change)
    finally:
        menu.discard()
        wait_until(lambda: not menu.state()["session"]["dirty"], 3, what="the change discarded")
        report.notes.append(f"FRIK's live preview: {changes} changes in {HOLD_SECONDS} s.")
