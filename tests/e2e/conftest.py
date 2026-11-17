"""The session the end-to-end tests run in. devbench's pytest plugin enters the game in QASmoke, checks the mod's build,
FRIK and Immersive Flashlight, backs up the files a Save writes and writes the report; this conftest adds the mods the
menu must list, the menu itself, and a picture of the panel at the end of each test."""

import re
from pathlib import Path

import pytest

from devbench import DevbenchError, ModTool, close_dialogs
from devbench.testing import Area, Config, Dependency
from menu import MOD_TOOL, Menu

pytest_plugins = ["devbench.testing"]

ROOT = Path(__file__).resolve().parents[2]

# the MCM test set: the MCM mods of the author's Mad God Overhaul list, by their modName
TEST_SET = [
    "AutoEat",
    "Chemfluence AI Combat Dynamics",
    "FallUI",
    "FallUIWorkbench",
    "NPCs Use Items",
    "SS2",
    "UnlimitedSurvivalMode",
    "WorkshopFramework",
    "XDI",
]
# the mods of the test set a run needs, as the sweep of any run covers what the rest repeats: SS2's globals and
# properties, FallUI's ModSettings, text inputs and translations, XDI's floats; the rest are swept and measured when
# installed
REQUIRED_MCM = ["SS2", "FallUI", "XDI"]
MCM_SHOWCASE = "ModsConfigFO4_McmShowcase"
CONFIG_SPEC_SHOWCASE = "ModsConfigFO4_Showcase"
# the config specs of the mod, of FRIK and of Immersive Flashlight
CONFIG_SPECS = ["ModsConfigFO4VR", "FRIK", "ImmersiveFlashlightVR"]


def checkout_version():
    """The mod's version in this checkout's CMakeLists.txt, which the deployed build must report."""
    return re.search(r"^set\(VERSION ([^)]+)\)", (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"), re.MULTILINE).group(1)


DEVBENCH = Config(
    game="fallout4",
    vr=True,
    tool=MOD_TOOL,
    version=checkout_version,
    # every file the tests can make a Save write: MCM's ModSettings and hotkeys, the config specs' files in Data, and
    # the mods' files in My Games
    backup=[
        ("data", "MCM/Settings/*"),
        ("data", "ModsConfigFO4/Config/*/*"),
        ("mygames", "Mods_Config/**"),
        ("mygames", "FRIK_Config/*"),
    ],
    log="ModsConfigFO4VR.log",
    # the areas, by the file of their tests, in the order they run and the page shows them. Each file's tests carry its
    # marker (-m <marker>); perf runs only when asked for (pytest_collection_modifyitems)
    areas={
        "test_showcase.py": Area("Showcase walkthrough", "showcase"),
        "test_navigation.py": Area("Navigation", "navigation"),
        "test_rows.py": Area("Rows", "rows"),
        "test_search.py": Area("Search", "search"),
        "test_session.py": Area("The editing session", "session"),
        "test_dialog.py": Area("The dialog", "dialog"),
        "test_mcm_keep_revert.py": Area("MCM Keep and Revert", "mcm"),
        "test_live_preview.py": Area("Live preview", "live"),
        "test_sweep.py": Area("Sweep", "sweep"),
        "test_perf.py": Area("Perf", "perf"),
    },
    # the live preview tests read their config overrides through each mod's config action
    requires=[Dependency("frik", actions=["config"]), Dependency("immersiveflashlightvr", actions=["config"])],
    title="Mods Config FO4 VR",
    # the lit frame, dumped when a test fails: the panel in the world
    picture=6,
)


def pytest_collection_modifyitems(config, items):
    """
    Leave out the perf tests unless a run asks for them, by a `-m` that names perf or by naming test_perf.py: they hold
    each state for seconds, and measure best right after the game starts.
    """
    if "perf" in (config.getoption("markexpr") or "") or any("test_perf" in arg for arg in config.args):
        return
    perf = [item for item in items if item.get_closest_marker("perf")]
    if perf:
        config.hook.pytest_deselected(items=perf)
        items[:] = [item for item in items if not item.get_closest_marker("perf")]


def listed_mods(bench):
    """The mods the menu lists, by their name."""
    return {mod["name"]: mod for mod in ModTool(bench, MOD_TOOL).call("mods")["mods"]}


def _check_installed(bench):
    """Stop the run when a mod the tests need is not listed, or is listed but not available: the required mods of the
    test set, the showcases and the config specs."""
    listed = listed_mods(bench)
    missing = [name for name in [*REQUIRED_MCM, MCM_SHOWCASE, CONFIG_SPEC_SHOWCASE, *CONFIG_SPECS] if name not in listed]
    if missing:
        pytest.exit(f"the menu does not list {', '.join(missing)}: install them", returncode=2)
    unavailable = [f"{name} ({listed[name]['notAvailable']})" for name in REQUIRED_MCM + CONFIG_SPECS if listed[name]["notAvailable"]]
    if unavailable:
        pytest.exit(f"not available: {', '.join(unavailable)}", returncode=2)


@pytest.fixture(scope="session")
def game(game):
    """The plugin's game (in QASmoke, the mod's build checked, the files backed up), with every mod the tests need
    listed; the menu is closed, its changes discarded, before the plugin puts the files back."""
    _check_installed(game)
    yield game
    Menu(game).close_all()


@pytest.fixture(scope="session")
def test_set(game, report):
    """The mods of the MCM test set this run has: the required ones, and each other one the menu lists and can show.
    The report notes the ones left out."""
    listed = listed_mods(game)
    absent = [name for name in TEST_SET if name not in listed or listed[name]["notAvailable"]]
    if absent:
        report.note(f"Not installed or not available, so neither swept nor measured: {', '.join(absent)}.")
    return [name for name in TEST_SET if name not in absent]


def _picture(menu, report, nodeid):
    """Save a picture of the panel for the report, while it is shown."""
    state = menu.state()
    if not state["menu"]["open"] or state["menu"]["minimized"]:
        return
    path = report.picture(nodeid, label="the panel")
    try:
        menu.call("snapshot", path=str(path))
    except DevbenchError as error:
        print(f"no picture of the panel: {error}")


@pytest.fixture
def menu(game, report, request):
    """The menu, open with nothing selected, and closed after the test with its changes discarded; the report gets a
    picture of the panel as the test left it. A mod's dialog that popped up since is closed first."""
    report.dialogs.extend(close_dialogs(game))
    menu = Menu(game)
    menu.close_all()
    menu.open()
    yield menu
    _picture(menu, report, request.node.nodeid)
    menu.close_all()
