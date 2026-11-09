"""The live preview of a config spec's mod with a native client: a pending change is told to the mod's DLL, which
previews it as a session override of its config; a Discard takes it back, and a Save writes it and ends the preview.
Checked in the mod through its own devbench tool, in its file, and in the menu's events."""

import pytest

from devbench import MOD_TOOL, wait_until
from menu import same
from stored import file_value, stored_as
from test_sweep import candidates

pytestmark = pytest.mark.live

# a live setting of each mod: its config spec, its devbench tool, its page, and its key in its TOML
CASES = {
    "FRIK": ("FRIK", "frik", ["Body"], ["Fallout4VRBody", "fPlayerBodyOffsetForwardStanding"]),
    "ImmersiveFlashlightVR": ("ImmersiveFlashlightVR", "immersiveflashlightvr", ["General"], ["ImmersiveFlashlightVR", "bKeepFlashlightOnInPipboy"]),
}


def setting_of(menu, path):
    return next(row for row in menu.rows() if row["kind"] == "setting" and row["storage"].get("path") == path)


@pytest.mark.parametrize("case", CASES.values(), ids=CASES.keys())
def test_live_preview(menu, game, events, case):
    mod, tool, page, path = case
    menu.select(mod, page)
    row = setting_of(menu, path)
    assert row["applies"] == "live"

    def in_mod(value, overridden):
        """Whether the mod's config has the value, as a session override or not; it answers a value as its text."""
        config = game.tool(tool, action="config", section=path[0], key=path[1])
        return config["overridden"] is overridden and stored_as(config["value"], value)

    original = row["value"]
    new = next(candidates(row))
    assert in_mod(original, False)

    menu.set(row["id"], new)
    events.wait("client.change", where=lambda data: data["setting"] == row["id"] and data["phase"] == "preview" and data["told"])
    wait_until(lambda: in_mod(new, True), 3, what=f"{mod} previewing the change")

    menu.discard(row["id"])
    events.wait("client.change", where=lambda data: data["setting"] == row["id"] and data["phase"] == "discard")
    wait_until(lambda: in_mod(original, False), 3, what=f"{mod} ending the preview")

    menu.set(row["id"], new)
    menu.save()
    events.wait("client.saved", where=lambda data: data["told"] and any(saved["setting"] == row["id"] for saved in data["settings"]))
    wait_until(lambda: stored_as(file_value(game, mod, row["storage"]), new), 3, what="the change in its file")
    wait_until(lambda: in_mod(new, False), 3, what=f"{mod} reading the saved value")

    menu.set(row["id"], original)
    menu.save()


def test_client_set_value(menu, game, events):
    """The menu's own client hands an edit back, as a mod's own UI does: the change is pending, and told back to the
    client, which previews it."""
    menu.select("ModsConfigFO4VR", ["Panel"])
    row = setting_of(menu, ["Panel", "fTilt"])
    new = next(candidates(row))
    answer = menu.call("clientSetValue", id=row["id"], value=new)
    assert answer["set"] and answer["setting"]["dirty"] and same(answer["setting"]["value"], new)
    events.wait("client.change", where=lambda data: data["setting"] == row["id"] and data["told"])
    wait_until(lambda: game.tool(MOD_TOOL, action="config", section="Panel", key="fTilt")["overridden"], 3, what="the menu previewing its own change")
    menu.discard(row["id"])
    wait_until(lambda: not game.tool(MOD_TOOL, action="config", section="Panel", key="fTilt")["overridden"], 3, what="the preview taken back")
