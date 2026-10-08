"""Screenshots of the previews, to check a change before publishing.

Compiles tokens.json to CSS the way the Design System type does, adds components/bundle.css, and saves
<Name>.png (and the page it rendered, <Name>.html) for each component.
Needs Playwright: pip install playwright && python -m playwright install chromium
Usage: python tools/render.py [Component ...]        (all components when none is named)
       python tools/render.py --project DIR --out DIR [Component ...]
"""
import argparse
import asyncio
import json
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
ap = argparse.ArgumentParser()
ap.add_argument('--project', type=Path, default=HERE.parent / 'project')
ap.add_argument('--out', type=Path, default=HERE.parent / 'out')
ap.add_argument('only', nargs='*')
args = ap.parse_args()
ROOT, OUT = args.project.resolve(), args.out.resolve()
OUT.mkdir(parents=True, exist_ok=True)

t = json.loads((ROOT / 'tokens.json').read_text(encoding='utf-8'))
first = t['color']['themes'][0]['id']
lines = [':root, [data-theme="%s"] {' % first]
for tok in t['color']['tokens']:
    v = tok['value'] if isinstance(tok['value'], str) else tok['value'][first]
    lines.append(f'  --{tok["name"]}: {v};')
lines.append('}')
lines.append(':root {')
for fam in ('spacing', 'radius', 'size', 'border', 'opacity'):
    for tok in t[fam]['tokens']:
        lines.append(f'  --{tok["name"]}: {tok["value"]};')
for k, v in t['type']['families'].items():
    lines.append(f'  --font-{k}: {v};')
lines.append('}')
for g in t['type']['groups']:
    fam = t['type']['families'][g['family']]
    for s in g['styles']:
        lines.append(f'.{s["name"]} {{ font-family: {fam}; font-size: {s["fontSize"]}; line-height: {s["lineHeight"]}; font-weight: {s["fontWeight"]}; }}')
font_uri = (ROOT / t['type']['fonts'][0]['file']).as_uri()
lines.append(f'@font-face {{ font-family: "Roboto"; src: url("{font_uri}"); font-weight: 500; }}')
tokens_css = '\n'.join(lines)
bundle_css = (ROOT / 'components' / 'bundle.css').read_text(encoding='utf-8')


async def main():
    from playwright.async_api import async_playwright
    async with async_playwright() as p:
        b = await p.chromium.launch()
        for comp in sorted(d.name for d in (ROOT / 'components').iterdir() if d.is_dir()):
            f = ROOT / 'components' / comp / 'preview.html'
            if not f.exists() or (args.only and comp not in args.only):
                continue
            html = f.read_text(encoding='utf-8')
            m = re.match(r'<!-- @dsCard([^>]*)-->', html)
            attrs = dict(re.findall(r'(\w+)=("[^"]*"|\S+)', m.group(1))) if m else {}
            w = int(attrs.get('width', '960').strip('"'))
            h = int(attrs.get('height', '400').strip('"'))
            html = html.replace('<head>', f'<head><style>{tokens_css}\n{bundle_css}</style>', 1).replace('<html', f'<html data-theme="{first}"', 1)
            page = OUT / f'{comp}.html'
            page.write_text(html, encoding='utf-8')
            pg = await b.new_page(viewport={'width': w, 'height': h})
            await pg.goto(page.as_uri())
            await pg.wait_for_timeout(300)
            sh = await pg.evaluate('document.documentElement.scrollHeight')
            await pg.screenshot(path=str(OUT / f'{comp}.png'), full_page=True)
            print(comp, f'{w}x{h}', 'content height', sh, '(taller than the card)' if sh > h else '')
            await pg.close()
        await b.close()

asyncio.run(main())
