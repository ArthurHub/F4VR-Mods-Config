"""The search field of the tree: what a query finds in the mods and on the page, Back showing a filtered page whole, a
row hidden by a condition, an MCM row found by its text from the game, and the query the menu opens with."""

from conftest import CONFIG_SPEC_SHOWCASE, MCM_SHOWCASE
from devbench import wait_until
from menu import plain

CONDITIONS = ("Detailed Examples", "Conditions")
GAME_TEXTS = ("Text from the game",)


def search(menu, query):
    """Type a query in the search field, and return the state once the panel searched it."""
    menu.call("search", query=query)
    return wait_until(lambda: (s := menu.state()) and s["search"]["text"] == query and s, 5, interval=0.02, what=f"the search for {query!r}")


def found_in(menu, name):
    """How many rows of a mod the search shows, and None for a mod it found nothing in."""
    return next(mod for mod in menu.mods() if mod["name"] == name)["found"]


def test_a_query_finds_mods_and_rows_and_filters_the_page(menu):
    state = search(menu, "level")
    mods = menu.mods()
    assert state["search"]["matches"] == sum(mod["found"] or 0 for mod in mods), "the matches are the rows found in every mod"
    assert found_in(menu, CONFIG_SPEC_SHOWCASE), "the showcase's Conditions page has rows named Level"
    assert state["search"]["filtered"], "the menu's front page says how many settings match"

    menu.select(CONFIG_SPEC_SHOWCASE, CONDITIONS)
    page = menu.page()
    assert page["filtered"]
    # a setting is found when its name matches and its condition holds, as the selected mod's store has it
    settings = [row for row in page["rows"] if row["kind"] == "setting"]
    assert any(row["found"] for row in settings)
    for row in settings:
        matches = "level" in plain(row["text"]).lower()
        assert row["found"] == (matches and row["shown"]), f"{row['text']} found: {row['found']}"

    state = search(menu, "")
    assert state["search"]["matches"] == 0 and not state["search"]["filtered"]
    assert all(row["found"] is None for row in menu.rows()), "with no query no row is found or not"


def test_back_shows_the_page_whole_and_keeps_the_query(menu):
    menu.select(CONFIG_SPEC_SHOWCASE, CONDITIONS)
    assert search(menu, "level")["search"]["filtered"]

    menu.call("back")
    state = menu.state()
    assert not state["search"]["filtered"], "Back shows the page whole"
    assert state["search"]["text"] == "level", "with the query kept"
    assert menu.page()["page"] == "Conditions"

    # the next Back goes a step back, and the page it shows is filtered again
    menu.call("back")
    state = menu.state()
    assert menu.page()["page"] == "Detailed Examples"
    assert state["search"]["filtered"], "another page shown while searching is filtered"


def test_a_row_that_is_not_searchable_is_not_found_by_its_name(menu):
    menu.select(CONFIG_SPEC_SHOWCASE, ("Detailed Examples", "Switchers"))
    search(menu, "not searchable")
    assert not menu.setting("Not searchable")["found"]

    # its page found by its name shows it with the page's other rows
    search(menu, "switchers")
    assert menu.setting("Not searchable")["found"]


def test_a_row_a_condition_hides_is_not_found(menu):
    menu.select(CONFIG_SPEC_SHOWCASE, CONDITIONS)
    search(menu, "in the section")
    assert menu.setting("In the section")["found"]

    # its section is shown by Show rows, whose pending value the search asks of the store
    menu.set("showRows", False)
    menu.state()
    page = menu.page()
    row = next(row for row in page["rows"] if plain(row.get("text")) == "In the section")
    assert not row["shown"] and not row["found"]
    assert page["hiddenByConditions"], "the page says what matches is hidden by the values of other settings"

    menu.set("showRows", True)
    menu.state()
    assert menu.setting("In the section")["found"]
    assert not menu.page()["hiddenByConditions"]


def test_an_mcm_row_is_found_by_its_text_from_the_game(menu):
    menu.select(MCM_SHOWCASE, GAME_TEXTS)
    section = next(row for row in menu.rows() if row["kind"] == "section")
    name = plain(section["text"])
    assert "|" not in name, "the section's text is a form's name from the game"

    menu.call("select")
    search(menu, name)
    assert found_in(menu, MCM_SHOWCASE), f"the MCM showcase is found by {name!r}"
    menu.select(MCM_SHOWCASE, GAME_TEXTS)
    assert next(row for row in menu.rows() if row["kind"] == "section")["found"]


def test_the_menu_opens_with_no_query(menu):
    assert search(menu, "level")["search"]["matches"] > 0
    menu.call("close")
    menu.open()
    state = menu.state()
    assert state["search"]["text"] == "" and state["search"]["matches"] == 0
    assert all(mod["found"] is None for mod in menu.mods())
