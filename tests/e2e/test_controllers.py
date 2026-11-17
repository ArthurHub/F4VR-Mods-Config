"""The menu worked with the controllers, through devbench's input tool as the game reads the real ones: the opening
gesture, the trigger on rows and on the color editor, the help line under the ray, Back on the grip, the thumbstick
scrolling and stepping, and the pointing hand's controls kept from the game."""

import pytest

from conftest import CONFIG_SPEC_SHOWCASE
from controllers import Pointer
from devbench import console_number
from devbench.vr import handedness, set_handedness
from menu import Menu, plain

DETAILED = "Detailed Examples"


@pytest.fixture
def pointer(game, menu):
    """The primary hand's ray on the open menu, the devices handed back after the test."""
    with Pointer(game, menu) as pointer:
        yield pointer


def show(menu, page):
    menu.select(CONFIG_SPEC_SHOWCASE, [DETAILED, page])


def test_the_gesture_opens_and_closes_the_menu(game, report):
    menu = Menu(game)
    menu.close_all()
    with Pointer(game, menu) as pointer:
        pointer.gesture()
        pointer.wait_for(lambda: menu.state()["menu"]["open"], "the menu open")
        pointer.gesture()
        pointer.wait_for(lambda: not menu.state()["menu"]["open"], "the menu closed")


def test_the_trigger_selects_a_page_and_sets_a_switcher(pointer, menu):
    show(menu, "Switchers")
    pointer.press("tree", CONFIG_SPEC_SHOWCASE, [DETAILED, "Sliders"])
    pointer.wait_for(lambda: menu.page()["page"] == "Sliders", "the Sliders page shown")
    show(menu, "Switchers")
    row = menu.setting("No help")
    pointer.press("setting", row["id"])
    changed = menu.setting(id=row["id"])
    assert changed["value"] is (not row["value"]) and changed["dirty"]


def test_the_help_line_has_the_help_of_the_row_under_the_ray(pointer, menu):
    show(menu, "Switchers")
    row = next(row for row in menu.rows() if row["kind"] == "setting" and row["help"])
    pointer.point_at("setting", row["id"])
    help_line = menu.state()["helpLine"]
    assert help_line["onRow"] and help_line["text"].startswith(plain(row["text"])), help_line
    # the footer left of its buttons is the help line itself, no row: the line has the session's status
    discard = pointer.target("button", "Discard")
    pointer.aim(discard["x"] - 200, discard["y"] + discard["height"] / 2)
    help_line = menu.state()["helpLine"]
    assert not help_line["onRow"] and help_line["text"] == "No unsaved changes", help_line


def test_the_trigger_opens_the_color_editor_and_the_grip_gives_the_color_back(pointer, menu):
    show(menu, "Colors")
    row = menu.setting("Light color")
    pointer.press("setting", row["id"])
    assert menu.state()["editor"] == {"kind": "color", "setting": row["id"]}
    red = pointer.target("colorChannel", "Red")
    pointer.aim(red["x"] + 2, red["y"] + red["height"] / 2)
    pointer.trigger()
    assert menu.setting(id=row["id"])["value"].startswith("{ rgb = [0,"), "the left of the Red track is no red"
    pointer.grip()
    assert menu.state()["editor"] is None
    back = menu.setting(id=row["id"])
    assert back["value"] == row["value"] and not back["dirty"], "Back gives the color back"


def test_the_grip_steps_back_only_with_the_ray_on_the_panel(pointer, menu):
    show(menu, "Switchers")
    pointer.point_at("section", menu.row(kind="section", text=next(r["text"] for r in menu.rows() if r["kind"] == "section"))["text"])
    pointer.grip()
    pointer.wait_for(lambda: menu.page()["page"] != "Switchers", "Back from the Switchers page")
    left_at = menu.page()["page"]
    pointer.away()
    pointer.grip()
    assert menu.page()["page"] == left_at, "a grip with the ray off the panel is the game's"


def test_the_thumbstick_scrolls_the_list_under_the_ray(pointer, menu):
    show(menu, "Choices")
    first = next(row for row in menu.rows() if row["kind"] == "setting")
    pointer.point_at("setting", first["id"])
    assert pointer.scroll("page")[0] == 0
    pointer.stick(0, -1, 400)
    assert pointer.scroll("page")[0] > 0, "down scrolls the page"
    tree = pointer.target("tree", CONFIG_SPEC_SHOWCASE, [DETAILED, "Choices"])
    pointer.aim(tree["x"] + tree["width"] / 2, tree["y"] + tree["height"] / 2)
    tree_before = pointer.scroll("tree")[0]
    pointer.stick(0, -1, 400)
    assert pointer.scroll("tree")[0] > tree_before, "down scrolls the tree"


def test_the_thumbstick_steps_the_slider_and_the_stepper_under_the_ray(pointer, menu):
    show(menu, "Sliders")
    slider = menu.setting("From -100 to 100 by 5")
    pointer.point_at("setting", slider["id"])
    # a slider steps after the ray has been on its row a moment
    pointer.wait_for(lambda: (pointer.stick(1, 0), menu.setting(id=slider["id"])["dirty"])[1], "the slider stepped")
    assert menu.setting(id=slider["id"])["value"] == slider["value"] + 5
    show(menu, "Choices")
    stepper = next(row for row in menu.rows() if row["kind"] == "setting" and row["type"] == "stepper")
    options = [option["value"] for option in stepper["options"]]
    pointer.point_at("setting", stepper["id"])
    pointer.wait_for(lambda: (pointer.stick(1, 0), menu.setting(id=stepper["id"])["dirty"])[1], "the stepper stepped")
    assert options.index(menu.setting(id=stepper["id"])["value"]) == options.index(stepper["value"]) + 1


def test_the_pointing_hand_turns_the_player_only_with_the_ray_off_the_panel(pointer, menu, game):
    show(menu, "Switchers")
    heading = next(row for row in menu.rows() if row["kind"] == "section")
    pointer.point_at("section", heading["text"])
    angle = console_number(game, "player.GetAngle Z")
    pointer.stick(1, 0, 400)
    assert console_number(game, "player.GetAngle Z") == pytest.approx(angle, abs=0.5), "the stick on the panel is the menu's"
    pointer.away()
    pointer.stick(1, 0, 400)
    assert console_number(game, "player.GetAngle Z") != pytest.approx(angle, abs=0.5), "off the panel the stick turns the player"


def test_the_offhand_points_when_the_player_is_left_handed(game, menu):
    was = handedness(game)
    other = "left" if was == "right" else "right"
    with Pointer(game, menu) as pointer:
        set_handedness(game, other, owner=pointer.hands.owner)
    try:
        with Pointer(game, menu) as pointer:
            assert pointer.primary == other
            show(menu, "Switchers")
            row = menu.setting("No help")
            pointer.press("setting", row["id"])
            assert menu.setting(id=row["id"])["dirty"], "the primary hand of a left-handed player presses"
    finally:
        with Pointer(game, menu) as pointer:
            set_handedness(game, was, owner=pointer.hands.owner)
