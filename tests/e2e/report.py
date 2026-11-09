"""The report of a run: one HTML page with each test's result, the sweep of each mod, the game's dialogs the run closed,
and a picture of the panel at the end of each test that had the menu open.

It is written to build/e2e/<date>-<time>/index.html, with the pictures beside it, and the run ends with its link.
"""

import html
import re
import time
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# the areas, by the file of their tests, in the order the page shows them
AREAS = {
    "test_showcase.py": "Showcase walkthrough",
    "test_navigation.py": "Navigation",
    "test_rows.py": "Rows",
    "test_session.py": "The editing session",
    "test_dialog.py": "The dialog",
    "test_mcm_keep_revert.py": "MCM Keep and Revert",
    "test_live_preview.py": "Live preview",
    "test_sweep.py": "Sweep",
}

STYLE = """
:root { --bg: #ffffff; --fg: #1d2125; --muted: #5f6b76; --line: #d8dde2; --card: #f5f7f9;
  --pass: #1f7a3a; --fail: #b3261e; --skip: #8a6d00; --known: #6a4fb3; }
@media (prefers-color-scheme: dark) { :root { --bg: #15181b; --fg: #e3e7ea; --muted: #97a3ad; --line: #2c3237;
  --card: #1d2125; --pass: #5cc97f; --fail: #ff8a80; --skip: #e3c55a; --known: #b9a2ff; } }
* { box-sizing: border-box; }
body { margin: 0; padding: 24px 16px 48px; background: var(--bg); color: var(--fg);
  font: 14px/1.5 system-ui, -apple-system, "Segoe UI", sans-serif; }
main { max-width: 1100px; margin: 0 auto; }
h1 { font-size: 22px; margin: 0 0 4px; } h2 { font-size: 17px; margin: 32px 0 8px; }
.meta { color: var(--muted); margin-bottom: 16px; }
.totals { display: flex; flex-wrap: wrap; gap: 8px; margin: 12px 0; }
.total { background: var(--card); border: 1px solid var(--line); border-radius: 8px; padding: 8px 14px; min-width: 96px; }
.total b { display: block; font-size: 20px; }
table { width: 100%; border-collapse: collapse; }
th, td { text-align: left; padding: 6px 8px; border-bottom: 1px solid var(--line); vertical-align: top; }
th { color: var(--muted); font-weight: 600; font-size: 12px; text-transform: uppercase; letter-spacing: .04em; }
.passed { color: var(--pass); } .failed, .error { color: var(--fail); } .skipped { color: var(--skip); } .known { color: var(--known); }
.badge { font-weight: 600; }
pre { white-space: pre-wrap; word-break: break-word; background: var(--card); border: 1px solid var(--line);
  border-radius: 6px; padding: 8px; margin: 6px 0 0; font-size: 12px; }
details summary { cursor: pointer; color: var(--muted); }
ul.list { margin: 4px 0 0; padding-left: 18px; font-size: 13px; }
.shot img { width: 220px; border: 1px solid var(--line); border-radius: 4px; display: block; }
.gallery { display: grid; grid-template-columns: repeat(auto-fill, minmax(220px, 1fr)); gap: 12px; }
.gallery figure { margin: 0; } .gallery img { width: 100%; border: 1px solid var(--line); border-radius: 4px; }
.gallery figcaption { font-size: 12px; color: var(--muted); }
@media (max-width: 640px) { .shot img { width: 120px; } td, th { padding: 4px; } }
"""


class Report:
    """What a run did, collected as it runs and written as one page when it ends."""

    def __init__(self):
        self.started = time.monotonic()
        self.date = datetime.now()
        self.folder = ROOT / "build" / "e2e" / self.date.strftime("%Y%m%d-%H%M%S")
        self.tests = []
        self.sweeps = {}
        self.dialogs = []
        self.snapshots = {}
        self.notes = []

    def picture(self, nodeid):
        """The file a picture of the panel at the end of a test is written to."""
        name = "".join(c if c.isalnum() or c in "-_" else "_" for c in nodeid.split("::", 1)[-1])[:120]
        path = self.folder / "snapshots" / f"{len(self.snapshots):03d}-{name}.png"
        path.parent.mkdir(parents=True, exist_ok=True)
        return path

    def add_result(self, report):
        """A test's result, from pytest's report of its setup, call or teardown: the call's, or a setup's or a
        teardown's that failed."""
        if report.when != "call" and report.outcome == "passed":
            return
        nodeid = report.nodeid
        entry = next((test for test in self.tests if test["nodeid"] == nodeid), None)
        outcome = report.outcome if report.when == "call" else ("error" if report.failed else report.outcome)
        message = ""
        if report.failed:
            message = getattr(report.longrepr, "reprcrash", None)
            message = message.message if message else str(report.longrepr)[-3000:]
        elif report.skipped and isinstance(report.longrepr, tuple):
            message = report.longrepr[2]
        if entry:
            if outcome != "passed":
                entry.update(outcome=outcome, message=entry["message"] or message)
            return
        file, _, name = nodeid.partition("::")
        self.tests.append({"nodeid": nodeid, "file": Path(file).name, "name": name, "outcome": outcome, "duration": report.duration, "message": message})

    def write(self):
        """Write the page, and return its path."""
        self.folder.mkdir(parents=True, exist_ok=True)
        page = self.folder / "index.html"
        page.write_text(self._html(), encoding="utf-8")
        return page

    def _shot(self, nodeid):
        path = self.snapshots.get(nodeid)
        if not path:
            return ""
        return f'<a class="shot" href="{html.escape(path)}"><img src="{html.escape(path)}" alt="the panel" loading="lazy"></a>'

    def _html(self):
        counts = {}
        for test in self.tests:
            counts[test["outcome"]] = counts.get(test["outcome"], 0) + 1
        seconds = time.monotonic() - self.started
        known = sum(len(sweep["known"]) for sweep in self.sweeps.values())
        out = [
            "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>",
            f"<title>E2E Run Report</title><style>{STYLE}</style></head><body><main>",
            "<h1>Mods Config FO4 VR: end-to-end run</h1>",
            f"<div class='meta'>{self.date:%Y-%m-%d %H:%M} &middot; {seconds / 60:.1f} min &middot; {len(self.tests)} tests</div>",
            "<div class='totals'>",
        ]
        for outcome in ("passed", "failed", "error", "skipped"):
            out.append(f"<div class='total'><b class='{outcome}'>{counts.get(outcome, 0)}</b>{outcome}</div>")
        out.append(f"<div class='total'><b class='known'>{known}</b>known issues</div></div>")
        for note in self.notes:
            out.append(f"<div class='meta'>{html.escape(note)}</div>")

        failures = [test for test in self.tests if test["outcome"] in ("failed", "error")]
        if failures:
            out.append("<h2>Failures</h2><table><tr><th>Test</th><th>What</th><th>The panel</th></tr>")
            for test in failures:
                out.append(f"<tr><td class='failed'>{html.escape(test['name'])}<br><small>{html.escape(test['file'])}</small></td>"
                           f"<td><pre>{html.escape(test['message'])}</pre></td><td>{self._shot(test['nodeid'])}</td></tr>")
            out.append("</table>")

        for file, area in AREAS.items():
            tests = [test for test in self.tests if test["file"] == file]
            if not tests:
                continue
            out.append(f"<h2>{html.escape(area)}</h2>")
            if file == "test_sweep.py":
                out.append(self._sweep_table(tests))
                continue
            out.append("<table><tr><th>Test</th><th>Result</th><th>Time</th><th>The panel</th></tr>")
            for test in tests:
                out.append(f"<tr><td>{html.escape(test['name'])}</td><td class='badge {test['outcome']}'>{test['outcome']}</td>"
                           f"<td>{test['duration']:.1f} s</td><td>{self._shot(test['nodeid'])}</td></tr>")
            out.append("</table>")

        if self.dialogs:
            out.append("<h2>Game dialogs the run closed</h2><ul class='list'>")
            out += [f"<li>{html.escape(said)}</li>" for said in self.dialogs]
            out.append("</ul>")

        if self.snapshots:
            out.append("<h2>The panel at the end of each test</h2><div class='gallery'>")
            for nodeid, path in self.snapshots.items():
                out.append(f"<figure><a href='{html.escape(path)}'><img src='{html.escape(path)}' alt='' loading='lazy'></a>"
                           f"<figcaption>{html.escape(nodeid.split('::', 1)[-1])}</figcaption></figure>")
            out.append("</div>")
        out.append("</main></body></html>")
        return "\n".join(out)

    def _sweep_table(self, tests):
        out = ["<table><tr><th>Mod</th><th>Result</th><th>Changed</th><th>Skipped</th><th>Failed</th><th>Known</th><th>The panel</th></tr>"]
        for test in tests:
            mod = test["name"].removeprefix("test_sweep[").removesuffix("]")
            sweep = self.sweeps.get(mod, {"changed": 0, "skipped": [], "failed": [], "known": []})
            details = ""
            for kind in ("failed", "known", "skipped"):
                if sweep[kind]:
                    items = "".join(f"<li>{_linked(line)}</li>" for line in sweep[kind])
                    details += f"<details><summary>{len(sweep[kind])} {kind}</summary><ul class='list'>{items}</ul></details>"
            out.append(f"<tr><td>{html.escape(mod)}{details}</td><td class='badge {test['outcome']}'>{test['outcome']}</td><td>{sweep['changed']}</td>"
                       f"<td>{len(sweep['skipped'])}</td><td class='{'failed' if sweep['failed'] else ''}'>{len(sweep['failed'])}</td>"
                       f"<td class='{'known' if sweep['known'] else ''}'>{len(sweep['known'])}</td><td>{self._shot(test['nodeid'])}</td></tr>")
        out.append("</table>")
        return "\n".join(out)


def _linked(text):
    """A line of text as HTML, with its web addresses as links, as a known failure names its issue."""
    return re.sub(r"https?://[^\s)<]+", lambda match: f"<a href='{match.group(0)}'>{match.group(0)}</a>", html.escape(text))


REPORT = Report()
