"""Contrast of the text colors in tokens.json, with a white world behind the glass (the worst case).

Prints the table of the Color section of project/README.md. Run it after changing a color.
Usage: python tools/check_contrast.py [project dir]
"""
import json
import sys
from pathlib import Path

ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / 'project'
WORLD = (255, 255, 255)


def rgba(h):
    h = h.lstrip('#')
    c = tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))
    return c, (int(h[6:8], 16) / 255 if len(h) == 8 else 1.0)


def over(fg, a, bg):
    return tuple(round(f * a + b * (1 - a)) for f, b in zip(fg, bg))  # 8-bit, as the render target holds it


def lum(rgb):
    def lin(c):
        c /= 255
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(x) for x in rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def ratio(a, b):
    la, lb = lum(a), lum(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)


tokens = json.loads((ROOT / 'tokens.json').read_text(encoding='utf-8'))
theme = tokens['color']['themes'][0]['id']
col = {t['name']: rgba(t['value'][theme] if isinstance(t['value'], dict) else t['value']) for t in tokens['color']['tokens']}

glass = over(*col['glass'], WORLD)
grounds = {'glass': glass, 'hover': over(*col['hover'], glass), 'pressed': over(*col['pressed'], glass)}
print('| Text | ' + ' | '.join(f'On `{g}`' for g in grounds) + ' |')
for name in ('green', 'green-muted', 'amber', 'red', 'accent'):
    fg = col[name][0]
    print(f'| `{name}` | ' + ' | '.join(f'{ratio(fg, bg):.1f}:1' for bg in grounds.values()) + ' |')
print(f"| `on-green` on `green` | {ratio(col['on-green'][0], col['green'][0]):.1f}:1 |")
print(f"`frame-border` on `glass`: {ratio(over(*col['frame-border'], glass), glass):.1f}:1, "
      f"`rule` on `glass`: {ratio(over(*col['rule'], glass), glass):.1f}:1")
