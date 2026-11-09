"""The showcase walkthrough (docs/for-mod-authors/showcase.md), section by section: each test does what its section tells
the player, and checks what it says happens, in the menu, in the setting's file, and in the game."""

import pytest

from conftest import CONFIG_SPEC_SHOWCASE
from devbench import DevbenchError, wait_until
from stored import console_number, file_value, inline_table, stored_as, table

pytestmark = pytest.mark.showcase

DETAILED = "Detailed Examples"


@pytest.fixture
def show(menu):
    """Show a page of the showcase by its name under Detailed Examples, or its front page or General by their path."""

    def show(*path):
        menu.select(CONFIG_SPEC_SHOWCASE, path if not path or path[0] in ("General", DETAILED) else (DETAILED, *path))
        return menu

    return show


def save_and_check(menu, game, text, value):
    """Set a setting of the page that is shown by its text, save it, and check its file has the value."""
    row = menu.setting(text)
    changed = menu.set(row["id"], value)
    menu.save()
    wait_until(lambda: stored_as(file_value(game, CONFIG_SPEC_SHOWCASE, row["storage"]), changed["value"]), 3, what=f"{text} in its file")
    return menu.setting(id=row["id"])


def test_front_page_and_general(show, game):
    menu = show()
    texts = [row.get("text") for row in menu.rows()]
    assert {"About", "Help", "Config spec schema"} <= set(texts)
    menu = show("General")
    types = {row["type"] for row in menu.rows() if row["kind"] == "setting"}
    assert {"switcher", "slider", "stepper", "dropdown", "textinput", "cf.inputBinding", "cf.haptic", "cf.color", "cf.transform"} <= types
    save_and_check(menu, game, "Switcher", False)


def test_switchers(show, game):
    save_and_check(show("Switchers"), game, "No help", True)


def test_sliders(show, game):
    row = save_and_check(show("Sliders"), game, "From -100 to 100 by 5", -15)
    assert row["range"] == {"min": -100, "max": 100, "step": 5}


def test_choices(show, game):
    menu = show("Choices")
    save_and_check(menu, game, "Values", "fast")
    assert len(menu.setting("Settlements")["options"]) > 1, "the settlements of the game's FormList"
    assert menu.setting("Texture")["options"], "the files of the showcase's texture folder"


def test_text_inputs(show, game):
    menu = show("Text inputs")
    code = menu.setting("Code of 3 to 8 characters")
    assert menu.set(code["id"], "AB")["invalid"], "fewer than 3 characters is invalid"
    assert not menu.call("save")["saved"], "a Save is refused while a change is invalid"
    save_and_check(menu, game, "Code of 3 to 8 characters", "ABCD")


def test_buttons(show, game):
    menu = show("Buttons")
    assert menu.call("run", text="Evening")["ran"]
    wait_until(lambda: abs(console_number(game, "GetGlobalValue GameHour") - 20) < 0.2, 3, what="the time of day 20")
    assert menu.call("run", text="Noon")["ran"]
    wait_until(lambda: abs(console_number(game, "GetGlobalValue GameHour") - 12) < 0.2, 3, what="the time of day 12")
    switch = menu.setting("Buttons enabled")
    menu.set(switch["id"], False)
    assert not menu.row("Enabled by the switcher", kind="button")["enabled"]
    with pytest.raises(DevbenchError):
        menu.call("run", text="Enabled by the switcher")


def test_bindings(show, game):
    menu = show("Bindings")
    limited = menu.setting("Limited")
    assert menu.set(limited["id"], inline_table({"hand": "primary", "type": "press", "button": "trigger"}))["invalid"], "the trigger is not one it takes"
    menu.discard(limited["id"])
    row = save_and_check(menu, game, "Limited", inline_table({"hand": "primary", "type": "longpress", "button": "a"}))
    assert table(row["value"])["button"] == "a"


def test_haptics(show, game):
    save_and_check(show("Haptics"), game, "Double click", "Tick")


def test_colors(show, game):
    save_and_check(show("Colors"), game, "Light color", inline_table({"rgb": [250, 200, 160]}))


def test_transforms(show, game):
    save_and_check(show("Transforms"), game, "Body grab zone", inline_table({"pos": [0.0, 0.0, 20.0], "scale": 1.5}))


def test_activation_spheres(show):
    menu = show("Activation spheres")
    spheres = [row for row in menu.rows() if row["kind"] == "section" and row["role"] == "activationSphere"]
    assert len(spheres) == 3


def test_images(show):
    menu = show("Images")
    images = [row for row in menu.rows() if row["kind"] == "image"]
    assert images and all(row["texture"].startswith("ModsConfigFO4_Showcase/") for row in images), "each draws a texture of the showcase"
    picture = menu.setting("Show the picture")
    shown_before = [row["shown"] for row in menu.rows() if row["kind"] == "image"]
    menu.set(picture["id"], False)
    assert [row["shown"] for row in menu.rows() if row["kind"] == "image"] != shown_before, "the switcher hides the picture"


def test_numbers(show, game):
    save_and_check(show("Numbers"), game, "Meters", 1.8)


def test_sections(show):
    menu = show("Sections")
    advanced = menu.row("Advanced", kind="section")
    assert advanced["collapsible"] and advanced["folded"], "Advanced is folded each time the page is shown"
    assert menu.call("fold", text="Advanced", folded=False)["folded"] is False


def test_pages(show, game):
    menu = show("Pages", "Under a page", "Two levels down")
    save_and_check(menu, game, "Two levels down", True)


def test_read_only(show):
    menu = show("Read-only")
    height = menu.setting("Measured height")
    assert height["readOnly"] and height["default"] is None
    try:
        assert not menu.call("setValue", id=height["id"], value=1.9)["set"]
    except DevbenchError:
        pass


def test_files(show, game):
    menu = show("Files")
    player = save_and_check(menu, game, "Player's switcher", False)
    assert (player["storage"]["root"], player["storage"]["format"]) == ("mygames", "toml")
    ini = save_and_check(menu, game, "INI switcher", False)
    assert ini["storage"]["format"] == "ini"
    deep = save_and_check(menu, game, "A value in a table in a table", False)
    assert deep["storage"]["path"] == ["Files", "Tables", "bDeep"]


def test_in_the_game(show, game):
    menu = show("In the game")

    def game_hour_is(hour):
        return wait_until(lambda: abs(console_number(game, "GetGlobalValue GameHour") - hour) < 0.2, 3, what=f"the time of day {hour}")

    time_of_day = menu.setting(id="timeOfDay")
    menu.set("timeOfDay", 18.0)
    game_hour_is(18)
    menu.discard("timeOfDay")
    game_hour_is(time_of_day["value"])
    menu.set("timeOfDay", 6.0)
    menu.save()
    game_hour_is(6)


def test_when_a_change_takes_effect(show):
    menu = show("When a change takes effect")
    after_save = menu.setting("After Save")
    assert menu.set(after_save["id"], not after_save["value"])["tag"], "a pending change tells when it takes effect"
    menu.discard()
    restarts = next(mod for mod in menu.mods() if mod["name"] == CONFIG_SPEC_SHOWCASE)["restarts"]
    restart = menu.setting("After a restart")
    menu.set(restart["id"], not restart["value"])
    menu.save()
    assert next(mod for mod in menu.mods() if mod["name"] == CONFIG_SPEC_SHOWCASE)["restarts"] == restarts + 1


def test_conditions(show):
    menu = show("Conditions")
    assert menu.row("Shown by a switcher", kind="section")["shown"]
    menu.set("showRows", False)
    assert not menu.row("Shown by a switcher", kind="section")["shown"]
    any_of_two = menu.setting("Any of two")
    assert any_of_two["shown"], "shown while Show rows is off"


WAND_MOVEMENT = "bUseWandDirectionalMovement:VRInput"


def wand_movement_text(menu):
    """The value of the game's movement by controller direction, as the text of the Conditions page inserts it."""
    prefix = "Movement by controller direction: "
    for row in menu.rows():
        for line in row["lines"] if row["kind"] == "text" else []:
            if line.startswith(prefix):
                return line[len(prefix):]
    raise AssertionError("the page has no text of the game's movement by controller direction")


def test_conditions_game_ini_setting(show, game):
    menu = show("Conditions")
    shown = wand_movement_text(menu)
    assert shown in ("true", "false"), "the text has the game's setting"
    was = shown == "true"
    assert menu.setting("With movement by controller direction")["enabled"] is was
    try:
        # the console changes the setting as the game's settings do, and the page follows it with the menu open
        for value in (not was, was):
            game.console(f'setini "{WAND_MOVEMENT}" {int(value)}')
            assert wand_movement_text(menu) == str(value).lower()
            assert menu.setting("With movement by controller direction")["enabled"] is value
    finally:
        game.console(f'setini "{WAND_MOVEMENT}" {int(was)}')


def test_warnings(show):
    menu = show("Warnings")
    assert menu.setting(id="playerHeight")["warning"], "it warns above 1.4 m, as it is shipped"
    assert menu.set("playerHeight", 1.4)["warning"] is None


def test_translations(show):
    menu = show("Translations")
    assert menu.setting("A token with a fallback"), "a token no file has shows its fallback"


def test_mistakes(show, game):
    show("Mistakes")
    report = game.mod("report", mod=CONFIG_SPEC_SHOWCASE)["configSpecs"][0]
    assert report["errors"] > 0, "the Mistakes page has a row for each error"
