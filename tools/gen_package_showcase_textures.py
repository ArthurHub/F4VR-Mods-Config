"""Writes the textures of the package showcase: the images of its "Text, spacers and images" page.

    python tools/gen_package_showcase_textures.py

They are the images of the MCM showcase's lib.swf, so the two showcases show the same pictures. Each is a DDS
file of uncompressed 32-bit pixels, with the header the framework's vrui_atlas.py writes for its RGBA atlas,
which the game loads. The unit tests' render reads them as they are, with no decoder.

The files are checked in under the showcase's Textures folder. Run this again to change an image, and say in
the config what the new one shows.
"""

import struct
from pathlib import Path

from gen_mcm_showcase_lib import checker, ruler, swatches

OUTPUT = Path(__file__).resolve().parent.parent / "data/showcase/Textures/ModsConfigVR_Showcase"

DDSD_CAPS = 0x1
DDSD_HEIGHT = 0x2
DDSD_WIDTH = 0x4
DDSD_PITCH = 0x8
DDSD_PIXELFORMAT = 0x1000
DDSD_MIPMAPCOUNT = 0x20000
DDPF_ALPHAPIXELS = 0x1
DDPF_RGB = 0x40
DDSCAPS_TEXTURE = 0x1000


def dds(width, height, pixel):
    """A DDS file of one surface with no mipmaps, its pixels as blue, green, red and alpha, from the top row."""
    header = bytearray(128)
    header[0:4] = b"DDS "
    flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT
    struct.pack_into("<IIIIIII", header, 4, 124, flags, height, width, width * 4, 1, 1)
    struct.pack_into("<II", header, 76, 32, DDPF_RGB | DDPF_ALPHAPIXELS)
    struct.pack_into("<IIIII", header, 88, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    struct.pack_into("<I", header, 108, DDSCAPS_TEXTURE)
    data = bytearray()
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixel(x, y)
            data += bytes((blue, green, red, alpha))
    return bytes(header) + bytes(data)


def main():
    # each image: its file, its size, and its pixels
    images = [
        ("swatches.dds", (256, 64), swatches),
        ("ruler.dds", (1024, 64), ruler),
        ("checker.dds", (128, 128), checker),
    ]
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, (width, height), pixel in images:
        (OUTPUT / name).write_bytes(dds(width, height, pixel))
        print(f"Wrote {OUTPUT / name}")


if __name__ == "__main__":
    main()
