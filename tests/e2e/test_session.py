"""A loading screen ends the editing session: the menu closes without asking, as the world its values are shown in is
left, and a config spec's pending changes are discarded with their previews (docs/design/architecture.md)."""

import pytest

from conftest import wait_for_game
from stored import file_value, stored_as
from test_sweep import candidates

pytestmark = pytest.mark.session


def test_a_loading_screen_ends_the_session(menu, game, events):
    menu.select("FRIK", ["Body"])
    row = next(row for row in menu.rows() if row["kind"] == "setting" and row["storage"].get("path") == ["Fallout4VRBody", "fPlayerBodyOffsetForwardStanding"])
    new = next(candidates(row))
    menu.set(row["id"], new)
    assert game.tool("frik", action="config", section="Fallout4VRBody", key="fPlayerBodyOffsetForwardStanding")["overridden"]

    game.console("coc qasmoke")
    events.wait("menu.closed", timeout=60)
    assert not any(event for event in events.all("dialog.asked")), "a loading screen closes the menu without asking"
    wait_for_game(game)

    config = game.tool("frik", action="config", section="Fallout4VRBody", key="fPlayerBodyOffsetForwardStanding")
    assert not config["overridden"] and stored_as(config["value"], row["value"]), "the preview is discarded with the change"
    assert stored_as(file_value(game, "FRIK", row["storage"]), row["value"]), "nothing is written"
    menu.open()
    menu.select("FRIK", ["Body"])
    assert menu.page()["session"]["dirty"] == 0
