"""The menu, driven through the mod's devbench tool as the player's presses drive it."""

import math
import re

from devbench import wait_until

TAG = re.compile(r"<[^>]*>")


def plain(text):
    """A row's text without its HTML tags."""
    return TAG.sub("", text or "").strip()


def same(a, b):
    """Whether two values are the same, a number within a rounding of the other, a table and a list part by part."""
    if isinstance(a, bool) or isinstance(b, bool):
        return a is b
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return math.isclose(a, b, rel_tol=1e-4, abs_tol=1e-4)
    if isinstance(a, dict) and isinstance(b, dict):
        return a.keys() == b.keys() and all(same(a[key], b[key]) for key in a)
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(same(x, y) for x, y in zip(a, b))
    return a == b


def page_paths(pages, above=()):
    """The path of names of each page of a mod, as the mods action lists its pages, the pages under a page after it."""
    for page in pages:
        name = page if isinstance(page, str) else page["name"]
        yield [*above, name]
        if isinstance(page, dict):
            yield from page_paths(page["pages"], [*above, name])


class Menu:
    """The menu of the running game."""

    def __init__(self, bench):
        self.bench = bench

    def call(self, action, **args):
        return self.bench.mod(action, **args)

    def state(self):
        """The mod's state as of a frame after this call: an action's change is in it."""
        frame = self.call("state")["liveness"]["frame"]
        return wait_until(lambda: (s := self.call("state")) and s["liveness"]["frame"] > frame and s, 5, interval=0.02, what="a new frame of the state")

    def close_all(self):
        """Close the menu, its changes discarded, whatever it shows."""
        state = self.state()
        if not state["menu"]["open"]:
            return
        if state["selection"]["asking"]:
            self.call("answer", button="discard")
        if state["session"] and state["session"]["dirty"]:
            self.call("discard")
        self.call("close")
        if self.state()["selection"]["asking"]:
            self.call("answer", button="discard")
            self.call("close")

    def mods(self):
        return self.call("mods")["mods"]

    def open(self):
        self.call("open")

    def select(self, mod, page=()):
        """Show a mod's front page, or a page by the names on the way to it; it fails when the dialog asks first."""
        answer = self.call("select", mod=mod, page=list(page))
        if answer["asking"]:
            raise AssertionError(f"the dialog asks before {mod} {list(page)} is shown")
        return answer

    def page(self):
        return self.call("page")

    def rows(self):
        return self.page()["rows"]

    def row(self, text=None, id=None, kind=None):
        """The row of the page that is shown with a text, as it is drawn or without its tags, or a setting's id."""
        for row in self.rows():
            if kind and row["kind"] != kind:
                continue
            if id is not None and row.get("id") == id:
                return row
            if text is not None and (row.get("text") == text or plain(row.get("text")) == text):
                return row
        raise AssertionError(f"the page has no row {text or id!r}")

    def setting(self, text=None, id=None):
        return self.row(text=text, id=id, kind="setting")

    def set(self, id, value):
        """Set a setting as its control does, and return its row then."""
        answer = self.call("setValue", id=id, value=value)
        assert answer["set"], f"{id} was not set to {value!r}"
        return answer["setting"]

    def save(self):
        answer = self.call("save")
        assert answer["saved"], f"the Save failed: {answer['reason']}"
        return answer

    def discard(self, id=None):
        return self.call("discard", **({"id": id} if id else {}))

    def answer(self, button):
        return self.call("answer", button=button)
