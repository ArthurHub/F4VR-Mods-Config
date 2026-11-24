"""The framework's VR keyboard in the menu: a press on a text input or the search field shows it, the ray types on it,
its line and caret keys move the caret, its pages type symbols and accented letters, and Done and Back keep what was
typed. A key is pressed with the ray where the test is of the ray, and through the keyboard's devbench action
elsewhere, which presses it as a pull does."""

import pytest

from conftest import CONFIG_SPEC_SHOWCASE
from controllers import Pointer
from devbench import wait_until

TEXT_INPUTS = ("Detailed Examples", "Text inputs")


@pytest.fixture
def pointer(game, menu):
    """The primary hand's ray on the open menu, the devices handed back after the test."""
    with Pointer(game, menu) as pointer:
        yield pointer


def edit(pointer, menu, text):
    """Show the text inputs and press the row of one, which shows the keyboard: its row as it was."""
    menu.select(CONFIG_SPEC_SHOWCASE, list(TEXT_INPUTS))
    row = menu.setting(text)
    pointer.press("setting", row["id"])
    wait_until(lambda: pointer.keyboard()["open"], 3, interval=0.05, what="the keyboard shown")
    assert menu.state()["editor"] == {"kind": "keyboard", "setting": row["id"]}
    return row


def press(pointer, *keys):
    """Press keys of the keyboard through its devbench action, each a frame apart."""
    for key in keys:
        assert pointer.keyboard(key)["open"], f"the keyboard closed before {key!r}"
        pointer.menu.state()


def line(pointer, text):
    """Wait for the keyboard's line to show a text, which ImGui takes a stroke a frame, and return the keyboard."""
    return wait_until(lambda: (k := pointer.keyboard()) and k["text"] == text and k, 3, interval=0.05, what=f"the line showing {text!r}")


def closed(pointer):
    wait_until(lambda: not pointer.keyboard()["open"], 3, interval=0.05, what="the keyboard closed")


def test_the_ray_types_on_the_keyboard_and_done_keeps_the_text(pointer, menu):
    row = edit(pointer, menu, "HTML: Text")
    shown = pointer.keyboard()
    assert shown["page"] == "letters" and shown["text"] == row["value"] and shown["cursor"] == len(row["value"])
    pointer.type_key("Shift")
    # a key shows what it types: W while Shift is on
    pointer.type_key("W")
    line(pointer, row["value"] + "W")
    pointer.type_key("Backspace")
    line(pointer, row["value"])
    pointer.type_key("q")
    line(pointer, row["value"] + "q")
    press(pointer, "Done")
    closed(pointer)
    typed = menu.setting(id=row["id"])
    assert typed["value"] == row["value"] + "q" and typed["dirty"]
    assert menu.state()["editor"] is None


def test_the_grip_is_done_and_steps_no_page(pointer, menu):
    row = edit(pointer, menu, "HTML: Text")
    press(pointer, "z")
    line(pointer, row["value"] + "z")
    # with the ray on the keyboard, and then on the panel
    pointer.find_keyboard()
    pointer.grip()
    closed(pointer)
    assert menu.setting(id=row["id"])["value"] == row["value"] + "z"
    assert menu.page()["page"] == TEXT_INPUTS[-1], "Back with the keyboard open steps no page"
    pointer.press("setting", row["id"])
    wait_until(lambda: pointer.keyboard()["open"], 3, interval=0.05, what="the keyboard shown again")
    press(pointer, "y")
    line(pointer, row["value"] + "zy")
    pointer.point_at("setting", row["id"])
    pointer.grip()
    closed(pointer)
    assert menu.setting(id=row["id"])["value"] == row["value"] + "zy"
    assert menu.page()["page"] == TEXT_INPUTS[-1]


def test_the_caret_keys_and_the_line_move_the_caret(pointer, menu):
    row = edit(pointer, menu, "HTML: Text")
    end = len(row["value"])
    pointer.type_key("Left")
    wait_until(lambda: pointer.keyboard()["cursor"] == end - 1, 3, interval=0.05, what="the caret a character left")
    press(pointer, "Right")
    wait_until(lambda: pointer.keyboard()["cursor"] == end, 3, interval=0.05, what="the caret back at the end")
    # a pull on the line before its text puts the caret at the start
    text_line = pointer.key("Line")
    pointer.aim_keyboard(text_line["x"] + 4, text_line["y"] + text_line["height"] / 2, text_line["height"] / 4)
    pointer.trigger()
    wait_until(lambda: pointer.keyboard()["cursor"] == 0, 3, interval=0.05, what="the caret at the start")
    press(pointer, "a")
    line(pointer, "a" + row["value"])


def test_the_pages_type_symbols_and_accented_letters(pointer, menu):
    row = edit(pointer, menu, "HTML: Text")
    press(pointer, "?123")
    assert pointer.keyboard()["page"] == "symbols"
    press(pointer, "€", "áé")
    assert pointer.keyboard()["page"] == "accents"
    press(pointer, "Shift", "É", "abc")
    shown = line(pointer, row["value"] + "€É")
    assert shown["page"] == "letters"
    press(pointer, "Done")
    closed(pointer)
    assert menu.setting(id=row["id"])["value"] == row["value"] + "€É"


def test_enter_does_nothing_on_a_one_line_field(pointer, menu):
    row = edit(pointer, menu, "HTML: Text")
    press(pointer, "Enter", "x")
    shown = line(pointer, row["value"] + "x")
    assert shown["open"], "Enter leaves no one-line field"


def test_a_number_field_takes_only_the_characters_of_a_number(pointer, menu):
    row = edit(pointer, menu, "Whole number")
    press(pointer, "Clear")
    line(pointer, "")
    press(pointer, "7", "q", "3")
    line(pointer, "73")
    press(pointer, "Done")
    closed(pointer)
    typed = menu.setting(id=row["id"])
    assert str(typed["value"]) == "73" and typed["dirty"]


def test_the_search_field_is_typed_on_the_keyboard(pointer, menu):
    # the menu opens on its front page
    pointer.press("search", "Field")
    wait_until(lambda: pointer.keyboard()["open"], 3, interval=0.05, what="the keyboard shown for the search")
    assert menu.state()["search"]["edited"]
    press(pointer, "f", "r")
    # the query filters as each key is typed
    wait_until(lambda: menu.state()["search"]["text"] == "fr", 3, interval=0.05, what="the query 'fr' searched")
    press(pointer, "i", "k")
    wait_until(lambda: menu.state()["search"]["text"] == "frik", 3, interval=0.05, what="the query 'frik' searched")
    pointer.find_keyboard()
    pointer.grip()
    closed(pointer)
    state = menu.state()
    assert state["search"]["text"] == "frik" and not state["search"]["edited"], "Back keeps the query"
    assert next(mod for mod in menu.mods() if mod["name"] == "FRIK")["found"], "the query finds FRIK"
