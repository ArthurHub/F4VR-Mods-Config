"""What a row does beyond its value: Reset, a button's run, a client's state in a text, an MCM row's text from the game,
its image and its page's plugin requirements, a translated text, and a config spec's report."""

import pytest

from conftest import CONFIG_SPEC_SHOWCASE, MCM_SHOWCASE
from devbench import DevbenchError, wait_until
from menu import page_paths, same
from stored import file_value, stored_as

pytestmark = pytest.mark.rows

BUTTONS = ("Detailed Examples", "Buttons")


def test_reset_sets_the_default(menu):
    menu.select(CONFIG_SPEC_SHOWCASE, ("Detailed Examples", "Sliders"))
    row = menu.setting("From -100 to 100 by 5")
    menu.set(row["id"], row["default"] + 10)
    answer = menu.call("reset", id=row["id"])
    assert answer["set"] and same(answer["setting"]["value"], row["default"])
    assert not answer["setting"]["dirty"], "the default is the saved value, so the change is gone"


def test_run_without_saving_and_save_and_run(menu, game):
    menu.select(CONFIG_SPEC_SHOWCASE, BUTTONS)
    switch = menu.setting("Buttons enabled")
    menu.set(switch["id"], not switch["value"])
    assert menu.call("run", text="Noon", withoutSaving=True)["ran"]
    assert menu.page()["session"]["dirty"] == 1, "Run without saving leaves the change pending"
    assert menu.call("run", text="Noon")["session"]["dirty"] == 0, "a script's function saves the changes first"
    wait_until(lambda: stored_as(file_value(game, CONFIG_SPEC_SHOWCASE, switch["storage"]), not switch["value"]), 3, what="the change saved")
    menu.set(switch["id"], switch["value"])
    menu.save()


def test_a_confirmation_is_pressed(menu):
    menu.select(CONFIG_SPEC_SHOWCASE, BUTTONS)
    assert menu.row("With a confirmation", kind="button")["confirmation"]
    assert menu.call("run", text="With a confirmation")["ran"]


def test_an_mcm_button(menu):
    menu.select(MCM_SHOWCASE, ("Buttons, text input and others",))
    assert menu.call("run", text="CallGlobalFunction")["ran"]
    # the form is looked up when the button runs: one that is not there runs nothing, and nothing fails
    assert menu.row("CallFunction on a form that is not there", kind="button")["runnable"]
    missing = menu.call("run", text="CallFunction on a form that is not there")
    assert not missing["ran"] and not missing["failed"]
    with pytest.raises(DevbenchError):
        menu.call("run", text="A button with no action")


def test_a_client_state_in_a_text(menu):
    menu.select(CONFIG_SPEC_SHOWCASE, ("Detailed Examples", "Text: tags and plain"))
    lines = [line for row in menu.rows() if row["kind"] == "text" for line in row["lines"]]
    assert "Version Unknown (version)" in lines, "the showcase has no DLL, so its states are unknown"
    button = next(row for row in menu.rows() if row["kind"] == "button" and row["text"].startswith("Notify"))
    assert "{clientState:" not in button["text"] + button["help"] and "Unknown (status)" in button["text"]


def test_an_mcm_text_from_the_game(menu):
    menu.select(MCM_SHOWCASE, ("Text from the game",))
    rows = menu.rows()
    assert not any("Fallout4.esm|" in (row.get("text") or "") for row in rows), "a form's name in place of its id"
    assert any(row.get("text", "").startswith("A property that is not in the game") for row in rows), "the row's own text when the property is not there"


def test_mcm_images(menu):
    menu.select(MCM_SHOWCASE, ("Images",))
    images = [row for row in menu.rows() if row["kind"] == "image"]
    assert len(images) == 5
    assert sum(row["read"] for row in images) == 4, "every image of lib.swf but the class it does not have"


def test_mcm_page_plugin_requirements(menu):
    pages = next(mod for mod in menu.mods() if mod["name"] == MCM_SHOWCASE)["pages"]
    names = [path[-1] for path in page_paths(pages)]
    assert "Plugin requirements" in names and "Plugin requirements: hidden" not in names, "hideIfMissingReqs leaves a page out"
    assert menu.call("select", mod=MCM_SHOWCASE, page=["Plugin requirements"])["page"] == [], "a page whose plugin is not loaded is not shown"


def test_fallui_texts_are_translated(menu):
    pages = next(mod for mod in menu.mods() if mod["name"] == "FallUI")["pages"]
    untranslated = []
    for path in [[], *page_paths(pages)]:
        menu.select("FallUI", path)
        for row in menu.rows():
            # a spacer's lines are its height
            lines = row["lines"] if row["kind"] == "text" else []
            texts = [row.get("text") or "", row.get("help") or "", *lines]
            untranslated += [f"{' / '.join(path)}: {text}" for text in texts if "$" in text]
    assert not untranslated, "FallUI's $ tokens are read from its own translation files:\n" + "\n".join(untranslated[:20])


def test_a_config_spec_report(game):
    report = game.mod("report", mod=CONFIG_SPEC_SHOWCASE)["configSpecs"][0]
    assert report["loaded"] and report["liveCopy"] is None
    assert report["errors"] + report["warnings"] == len(report["problems"])
    for problem in report["problems"]:
        assert problem["severity"] in ("error", "warning") and problem["message"] and problem["path"]
