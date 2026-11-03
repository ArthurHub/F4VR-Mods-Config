"""Writes the mod's own icons of the wrist buttons, beside Exit's, which is the framework's: Restore, Cancel and Done.

    python tools/gen_wrist_icons.py

Restore shows the menu's panel again: a panel with its title bar, and an arrow up in it. Cancel and Done end Adjust in
world: an X, and a check. They are drawn as the framework's vrui icons are, as Exit's: white strokes about 6 px wide, in the middle of a
140 x 100 texture of uncompressed 32-bit pixels with no mipmaps, and its alpha is the coverage of its edges.

The files are checked in under the mod's Textures folder. Run this again to change an icon.
"""

import math
from pathlib import Path

from gen_config_spec_showcase_textures import dds

OUTPUT = Path(__file__).resolve().parent.parent / "data/mod/Textures/ModsConfigFO4VR/vrui"

WIDTH = 140
HEIGHT = 100
STROKE = 6.0

# samples of a pixel on each axis, for the coverage of its edges
SAMPLES = 4


def in_rounded_rect(x, y, left, top, right, bottom, radius):
    """Whether a point is inside a rectangle whose corners are rounded."""
    if not (left <= x <= right and top <= y <= bottom):
        return False
    cx = min(max(x, left + radius), right - radius)
    cy = min(max(y, top + radius), bottom - radius)
    return math.hypot(x - cx, y - cy) <= radius


def near_segment(x, y, ax, ay, bx, by, half):
    """Whether a point is within a distance of a segment, so a line with round ends."""
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(x - (ax + t * dx), y - (ay + t * dy)) <= half


def restore(x, y):
    """Whether a point is on the Restore icon: the panel's outline, its title bar's line, and the arrow up."""
    left, top, right, bottom, radius = 27.0, 17.0, 111.0, 83.0, 9.0
    if in_rounded_rect(x, y, left, top, right, bottom, radius) and not in_rounded_rect(
        x, y, left + STROKE, top + STROKE, right - STROKE, bottom - STROKE, radius - STROKE
    ):
        return True
    half = STROKE / 2
    middle = (left + right) / 2
    return (
        near_segment(x, y, left + half, 34.0, right - half, 34.0, half)
        or near_segment(x, y, middle, 72.0, middle, 46.0, half)
        or near_segment(x, y, middle - 12.0, 58.0, middle, 46.0, half)
        or near_segment(x, y, middle + 12.0, 58.0, middle, 46.0, half)
    )


def cancel(x, y):
    """Whether a point is on the Cancel icon: an X, as tall as the texture leaves room for, as Exit's icon is."""
    half = STROKE / 2
    return near_segment(x, y, 30.0, 10.0, 110.0, 90.0, half) or near_segment(x, y, 110.0, 10.0, 30.0, 90.0, half)


def done(x, y):
    """Whether a point is on the Done icon: a check, as tall as the texture leaves room for, as Exit's icon is."""
    half = STROKE / 2
    return near_segment(x, y, 22.0, 52.0, 56.0, 86.0, half) or near_segment(x, y, 56.0, 86.0, 118.0, 12.0, half)


def icon(shape):
    """The pixels of an icon, white, with the coverage of the shape as their alpha."""

    def pixel(x, y):
        covered = sum(
            shape(x + (i + 0.5) / SAMPLES, y + (j + 0.5) / SAMPLES) for i in range(SAMPLES) for j in range(SAMPLES)
        )
        alpha = round(255 * covered / (SAMPLES * SAMPLES))
        return (255, 255, 255, alpha) if alpha else (0, 0, 0, 0)

    return pixel


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, shape in (("restore.dds", restore), ("cancel.dds", cancel), ("done.dds", done)):
        path = OUTPUT / name
        path.write_bytes(dds(WIDTH, HEIGHT, icon(shape)))
        print(f"Wrote {path}")


if __name__ == "__main__":
    main()
