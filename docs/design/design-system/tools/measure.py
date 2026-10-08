"""The tree's geometry in rendered pages: its height, the rows' area, whether they overflow, and names cut short.

Run render.py first; this reads the pages it saved.
Usage: python tools/measure.py [Component ...]   (default: the panels and ModTree)
"""
import asyncio
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / 'out'
COMPS = sys.argv[1:] or ['ModTree', 'FullPanel', 'FullPanelPage', 'FullPanelHome', 'FullPanelCategories']
JS = '''() => [...document.querySelectorAll('.mc-nav')].map(n => {
  const b = n.getBoundingClientRect(), list = n.querySelector('.mc-nav-rows') || n;
  const rows = n.querySelectorAll('.mc-tree-row, .mc-cat');
  return {navH: b.height, rowsH: list.clientHeight, rowsContentH: list.scrollHeight, overflows: list.scrollHeight > list.clientHeight + 1,
          rows: rows.length, rowW: rows.length ? rows[0].getBoundingClientRect().width : 0,
          cutShort: [...n.querySelectorAll('.name')].filter(e => e.scrollWidth > e.clientWidth + 0.5).map(e => e.textContent)};
})'''


async def main():
    from playwright.async_api import async_playwright
    async with async_playwright() as p:
        b = await p.chromium.launch()
        for c in COMPS:
            pg = await b.new_page(viewport={'width': 1300, 'height': 900})
            await pg.goto((OUT / f'{c}.html').as_uri())
            await pg.wait_for_timeout(300)
            for i, r in enumerate(await pg.evaluate(JS)):
                print(c, i, r)
            await pg.close()
        await b.close()

asyncio.run(main())
