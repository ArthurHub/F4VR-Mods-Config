"""Going about the menu as the player does: Back, the breadcrumbs of the title bar, the Help page, and Minimize and
Restore, on the pages of the config spec showcase."""

import pytest

from conftest import CONFIG_SPEC_SHOWCASE
from devbench import DevbenchError

pytestmark = pytest.mark.navigation

# a page under a page under a page: its crumbs are the menu's, the mod's, and one for each page on the way
DEEP = ("Detailed Examples", "Pages", "Under a page", "Two levels down")
SWITCHERS = ("Detailed Examples", "Switchers")


def change(menu):
    """Change the switcher of the Switchers page, which is shown, and leave it unsaved."""
    row = menu.setting("No help")
    menu.set(row["id"], not row["value"])


def test_back_one_step_at_a_time(menu, events):
    menu.select(CONFIG_SPEC_SHOWCASE, DEEP)
    for depth in range(len(DEEP) - 1, -1, -1):
        answer = menu.call("back")
        assert answer["open"] and len(answer["page"]) == depth, f"Back goes to the page {depth} deep"
    answer = menu.call("back")
    assert answer["open"] and answer["mod"] is None, "Back from the mod's front page goes to the menu's"
    assert not menu.call("back")["open"], "Back from the menu's front page closes it"
    events.wait("menu.closed")
    shown = [(event["data"]["mod"], event["data"]["page"]) for event in events.all("selection.changed")]
    steps = [(CONFIG_SPEC_SHOWCASE, list(DEEP[:depth])) for depth in range(len(DEEP) - 1, -1, -1)] + [(None, [])]
    assert shown[-len(steps) :] == steps, "each step is published as the selection it shows"


def test_back_that_leaves_a_mod_with_changes_asks(menu):
    menu.select(CONFIG_SPEC_SHOWCASE, SWITCHERS)
    change(menu)
    for _ in SWITCHERS:
        menu.call("back")
    assert not menu.state()["selection"]["asking"], "Back within the mod is not asked about"
    menu.call("back")
    assert menu.state()["selection"]["asking"], "the dialog asks before Back leaves the mod with unsaved changes"
    answer = menu.answer("cancel")
    assert answer["session"]["dirty"] == 1
    assert menu.page()["mod"] == CONFIG_SPEC_SHOWCASE


def test_breadcrumbs(menu, events):
    menu.select(CONFIG_SPEC_SHOWCASE, DEEP)
    with pytest.raises(DevbenchError):
        menu.call("breadcrumb", level=len(DEEP) + 1)  # the last crumb is where the panel is
    assert len(menu.call("breadcrumb", level=3)["page"]) == 2
    assert menu.page()["page"] == "Pages"
    assert menu.call("breadcrumb", level=1)["page"] == [], "the mod's crumb shows its front page"
    events.wait("selection.changed", where=lambda data: data["mod"] == CONFIG_SPEC_SHOWCASE and data["page"] == [])
    assert menu.call("breadcrumb", level=0)["mod"] is None, "the menu's crumb shows its front page"


def test_a_breadcrumb_that_leaves_a_mod_with_changes_asks(menu):
    menu.select(CONFIG_SPEC_SHOWCASE, SWITCHERS)
    change(menu)
    assert menu.call("breadcrumb", level=2)["asking"] is False, "a page of the same mod is not asked about"
    assert menu.call("breadcrumb", level=0)["asking"], "the dialog asks before the menu's crumb leaves the mod"
    menu.answer("discard")
    assert menu.state()["selection"]["mod"] is None


def test_help(menu, events):
    menu.select(CONFIG_SPEC_SHOWCASE, SWITCHERS)
    assert menu.call("help")["helpShown"]
    events.wait("help.shown")
    assert menu.page()["helpShown"]
    with pytest.raises(DevbenchError):
        menu.call("breadcrumb", level=1)  # over the Help page the menu's crumb is the only link
    assert not menu.call("help", shown=False)["helpShown"]
    events.wait("help.hidden")
    assert menu.page()["page"] == "Switchers", "the page Help was shown over is shown again"


def test_minimize_and_restore(menu, events):
    menu.select("ModsConfigFO4VR", ["Panel"])
    tilt = next(row for row in menu.rows() if row["kind"] == "setting" and row["storage"].get("path") == ["Panel", "fTilt"])
    assert menu.call("minimize")["minimized"]
    events.wait("menu.minimized")
    shown = menu.state()["selection"]
    menu.call("back")
    assert menu.state()["selection"] == shown, "Back does nothing while the panel is hidden"
    answer = menu.call("clientSetValue", id=tilt["id"], value=tilt["value"] + 1)
    assert answer["set"] and answer["setting"]["dirty"], "the menu takes a client's edit while it is minimized"
    assert not menu.call("minimize", minimized=False)["minimized"]
    events.wait("menu.restored")
    assert menu.state()["session"]["dirty"] == 1, "the change waits through Minimize and Restore"
