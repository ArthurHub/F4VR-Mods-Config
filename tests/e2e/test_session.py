"""A loading screen ends the editing session: the menu closes without asking, as the world its values are shown in is
left, and a config spec's pending changes are discarded with their previews (docs/design/architecture.md)."""

from devbench import close_dialogs, console, open_menus, wait_until
from conftest import DEVBENCH
from devbench.games import fallout4
from stored import file_value, stored_as
from test_sweep import candidates


def wait_for_game(game, report):
    """Wait for a loading screen to end and the game to settle, closing the dialogs mods pop up meanwhile. A coc is
    neither a new game nor a load to F4SE: the game is in once the loading screen is gone and the HUD is up."""

    def in_game():
        report.dialogs.extend(close_dialogs(game))
        menus = open_menus(game)
        return "HUDMenu" in menus and "MainMenu" not in menus and "LoadingMenu" not in menus

    wait_until(in_game, 300, interval=1, what=f"the game in {DEVBENCH.cell}")
    report.dialogs.extend(fallout4.settle(game).dialogs)


def test_a_loading_screen_ends_the_session(menu, game, events, report):
    menu.select("FRIK", ["Body"])
    row = next(row for row in menu.rows() if row["kind"] == "setting" and row["storage"].get("path") == ["Fallout4VRBody", "fPlayerBodyOffsetForwardStanding"])
    new = next(candidates(row))
    menu.set(row["id"], new)
    assert game.tool("frik", action="config", section="Fallout4VRBody", key="fPlayerBodyOffsetForwardStanding")["overridden"]

    console(game, f"coc {DEVBENCH.cell}")
    events.wait("menu.closed", timeout=60)
    assert not any(event for event in events.all("dialog.asked")), "a loading screen closes the menu without asking"
    wait_for_game(game, report)

    config = game.tool("frik", action="config", section="Fallout4VRBody", key="fPlayerBodyOffsetForwardStanding")
    assert not config["overridden"] and stored_as(config["value"], row["value"]), "the preview is discarded with the change"
    assert stored_as(file_value(game, "FRIK", row["storage"]), row["value"]), "nothing is written"
    menu.open()
    menu.select("FRIK", ["Body"])
    assert menu.page()["session"]["dirty"] == 0
