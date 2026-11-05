"""Writes the pictures of the config spec showcase walkthrough: a PNG of each of its pages and of the editors a
press opens, from the render tests.

    build/tests/Release/ModsConfigFO4VR_tests "[render]"   (ctest does not run them)
    python tools/gen_docs_renders.py

It reads the renders in build/tests/render/ and writes docs/assets/showcase/, each in 256 colors, which a flat
panel needs and which keeps the docs small. Needs Pillow (pip install pillow).

The files are checked in. Run the render test and this again when a page of the showcase or the look of the
panel changes, and add a page here when the walkthrough gains one.
"""

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
RENDERS = ROOT / "build/tests/render"
OUTPUT = ROOT / "docs/assets/showcase"

# the render of each page, as the render test names it after the page's place, and its name in the docs. The
# editors are of the showcase's rows where its tests open them, and of the tests' own rows otherwise
PAGES = {
    "config-spec-showcase-front-page": "front-page",
    "config-spec-showcase-page-1": "general",
    "config-spec-showcase-page-2-1": "switchers",
    "config-spec-showcase-page-2-2": "sliders",
    "config-spec-showcase-page-2-3": "choices",
    "config-spec-showcase-page-2-4": "text-inputs",
    "config-spec-showcase-page-2-5": "buttons",
    "config-spec-showcase-page-2-6": "bindings",
    "config-spec-showcase-page-2-7": "haptics",
    "config-spec-showcase-page-2-8": "colors",
    "config-spec-showcase-page-2-9": "transforms",
    "config-spec-showcase-page-2-10": "activation-spheres",
    "config-spec-showcase-page-2-11": "text-sizes",
    "config-spec-showcase-page-2-12": "text-colors",
    "config-spec-showcase-page-2-13": "text-align",
    "config-spec-showcase-page-2-14": "text-tags",
    "config-spec-showcase-page-2-15": "images",
    "config-spec-showcase-page-2-16": "numbers",
    "config-spec-showcase-page-2-17": "sections",
    "config-spec-showcase-page-2-18": "pages",
    "config-spec-showcase-page-2-19": "read-only",
    "config-spec-showcase-page-2-20": "files",
    "config-spec-showcase-page-2-21": "in-the-game",
    "config-spec-showcase-page-2-22": "applies",
    "config-spec-showcase-page-2-23": "conditions",
    "config-spec-showcase-page-2-24": "warnings",
    "config-spec-showcase-page-2-25": "translations",
    "config-spec-showcase-page-2-26": "mistakes",
    # the editors and lists a press opens
    "dropdown-open": "dropdown-open",
    "file-choices-open": "folder-choice-open",
    "button-confirmation": "button-confirmation",
    "binding-editor": "binding-editor",
    "config-spec-haptics-list": "haptic-list",
    "config-spec-color-editor": "color-editor",
    "config-spec-transform-editor": "transform-editor",
}


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for render, name in PAGES.items():
        image = Image.open(RENDERS / f"{render}.bmp").convert("RGB")
        image = image.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        image.save(OUTPUT / f"{name}.png", optimize=True)
    print(f"Wrote {len(PAGES)} pictures to {OUTPUT}")


if __name__ == "__main__":
    main()
