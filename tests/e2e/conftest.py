"""The session the end-to-end tests run in: devbench, a game in QASmoke, the backup of the files, and the menu."""

import time

import pytest

import backup
from devbench import Devbench, DevbenchError, Events, find_port, wait_until
from menu import Menu
from report import REPORT
from stored import console_number

# the MCM test set: the MCM mods of the author's Mad God Overhaul list, by their modName
TEST_SET = [
    "AutoEat",
    "Chemfluence AI Combat Dynamics",
    "FallUI",
    "FallUIWorkbench",
    "Kabuto",
    "NPCs Use Items",
    "SS2",
    "UnlimitedSurvivalMode",
    "WorkshopFramework",
    "XDI",
]
MCM_SHOWCASE = "ModsConfigFO4_McmShowcase"
CONFIG_SPEC_SHOWCASE = "ModsConfigFO4_Showcase"
# the config specs of the mod, of FRIK and of Immersive Flashlight
CONFIG_SPECS = ["ModsConfigFO4VR", "FRIK", "ImmersiveFlashlightVR"]

# a new game is stable once no mod's dialog has popped up for QUIET_SECONDS, which gives its quests time to start
# before a test reads what they set; a game that keeps popping them up is used after SETTLE_LIMIT_SECONDS
QUIET_SECONDS = 10
SETTLE_LIMIT_SECONDS = 120


def pytest_addoption(parser):
    parser.addoption("--devbench-port", type=int, help="devbench's port; by default the one its runtime.json has")
    parser.addoption("--e2e-restore", action="store_true", help="put back the files of a backup a run left, and run no test")


def pytest_sessionstart(session):
    if not session.config.getoption("--e2e-restore"):
        return
    if not backup.pending():
        pytest.exit("no backup is waiting to be put back", returncode=0)
    bench = _connect(session.config)
    written, deleted = backup.restore(bench)
    pytest.exit(f"put back the backup: {len(written)} files written, {len(deleted)} deleted", returncode=0)


def pytest_runtest_logreport(report):
    REPORT.add_result(report)


def pytest_terminal_summary(terminalreporter, config):
    if config.getoption("collectonly") or not REPORT.tests:
        return
    terminalreporter.write_line(f"Report: {REPORT.write().as_uri()}", bold=True)


def _connect(config):
    port = config.getoption("--devbench-port") or find_port()
    bench = Devbench(port)
    try:
        ping = bench.tool("ping")
    except (OSError, DevbenchError) as error:
        pytest.exit(f"devbench does not answer on port {port}: {error}", returncode=2)
    if ping.get("game") != "fallout4" or not ping.get("vr"):
        pytest.exit(f"devbench on port {port} is not Fallout 4 VR: {ping}", returncode=2)
    return bench


@pytest.fixture(scope="session")
def bench(pytestconfig):
    bench = _connect(pytestconfig)
    yield bench
    bench.close()


def _open_menus(bench):
    return bench.tool("menu", action="list")["openMenus"]


def close_dialogs(bench):
    """Answer each game message box that is open with its first button, as the mods' popups of a new game block it,
    and return what each said."""
    answered = []
    while bench.tool("menu", action="list")["messageBoxOpen"] and len(answered) < 20:
        try:
            box = bench.tool("menu", action="describe")
            said = f"{box.get('headerText') or ''} {box.get('bodyText') or ''}".strip()
        except DevbenchError as error:
            said = f"(not read: {error})"
        bench.tool("menu", action="accept", index=0)
        print(f"closed a game dialog: {said[:200]!r}")
        answered.append(said)
        REPORT.dialogs.append(said)
        time.sleep(0.5)
    return answered


def _settle(bench):
    """Wait until no dialog has popped up for QUIET_SECONDS, closing each one that does."""
    deadline = time.monotonic() + SETTLE_LIMIT_SECONDS
    quiet_since = time.monotonic()
    while time.monotonic() < deadline:
        if close_dialogs(bench):
            quiet_since = time.monotonic()
        elif time.monotonic() - quiet_since >= QUIET_SECONDS:
            return
        time.sleep(1)
    print(f"the game still pops up dialogs after {SETTLE_LIMIT_SECONDS} s: the tests start anyway")


def _start_game(bench):
    """A game in QASmoke, so nothing the tests do reaches a save: a new one from the main menu, or the one that is
    loaded when it is in QASmoke already."""
    # the console's buffer drops what a command prints until the Console menu is first made
    bench.tool("menu", action="open", name="Console")
    bench.tool("menu", action="close", name="Console")
    # the player exists at the main menu too: the main menu is what tells it from a game
    if "MainMenu" not in _open_menus(bench):
        if not bench.tool("inspect", kind="state")["playerLoaded"]:
            pytest.exit("the game is neither at the main menu nor in a game", returncode=2)
        if console_number(bench, "player.GetInCell QASmoke") != 1:
            pytest.exit("a game that is not in QASmoke is loaded: quit to the main menu, and the tests start one there", returncode=2)
        close_dialogs(bench)
        REPORT.notes.append("Ran in the game already in QASmoke.")
        return
    REPORT.notes.append("Started a new game in QASmoke from the main menu.")
    bench.console("coc qasmoke")
    wait_for_game(bench)


def wait_for_game(bench):
    """Wait for a loading screen to end and the game to be stable, closing the dialogs mods pop up meanwhile. A coc is
    neither a new game nor a load to F4SE, so no mod is told its session loaded: the game is in once the main menu and
    the loading screen are gone and the HUD is up."""

    def in_game():
        close_dialogs(bench)
        menus = _open_menus(bench)
        return "HUDMenu" in menus and "MainMenu" not in menus and "LoadingMenu" not in menus

    wait_until(in_game, 300, interval=1, what="the game in QASmoke")
    _settle(bench)


def _check_installed(bench):
    listed = {mod["name"]: mod for mod in bench.mod("mods")["mods"]}
    missing = [name for name in [*TEST_SET, MCM_SHOWCASE, CONFIG_SPEC_SHOWCASE, *CONFIG_SPECS] if name not in listed]
    if missing:
        pytest.exit(f"the menu does not list {', '.join(missing)}: install the test set and the showcases", returncode=2)
    unavailable = [f"{name} ({listed[name]['notAvailable']})" for name in listed if listed[name]["notAvailable"] and name in TEST_SET + CONFIG_SPECS]
    if unavailable:
        pytest.exit(f"not available: {', '.join(unavailable)}", returncode=2)


@pytest.fixture(scope="session")
def game(bench):
    """The game the tests run in, with its files backed up for the whole run and put back after it."""
    if backup.pending():
        backup.restore(bench)
    _start_game(bench)
    _check_installed(bench)
    files = backup.take(bench)
    yield bench
    Menu(bench).close_all()
    written, deleted = backup.restore(bench)
    REPORT.notes.append(f"Backed up {files} files, and put them back after: {len(written)} written again, {len(deleted)} the run made deleted.")


def _picture(game, menu, nodeid):
    """Save a picture of the panel for the report, while it is shown."""
    state = menu.state()
    if not state["menu"]["open"] or state["menu"]["minimized"]:
        return
    path = REPORT.picture(nodeid)
    try:
        game.mod("snapshot", path=str(path))
    except DevbenchError as error:
        print(f"no picture of the panel: {error}")
        return
    REPORT.snapshots[nodeid] = path.relative_to(REPORT.folder).as_posix()


@pytest.fixture
def menu(game, request):
    """The menu, open with nothing selected, and closed after the test with its changes discarded; the report gets a
    picture of the panel as the test left it. A mod's dialog that popped up since is closed first."""
    close_dialogs(game)
    menu = Menu(game)
    menu.close_all()
    menu.open()
    yield menu
    _picture(game, menu, request.node.nodeid)
    menu.close_all()


@pytest.fixture
def report():
    """The report of the run, which a test adds what it found to."""
    return REPORT


@pytest.fixture
def events(game):
    """The events published from the start of the test."""
    return Events(game)
