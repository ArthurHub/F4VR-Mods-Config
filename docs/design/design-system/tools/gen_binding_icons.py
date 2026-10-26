"""The binding icons for the previews: writes tools/binding_icons.json, every icon of the framework's set the mod
ships (data/mod/Textures/ModsConfigFO4/f4cf/bindings/) as PNG data, which gen_previews.py draws the InputBinding
previews with, so the previews need no Pillow.

The framework draws the icons (tools/gen_binding_icons.py in F4VR-CommonFramework) into its mod-template. Copy them
from there into the mod's f4cf folder when the framework updates, then run this again.
Needs Pillow: pip install pillow
Usage: python tools/gen_binding_icons.py
"""
import base64
import io
import json
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
ICONS = ROOT / 'data' / 'mod' / 'Textures' / 'ModsConfigFO4' / 'f4cf' / 'bindings'


def write_json():
    icons = {}
    for f in sorted(ICONS.glob('*.dds')):
        im = Image.open(f).convert('RGBA')
        buf = io.BytesIO()
        im.save(buf, 'PNG', optimize=True)
        icons[f.stem] = {'w': im.width, 'h': im.height, 'png': base64.b64encode(buf.getvalue()).decode('ascii')}
    (HERE / 'binding_icons.json').write_text(json.dumps(icons, indent=1) + '\n', encoding='utf-8', newline='\n')
    print(len(icons), 'icons in binding_icons.json')


write_json()
