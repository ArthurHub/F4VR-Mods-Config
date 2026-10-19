"""Writes the lib.swf of the MCM showcase: the images of its "Images" section.

    python tools/gen_mcm_showcase_lib.py

MCM's menu makes an object of the class an image row names, and shows it only where it is a MovieClip.
So each image is what a Flash editor stores for a bitmap that is put on the stage of a movie clip: the
bitmap, a shape that is filled with it, a sprite that places the shape, and a class of the sprite that
extends MovieClip.

The file is checked in beside the showcase's config.json. Run this again to change an image, and say
in the config what the new one shows.
"""

import struct
import zlib
from pathlib import Path

OUTPUT = Path(__file__).resolve().parent.parent / "data/showcase/MCM/Config/ModsConfigVR_McmShowcase/lib.swf"

TWIPS = 20

TAG_END = 0
TAG_SHOW_FRAME = 1
TAG_DEFINE_SHAPE = 2
TAG_SET_BACKGROUND_COLOR = 9
TAG_PLACE_OBJECT2 = 26
TAG_DEFINE_BITS_LOSSLESS2 = 36
TAG_DEFINE_SPRITE = 39
TAG_FILE_ATTRIBUTES = 69
TAG_SYMBOL_CLASS = 76
TAG_DO_ABC = 82


class Bits:
    """Values that a file stores as bits, from the high bit of a byte."""

    def __init__(self):
        self.bits = []

    def add(self, value, count):
        self.bits += [(value >> i) & 1 for i in range(count - 1, -1, -1)]
        return self

    def to_bytes(self):
        bits = self.bits + [0] * (-len(self.bits) % 8)
        return bytes(int("".join(map(str, bits[i:i + 8])), 2) for i in range(0, len(bits), 8))


def bits_for(*values):
    """How many bits hold each of the signed values."""
    return max(abs(value).bit_length() for value in values) + 1


def rect(width, height):
    """A rectangle from the origin, of a size in twips."""
    count = bits_for(width, height)
    return Bits().add(count, 5).add(0, count).add(width, count).add(0, count).add(height, count).to_bytes()


def matrix(scale_x=None, scale_y=None):
    """A matrix that scales, or one that changes nothing."""
    bits = Bits()
    if scale_x is None:
        bits.add(0, 1)
    else:
        fixed_x, fixed_y = round(scale_x * 65536), round(scale_y * 65536)
        count = bits_for(fixed_x, fixed_y)
        bits.add(1, 1).add(count, 5).add(fixed_x, count).add(fixed_y, count)
    return bits.add(0, 1).add(0, 5).to_bytes()


def tag(code, body=b""):
    if len(body) < 63 and code != TAG_DEFINE_BITS_LOSSLESS2:
        return struct.pack("<H", code << 6 | len(body)) + body
    return struct.pack("<HI", code << 6 | 63, len(body)) + body


def bitmap(character, width, height, pixel):
    """A bitmap of four bytes a pixel. pixel(x, y) is its color, as (red, green, blue, alpha)."""
    data = bytearray()
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixel(x, y)
            # a file stores a color multiplied by its alpha
            data += bytes((alpha, red * alpha // 255, green * alpha // 255, blue * alpha // 255))
    return tag(TAG_DEFINE_BITS_LOSSLESS2, struct.pack("<HBHH", character, 5, width, height) + zlib.compress(bytes(data), 9))


def bitmap_shape(character, bitmap_character, bitmap_width, bitmap_height, width, height):
    """A rectangle of a size in pixels that is filled with the whole of a bitmap."""
    fill_bitmap = 0x41
    body = struct.pack("<H", character) + rect(width * TWIPS, height * TWIPS)
    body += bytes((2,))
    # the fill a Flash editor stores first, which names no bitmap
    body += struct.pack("<BH", fill_bitmap, 0xFFFF) + matrix(TWIPS, TWIPS)
    body += struct.pack("<BH", fill_bitmap, bitmap_character) + matrix(TWIPS * width / bitmap_width, TWIPS * height / bitmap_height)
    body += bytes((0,))

    edge_bits = bits_for(width * TWIPS, height * TWIPS)
    records = Bits().add(2, 4).add(0, 4)
    # the style of the edges: they start at the origin, with the second fill at their right
    records.add(0b000101, 6).add(1, 5).add(0, 1).add(0, 1).add(2, 2)
    for vertical, delta in ((0, width * TWIPS), (1, height * TWIPS), (0, -width * TWIPS), (1, -height * TWIPS)):
        records.add(0b11, 2).add(edge_bits - 2, 4).add(0, 1).add(vertical, 1).add(delta & (1 << edge_bits) - 1, edge_bits)
    records.add(0, 6)
    return tag(TAG_DEFINE_SHAPE, body + records.to_bytes())


def sprite(character, shape_character):
    """A movie clip of one frame that places a shape."""
    has_character_and_matrix = 0x06
    place = tag(TAG_PLACE_OBJECT2, struct.pack("<BHH", has_character_and_matrix, 1, shape_character) + matrix())
    return tag(TAG_DEFINE_SPRITE, struct.pack("<HH", character, 1) + place + tag(TAG_SHOW_FRAME) + tag(TAG_END))


def movie_clip_class(name):
    """The code of a class with the given name that extends MovieClip and adds nothing, as a Flash editor compiles it."""
    text = name.encode("ascii")
    assert len(text) < 128
    strings = [b"", text, b"flash.display", b"MovieClip", b"Object", b"flash.events", b"EventDispatcher", b"DisplayObject", b"InteractiveObject",
               b"DisplayObjectContainer", b"Sprite"]
    abc = bytes.fromhex("10002e00" "00" "00" "00")
    abc += bytes((len(strings) + 1,)) + b"".join(bytes((len(string),)) + string for string in strings)
    abc += bytes.fromhex(
        # the namespaces, no namespace sets, and the names of the class and of those it extends
        "05" "1601" "1603" "1802" "1606" "00"
        "09" "070102" "070204" "070105" "070407" "070208" "070209" "07020a" "07020b"
        # three methods with no arguments, no metadata, the class and its script
        "03" "00000000" "00000000" "00000000" "00"
        "01" "01020803" "00" "01" "00" "00" "00"
        "01" "02" "01" "01040100"
        # the bodies of the methods: the class's and the instance's constructors, and the script that makes the class
        "03"
        "00" "01" "01" "09" "0a" "03" "d03047" "00" "00"
        "01" "01" "01" "0a" "0b" "06" "d030d0490047" "00" "00"
        "02" "02" "01" "01" "09" "27" "d0306500600330600430600530600630600730600830600230600258001d1d1d1d1d1d1d680147" "00" "00")
    lazy = 1
    return tag(TAG_DO_ABC, struct.pack("<I", lazy) + b"\0" + abc)


def symbols(classes):
    body = struct.pack("<H", len(classes))
    for character, name in classes:
        body += struct.pack("<H", character) + name.encode("ascii") + b"\0"
    return tag(TAG_SYMBOL_CLASS, body)


def swf(tags):
    version = 10
    frame_rate = 24 << 8
    data = rect(550 * TWIPS, 400 * TWIPS) + struct.pack("<HH", frame_rate, 1) + b"".join(tags)
    return b"CWS" + struct.pack("<BI", version, len(data) + 8) + zlib.compress(data, 9)


def swatches(x, y):
    """256 by 64: red, green, blue and white, then black to white, then white from clear to solid.
    The yellow corner is its top left one."""
    if x < 16 and y < 16:
        return 255, 255, 0, 255
    if x < 128:
        return ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255))[x // 32] + (255,)
    if x < 192:
        grey = (x - 128) * 255 // 63
        return grey, grey, grey, 255
    return 255, 255, 255, (x - 192) * 255 // 63


def ruler(x, y):
    """1024 by 64: a band of the panel's green that gets lighter to the right, with a white mark every 64 pixels
    and a longer one every 256."""
    if x % 64 < 2 and (y < 24 or x % 256 < 2):
        return 255, 255, 255, 255
    light = x * 255 // 1023
    return 10 * light // 255, 60 + 190 * light // 255, 40 + 80 * light // 255, 255


def checker(x, y):
    """128 by 128: squares of 16 pixels, white and half clear green, in a white frame."""
    if min(x, y) < 4 or max(x, y) >= 124:
        return 255, 255, 255, 255
    return (255, 255, 255, 255) if (x // 16 + y // 16) % 2 == 0 else (10, 250, 120, 128)


def main():
    # each image: its class, the size of its bitmap, the size it is shown at, and its pixels
    images = [
        ("Swatches", (256, 64), (256, 64), swatches),
        ("Ruler", (1024, 64), (1024, 64), ruler),
        ("Checker", (128, 128), (64, 64), checker),
    ]
    tags = [tag(TAG_FILE_ATTRIBUTES, struct.pack("<I", 0x08)), tag(TAG_SET_BACKGROUND_COLOR, b"\xff\xff\xff")]
    classes = []
    for index, (name, (bitmap_width, bitmap_height), (width, height), pixel) in enumerate(images):
        first = index * 3 + 1
        tags.append(bitmap(first, bitmap_width, bitmap_height, pixel))
        tags.append(bitmap_shape(first + 1, first, bitmap_width, bitmap_height, width, height))
        tags.append(sprite(first + 2, first + 1))
        classes.append((first + 2, name))
    tags += [movie_clip_class(name) for _, name in classes]
    tags += [symbols(classes), tag(TAG_SHOW_FRAME), tag(TAG_END)]

    OUTPUT.write_bytes(swf(tags))
    print(f"Wrote {OUTPUT}")


if __name__ == "__main__":
    main()
