"""An MCM mod's change, which is made as it is made, and its Save bar's Keep and Revert: a ModSetting of the MCM
showcase is checked in the file MCM writes in the game as it is set, and in the events MCM's scripts are sent."""

import pytest

from conftest import CONFIG_SPEC_SHOWCASE, MCM_SHOWCASE
from devbench import wait_until
from stored import file_value, stored_as

pytestmark = pytest.mark.mcm

PAGE = ("Switchers",)
SETTING = "On by default"


def holds(game, row, value):
    """Whether MCM in the game has the value: the menu sets a ModSetting through MCM's Papyrus function, and MCM writes
    its file on every set. The console prints nothing of a cgf's result, so MCM is not asked."""
    return stored_as(file_value(game, MCM_SHOWCASE, row["storage"]), value)


@pytest.fixture
def changed(menu, events):
    """The MCM showcase's switcher, changed: its row before the change."""
    menu.select(MCM_SHOWCASE, PAGE)
    row = menu.setting(SETTING)
    assert row["storage"]["kind"] == "modSetting"
    menu.set(row["id"], not row["value"])
    return row


def test_a_change_is_made_as_it_is_made(menu, game, events, changed):
    wait_until(lambda: holds(game, changed, not changed["value"]), 3, what="the change in MCM's file and in the game")
    events.wait("mcm.event", where=lambda data: data["event"] == "OnMCMSettingChange")


def test_revert_sets_it_back(menu, game, changed):
    assert menu.discard()["session"]["dirty"] == 0
    wait_until(lambda: holds(game, changed, changed["value"]), 3, what="the value set back in MCM's file and in the game")


def test_keep_keeps_it(menu, game, events, changed):
    assert menu.save()["session"]["dirty"] == 0
    events.wait("mcm.pauseMenu")
    wait_until(lambda: holds(game, changed, not changed["value"]), 3, what="the change kept in MCM's file and in the game")


def test_the_dialog_has_keep_and_revert(menu, game, changed):
    assert menu.call("select", mod=CONFIG_SPEC_SHOWCASE)["asking"], "the dialog asks before an MCM mod with changes is left"
    menu.answer("discard")
    wait_until(lambda: holds(game, changed, changed["value"]), 3, what="the value its Revert set back")
