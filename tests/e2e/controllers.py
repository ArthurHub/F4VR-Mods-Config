"""The menu worked by the controllers as the player works it: devbench's input tool holds the headset and both
controllers, the ray of the primary hand is turned onto a place of the panel or of the framework's keyboard, and its
buttons and thumbstick are pressed there. The places are the panel's targets and the keyboard's keys, which the mod's
devbench tool tells as they were drawn."""

from devbench import Hands, wait_until
from devbench.vr import handedness

# the owner of the held pose, which devbench lets press over it
OWNER = "e2e-controllers"

# how long the pose is held, longer than any test
HOLD_MS = 600_000

# how far the pointer moves on the panel for a degree of the controller's pitch or yaw, as measured with the panel
# where it opens: a first guess, which each step of an aim corrects
PIXELS_PER_DEGREE = 13.5

# how close to its target the pointer has to come, in the panel's pixels
AIM_TOLERANCE = 3.0
AIM_STEPS = 16
MAX_STEP_DEGREES = 8.0

# the opening gesture of ModsConfigFO4VR.toml: both thumbsticks pressed in, held past its 1.0 s
GESTURE_MS = 1500

# the pitch that points a controller well under the panel, where its ray is on nothing
POINTED_DOWN = -70.0

# how far each step of the search for the keyboard turns the controller down
KEYBOARD_SEARCH_DEGREES = 2.0


def middle(target):
    return target["x"] + target["width"] / 2, target["y"] + target["height"] / 2


class Pointer:
    """
    The ray of the primary hand on the panel. The offhand points down, so the primary hand owns the pointer. Its wand
    stays where the ready pose puts it, and the controller turns in place, pitch for up and down and yaw for left and
    right. The primary hand is turned by its physical side, which saves devbench's turn a handedness call each step.
    """

    def __init__(self, bench, menu):
        self.bench = bench
        self.menu = menu
        self.hands = Hands(bench, OWNER, hold_ms=HOLD_MS)
        self.primary = "right"

    def start(self):
        """
        Take over the devices in the ready pose, with the offhand pointing down. The panel is placed in front of the
        head as the menu opens, and the head was the physical headset's then, so an open menu is opened again, as it
        was, under the held one: what it shows is put back by the test, after.
        """
        self.primary = handedness(self.bench)
        self.hands.start()
        self.hands.turn("left" if self.primary == "right" else "right", pitch=POINTED_DOWN, move_ms=0)
        if self.menu.state()["menu"]["open"]:
            self.menu.close_all()
            self.menu.open()
        return self

    def stop(self):
        self.hands.stop()

    def __enter__(self):
        return self.start()

    def __exit__(self, *exc_info):
        self.stop()

    @property
    def pitch(self):
        return self.hands.placed[self.primary]["pitch"]

    @property
    def yaw(self):
        return self.hands.placed[self.primary]["yaw"]

    def turn(self, pitch, yaw):
        self.hands.turn(self.primary, pitch=pitch, yaw=yaw, move_ms=0)

    def pointer(self):
        """Where the pointer is on the panel as of a frame after now, None while the ray is on no canvas."""
        state = self.menu.state()["pointer"]
        return (state["x"], state["y"]) if state["onPanel"] else None

    def find_panel(self):
        """
        Turn the controller across the room in front until its ray is on the panel, as a hand's ready pose can point
        beside it.
        """
        if self.pointer() is not None:
            return
        for yaw in (0.0, -15.0, 15.0, -30.0, 30.0, -45.0, 45.0):
            for pitch in (10.0, 0.0, 20.0):
                self.turn(pitch, yaw)
                if self.pointer() is not None:
                    return
        raise AssertionError("no turn of the controller puts its ray on the panel")

    def aim(self, x, y):
        """Turn the controller until the pointer is on a place of the panel, within AIM_TOLERANCE."""
        self.find_panel()
        return self._aim(x, y, self.pointer, AIM_TOLERANCE)

    def _aim(self, x, y, locate, tolerance):
        """
        Turn the controller until the ray is on a place of a canvas, as locate tells where it is, within tolerance. The
        pixels a degree moves the ray change across the canvas, so each step learns them from the step before, and a
        step goes MAX_STEP_DEGREES at the most. A step that takes the ray off the canvas is taken back and halved.
        """
        gain = [PIXELS_PER_DEGREE, PIXELS_PER_DEGREE]
        at = locate()
        for _ in range(AIM_STEPS):
            dx, dy = x - at[0], y - at[1]
            if abs(dx) <= tolerance and abs(dy) <= tolerance:
                return at
            # the ray goes left as the controller yaws left, and up as it pitches up
            step_yaw = max(-MAX_STEP_DEGREES, min(MAX_STEP_DEGREES, -dx / gain[0]))
            step_pitch = max(-MAX_STEP_DEGREES, min(MAX_STEP_DEGREES, -dy / gain[1]))
            pitch, yaw = self.pitch, self.yaw
            self.turn(pitch + step_pitch, yaw + step_yaw)
            moved = locate()
            if moved is None:
                self.turn(pitch, yaw)
                gain = [gain[0] * 2, gain[1] * 2]
                continue
            for axis, step, delta in ((0, step_yaw, moved[0] - at[0]), (1, step_pitch, moved[1] - at[1])):
                if abs(step) > 0.5 and abs(delta) > 2:
                    gain[axis] = max(2.0, abs(delta / step))
            at = moved
        raise AssertionError(f"the ray did not come to ({x:.0f}, {y:.0f}): it is at {locate()}")

    def away(self):
        """Point the controller over the panel, at nothing."""
        yaw = self.yaw
        for pitch, turned in ((60.0, yaw), (0.0, yaw + 90.0), (0.0, yaw - 90.0), (-80.0, yaw)):
            self.turn(pitch, turned)
            if self.pointer() is None:
                return
        raise AssertionError("every turn tried leaves the ray on the panel")

    def targets(self):
        """The panel's targets as of the last frame drawn after now: the first call starts the recording."""
        answer = self.menu.call("targets")
        if not answer["recording"]:
            self.menu.state()
        self.menu.state()
        return self.menu.call("targets")

    def target(self, kind, key, path=None):
        """A target of the panel by its kind and key, and for a tree's page row its path."""
        found = self.targets()["targets"]
        for target in found:
            if target["kind"] == kind and target["key"] == key and (path is None or target.get("path") == list(path)):
                return target
        shown = ", ".join(f"{t['kind']}:{t['key']}" for t in found)
        raise AssertionError(f"the panel shows no {kind} {key!r} {path or ''}: it shows {shown}")

    def scroll(self, area):
        """How far a list of the panel is scrolled, and how far it can be: None while it is not drawn."""
        return next(((s["y"], s["max"]) for s in self.targets()["scrolls"] if s["area"] == area), None)

    def point_at(self, kind, key, path=None):
        """Aim at the middle of a target, and return it."""
        target = self.target(kind, key, path)
        self.aim(*middle(target))
        return target

    def trigger(self):
        """A pull of the primary hand's trigger, and the frames it takes to act."""
        self.hands.tap("primary", ["trigger"])
        self.menu.state()

    def press(self, kind, key, path=None):
        """Aim at a target and pull the trigger."""
        self.point_at(kind, key, path)
        self.trigger()

    def grip(self):
        """A tap of the primary hand's grip, which is Back while its ray is on the panel."""
        self.hands.tap("primary", ["grip"])
        self.menu.state()

    def stick(self, x, y, duration_ms=150):
        """A push of the primary hand's thumbstick, x right and y up, each -1..1."""
        self.hands.stick("primary", x, y, duration_ms)
        self.menu.state()

    def keyboard(self, key=None):
        """
        The framework's keyboard as the mod's devbench tool reports it, with a key of its page pressed first, by its
        label or its action, as a pull on it does.
        """
        return self.menu.call("keyboard", **({"key": key} if key else {}))

    def on_keyboard(self):
        """Where the primary hand's ray is on the keyboard, in its canvas's pixels, None while it is not on it."""
        hand = self.keyboard()["hands"]["primary"]
        return (hand["x"], hand["y"]) if hand["on"] else None

    def find_keyboard(self):
        """Turn the controller down from where it points, as the keyboard is under the panel, until its ray is on it."""
        if self.on_keyboard() is not None:
            return
        pitch, yaw = self.pitch, self.yaw
        for step in range(1, 40):
            self.turn(pitch - step * KEYBOARD_SEARCH_DEGREES, yaw)
            if self.on_keyboard() is not None:
                return
        raise AssertionError("no turn of the controller down puts its ray on the keyboard")

    def key(self, label):
        """A key of the keyboard's page by its label as shown, or by its action for one that types no character."""
        keys = self.keyboard()["keys"]
        found = next((k for k in keys if k["label"] == label), None) or next((k for k in keys if k["action"] == label), None)
        assert found, f"the keyboard's page has no key {label!r}: it has {[k['label'] or k['action'] for k in keys]}"
        return found

    def aim_keyboard(self, x, y, tolerance):
        """Turn the controller until its ray is on a place of the keyboard, within tolerance of its pixels."""
        self.find_keyboard()
        return self._aim(x, y, self.on_keyboard, tolerance)

    def point_at_key(self, label):
        """Aim at the middle of a key of the keyboard, within a quarter of the key, and return it."""
        found = self.key(label)
        self.aim_keyboard(*middle(found), min(found["width"], found["height"]) / 4)
        return found

    def type_key(self, label):
        """Aim at a key of the keyboard and pull the trigger."""
        self.point_at_key(label)
        self.trigger()

    def gesture(self):
        """The opening gesture: both thumbsticks pressed in together and held."""
        self.hands.press(["primary", "offhand"], ["thumbstick"], GESTURE_MS)

    def wait_for(self, condition, what, timeout=3):
        return wait_until(condition, timeout, interval=0.05, what=what)
