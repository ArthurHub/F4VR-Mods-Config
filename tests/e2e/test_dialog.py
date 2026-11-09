"""The dialog that asks what to do with a mod's unsaved changes, as the mod is left and as the menu is closed: Cancel
stays, Discard drops the changes, Save writes them, each checked in the file of the config spec showcase."""

import pytest

from conftest import CONFIG_SPEC_SHOWCASE, MCM_SHOWCASE
from devbench import wait_until
from stored import file_value, stored_as

pytestmark = pytest.mark.dialog

PAGE = ("Detailed Examples", "Switchers")
SETTING = "No help"


@pytest.fixture
def changed(menu):
    """The showcase's switcher, changed and not saved: its row before the change."""
    menu.select(CONFIG_SPEC_SHOWCASE, PAGE)
    row = menu.setting(SETTING)
    menu.set(row["id"], not row["value"])
    return row


def in_file(game, row, value):
    return stored_as(file_value(game, CONFIG_SPEC_SHOWCASE, row["storage"]), value)


def leave(menu):
    """Show another mod, which the dialog asks about first."""
    answer = menu.call("select", mod=MCM_SHOWCASE)
    assert answer["asking"], "the dialog asks before a mod with unsaved changes is left"


def test_cancel_stays(menu, events, changed):
    leave(menu)
    events.wait("dialog.asked", where=lambda data: data["mod"] == CONFIG_SPEC_SHOWCASE)
    answer = menu.answer("cancel")
    assert not answer["asking"]
    assert answer["session"]["dirty"] == 1, "the change waits"
    events.wait("dialog.answered")
    assert menu.page()["mod"] == CONFIG_SPEC_SHOWCASE


def test_discard_drops_the_change(menu, game, events, changed):
    leave(menu)
    menu.answer("discard")
    events.wait("session.discarded", where=lambda data: data["mod"] == CONFIG_SPEC_SHOWCASE)
    assert menu.page()["mod"] == MCM_SHOWCASE, "the mod that was asked for is shown"
    assert not in_file(game, changed, not changed["value"])


def test_save_writes_the_change(menu, game, events, changed):
    leave(menu)
    menu.answer("save")
    events.wait("session.saved", where=lambda data: data["mod"] == CONFIG_SPEC_SHOWCASE and data["saved"])
    assert menu.page()["mod"] == MCM_SHOWCASE
    wait_until(lambda: in_file(game, changed, not changed["value"]), 3, what="the change in its file")


def test_closing_asks(menu, events, changed):
    assert menu.call("close", ask=True)["asking"], "the dialog asks before the menu with unsaved changes is closed"
    menu.answer("discard")
    events.wait("menu.closed")
    assert not menu.state()["menu"]["open"]
