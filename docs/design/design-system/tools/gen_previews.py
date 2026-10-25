"""Writes components/<Name>/preview.html of every component but Cover, which is written by hand.

The previews are generated: change them here (and the classes in components/bundle.css), not in the HTML.
Usage: python tools/gen_previews.py [project dir]
"""
import os
import re
import sys
from pathlib import Path

ROOT = sys.argv[1] if len(sys.argv) > 1 else str(Path(__file__).resolve().parent.parent / 'project')

# ---- drawn glyphs (ImGui draws these with ImDrawList; no icon font) ----
ARROW_R = '<svg viewBox="0 0 14 14"><path d="M4 2 L11 7 L4 12 Z"/></svg>'
ARROW_D = '<svg viewBox="0 0 14 14"><path d="M2 4 L12 4 L7 11 Z"/></svg>'
TRI_L = '<svg class="tri" viewBox="0 0 14 14"><path d="M10 2 L3 7 L10 12 Z"/></svg>'
TRI_R = '<svg class="tri" viewBox="0 0 14 14"><path d="M4 2 L11 7 L4 12 Z"/></svg>'
X = '<svg class="mc-ico" viewBox="0 0 24 24"><path d="M6 6 L18 18 M18 6 L6 18"/></svg>'
MIN = '<svg class="mc-ico" viewBox="0 0 24 24"><path d="M6 17 H18"/></svg>'
RESET = '<svg class="mc-ico" viewBox="0 0 24 24"><path d="M4 5 V10 H9"/><path d="M5.2 15 A7.5 7.5 0 1 0 6.6 7 L4 10"/></svg>'
CHECK = '<svg class="mc-ico check" viewBox="0 0 24 24"><path d="M4 12.5 L9.5 18 L20 6.5"/></svg>'
MINUS = '<svg class="mc-ico" viewBox="0 0 24 24"><path d="M6 12 H18"/></svg>'
PLUS = '<svg class="mc-ico" viewBox="0 0 24 24"><path d="M6 12 H18 M12 6 V18"/></svg>'
WARN = '<svg class="mc-warn" viewBox="0 0 20 18"><path class="tri" d="M10 1 L19 17 H1 Z"/><rect class="bang" x="9" y="6" width="2" height="6"/><rect class="bang" x="9" y="13.4" width="2" height="2"/></svg>'
DOT = '<span class="mc-dot"></span>'

# when a pending change takes effect (a config spec's applies), drawn in amber in place of the pending dot: live, save
# and restart. An unknown one keeps the dot
LIVE = ('<svg class="mc-applies" viewBox="0 0 16 16"><circle class="fill" cx="8" cy="8" r="2.2"/>'
        '<path d="M4.6 3.6 A6 6 0 0 0 4.6 12.4 M11.4 3.6 A6 6 0 0 1 11.4 12.4"/></svg>')
SAVE = ('<svg class="mc-applies" viewBox="0 0 16 16"><path d="M2 2.5 Q2 2 2.5 2 H11.5 L14 4.5 V13.5 Q14 14 13.5 14 H2.5 Q2 14 2 13.5 Z"/>'
        '<rect class="fill" x="5" y="2" width="5" height="3.5"/><rect class="fill" x="4.5" y="9" width="7" height="5"/></svg>')
RESTART = '<svg class="mc-applies" viewBox="0 0 16 16"><path d="M5 3.8 A5.8 5.8 0 1 0 11 3.8"/><path d="M8 1.2 V8"/></svg>'

def ptag(mark, word):
    """The help line's tag of when a pending change takes effect: its mark and its word, all in amber."""
    return f'<span class="mc-tag is-pending t-detail">{mark}{word}</span>'

def helpline(name, text, long=False, error=False, tag=''):
    """The left of the footer while the ray is on a setting: "Name: help", after the tag of a pending change. Long
    help drops to t-detail."""
    cls = 'help ' + ('t-detail is-long' if long else 't-body') + (' is-error' if error else '')
    mark = WARN if error else ''
    return f'<div class="{cls}">{tag}{mark}<span class="name">{name}:</span> {text}</div>'

def pagehelp(text):
    return f'<div class="mc-pagehelp t-body">{text}</div>'

def reset_btn(show=True):
    return f'<span class="mc-btn is-icon{"" if show else " is-slot"}" title="Reset">{RESET}</span>'

def slider(value, pct, state='', vstate='', show_reset=False):
    return (f'<div class="main"><span class="mc-track {state}"><i class="fill" style="width:{pct*2.6:.0f}px"></i>'
            f'<i class="grab" style="left:{10+pct*2.6:.0f}px"></i></span>'
            f'<span class="mc-value {vstate}">{value}</span></div>{reset_btn(show_reset)}')

def toggle(on=True, pending=False, show_reset=False):
    cls = 'mc-toggle' + (' is-pending' if pending else '')
    off_cls = '' if on else ' class="is-off"'
    on_cls = ' class="is-on"' if on else ''
    return f'<div class="main"><span class="{cls}"><span{off_cls}>Off</span><span{on_cls}>On</span></span></div>{reset_btn(show_reset)}'

def combo(label, open_=False, show_reset=False, pending=False):
    cls = 'mc-combo' + (' is-open' if open_ else '') + (' is-pending' if pending else '')
    return f'<div class="main"><span class="{cls}"><span>{label}</span>{ARROW_D}</span></div>{reset_btn(show_reset)}'

def num_stepper(value, pending=False, show_reset=False):
    return (f'<div class="main"><span class="mc-stepper"><span class="mc-btn is-icon">{MINUS}</span>'
            f'<span class="mc-value{" is-pending" if pending else ""}" style="justify-content:center">{value}</span>'
            f'<span class="mc-btn is-icon">{PLUS}</span></span></div>{reset_btn(show_reset)}')

def choice_stepper(label, pending=False, show_reset=False):
    return (f'<div class="main"><span class="mc-stepper"><span class="mc-btn is-icon">{TRI_L}</span>'
            f'<span class="label{" is-pending" if pending else ""}">{label}</span>'
            f'<span class="mc-btn is-icon">{TRI_R}</span></span></div>{reset_btn(show_reset)}')

MARK_RE = re.compile(r'<span class="mc-tag[^"]*"[^>]*>.*?</span>|<svg\b.*?</svg>', re.S)

def row(name, ctl, state='', gutter='', mark=None):
    """A row: its name, set in from the row's left inside its frame, and its marks after it, in this order: the pending
    mark, then the warning or the invalid mark, or a button's result. A mark in the name's text (a tag or a drawn mark)
    is moved there; `gutter` is a state mark (the pending mark, the invalid mark, a result)."""
    found = MARK_RE.findall(name)
    text = MARK_RE.sub('', name)
    tags = [m for m in found if m.startswith('<span')]
    drawn = [m for m in found if m.startswith('<svg')]
    marks = ''.join(tags) + gutter + ''.join(drawn)
    mk = f'<span class="mc-mark" style="left:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return (f'<div class="mc-row {state}"><span class="name t-body"><span class="txt">{text}</span>{marks}</span>'
            f'<div class="ctl t-body">{ctl}</div>{mk}</div>')

def setin(depth):
    """A section in a section is set in with its rows by IndentSpacing (space-8) a level, up to the third."""
    return f' mc-in-{min(depth, 3)}' if depth else ''

def section(title, depth=0):
    return f'<div class="mc-section t-body{setin(depth)}">{title}</div>'

def collapse(title, open_=False, hover=False, depth=0, invalid=0, unsaved=0, mark=None):
    """A section that folds: its heading is a row that is pressed, with a rule from its label as a heading has.
    Folded, it says what it hides at the end of the rule: the invalid values first, by the warning mark, then the
    unsaved changes, by the pending dot, each with its count."""
    marks = ''
    if not open_ and (invalid or unsaved):
        marks = '<span class="marks t-detail">'
        if invalid:
            marks += f'{WARN}<span class="mc-red">{invalid} invalid</span>'
        if unsaved:
            marks += f'{DOT}<span class="mc-amber">{unsaved} unsaved</span>'
        marks += '</span>'
    mk = f'<span class="mc-mark" style="left:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return (f'<div class="mc-collapse t-body{" is-hover" if hover else ""}{setin(depth)}">{ARROW_D if open_ else ARROW_R}'
            f'<span>{title}</span><span class="line"></span>{marks}{mk}</div>')

def sub(heading, rows=''):
    """A section in a section: its heading and its rows set in a level, ending where the other rows do. While it is
    open and has rows, a rule line runs from under its heading, space-1 in, down to the bottom of its last
    row, and turns there toward the rows; its rows' marks move a little right to clear it."""
    inner = f'<div class="mc-sub">{rows}</div>' if rows else ''
    return f'<div class="mc-in">{heading}{inner}</div>'

def tree_row(name, kind='mod', state='', arrow=None, mark=None, cat=False, depth=None):
    """A row of the tree. Each level is set in by IndentSpacing (space-8), up to the third: a mod's pages are a level
    in, a page's pages a level further, and in a grouped tree the category is the top. Every row keeps the arrow's
    column, so the names of a level line up: a mod always has its arrow, and a page has one only when it has pages
    of its own."""
    if depth is None:
        depth = (1 if kind == 'page' else 0) + (1 if cat else 0)
    a = {'r': ARROW_R, 'd': ARROW_D, None: ''}[arrow]
    cls = 'mc-tree-row t-body' + (f' d{min(depth, 3)}' if depth else '') + (f' {state}' if state else '')
    mk = f'<span class="mc-mark" style="right:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return f'<div class="{cls}"><span class="arrow">{a}</span><span class="name">{name}</span>{mk}</div>'

def cat_row(name, count, open_=False, state='', mark=None):
    """A category: opened and closed by pressing it, never selected. Its rule and count set it apart from a mod."""
    mk = f'<span class="mc-mark" style="right:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return (f'<div class="mc-cat t-body{" " + state if state else ""}"><span class="arrow">{ARROW_D if open_ else ARROW_R}</span>'
            f'<span class="name">{name}</span><span class="line"></span><span class="count t-detail">{count}</span>{mk}</div>')

TREE_H = 525      # the Full panel's tree, as laid out by the panel (measured)
TREE_ROWS_H = TREE_H - 8 - 40 - 4 - 8   # minus the pane padding, the Mods header and its gap
ROW_H, ROW_GAP = 48, 4

def nav(rows, root_state='', count=5, offset=0, height=None):
    """The tree: the mods, alphabetical. The menu's front page has no row: the first breadcrumb is its entry.
    The header stays; the rows scroll. The bar's room is always kept, and the bar shows only while the rows overflow."""
    n = rows.count('class="mc-tree-row') + rows.count('class="mc-cat')
    total = n * ROW_H + (n - 1) * ROW_GAP
    bar = ''
    if total > TREE_ROWS_H + 1:
        bar = (f'<div class="mc-scrollbar"><i style="top:{offset / total * 100:.1f}%;'
               f'height:{TREE_ROWS_H / total * 100:.1f}%"></i></div>')
    st = f' style="height:{height}px"' if height else ''
    lst = f' style="margin-top:-{offset}px"' if offset else ''
    return (f'<div class="mc-nav"{st}><div class="mc-nav-head t-detail"><span>Mods</span><span>{count}</span></div>'
            f'<div class="mc-nav-rows"><div class="list"{lst}>{rows}</div></div>{bar}</div>')

def flat_mods(open_mod=None, pages=(), selected=None, hover=None):
    """One install of 11 mods, the same in every Full panel mock: too many for the tree, so it scrolls."""
    out = []
    for m in ('Baka Fullscreen Pip-Boy', 'Better Locational Damage', 'Companion Command', 'FallUI - HUD', 'FRIK',
              'Immersive Flashlight VR', 'Mods Config VR', 'Sim Settlements 2', 'Virtual Holsters', 'Virtual Reloads',
              'Weather Overhaul'):
        st = 'is-disabled' if m == 'Sim Settlements 2' else ''
        if m == selected: st = 'is-selected'
        if hover and m == hover[0]: st, mk = 'is-hover', hover[1]
        else: mk = None
        out.append(tree_row(m, arrow='d' if m == open_mod else 'r', state=st, mark=mk))
        if m == open_mod:
            for pg in pages:
                name, depth, arrow = (pg, 1, None) if isinstance(pg, str) else pg
                pst = 'is-selected' if name == selected else ''
                if hover and name == hover[0]: pst, pmk = 'is-hover', hover[1]
                else: pmk = None
                out.append(tree_row(name, kind='page', state=pst, mark=pmk, depth=depth, arrow=arrow))
    return ''.join(out)

def page(marker, title, body):
    return f'''{marker}
<!doctype html>
<html lang="en">
<head><meta charset="utf-8"><title>{title} — preview</title></head>
<body>
{body}
</body>
</html>
'''

def labeled(rows, width=772):
    out = [f'<div class="mc-world"><div class="mc-sheet" style="display:grid;grid-template-columns:120px {width-32}px;column-gap:16px;align-items:start;width:{width+136}px;padding-bottom:8px">']
    for label, html in rows:
        out.append(f'<span class="t-detail mc-muted" style="height:56px;display:flex;align-items:center">{label}</span><div>{html}</div>')
    out.append('</div></div>')
    return '\n'.join(out)

def write(comp, html):
    d = os.path.join(ROOT, 'components', comp)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, 'preview.html'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(html)

# ---------------- SettingRow ----------------
write('SettingRow', page('<!-- @dsCard group="Settings" height=780 width=940 subtitle="Idle, hover, focused, active, pending, and when a pending change takes effect, invalid, disabled" -->', 'Setting row', labeled([
    ('Idle', row('Brightness', slider('1.00', 50))),
    ('Hover', row('Brightness', slider('1.00', 50), 'is-hover', mark=(212, 20))),
    ('Focused', row('Brightness', slider('1.00', 50), 'is-focus')),
    ('Active (drag)', row('Brightness', slider('1.10', 60, 'is-active'), 'is-active')),
    ('Pending', row('Brightness', slider('1.25', 75, 'is-pending', 'is-pending', True), gutter=DOT)),
    ('Pending, live', row('Brightness', slider('1.25', 75, 'is-pending', 'is-pending', True), gutter=LIVE)),
    ('Pending, after Save', row('Holster size', slider('0.80', 40, 'is-pending', 'is-pending', True), gutter=SAVE)),
    ('Pending, restart', row('Shadow resolution', choice_stepper('4096', True, True), gutter=RESTART)),
    ('Invalid', row('Brightness', slider('2.00', 100, 'is-invalid', 'is-invalid', True), 'is-invalid', gutter=WARN)),
    ('Disabled', row('Beam shadows', toggle(False), 'is-disabled')),
])))

# ---------------- Toggle ----------------
write('Toggle', page('<!-- @dsCard group="Settings" height=420 width=940 subtitle="Off, on, pending, hover, disabled" -->', 'Toggle', labeled([
    ('Off', row('Drain battery', toggle(False))),
    ('On', row('Enabled', toggle(True))),
    ('Pending', row('Enabled', toggle(True, pending=True, show_reset=True), gutter=DOT)),
    ('Hover (whole row)', row('Enabled', toggle(True), 'is-hover', mark=(96, 22))),
    ('Disabled', row('Beam shadows', toggle(False), 'is-disabled')),
])))

# ---------------- Slider ----------------
write('Slider', page('<!-- @dsCard group="Settings" height=360 width=940 subtitle="Track, grab, value box and Reset; float and int" -->', 'Slider', labeled([
    ('Float', row('Brightness', slider('1.00', 50))),
    ('Dragging', row('Brightness', slider('1.10', 60, 'is-active'), 'is-active')),
    ('Int with unit', row('Beam angle', slider('40°', 30, 'is-pending', 'is-pending', True), gutter=DOT)),
    ('Out of range', row('Beam angle', slider('140°', 100, 'is-invalid', 'is-invalid', True), 'is-invalid', gutter=WARN)),
])))

# ---------------- Stepper ----------------
write('Stepper', page('<!-- @dsCard group="Settings" height=300 width=940 subtitle="Numeric stepper and previous/next choice" -->', 'Stepper', labeled([
    ('Numeric', row('Hours per battery', num_stepper('8'))),
    ('Numeric, pending', row('Hours per battery', num_stepper('12', pending=True, show_reset=True), gutter=DOT)),
    ('Choice', row('Ammo display', choice_stepper('Ballistic'))),
    ('Choice, pending', row('Ammo display', choice_stepper('Energy', pending=True, show_reset=True), gutter=DOT)),
])))

# ---------------- Dropdown ----------------
popup = (f'<div class="mc-popup t-body" style="position:absolute;right:72px;top:68px;width:392px;z-index:2">'
         f'<div class="mc-option is-current">{CHECK}<span>Spot</span></div>'
         f'<div class="mc-option is-hover"><span class="check"></span><span>Flood</span><span class="mc-mark" style="position:relative;margin-left:120px"></span></div>'
         f'<div class="mc-option"><span class="check"></span><span>Lantern</span></div></div>')
dd_body = f'''<div class="mc-world mc-stack" style="gap:0"><div class="mc-sheet" style="width:772px;position:relative;border-radius:8px 8px 0 0">
{row('Beam mode', combo('Spot', open_=True), 'is-active')}
{row('Beam angle', slider('40°', 30))}
{row('Hours per battery', num_stepper('8'))}
{row('Drain battery', toggle(False))}
{row('Beam shadows', toggle(True))}
{popup}
</div>
<div class="mc-sheet" style="width:1120px;padding:0;border-radius:0 8px 8px 8px"><div class="mc-footer">{helpline('Flood', 'A wide, even beam with a short reach, for lighting a room.')}<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span></div></div>
</div>'''
write('Dropdown', page('<!-- @dsCard group="Settings" height=460 width=1170 subtitle="Closed and open; help for the pointed option shows in the footer" -->', 'Dropdown', dd_body))

# ---------------- DescriptionArea: the help line, left of the footer ----------------
def fbar(left, dirty=True):
    b = ('<span class="mc-btn t-body">Discard</span><span class="mc-btn t-body is-primary">Save</span>' if dirty
         else '<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span>')
    return f'<div class="mc-footer">{left}{b}</div>'
da_body = f'''<div class="mc-world"><div class="mc-sheet mc-stack" style="width:1120px">
<span class="t-detail mc-muted">The ray is on a setting: its help, "Name: help", two lines at t-body</span>
{fbar(helpline('Brightness', "How bright the beam is. 1.00 is the game's own flashlight; above 1.25 the beam starts to wash out close surfaces. Default 1.00."))}
<span class="t-detail mc-muted">A pending change of a setting whose mod says when it takes effect: its tag first, in amber</span>
{fbar(helpline('Shadow size', 'How sharp the beam&#39;s shadows are. Larger costs frame time. (Default: 2048)', tag=ptag(RESTART, 'Restart')))}
<span class="t-detail mc-muted">Help that needs a third line drops to t-detail; past three lines it ends in an ellipsis</span>
{fbar(helpline('Hide HUD in Pip-Boy', "Hides the compass, health, action points and the ammo counter while the Pip-Boy is raised, and brings them back when it is lowered. Widgets that FallUI HUD does not own, such as quest markers from other mods, keep their own setting. Default off.", long=True))}
<span class="t-detail mc-muted">A validation error comes first</span>
{fbar(helpline('Brightness', "2.00 is out of range. Enter a value from 0.50 to 1.50, or Reset it to 1.00.", error=True))}
<span class="t-detail mc-muted">The ray is off the settings: the session status</span>
<div class="mc-footer"><div class="status t-body"><span class="mc-dot"></span><span class="mc-amber">1 unsaved change</span></div><span class="mc-btn t-body">Discard</span><span class="mc-btn t-body is-primary">Save</span></div>
</div></div>'''
write('DescriptionArea', page('<!-- @dsCard group="Settings" height=610 width=1170 subtitle="Left of the footer: an error, else help for the pointed setting, else the status" -->', 'Help line', da_body))

# ---------------- SectionHeader ----------------
sh_states = ''.join(f'<span class="t-detail mc-muted" style="height:56px;display:flex;align-items:center">{label}</span><div>{html}</div>' for label, html in [
    ('Heading', section('Holsters')),
    ('In a section', section('Left hip', 1)),
    ('Folds, folded', collapse('Advanced', depth=1)),
    ('Hides changes', collapse('Advanced', depth=1, invalid=1, unsaved=2)),
    ('Under the ray', collapse('Advanced', depth=1, unsaved=2, hover=True, mark=(150, 22))),
    ('Open', collapse('Advanced', open_=True, depth=1)),
])
sh_states = f'<div class="mc-sheet" style="display:grid;grid-template-columns:120px 740px;column-gap:16px;align-items:start;width:908px">{sh_states}</div>'
sh_page = ''.join([
    section('Holsters'),
    row('Enable holsters', toggle(True)),
    row('Holster size', slider('1.00', 50)),
    sub(section('Left hip'), ''.join([
        row('Forward', slider('4.0', 40)),
        row('Down', slider('12.0', 60, 'is-pending', 'is-pending', True), gutter=DOT),
        sub(collapse('Advanced', invalid=1, unsaved=2, hover=True, mark=(150, 22))),
    ])),
    section('Weapons'),
    row('Draw with the grip', toggle(True)),
    sub(collapse('Advanced', open_=True), ''.join([
        row('Draw angle', slider('35°', 35)),
        row('Snap distance', slider('2.50', 95, '', 'is-invalid', True), 'is-invalid', gutter=WARN),
    ])),
])
sh_body = f'''<div class="mc-world" style="display:flex;gap:24px;align-items:flex-start">
<div class="mc-stack"><span class="t-detail mc-caption">The headings: a section in a section is set in a level, a section that folds is a row</span>{sh_states}</div>
<div class="mc-stack"><span class="t-detail mc-caption">A page: a section in a section is set in, with a line down to its last row, and its rows end where the others do</span>
<div class="mc-sheet" style="width:772px">{sh_page}</div>
<div class="mc-sheet" style="width:772px;padding:0">{fbar(helpline('Advanced', 'Fine-tuning of the left hip holster. Folded: 1 invalid value, 2 unsaved changes.'))}</div></div>
</div>'''
write('SectionHeader', page('<!-- @dsCard group="Settings" height=1010 width=1880 subtitle="Headings; a section in a section set in with a line to its last row; a section that folds and says what it hides" -->', 'Section header', sh_body))


# ---------------- Button ----------------
def btn(label, kind='', state=''):
    return f'<span class="mc-btn t-body {kind} {state}">{label}</span>'
b_rows = []
for kind, label in [('is-primary', 'Save'), ('', 'Discard'), ('is-quiet', 'Cancel')]:
    b_rows.append('<div class="mc-line">' + ''.join(btn(label, kind, s) for s in ['', 'is-hover', 'is-active', 'is-focus', 'is-disabled']) + '</div>')
b_rows.append('<div class="mc-line">' + ''.join(f'<span class="mc-btn is-icon {s}">{g}</span>' for g, s in [(RESET, ''), (RESET, 'is-hover'), (X, ''), (MIN, ''), ('<span class="t-body">?</span>', '')]) + '</div>')
cols = '<div class="mc-line t-detail mc-muted" style="gap:12px"><span style="width:112px">Idle</span><span style="width:112px">Hover</span><span style="width:112px">Pressed</span><span style="width:112px">Focused</span><span style="width:112px">Disabled</span></div>'
bt_body = f'<div class="mc-world"><div class="mc-sheet mc-stack" style="width:680px;gap:12px">{cols}{"".join(b_rows)}</div></div>'
write('Button', page('<!-- @dsCard group="Actions" height=380 width=740 subtitle="Primary, secondary, quiet, icon; five states" -->', 'Button', bt_body))

# ---------------- SaveBar ----------------
def footer(status, buttons):
    return f'<div class="mc-footer t-body"><div class="status">{status}</div>{buttons}</div>'
sb_body = f'''<div class="mc-world"><div class="mc-sheet mc-stack" style="width:1120px;gap:12px">
<span class="t-detail mc-muted">Clean</span>
{footer('<span class="mc-muted">No unsaved changes</span>', btn('Discard', '', 'is-disabled') + btn('Save', 'is-primary', 'is-disabled'))}
<span class="t-detail mc-muted">Pending</span>
{footer(DOT + '<span class="mc-amber">1 unsaved change</span>', btn('Discard') + btn('Save', 'is-primary'))}
<span class="t-detail mc-muted">Blocked by an invalid value</span>
{footer(WARN + '<span class="mc-red">2 invalid values</span>', btn('Discard') + btn('Save', 'is-primary', 'is-disabled'))}
<span class="t-detail mc-muted">Not saved: a Save that failed</span>
{footer(WARN + '<span class="mc-red">Not saved: showcase.toml cannot be written.</span>', btn('Discard') + btn('Save', 'is-primary'))}
</div></div>'''
write('SaveBar', page('<!-- @dsCard group="Actions" height=560 width=1180 subtitle="Clean, pending, blocked, not saved: every mod, MCM too" -->', 'Save bar', sb_body))

# ---------------- ConfirmDialog ----------------
cd_body = f'''<div class="mc-world"><div class="mc-sheet mc-scrim" style="width:820px;height:400px;padding:0">
<div class="mc-dialog">
<div class="t-title">Unsaved changes</div>
<div class="t-body">Immersive Flashlight VR has 1 unsaved change. Save it before you open FRIK?</div>
<div class="actions">{btn('Cancel', 'is-quiet')}{btn('Discard')}{btn('Save', 'is-primary')}</div>
</div></div></div>'''
write('ConfirmDialog', page('<!-- @dsCard group="Actions" height=450 width=870 subtitle="Save / Discard / Cancel, centered on the panel" -->', 'Confirmation dialog', cd_body))

# ---------------- ModTree ----------------
home_tree = ''.join([
    tree_row('Companion Command', arrow='r'),
    tree_row('FallUI - HUD', arrow='r'),
    tree_row('FRIK', arrow='r', state='is-hover', mark=(120, 20)),
    tree_row('Immersive Flashlight VR', arrow='r'),
    tree_row('Mods Config VR', arrow='r'),
    tree_row('Sim Settlements 2', arrow='r', state='is-disabled'),
    tree_row('Virtual Holsters', arrow='r'),
])
page_tree = flat_mods('Immersive Flashlight VR', ('Beam', 'Hand mount'), selected='Beam')
# a config spec's page with pages of its own: open while one of them is selected, a level further in; another that has
# pages is closed, its arrow pointing right
NESTED_PAGES = (('Beam', 1, 'r'), ('Hand mount', 1, 'd'), ('Primary hand', 2, None), ('Off hand', 2, None), ('Battery', 1, None))
nested_tree = flat_mods('Immersive Flashlight VR', NESTED_PAGES, selected='Off hand')
grouped_tree = ''.join([
    cat_row('Audio', 2), cat_row('Characters', 3), cat_row('Combat', 6, state='is-hover', mark=(150, 20)),
    cat_row('Gameplay', 5), cat_row('Interface', 3), cat_row('Settlements', 4), cat_row('Utilities', 2),
    cat_row('Visuals', 3), cat_row('VR', 4), cat_row('No category', 2),
])
mt_body = f'''<div class="mc-world" style="display:flex;gap:24px;align-items:flex-start">
<div class="mc-stack"><span class="t-detail mc-caption">34 mods, grouped: the list scrolls</span>
<div class="mc-sheet" style="width:336px;padding:0">{nav(grouped_tree, '', 34, height=TREE_H)}</div></div>
<div class="mc-stack"><span class="t-detail mc-caption">7 mods: the list fits, no bar, its room kept</span>
<div class="mc-sheet" style="width:336px;padding:0">{nav(home_tree, '', 7, height=TREE_H)}</div></div>
<div class="mc-stack"><span class="t-detail mc-caption">11 mods, a page of a mod selected</span>
<div class="mc-sheet" style="width:336px;padding:0">{nav(page_tree, '', 11, height=TREE_H)}</div></div>
<div class="mc-stack"><span class="t-detail mc-caption">A page of a page: its page open, a level in</span>
<div class="mc-sheet" style="width:336px;padding:0">{nav(nested_tree, '', 11, offset=4 * (ROW_H + ROW_GAP), height=TREE_H)}</div></div>
</div>'''
write('ModTree', page('<!-- @dsCard group="Navigation" height=660 width=1470 subtitle="Flat under 30 mods, grouped by category from 30; the pages of a page a level in; a bar while the list scrolls" -->', 'Mod tree', mt_body))

# ---------------- FullPanel ----------------
fp_nav = flat_mods('Immersive Flashlight VR', ('Beam', 'Hand mount'), selected='Immersive Flashlight VR')
fp_rows = ''.join([
    pagehelp('A flashlight you hold, mount on your hand, and aim.'),
    section('General'),
    row('Enabled', toggle(True)),
    row('Brightness', slider('1.25', 75, 'is-pending', 'is-pending', True), 'is-hover', gutter=DOT, mark=(560, 22)),
    row('Beam mode', combo('Spot')),
    row('Beam angle', slider('40°', 30)),
    section('Battery'),
    row('Drain battery', toggle(False)),
    row('Hours per battery', num_stepper('8')),
    collapse('Advanced'),
])
fp_body = f'''<div class="mc-world">
<div class="mc-panel">
<div class="mc-titlebar"><div class="mc-crumbs t-title"><span class="mc-crumb">MODS CONFIG VR</span><span class="mc-crumb-sep">»</span><span class="mc-crumb is-mod is-current">Immersive Flashlight VR</span></div>
<span class="mc-btn is-icon t-body">?</span><span class="mc-btn is-icon">{MIN}</span><span class="mc-btn is-icon">{X}</span></div>
<div class="mc-body">
{nav(fp_nav, '', 11)}
<div class="mc-content">
<div class="mc-scroll"><div class="rows">{fp_rows}</div><div class="mc-scrollbar"><i style="top:0;height:100%"></i></div></div>
</div>
</div>
<div class="mc-footer">{helpline('Brightness', "How bright the beam is. 1.00 is the game's own flashlight. Default 1.00.")}{btn('Discard')}{btn('Save', 'is-primary')}</div>
</div>
</div>'''
write('FullPanel', page('<!-- @dsCard group="Panels" height=768 width=1200 subtitle="A mod front page. 1152 x 720 layout px: 24 x 15 vrui units at scale 6" -->', 'Full panel', fp_body))

# ---------------- FocusedPanel ----------------
fo_body = f'''<div class="mc-world" style="display:flex;gap:24px;align-items:flex-start">
<div class="mc-focused">
<div class="fhead"><span class="grow t-body">Immersive Flashlight VR</span><span class="mc-btn t-body" style="min-width:88px">Full</span><span class="mc-btn is-icon">{MIN}</span></div>
<div class="mc-desc t-body"><span class="name">Brightness:</span> How bright the beam is. 1.00 is the game's own flashlight. Default 1.00.</div>
<div class="item"><div class="t-body" style="display:flex;align-items:center;gap:8px"><span>Brightness</span>{DOT}</div>
<div class="ctl t-body"><span class="mc-track is-pending"><i class="fill" style="width:170px"></i><i class="grab" style="left:180px"></i></span><span class="mc-value is-pending">1.25</span><span class="mc-btn is-icon">{RESET}</span></div></div>
<div class="item"><div class="t-body">Beam mode</div>
<div class="ctl t-body"><span class="mc-stepper" style="flex:1"><span class="mc-btn is-icon">{TRI_L}</span><span class="label" style="flex:1;width:auto">Spot</span><span class="mc-btn is-icon">{TRI_R}</span></span><span class="mc-btn is-icon is-slot">{RESET}</span></div></div>
<div class="mc-footer t-body"><div class="status">{DOT}<span class="mc-amber">1 unsaved</span></div>{btn('Discard')}{btn('Save', 'is-primary')}</div>
</div>
</div>'''
write('FocusedPanel', page('<!-- @dsCard group="Panels" height=480 width=624 subtitle="576 x 432 layout px on the offhand; a choice becomes a stepper" -->', 'Focused panel', fo_body))

# ---------------- WristButtons ----------------
def wb(label):
    return f'<div class="mc-wristbtn"><span class="top t-detail">CLICK</span><span class="t-body">{label}</span></div>'
wr_body = f'''<div class="mc-world mc-stack" style="gap:16px">
<span class="t-detail mc-caption">Menu open</span>
<div class="mc-wrist">{wb('MINIMIZE')}{wb('CLOSE')}</div>
<span class="t-detail mc-caption">Minimized</span>
<div class="mc-wrist">{wb('RESUME')}{wb('CLOSE')}</div>
</div>'''
write('WristButtons', page('<!-- @dsCard group="Panels" height=360 width=520 subtitle="vrui UIButtonPanel, 3 x 2.2 units, F4VR_BUTTON_STYLE" -->', 'Wrist buttons', wr_body))
print('ok')

# ---------------- FullPanelPage: a page of a mod selected, not its front page ----------------
fpp_nav = flat_mods('Immersive Flashlight VR', ('Beam', 'Hand mount'), selected='Beam', hover=('Hand mount', (64, 20)))
fpp_rows = ''.join([
    pagehelp('The shape and reach of the light.'),
    section('Shape'),
    row('Beam mode', combo('Flood')),
    row('Beam angle', slider('60°', 56)),
    row('Reach', slider('1200', 40)),
    row('Cone softness', slider('0.30', 30)),
    section('Light'),
    row('Warm tint', toggle(True)),
    row('Flicker when the battery is low', toggle(False)),
])
fpp_body = f'''<div class="mc-world">
<div class="mc-panel">
<div class="mc-titlebar"><div class="mc-crumbs t-title"><span class="mc-crumb">MODS CONFIG VR</span><span class="mc-crumb-sep">»</span><span class="mc-crumb is-mod">Immersive Flashlight VR</span><span class="mc-crumb-sep">»</span><span class="mc-crumb is-current">Beam</span></div>
<span class="mc-btn is-icon t-body">?</span><span class="mc-btn is-icon">{MIN}</span><span class="mc-btn is-icon">{X}</span></div>
<div class="mc-body">
{nav(fpp_nav, '', 11)}
<div class="mc-content">
<div class="mc-scroll"><div class="rows">{fpp_rows}</div><div class="mc-scrollbar"><i style="top:0;height:92%"></i></div></div>
</div>
</div>
<div class="mc-footer t-body"><div class="status"><span class="mc-muted">No unsaved changes</span></div>{btn('Discard', '', 'is-disabled')}{btn('Save', 'is-primary', 'is-disabled')}</div>
</div>
</div>'''
write('FullPanelPage', page('<!-- @dsCard group="Panels" height=768 width=1200 subtitle="A page selected: the crumbs in the title bar, the page row carries the tip" -->', 'Full panel, page selected', fpp_body))
print('ok page')

# ---------------- Breadcrumbs: where the player is, in the title bar ----------------
def crumbs(parts, buttons=True, alone=True):
    """parts: (text, kind) from the first: kind is '' (pressable), 'mod' (shortens), 'more' (the pages that do not fit),
    'current' (the last), 'selected' (the menu's front page), or 'hover'."""
    out = []
    for i, (text, kind) in enumerate(parts):
        if i:
            out.append('<span class="mc-crumb-sep">»</span>')
        cls = {'': '', 'mod': ' is-mod', 'more': ' is-more', 'current': ' is-current', 'selected': ' is-selected',
               'hover': ' is-hover', 'mod-hover': ' is-mod is-hover', 'mod-current': ' is-mod is-current'}[kind]
        out.append(f'<span class="mc-crumb{cls}">{text}</span>')
    btns = f'<span class="mc-btn is-icon t-body">?</span><span class="mc-btn is-icon">{MIN}</span><span class="mc-btn is-icon">{X}</span>' if buttons else ''
    width = ' style="width:calc(var(--panel-width) - 2 * var(--panel-padding))"' if alone else ''
    return f'<div class="mc-titlebar"{width}><div class="mc-crumbs t-title">{"".join(out)}</div>{btns}</div>'

R = ('MODS CONFIG VR', '')
bc_body = f'''<div class="mc-world"><div class="mc-sheet mc-stack" style="width:1184px">
<span class="t-detail mc-muted">The menu's front page: the first crumb alone, selected</span>
{crumbs([('MODS CONFIG VR', 'selected')])}
<span class="t-detail mc-muted">A page of a page: a crumb for every level, each before the last pressed to go there</span>
{crumbs([R, ('Immersive Flashlight VR', 'mod'), ('Hand mount', 'hover'), ('Off hand', 'current')])}
<span class="t-detail mc-muted">Too long: the pages between the mod and the page become one "…", which is not pressed</span>
{crumbs([R, ('Immersive Flashlight VR', 'mod'), ('…', 'more'), ('Fine adjustments for power armor gloves', 'current')])}
<span class="t-detail mc-muted">Still too long: then the mod's name shortens</span>
{crumbs([R, ('Sim Settlements 2 Chapter 3: City Plans and Settlement Management', 'mod'), ('…', 'more'), ('Upkeep of the workshop plots', 'current')])}
</div></div>'''
write('Breadcrumbs', page('<!-- @dsCard group="Navigation" height=520 width=1240 subtitle="A crumb for every level of pages; past the width the pages between become one ellipsis, then the mod shortens" -->', 'Breadcrumbs', bc_body))

# ---------------- FullPanelSubpage: a page of a page selected, with a section in a section ----------------
fps_nav = flat_mods('Immersive Flashlight VR', NESTED_PAGES, selected='Off hand', hover=('Hand mount', (64, 20)))
fps_rows = ''.join([
    pagehelp('Where the light sits when it is mounted on the off hand.'),
    section('Position'),
    row('Forward', slider('2.0', 45)),
    row('Up', slider('-1.5', 35)),
    row('Out from the wrist', slider('3.0', 55, 'is-pending', 'is-pending', True), gutter=DOT),
    sub(section('With power armor'), ''.join([
        row('Forward', slider('4.5', 60)),
        row('Up', slider('0.0', 50)),
    ])),
    sub(collapse('Advanced', unsaved=1)),
])
fps_body = f'''<div class="mc-world">
<div class="mc-panel">
{crumbs([R, ('Immersive Flashlight VR', 'mod'), ('Hand mount', ''), ('Off hand', 'current')], alone=False)}
<div class="mc-body">
{nav(fps_nav, '', 11, offset=4 * (ROW_H + ROW_GAP))}
<div class="mc-content">
<div class="mc-scroll"><div class="rows">{fps_rows}</div><div class="mc-scrollbar"><i style="top:0;height:86%"></i></div></div>
</div>
</div>
<div class="mc-footer">{helpline('Hand mount', 'Where the light sits when it is mounted on a hand.')}{btn('Discard')}{btn('Save', 'is-primary')}</div>
</div>
</div>'''
write('FullPanelSubpage', page('<!-- @dsCard group="Panels" height=768 width=1200 subtitle="A page of a page: its page open in the tree, a crumb for each level, a section in a section" -->', 'Full panel, page of a page', fps_body))


# ---------------- FullPanelHome: the menu's front page ----------------
def launch_row(name, state='', mark=None):
    return row(name, '<div class="main"><span class="mc-btn t-body" style="min-width:160px">Open</span></div>' + reset_btn(False), state, mark=mark)
home_nav = flat_mods()
home_rows = ''.join([
    pagehelp('Settings for the mods you have installed. Pick a mod on the left to see its settings.<br>Version 1.0.0'),
    section('Quick launch'),
    launch_row('FRIK', 'is-hover', mark=(520, 20)),
    launch_row('Virtual Holsters'),
    section('Notices'),
    pagehelp('Sim Settlements 2 is not available: its plugin SS2.esm is not loaded.'),
])
fph_body = f'''<div class="mc-world">
<div class="mc-panel">
<div class="mc-titlebar"><div class="mc-crumbs t-title"><span class="mc-crumb is-selected">MODS CONFIG VR</span></div>
<span class="mc-btn is-icon t-body">?</span><span class="mc-btn is-icon">{MIN}</span><span class="mc-btn is-icon">{X}</span></div>
<div class="mc-body">
{nav(home_nav, '', 11)}
<div class="mc-content">
<div class="mc-scroll"><div class="rows">{home_rows}</div></div>
</div>
</div>
<div class="mc-footer">{helpline('FRIK', "Opens FRIK's own configuration in the world: body height, arm length, and the weapon and Pip-Boy positions.")}<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span></div>
</div>
</div>'''
write('FullPanelHome', page('<!-- @dsCard group="Panels" height=768 width=1200 subtitle="The menu front page: what the menu opens on, with quick launch" -->', 'Full panel, menu front page', fph_body))
print('ok home')

# ---------------- FullPanelHelp: the Help page, over a page of a mod ----------------
def control(name, text):
    """A control of the menu on the Help page, as the help line has a row's help: "Name: text"."""
    return f'<div class="mc-control t-body"><span class="name">{name}:</span> {text}</div>'

def legend(mark, name, text):
    """A mark of when a change takes effect on the Help page: the mark in amber, then "Name: text"."""
    return f'<div class="mc-control t-body"><span class="name">{mark}{name}:</span> {text}</div>'

def legend(mark, name, text):
    """A mark of when a change takes effect on the Help page: the mark in amber, then "Name: text"."""
    return f'<div class="mc-control t-body"><span class="name">{mark}{name}:</span> {text}</div>'

def legend(mark, name, text):
    """A mark of when a change takes effect on the Help page: the mark in amber, then "Name: text"."""
    return f'<div class="mc-control t-body"><span class="name">{mark}{name}:</span> {text}</div>'
fhp_nav = flat_mods('Immersive Flashlight VR', ('Beam', 'Hand mount'), selected='Beam')
fhp_rows = ''.join([
    pagehelp('How the menu is used, with the buttons your config has.'),
    section('Controls'),
    control('Point and press', 'Point at the menu with either hand, and pull the trigger on what the ray is on.'),
    control('Scroll', 'Push the thumbstick up or down on the hand that points: the list the ray is on scrolls.'),
    control('Step a value', 'Push the thumbstick left or right on the hand that points: the slider or the choice the ray is on steps.'),
    control('Back', "Press the grip briefly on the hand that points: one step back, and from the menu's front page the menu closes."),
    control('Move the menu', 'Point at the menu and hold the grip on the hand that points. The menu follows the hand until you let go.'),
    control('Recenter', 'Long press the thumbstick on the off hand: the menu is put in front of you again, also after you moved it.'),
    control('Open and close the menu', 'Long press the thumbstick on the primary hand for 1.5 s, holding the thumbstick on the off hand.'),
    control('Minimize', 'Minimize, on the title bar or on your offhand wrist, hides the menu as you left it. Resume on the wrist shows it again.'),
    section('Saving'),
    control('Save', 'Writes the changes of the mod you are on, and the mod acts on them in its own way. A mod can say when: a changed row then has its mark, and the help line its tag.'),
    legend(LIVE, 'Live', 'As you change it. Discard takes it back.'),
    legend(SAVE, 'Save', 'After Save.'),
    legend(RESTART, 'Restart', 'After the game is restarted.'),
    legend(DOT, 'No mark', 'The mod does not say, as no MCM mod does. Save writes it all the same.'),
])
fhp_body = f'''<div class="mc-world">
<div class="mc-panel">
<div class="mc-titlebar"><div class="mc-crumbs t-title"><span class="mc-crumb">MODS CONFIG VR</span><span class="mc-crumb-sep">»</span><span class="mc-crumb is-current">Help</span></div>
<span class="mc-btn is-icon t-body is-hover">?</span><span class="mc-btn is-icon">{MIN}</span><span class="mc-btn is-icon">{X}</span></div>
<div class="mc-body">
{nav(fhp_nav, '', 11)}
<div class="mc-content">
<div class="mc-scroll"><div class="rows" style="margin-top:-330px">{fhp_rows}</div><div class="mc-scrollbar"><i style="top:38%;height:62%"></i></div></div>
</div>
</div>
<div class="mc-footer">{helpline('Help', 'What each control of the menu does, with the buttons your config has for it.')}{btn('Discard')}{btn('Save', 'is-primary')}</div>
</div>
</div>'''
write('FullPanelHelp', page('<!-- @dsCard group="Panels" height=768 width=1200 subtitle="The Help page over a page of a mod, scrolled to its end: each control with the buttons of the config, and what Save does with the marks of when a change takes effect" -->', 'Full panel, Help page', fhp_body))
print('ok help')

# ---------------- FullPanelCategories: Mods Config VR's own Categories page, with the tree grouped ----------------
cat_tree = ''.join([
    cat_row('Audio', 2), cat_row('Characters', 3), cat_row('Combat', 6), cat_row('Gameplay', 5), cat_row('Interface', 3, open_=True),
    tree_row('Baka Fullscreen Pip-Boy', arrow='r', cat=True),
    tree_row('FallUI - HUD', arrow='r', cat=True),
    tree_row('Mods Config VR', arrow='d', cat=True),
    tree_row('Categories', kind='page', state='is-selected', cat=True),
    cat_row('Settlements', 4), cat_row('Utilities', 2), cat_row('Visuals', 3), cat_row('VR', 4), cat_row('No category', 2),
])
NEW_OPTS = (f'<div class="mc-popup has-scroll t-body" style="position:absolute;right:56px;top:52px;width:392px;z-index:2">'
            f'<div class="mc-option"><span class="check"></span><span>Visuals</span></div>'
            f'<div class="mc-option"><span class="check"></span><span>VR</span></div>'
            f'<div class="mc-option"><span class="check"></span><span>Weapons and Armor</span></div>'
            f'<div class="mc-option"><span class="check"></span><span>No category</span></div>'
            f'<div class="mc-option is-new is-hover"><span class="check"></span><span>Type a new name</span><span class="mc-mark" style="position:relative;margin-left:60px"></span></div>'
            f'<div class="mc-scrollbar"><i style="bottom:0;height:42%"></i></div></div>')
open_row = row('Baka Fullscreen Pip-Boy', combo('Interface', open_=True), 'is-active')
open_row = open_row[:-len('</div>')] + NEW_OPTS + '</div>'
cat_rows = ''.join([
    pagehelp('The category each mod is listed under. A mod you leave alone follows its author and the table of known mods.'),
    row('Import from MCM Categorizer', '<div class="main"><span class="mc-btn t-body" style="min-width:160px">Import</span></div>' + reset_btn(False)),
    section('Mods'),
    open_row,
    row('Better Locational Damage', combo('Combat')),
    row('Companion Command', combo('Characters', show_reset=True, pending=True), gutter=DOT),
    row('FallUI - HUD', combo('Interface')),
    row('FRIK', combo('VR')),
])
fpc_body = f'''<div class="mc-world">
<div class="mc-panel">
<div class="mc-titlebar"><div class="mc-crumbs t-title"><span class="mc-crumb">MODS CONFIG VR</span><span class="mc-crumb-sep">»</span><span class="mc-crumb is-mod">Mods Config VR</span><span class="mc-crumb-sep">»</span><span class="mc-crumb is-current">Categories</span></div>
<span class="mc-btn is-icon t-body">?</span><span class="mc-btn is-icon">{MIN}</span><span class="mc-btn is-icon">{X}</span></div>
<div class="mc-body">
{nav(cat_tree, '', 34, offset=2 * (ROW_H + ROW_GAP))}
<div class="mc-content">
<div class="mc-scroll"><div class="rows">{cat_rows}</div><div class="mc-scrollbar"><i style="top:0;height:18%"></i></div></div>
</div>
</div>
<div class="mc-footer">{helpline('Type a new name', 'Opens the keyboard to name a category that is not in the list.')}<span class="mc-btn t-body">Discard</span><span class="mc-btn t-body is-primary">Save</span></div>
</div>
</div>'''
write('FullPanelCategories', page('<!-- @dsCard group="Panels" height=768 width=1200 subtitle="The Categories page, with the tree grouped and a category list open" -->', 'Full panel, categories', fpc_body))
print('ok categories')

# ---------------- Haptic: a pattern of the framework's library, with its shape and Play ----------------
# The keyframes of vrcf/VRControllersHaptic.cpp, (seconds, start, end): the C++ draws them from getPattern().
HAPTICS = {
    'none': ('None', []),
    'Tick': ('Tick', [(0.02, .25, .25)]),
    'Click': ('Click', [(0.03, .7, .7)]),
    'DoubleClick': ('Double click', [(0.03, .5, .5), (0.05, 0, 0), (0.03, .5, .5)]),
    'TripleClick': ('Triple click', [(0.03, .5, .5), (0.05, 0, 0), (0.03, .5, .5), (0.08, 0, 0), (0.03, .5, .5)]),
    'Success': ('Success', [(0.05, .3, .3), (0.05, 0, 0), (0.08, .8, .8)]),
    'Warning': ('Warning', [(0.06, .9, .9), (0.05, 0, 0), (0.2, .3, .3)]),
    'Error': ('Error', [(0.05, .6, .6), (0.06, 0, 0), (0.05, .6, .6), (0.06, 0, 0), (0.16, 1, 1)]),
    'Notification': ('Notification', [(0.1, .7, .7), (0.12, 0, 0), (0.03, .4, .4), (0.12, 0, 0), (0.03, .3, .3)]),
    'Start': ('Start', [(0.5, 0, 1), (0.1, 1, 1)]),
    'Stop': ('Stop', [(0.1, 1, .8), (0.5, .8, 0)]),
    'RampUp': ('Ramp up', [(0.6, 0, 1)]),
    'RampDown': ('Ramp down', [(0.6, 1, 0)]),
    'Heartbeat1': ('Heartbeat 1', [(0.08, .8, .8), (0.1, 0, 0), (0.06, .45, .45)]),
    'Heartbeat2': ('Heartbeat 2', [(0.08, .8, .8), (0.1, 0, 0), (0.06, .45, .45), (0.4, 0, 0)] * 1 + [(0.08, .8, .8), (0.1, 0, 0), (0.06, .45, .45)]),
    'Heartbeat3': ('Heartbeat 3', ([(0.08, .8, .8), (0.1, 0, 0), (0.06, .45, .45), (0.4, 0, 0)] * 2) + [(0.08, .8, .8), (0.1, 0, 0), (0.06, .45, .45)]),
    'Buzz': ('Buzz', [(0.25, .5, .5)]),
    'MidBuzz': ('Mid buzz', [(0.5, .5, .5)]),
    'LongBuzz': ('Long buzz', [(0.7, .6, .6)]),
}
ENV_W, ENV_H, ENV_BASE, ENV_PX_S = 96, 24, 22, 200   # an intensity of 1 is 22 px over the 2 px base

def env_scale(name):
    """200 px a second, or less, so a pattern longer than 0.48 s fits the 96 px: a tap stays narrow, a rumble fills."""
    total = sum(d for d, _, _ in HAPTICS[name][1])
    return min(ENV_PX_S, ENV_W / total) if total else ENV_PX_S

def env(name):
    """The shape of a pattern: a quad over the base for each keyframe of intensity, at least 2 px wide."""
    quads, t, k = [], 0.0, env_scale(name)
    for dur, a, b in HAPTICS[name][1]:
        x0, x1 = t * k, (t + dur) * k
        t += dur
        if a == 0 and b == 0:
            continue
        x1 = max(x1, x0 + 2)
        quads.append(f'M{x0:.1f} {ENV_BASE} L{x0:.1f} {ENV_BASE - a * ENV_BASE:.1f} L{x1:.1f} {ENV_BASE - b * ENV_BASE:.1f} L{x1:.1f} {ENV_BASE} Z')
    wave = f'<path class="wave" d="{" ".join(quads)}"/>' if quads else ''
    return f'<svg class="mc-env" viewBox="0 0 {ENV_W} {ENV_H}"><rect class="base" x="0" y="{ENV_BASE}" width="{ENV_W}" height="2"/>{wave}</svg>'

PLAY = ('<svg class="mc-ico" viewBox="0 0 24 24"><circle class="dot" cx="12" cy="12" r="2.5"/>'
        '<path d="M8.5 8.5 A5 5 0 0 0 8.5 15.5 M15.5 8.5 A5 5 0 0 1 15.5 15.5 M5.6 5.6 A9 9 0 0 0 5.6 18.4 M18.4 5.6 A9 9 0 0 1 18.4 18.4"/></svg>')

def play_btn(state=''):
    return f'<span class="mc-btn is-icon{" " + state if state else ""}" title="Play">{PLAY}</span>'

def haptic(name, state='', play='', show_reset=False, text=None, open_=False):
    """The frame (label, shape, arrow), Play and the Reset slot. An invalid value is its text, with no shape."""
    cls = 'mc-combo is-haptic' + (f' {state}' if state else '') + (' is-open' if open_ else '')
    label = text if text is not None else HAPTICS[name][0]
    shape = '' if state == 'is-invalid' else env(name)
    return f'<div class="main"><span class="{cls}"><span>{label}</span>{shape}{ARROW_D}</span>{play_btn(play)}</div>{reset_btn(show_reset)}'

def h_option(name, state='', mark=None):
    check = CHECK if state == 'is-current' else '<span class="check"></span>'
    mk = f'<span class="mc-mark" style="left:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return (f'<div class="mc-option{" " + state if state else ""}" style="position:relative">{check}'
            f'<span class="text">{HAPTICS[name][0]}</span>{env(name)}{mk}</div>')

def h_sheet(rows):
    """labeled() without its world, so several parts share one."""
    out = ['<div class="mc-sheet" style="display:grid;grid-template-columns:120px 740px;column-gap:16px;align-items:start;width:908px;padding-bottom:8px">']
    for label, html in rows:
        out.append(f'<span class="t-detail mc-muted" style="height:56px;display:flex;align-items:center">{label}</span><div>{html}</div>')
    out.append('</div>')
    return '\n'.join(out)

h_states = h_sheet([
    ('Idle', row('Press haptic', haptic('Tick'))),
    ('Ray on Play', row('Press haptic', haptic('Tick', play='is-hover'), 'is-hover', mark=(656, 20))),
    ('Pending', row('Press haptic', haptic('DoubleClick', 'is-pending', show_reset=True), gutter=DOT)),
    ('None', row('Press haptic', haptic('none', play='is-disabled', show_reset=True), gutter=DOT)),
    ('Invalid', row('Press haptic', haptic('none', 'is-invalid', 'is-disabled', True, text='Tik'), 'is-invalid', gutter=WARN)),
    ('Disabled', row('Entry haptic', haptic('Heartbeat2'), 'is-disabled')),
])
h_popup = (f'<div class="mc-popup has-scroll t-body" style="position:absolute;right:128px;top:68px;width:336px;z-index:2">'
           + h_option('none') + h_option('Tick', 'is-current') + h_option('Click')
           + h_option('DoubleClick', 'is-hover', mark=(150, 20)) + h_option('TripleClick')
           + '<div class="mc-scrollbar"><i style="top:0;height:26%"></i></div></div>')
h_list = f"""<div class="mc-stack" style="gap:0"><div class="mc-sheet" style="width:772px;position:relative;border-radius:8px 8px 0 0">
{row('Press haptic', haptic('Tick', open_=True), 'is-active')}
{row('Scroll speed', slider('30', 30))}
{row('Mark size', slider('0.45', 45))}
{row('Mark border width', slider('0.08', 16))}
{row('Ray length', slider('64', 50))}
{row('Show the ray', toggle(True))}
{h_popup}
</div>
<div class="mc-sheet" style="width:1120px;padding:0;border-radius:0 8px 8px 8px"><div class="mc-footer">{helpline('Double click', 'Two quick taps. For turning something on, or a change of mode. 0.11 s.')}<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span></div></div>
</div>"""
h_focused = f"""<div class="mc-focused" style="height:auto">
<div class="mc-desc t-body"><span class="name">Entry haptic:</span> Plays once when a hand enters the zone. Default Tick.</div>
<div class="item"><div class="t-body" style="display:flex;align-items:center;gap:8px"><span>Entry haptic</span>{DOT}</div>
<div class="ctl t-body"><span class="mc-stepper" style="flex:1"><span class="mc-btn is-icon">{TRI_L}</span><span class="label is-haptic is-pending" style="flex:1;width:auto"><span>Success</span>{env('Success')}</span><span class="mc-btn is-icon">{TRI_R}</span></span>{play_btn()}<span class="mc-btn is-icon">{RESET}</span></div></div>
</div>"""
h_library = '<div class="mc-sheet mc-popup t-body" style="width:336px;border-color:var(--frame-border)">' + ''.join(h_option(n) for n in HAPTICS) + '</div>'
h_body = f"""<div class="mc-world" style="display:grid;grid-template-columns:auto 336px;gap:16px 24px;align-items:start">
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The states, in the menu's own page: Pointer</span>
{h_states}
<span class="t-detail mc-caption">The list open: the ray on Double click, its help in the footer</span>
{h_list}
<span class="t-detail mc-caption">The Focused panel: a choice stepper, each step plays</span>
{h_focused}
</div>
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">Every option, as the list draws it</span>
{h_library}
</div>
</div>"""
write('Haptic', page('<!-- @dsCard group="Settings" height=1260 width=1520 subtitle="A pattern with its shape and Play: the states, the list, the Focused stepper, and every option" -->', 'Haptic', h_body))
print('ok haptic')

# ---------------- Color: a swatch and the numbers, and an editor with a picker and a slider for each channel ----------------
import colorsys

def swatch(c, big=False):
    """The left half opaque, the right half at its alpha over the checkerboard. An RGB-only color (3 values) is opaque."""
    r, g, b = c[:3]
    if len(c) == 3:
        return f'<span class="mc-swatch{" is-big" if big else ""}"><i style="background:rgb({r},{g},{b})"></i></span>'
    a = c[3]
    return (f'<span class="mc-swatch{" is-big" if big else ""}"><i style="background:rgb({r},{g},{b})"></i>'
            f'<i class="alpha mc-checker" style="--c:rgba({r},{g},{b},{a / 255:.3f})"></i></span>')

def nums(c):
    return f'{c[0]}, {c[1]}, {c[2]}' + (f', alpha {c[3]}' if len(c) == 4 else '')

def color_ctl(c, state='', show_reset=False, text=None, open_=False):
    cls = 'mc-combo is-color' + (f' {state}' if state else '') + (' is-open' if open_ else '')
    sw = '' if state == 'is-invalid' else swatch(c)
    return f'<span class="{cls}">{sw}<span class="nums">{text if text is not None else nums(c)}</span>{ARROW_D}</span>{reset_btn(show_reset)}'

def toml_text(c):
    return f'{{ rgb = [{c[0]}, {c[1]}, {c[2]}]' + (f', alpha = {c[3]}' if len(c) == 4 else '') + ' }'

def chan(name, c, i, hover=False, active=False, mark=None):
    """A channel's line: its name, a gradient track of it from 0 to 255 with the others as they are, its value."""
    r, g, b = c[:3]
    if i < 3:
        lo, hi = list(c[:3]), list(c[:3])
        lo[i], hi[i] = 0, 255
        fill = f'<i class="fillc" style="background:linear-gradient(to right, rgb({lo[0]},{lo[1]},{lo[2]}), rgb({hi[0]},{hi[1]},{hi[2]}))"></i>'
        bg = ''
    else:
        fill = f'<i class="fillc" style="background:linear-gradient(to right, rgba({r},{g},{b},0), rgb({r},{g},{b}))"></i>'
        bg = ' mc-checker'
    v = c[i]
    x = f'calc(10px + {v / 255:.4f} * (100% - 20px))'
    mk = f'<span class="mc-mark" style="left:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return (f'<div class="mc-chan{" is-hover" if hover else ""}"><span class="lbl t-detail">{name}</span>'
            f'<span class="ctrack{bg}{" is-active" if active else ""}">{fill}<i class="grab" style="left:{x}"></i>{mk}</span>'
            f'<span class="mc-value t-body">{v}</span></div>')

def channels(c, hover=None, mark=None):
    return ''.join(chan(n, c, i, hover == i, False, mark if hover == i else None) for i, n in enumerate(('Red', 'Green', 'Blue', 'Alpha')[:len(c)]))

def picker(c):
    h, s_, v = colorsys.rgb_to_hsv(c[0] / 255, c[1] / 255, c[2] / 255)
    hue = colorsys.hsv_to_rgb(h, 1, 1)
    return (f'<div class="mc-picker"><div class="sv" style="--hue:rgb({hue[0]*255:.0f},{hue[1]*255:.0f},{hue[2]*255:.0f})">'
            f'<i class="cur" style="left:{s_ * 240:.0f}px;top:{(1 - v) * 240:.0f}px"></i></div>'
            f'<div class="hue"><i class="bar" style="top:{h * 240:.0f}px"></i></div></div>')

def color_editor(name, before, now, hover=None, mark=None, focused=False, style=''):
    head = (f'<div class="head"><span class="grow t-body">{name}</span><span class="t-detail mc-muted">Before</span>{swatch(before, True)}'
            f'<span class="t-detail mc-muted" style="margin-left:8px">Now</span>{swatch(now, True)}</div>')
    side = (f'<div class="mc-channels"{" style=width:100%" if focused else ""}>{channels(now, hover, mark)}'
            f'<span class="t-detail mc-muted" style="padding-left:64px">{toml_text(now)}</span></div>')
    body = f'<div class="body">{"" if focused else picker(now)}{side}</div>'
    foot = '<div class="foot"><span class="mc-btn t-body is-quiet">Cancel</span><span class="mc-btn t-body is-primary">Done</span></div>'
    short = '' if len(now) == 4 or focused else ' is-rgb'
    return f'<div class="mc-coloredit{short}"{style}>{head}{body}{foot}</div>'

RAY, RAY_NEW = (190, 255, 190, 200), (255, 190, 120, 200)
LIGHT, LIGHT_NEW = (255, 214, 170), (255, 236, 210)   # RGB only: "valueOptions": { "alpha": false }
MARK, BORDER = (215, 255, 215, 180), (215, 255, 215, 100)

c_states = h_sheet([
    ('Idle', row('Ray color', color_ctl(RAY))),
    ('Hover', row('Mark color', color_ctl(MARK), 'is-hover', mark=(326, 22))),
    ('Pending', row('Ray color', color_ctl(RAY_NEW, 'is-pending', True), gutter=DOT)),
    ('Low alpha', row('Mark border color', color_ctl((215, 255, 215, 40), 'is-pending', True), gutter=DOT)),
    ('RGB only', row('Light color', color_ctl(LIGHT))),
    ('Invalid', row('Ray color', color_ctl(RAY, 'is-invalid', True, text='{ rgb = [300, 0, 0] }'), 'is-invalid', gutter=WARN)),
    ('Disabled', row('Icon color', color_ctl((255, 255, 255, 255)), 'is-disabled')),
])
c_page_rows = ''.join([
    row('Ray color', color_ctl(RAY_NEW, 'is-pending', True, open_=True), 'is-active', gutter=DOT),
    row('Ray width', slider('0.20', 20)),
    row('Ray max length', slider('25.0', 50)),
    row('Ray fade', slider('4.0', 30)),
    row('Mark color', color_ctl(MARK)),
    row('Mark size', slider('0.45', 45)),
    row('Mark border color', color_ctl(BORDER)),
    row('Mark border width', slider('0.08', 16)),
])
c_list = f"""<div class="mc-stack" style="gap:0"><div class="mc-sheet" style="width:772px;height:531px;position:relative;border-radius:8px 8px 0 0;overflow:hidden">
{c_page_rows}
{color_editor('Ray color', RAY, RAY_NEW, hover=1, mark=(96, 20), style=' style="left:27px;top:58px"')}
</div>
<div class="mc-sheet" style="width:1120px;padding:0;border-radius:0 8px 8px 8px"><div class="mc-footer">{helpline('Green', 'The green of the color, 0 to 255. The thumbstick steps it by 1.')}<span class="mc-btn t-body">Discard</span><span class="mc-btn t-body is-primary">Save</span></div></div>
</div>"""
c_focused = f"""<div class="mc-focused">{color_editor('Light color', LIGHT, LIGHT_NEW, focused=True)}</div>"""
c_body = f"""<div class="mc-world" style="display:grid;grid-template-columns:908px 576px;gap:16px 24px;align-items:start">
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The states, in the menu's own page: Pointer, and a light's RGB-only color</span>
{c_states}
</div>
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The Focused panel: the channels only, here of an RGB-only color</span>
{c_focused}
</div>
<div class="mc-stack" style="gap:16px;grid-column:1 / span 2">
<span class="t-detail mc-caption">The editor open under its row, the ray on Green, its help in the footer</span>
{c_list}
</div>
</div>"""
write('Color', page('<!-- @dsCard group="Settings" height=1290 width=1560 subtitle="A swatch and the numbers; the editor with a picker and a slider per channel; RGB only; the Focused editor" -->', 'Color', c_body))
print('ok color')

# ---------------- InputBinding: the framework's icon and a sentence, an editor of its parts, and Record ----------------
import json as _json
BICONS = _json.loads((Path(__file__).resolve().parent / 'binding_icons.json').read_text(encoding='utf-8'))
BICON_SCALE = 0.55        # in a row: 44 px, inside the 48 px line
BICON_SCALE_STRIP = 0.7   # in the editor's strip: 56 px
LEFT_HANDED = False   # the previews are of a right-handed player: primary is the right controller

def is_right(hand):
    return {'right': True, 'left': False, 'offhand': LEFT_HANDED}.get(hand, not LEFT_HANDED)

def printed(button, right):
    """A button as the controller prints it, as vrui::bindingLabel resolves it."""
    return {'a': 'A' if right else 'X', 'b': 'B' if right else 'Y'}.get(button, button)

HAND_WORDS = {'primary': 'the primary hand', 'offhand': 'the off hand', 'right': 'the right hand', 'left': 'the left hand'}
VERBS = {'tap': 'Tap', 'press': 'Press', 'release': 'Release', 'longpress': 'Long press', 'double': 'Double press', 'hold': 'Hold', 'touch': 'Touch'}

def button_words(button, right):
    p_ = printed(button, right)
    return f'the {p_} button' if len(p_) == 1 else ('the system button' if p_ == 'system' else f'the {p_}')

def sentence(b):
    t = b.get('type')
    if t == 'none':
        return 'None'
    hand = b['hand']
    right = is_right(hand)
    if t == 'axis':
        axis = b.get('axis', 'thumbstick')
        out = f'Push the {axis} {b["direction"]} on {HAND_WORDS[hand]}'
        if 'threshold' in b:
            out += f', past {b["threshold"]:.2f}'
    else:
        out = f'{VERBS[t]} {button_words(b["button"], right)} on {HAND_WORDS[hand]}'
        if 'duration' in b:
            d = b['duration']
            out += {'longpress': f' for {d:g} s', 'hold': f', from {d:g} s', 'double': f', presses within {d:g} s', 'release': f', held no longer than {d:g} s'}.get(t, '')
    m = b.get('modifier')
    if m:
        mh = m.get('hand')
        out += f', holding {button_words(m["button"], is_right(mh or hand))}' + (f' on {HAND_WORDS[mh]}' if mh else '')
    return out

def icon_name(hand, t, button, axis=None, direction='up'):
    """The icon of a binding: the button as the controller prints it, and how it is pressed, as the framework's
    vrui::bindingIconPath names it in f4cf/bindings."""
    right = is_right(hand)
    side = 'right' if right else 'left'
    if t == 'axis':
        axis = axis or 'thumbstick'
        return f'{side}-thumbstick-{direction}' if axis == 'thumbstick' else f'{side}-{axis}'
    name = printed(button, right).lower()
    if name == 'system':
        return None
    if t in ('tap', 'press'):
        return name if len(name) == 1 else f'{side}-{name}'
    kind = {'longpress': 'longpress', 'hold': 'hold', 'double': 'double', 'release': 'release', 'touch': 'touch'}[t]
    return f'{side}-{kind}-{name}'

def bicon(name, scale=BICON_SCALE):
    i = BICONS[name]
    w, h = i['w'] * scale, i['h'] * scale
    url = f'url(data:image/png;base64,{i["png"]})'
    return f'<i class="mc-bicon" style="width:{w:.1f}px;height:{h:.1f}px;-webkit-mask-image:{url};mask-image:{url}"></i>'

def icons(b, scale=BICON_SCALE):
    """The binding's icon; for a chord, its icon, a + and the modifier's, each as a press of its button."""
    if b.get('type') == 'none':
        return ''
    m = b.get('modifier')
    if not m:
        n = icon_name(b['hand'], b['type'], b.get('button'), b.get('axis'), b.get('direction'))
        return bicon(n, scale) if n else ''
    first = icon_name(b['hand'], b['type'], b.get('button'), b.get('axis'), b.get('direction'))
    second = icon_name(m.get('hand', b['hand']), 'press', m['button'])
    return bicon(first, scale) + '<span class="mc-bplus t-body">+</span>' + bicon(second, scale) if first and second else ''

def toml_binding(b):
    parts = []
    for k in ('hand', 'type', 'button', 'axis', 'direction', 'duration', 'threshold', 'suppress', 'modifier'):
        if k not in b:
            continue
        v = b[k]
        if k == 'modifier':
            inner = ', '.join(f'{mk} = "{mv}"' for mk, mv in v.items())
            parts.append(f'modifier = {{ {inner} }}')
        elif isinstance(v, bool):
            parts.append(f'{k} = {"true" if v else "false"}')
        elif isinstance(v, (int, float)):
            parts.append(f'{k} = {v}')
        else:
            parts.append(f'{k} = "{v}"')
    return '{ ' + ', '.join(parts) + ' }'

def binding_ctl(b, state='', show_reset=False, text=None, open_=False):
    cls = 'mc-combo is-binding' + (f' {state}' if state else '') + (' is-open' if open_ else '')
    ic = '' if state == 'is-invalid' else icons(b)
    return f'<span class="{cls}">{ic}<span class="sent">{text if text is not None else sentence(b)}</span>{ARROW_D}</span>{reset_btn(show_reset)}'

def part(label, ctl, disabled=False, hover=False, mark=None):
    mk = f'<span class="mc-mark" style="left:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    cls = 'mc-part' + (' is-disabled' if disabled else '') + (' is-hover' if hover else '')
    return f'<div class="{cls}" style="position:relative"><span class="lbl t-detail">{label}</span>{ctl}{mk}</div>'

def pcombo(label):
    return f'<span class="mc-combo"><span>{label}</span>{ARROW_D}</span>'

def pslider(value, pct):
    return (f'<span class="mc-track"><i class="fill" style="width:{pct * 0.94:.0f}px"></i><i class="grab" style="left:{10 + pct * 0.94:.0f}px"></i></span>'
            f'<span class="mc-value">{value}</span>')

HAND_LABELS = {'primary': 'Primary hand', 'offhand': 'Off hand', 'right': 'Right hand', 'left': 'Left hand'}
HOW_LABELS = dict(VERBS, axis='Push', none='None')

def binding_editor(name, b, hover=None, mark=None, offers_suppress=False, style=''):
    t = b.get('type')
    head = f'<div class="head"><span class="grow t-body">{name}</span></div>'
    strip = f'<div class="strip t-body">{icons(b, BICON_SCALE_STRIP)}<span class="sent">{sentence(b)}</span></div>'
    if True:
        none = t == 'none'
        right = is_right(b.get('hand', 'primary'))
        btn = printed(b.get('axis', 'thumbstick') if t == 'axis' else b.get('button', 'trigger'), right)
        btn = btn if len(btn) == 1 else btn.capitalize()
        has_dur = t in ('longpress', 'double', 'hold', 'release', 'axis')
        dur_label = 'Threshold' if t == 'axis' else 'Duration'
        if t == 'axis':
            dval, dpct = f'{b.get("threshold", 0.85):.2f}', b.get('threshold', 0.85) * 100
        else:
            d = b.get('duration', {'longpress': 0.6, 'double': 0.4, 'hold': 0.0, 'release': 3.0}.get(t, 0.6))
            dval, dpct = f'{d:.2f} s', (d - 0.1) / 2.9 * 100
        m = b.get('modifier')
        if m:
            mb = printed(m['button'], is_right(m.get('hand', b.get('hand'))))
            mb = mb if len(mb) == 1 else mb.capitalize()
            hold = f'{mb}, ' + ('same hand' if 'hand' not in m else HAND_LABELS[m['hand']].lower())
        else:
            hold = 'Nothing'
        sup = b.get('suppress', False)
        tog = f'<span class="mc-toggle"><span{"" if sup else " class=is-off"}>Off</span><span{" class=is-on" if sup else ""}>On</span></span>'
        dirs = f'<span class="mc-stepper"><span class="mc-btn is-icon">{TRI_L}</span><span class="label">{b.get("direction", "up").capitalize()}</span><span class="mc-btn is-icon">{TRI_R}</span></span>'
        cells = [
            part('Hand', pcombo(HAND_LABELS.get(b.get('hand', 'primary'))), none, hover == 'hand', mark if hover == 'hand' else None),
            part(dur_label, pslider(dval, dpct), none or not has_dur, hover == 'dur', mark if hover == 'dur' else None),
            part('How', pcombo(HOW_LABELS[t]), False, hover == 'how', mark if hover == 'how' else None),
            part('While holding', pcombo(hold), none, hover == 'hold', mark if hover == 'hold' else None),
            part('Button', pcombo(btn), none, hover == 'button', mark if hover == 'button' else None),
            part('Suppress from the game', tog, none, hover == 'sup', mark if hover == 'sup' else None) if offers_suppress else '<div></div>',
            part('Direction', dirs, t != 'axis', hover == 'dir', mark if hover == 'dir' else None),
            '<div></div>',
        ]
        parts_html = '<div class="parts t-body">' + ''.join(cells) + '</div>'
    foot = (f'<div class="foot"><span class="toml t-detail">{toml_binding(b)}</span>'
            '<span class="mc-btn t-body is-quiet">Cancel</span><span class="mc-btn t-body is-primary">Done</span></div>')
    return f'<div class="mc-bindedit"{style}>{head}{strip}{parts_html}{foot}</div>'

GRAB = {'hand': 'offhand', 'type': 'tap', 'button': 'trigger', 'suppress': True}
DEBUG = {'hand': 'offhand', 'type': 'longpress', 'button': 'a'}
GRAB_NEW = {'hand': 'offhand', 'type': 'double', 'button': 'grip', 'suppress': True}
OPEN = {'hand': 'primary', 'type': 'longpress', 'button': 'thumbstick', 'duration': 1.5, 'modifier': {'hand': 'offhand', 'button': 'thumbstick'}}
SCROLL = {'hand': 'primary', 'type': 'axis', 'direction': 'up'}
NONE_B = {'type': 'none'}

b_states = h_sheet([
    ('Idle', row('Grab binding', binding_ctl(GRAB))),
    ('Hover', row('Debug draw toggle', binding_ctl(DEBUG), 'is-hover', mark=(560, 22))),
    ('Pending', row('Grab binding', binding_ctl(GRAB_NEW, 'is-pending', True), gutter=DOT)),
    ('Chord', row('Open the menu', binding_ctl(OPEN))),
    ('Push', row('Scroll up', binding_ctl(SCROLL))),
    ('Push, off hand', row('Previous page', binding_ctl({'hand': 'offhand', 'type': 'axis', 'direction': 'left'}))),
    ('None', row('Secondary binding', binding_ctl(NONE_B, show_reset=True), gutter=DOT)),
    ('Invalid', row('Grab binding', binding_ctl(GRAB, 'is-invalid', True, text='{ hand = "offhand", type = "tap", button = "thumbstik" }'), 'is-invalid', gutter=WARN)),
    ('Disabled', row('Recenter binding', binding_ctl({'hand': 'offhand', 'type': 'longpress', 'button': 'thumbstick'}), 'is-disabled')),
])
b_page_rows = ''.join([
    row('Open the menu', binding_ctl(OPEN, open_=True), 'is-active'),
    row('Back button', choice_stepper('Grip')),
    section('Panel'),
    row('Recenter binding', binding_ctl({'hand': 'offhand', 'type': 'longpress', 'button': 'thumbstick'})),
    row('Recenter distance', slider('60', 40)),
    row('Panel scale', slider('6.0', 50)),
    row('Follow speed', slider('0.40', 40)),
    row('Background opacity', slider('0.85', 85)),
])
b_list = f"""<div class="mc-stack" style="gap:0"><div class="mc-sheet" style="width:772px;height:531px;position:relative;border-radius:8px 8px 0 0;overflow:hidden">
{b_page_rows}
{binding_editor('Open the menu', OPEN, hover='hold', mark=(200, 20), style=' style="left:27px;top:58px"')}
</div>
<div class="mc-sheet" style="width:1120px;padding:0;border-radius:0 8px 8px 8px"><div class="mc-footer">{helpline('While holding', 'A button held down while the binding is pressed, on the same hand or the other. Now the thumbstick of the off hand.')}<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span></div></div>
</div>"""
b_focused = f"""<div class="mc-focused" style="height:auto">
<div class="mc-desc t-body"><span class="name">Grab binding:</span> Tap the trigger on the off hand. Change it in the Full panel.</div>
<div class="item mc-row is-disabled" style="display:block;height:auto;margin:0"><div class="t-body">Grab binding</div>
<div class="ctl t-body" style="margin-top:4px;display:flex"><span class="mc-combo is-binding" style="flex:1;width:auto">{icons(GRAB)}<span class="sent">{sentence(GRAB)}</span></span></div></div>
</div>"""
b_grab = f"""<div class="mc-sheet" style="width:772px;height:556px;position:relative">
{row('Grab binding', binding_ctl(GRAB_NEW, 'is-pending', True, open_=True), 'is-active', gutter=DOT)}
{binding_editor('Grab binding', GRAB_NEW, offers_suppress=True, style=' style="left:27px;top:74px"')}
</div>"""
b_body = f"""<div class="mc-world" style="display:grid;grid-template-columns:908px 772px;gap:16px 24px;align-items:start">
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The states: the icon and the sentence, a right-handed player</span>
{b_states}
</div>
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The Focused panel: shown, not edited</span>
{b_focused}
<span class="t-detail mc-caption">An activation sphere's binding: it offers Suppress from the game</span>
{b_grab}
</div>
<div class="mc-stack" style="gap:16px;grid-column:1 / span 2">
<span class="t-detail mc-caption">The editor open under its row, the ray on While holding, its help in the footer</span>
{b_list}
</div>
</div>"""
write('InputBinding', page('<!-- @dsCard group="Settings" height=1600 width=1760 subtitle="The icon and a sentence; the editor of its parts, with and without Suppress; the Focused panel" -->', 'Input binding', b_body))
print('ok binding')

# ---------------- List: a list of texts, an `a` key; the row says its entries, the editor has one line each ----------------
UP = '<svg class="mc-ico" viewBox="0 0 24 24"><path d="M6 15 L12 9 L18 15"/></svg>'
DOWN = '<svg class="mc-ico" viewBox="0 0 24 24"><path d="M6 9 L12 15 L18 9"/></svg>'

KEYWORDS = ['ArmorBodyPartHead', 'ObjectTypeArmor', 'ArmorTypePower', 'PowerArmorHelmetLightOverride']
ALLOW = ['0F6D86|Fallout4.esm', '0CEAC4|Fallout4.esm', '115AEB|Fallout4.esm', '04FA89|DLCCoast.esm', '0540FC|DLCCoast.esm',
         '0296B8|DLCNukaWorld.esm', '029C0D|DLCNukaWorld.esm', '02770C|DLCNukaWorld.esm', '02770D|DLCNukaWorld.esm',
         '02770E|DLCNukaWorld.esm', '027419|DLCNukaWorld.esm', '02740F|DLCNukaWorld.esm', '03B557|DLCNukaWorld.esm',
         '026BB0|DLCNukaWorld.esm', '026BB5|DLCNukaWorld.esm', '026BB6|DLCNukaWorld.esm', '026BB7|DLCNukaWorld.esm']
NODES = ['AddOnNode', 'FlashlightNode', 'LightAttachNode']

def list_ctl(items, state='', show_reset=False, text=None, open_=False):
    """The row: its entries, as many as fit and then an ellipsis, and how many there are. None when it is empty."""
    cls = 'mc-combo is-list' + (f' {state}' if state else '') + (' is-open' if open_ else '') + ('' if items or text else ' is-empty')
    shown = text if text is not None else (', '.join(items) if items else 'None')
    count = f'<span class="count t-detail">{len(items)}</span>' if items and text is None else ''
    return f'<div class="main"><span class="{cls}"><span class="items">{shown}</span>{count}{ARROW_D}</span></div>{reset_btn(show_reset)}'

def entry(i, text, ordered=False, n=0, state='', mark=None):
    """An entry: its text in a frame, pressed to type it again; Up and Down where the order means something; Remove."""
    num = f'<span class="num t-detail">{i + 1}</span>' if ordered else ''
    order = (f'<span class="mc-btn is-icon{" is-disabled" if i == 0 else ""}">{UP}</span>'
             f'<span class="mc-btn is-icon{" is-disabled" if i == n - 1 else ""}">{DOWN}</span>') if ordered else ''
    mk = f'<span class="mc-mark" style="left:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return (f'<div class="mc-entry t-body{" " + state if state else ""}" style="position:relative">{num}<span class="text"><span>{text}</span></span>'
            f'{order}<span class="mc-btn is-icon">{X}</span>{mk}</div>')

def list_editor(name, items, ordered=False, pending=(), hover=None, mark=None, visible=6, offset=0, style=''):
    n = len(items)
    rows = ''.join(entry(i, t, ordered, n, ' '.join(s for s in ('is-pending' if i in pending else '', 'is-hover' if i == hover else '') if s),
                         mark if i == hover else None) for i, t in enumerate(items))
    h = visible * 48 + (visible - 1) * 4
    bar = ''
    if n > visible:
        total = n * 48 + (n - 1) * 4
        bar = f'<div class="mc-scrollbar"><i style="top:{offset / total * 100:.1f}%;height:{h / total * 100:.1f}%"></i></div>'
    lst = f' style="height:{min(h, n * 48 + (n - 1) * 4)}px"'
    inner = f' style="margin-top:-{offset}px;display:flex;flex-direction:column;gap:4px"' if offset else ' style="display:flex;flex-direction:column;gap:4px"'
    head = f'<div class="head"><span class="grow t-body">{name}</span><span class="t-detail mc-muted">{n} entries{", in order" if ordered else ""}</span></div>'
    foot = (f'<div class="foot"><span class="mc-btn t-body">{PLUS}Add</span><span class="grow"></span>'
            f'<span class="mc-btn t-body is-quiet">Cancel</span><span class="mc-btn t-body is-primary">Done</span></div>')
    return f'<div class="mc-editor mc-listedit"{style}>{head}<div class="mc-entries"><div class="list"{lst}><div{inner}>{rows}</div></div>{bar}</div>{foot}</div>'

l_states = h_sheet([
    ('Idle', row('Head light keywords', list_ctl(KEYWORDS))),
    ('Hover', row('Allow list', list_ctl(ALLOW), 'is-hover', mark=(520, 22))),
    ('Pending', row('Deny list', list_ctl(['0316D4|Fallout4.esm', '0A506D|SS2.esm'], 'is-pending', True), gutter=DOT)),
    ('Empty', row('Debug draw channels to skip', list_ctl([]))),
    ('Invalid', row('Mesh nodes', list_ctl([], 'is-invalid', True, text='"AddOnNode"'), 'is-invalid', gutter=WARN)),
    ('Disabled', row('Auto-detect plugins', list_ctl(['VRCP_DOOMMerged.esp', 'TacticalMods.esp']), 'is-disabled')),
])
l_ordered = list_editor('Mesh nodes', NODES, ordered=True, pending=(2,), hover=0, mark=(470, 20), style=' style="position:static"')
l_rows = ''.join([
    row('Headgear requirement', combo('Immersive')),
    row('Head light keywords', list_ctl(KEYWORDS)),
    row('Allow list', list_ctl(ALLOW + ['0026D8|SS2Extended.esp'], 'is-pending', True, open_=True), 'is-active', gutter=DOT),
    row('Deny list', list_ctl(['0316D4|Fallout4.esm'])),
    row('Show the light only with headgear', toggle(True)),
    row('Light offset on the helmet', slider('2.0', 40)),
    row('Light angle on the helmet', slider('8°', 20)),
    row('Allow hats', toggle(False)),
    row('Allow hoods', toggle(False)),
])
l_page = f"""<div class="mc-stack" style="gap:0"><div class="mc-sheet" style="width:772px;height:531px;position:relative;border-radius:8px 8px 0 0;overflow:hidden">
{l_rows}
{list_editor('Allow list', ALLOW + ['0026D8|SS2Extended.esp'], pending=(17,), hover=17, mark=(300, 22), offset=12 * 52, style=' style="left:27px;top:26px"')}
</div>
<div class="mc-sheet" style="width:1120px;padding:0;border-radius:0 8px 8px 8px"><div class="mc-footer">{helpline('0026D8|SS2Extended.esp', 'Press it to type it again, or Remove it. Added: it is saved with Save.')}<span class="mc-btn t-body">Discard</span><span class="mc-btn t-body is-primary">Save</span></div></div>
</div>"""
l_body = f"""<div class="mc-world" style="display:grid;grid-template-columns:908px 717px;gap:16px 24px;align-items:start">
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The states: the entries as many as fit, and how many</span>
{l_states}
</div>
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">A list whose order means something: numbered, with Up and Down</span>
{l_ordered}
</div>
<div class="mc-stack" style="gap:16px;grid-column:1 / span 2">
<span class="t-detail mc-caption">The editor over its row, where the pane has no room under it, scrolled to an entry just added</span>
{l_page}
</div>
</div>"""
write('List', page('<!-- @dsCard group="Settings" height=1180 width=1720 subtitle="A list of texts: its entries in the row; an editor with a line an entry, Add and Remove, and Up and Down where order matters" -->', 'List', l_body))
print('ok list')

# ---------------- HandPose: an `h` key; a bar a finger in the row, an editor of each finger's joints ----------------
FINGERS = ('Thumb', 'Index', 'Middle', 'Ring', 'Pinky')
POSE = {'thumb': [0.48, 1.14, 0.97, 0.06], 'index': [0.75, 0.56, 0.29, 0.23], 'middle': [0.17, 0.55, 0.62, 0.03],
        'ring': [-0.38, 0.87, 0.53, 0.03], 'pinky': [-0.8, 0.96, 1.14, 0.16], 'palm': [15.0, 0.0]}
POSE_OVER = {'thumb': [0.99, 0.41, 0.56, 0.04], 'index': [0.29, 0.41, 0.4, 0.1], 'middle': [0.2, 0.43, 0.31, 0.12],
             'ring': [0.41, 0.42, 0.36, 0.15], 'pinky': [0.76, 0.23, 0.72, 0.11], 'palm': [0.0, 0.0]}
POSE_NEW = dict(POSE, index=[0.92, 0.80, 0.29, 0.23])
J_MIN, J_MAX = -1.0, 2.0      # a joint: 0 bent, 1 straight; the shipped poses go past both
S_MIN, S_MAX = -1.0, 1.0      # spread
P_MIN, P_MAX = -90.0, 90.0    # the palm, in degrees

def straight(f):
    """How straight a finger is, 0 to 1, for its bar: the mean of its three joints, 0 bent and 1 straight."""
    return max(0.0, min(1.0, sum(f[:3]) / 3))

def pose_glyph(p, big=False):
    bars = ''.join(f'<i style="height:{max(8, straight(p[k.lower()]) * 100):.0f}%"></i>' for k in FINGERS)
    return f'<span class="mc-pose{" is-big" if big else ""}">{bars}</span>'

def palm_text(p):
    return f'palm {p["palm"][0]:g}°, {p["palm"][1]:g}°'

def pose_ctl(p, state='', show_reset=False, text=None, open_=False):
    cls = 'mc-combo is-pose' + (f' {state}' if state else '') + (' is-open' if open_ else '')
    g = '' if state == 'is-invalid' else pose_glyph(p)
    return f'<div class="main"><span class="{cls}">{g}<span class="nums">{text if text is not None else palm_text(p)}</span>{ARROW_D}</span></div>{reset_btn(show_reset)}'

def finger(name, f, state='', mark=None):
    mk = f'<span class="mc-mark" style="left:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    bend = '' if name == 'Palm' else f'<span class="bend"><i style="width:{straight(f) * 100:.0f}%"></i></span>'
    return f'<div class="mc-finger t-body{" " + state if state else ""}" style="position:relative"><span class="grow">{name}</span>{bend}{mk}</div>'

def joint(label, v, lo, hi, fmt='{:.2f}', state='', hover=False, mark=None):
    w = 100 * (min(max(v, lo), hi) - lo) / (hi - lo)
    z = 100 * (0 - lo) / (hi - lo)
    fill_l, fill_w = (z, w - z) if w >= z else (w, z - w)
    span = 'calc(100% - 20px)'
    mk = f'<span class="mc-mark" style="left:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return (f'<div class="mc-joint{" is-hover" if hover else ""}" style="position:relative"><span class="lbl t-detail">{label}</span>'
            f'<span class="mc-track{" " + state if state else ""}"><i class="zero" style="left:calc(10px + {z / 100:.4f} * {span})"></i>'
            f'<i class="fill" style="left:calc(10px + {fill_l / 100:.4f} * {span});width:calc({fill_w / 100:.4f} * {span})"></i>'
            f'<i class="grab" style="left:calc(10px + {w / 100:.4f} * {span})"></i></span>'
            f'<span class="mc-value t-body{" " + state if state else ""}">{fmt.format(v)}</span>{mk}</div>')

def scale_marks(marks, lo, hi):
    """Labels under the tracks, each centred on its value."""
    out = ''.join(f'<span style="left:calc(10px + {(v - lo) / (hi - lo):.4f} * (100% - 20px))">{t}</span>' for v, t in marks)
    return f'<div class="scale t-detail"><div class="in">{out}</div></div>'

def pose_editor(name, before, now, sel='Index', changed=(), hover=None, mark=None, style=''):
    head = (f'<div class="head"><span class="grow t-body">{name}</span><span class="t-detail mc-muted">Before</span>{pose_glyph(before, True)}'
            f'<span class="t-detail mc-muted" style="margin-left:8px">Now</span>{pose_glyph(now, True)}</div>')
    fl = ''.join(finger(n, now.get(n.lower(), [0, 0]), ' '.join(s for s in ('is-selected' if n == sel else '',
                 'is-pending' if n in changed else '', 'is-hover' if n == hover else '') if s), mark if n == hover else None)
                 for n in FINGERS + ('Palm',))
    if sel == 'Palm':
        js = ''.join(joint(l, now['palm'][i], P_MIN, P_MAX, '{:g}°', 'is-pending' if before['palm'][i] != now['palm'][i] else '')
                     for i, l in enumerate(('Pitch', 'Yaw')))
        scale = scale_marks(((P_MIN, '−90°'), (0, '0°'), (P_MAX, '90°')), P_MIN, P_MAX)
    else:
        f, b = now[sel.lower()], before[sel.lower()]
        js = ''.join(joint(l, f[i], lo, hi, state='is-pending' if f[i] != b[i] else '', hover=(hover == l), mark=mark if hover == l else None)
                     for i, (l, lo, hi) in enumerate((('Base', J_MIN, J_MAX), ('Middle', J_MIN, J_MAX), ('Tip', J_MIN, J_MAX), ('Spread', S_MIN, S_MAX))))
        scale = scale_marks(((0, '0 bent'), (1, '1 straight')), J_MIN, J_MAX)
    body = f'<div class="body"><div class="mc-fingers">{fl}</div><div class="mc-joints">{js}{scale}</div></div>'
    foot = '<div class="foot"><span class="grow"></span><span class="mc-btn t-body is-quiet">Cancel</span><span class="mc-btn t-body is-primary">Done</span></div>'
    return f'<div class="mc-editor mc-poseedit"{style}>{head}{body}{foot}</div>'

hp_states = h_sheet([
    ('Idle', row('Forward grip pose', pose_ctl(POSE))),
    ('Hover', row('Overhand grip pose', pose_ctl(POSE_OVER), 'is-hover', mark=(560, 22))),
    ('Pending', row('Forward grip pose', pose_ctl(POSE_NEW, 'is-pending', True), gutter=DOT)),
    ('Invalid', row('Forward grip pose', pose_ctl(POSE, 'is-invalid', True, text='{ thumb = [0.48, 1.14], index = …'), 'is-invalid', gutter=WARN)),
    ('Disabled', row('Power armor grip pose', pose_ctl(POSE), 'is-disabled')),
])
hp_palm = pose_editor('Overhand grip pose', POSE_OVER, dict(POSE_OVER, palm=[10.0, 0.0]), sel='Palm', changed=('Palm',), style=' style="position:static"')
hp_rows = ''.join([
    section('Hand mount'),
    row('Mount on the off hand', toggle(True)),
    row('Forward grip pose', pose_ctl(POSE_NEW, 'is-pending', True, open_=True), 'is-active', gutter=DOT),
    row('Overhand grip pose', pose_ctl(POSE_OVER)),
    row('Power armor forward grip pose', pose_ctl(POSE)),
    row('Power armor overhand grip pose', pose_ctl(POSE_OVER)),
    row('Switch grip with', combo('Grip, double press')),
    row('Mount height', slider('4.0', 40)),
])
hp_page = f"""<div class="mc-stack" style="gap:0"><div class="mc-sheet" style="width:772px;height:531px;position:relative;border-radius:8px 8px 0 0;overflow:hidden">
{hp_rows}
{pose_editor('Forward grip pose', POSE, POSE_NEW, sel='Index', changed=('Index',), hover='Base', mark=(300, 22), style=' style="left:27px;top:20px"')}
</div>
<div class="mc-sheet" style="width:1120px;padding:0;border-radius:0 8px 8px 8px"><div class="mc-footer">{helpline('Base', 'How the base joint of the index finger bends: 0 is bent, 1 is straight. Your off hand shows the pose.')}<span class="mc-btn t-body">Discard</span><span class="mc-btn t-body is-primary">Save</span></div></div>
</div>"""
hp_body = f"""<div class="mc-world" style="display:grid;grid-template-columns:908px 717px;gap:16px 24px;align-items:start">
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The states: a bar a finger, thumb to pinky, as tall as it is straight, and the palm</span>
{hp_states}
</div>
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The palm selected: its pitch and yaw in degrees</span>
{hp_palm}
</div>
<div class="mc-stack" style="gap:16px;grid-column:1 / span 2">
<span class="t-detail mc-caption">The editor over its row, where the pane has no room under it: Index selected and changed, the ray on its base joint</span>
{hp_page}
</div>
</div>"""
write('HandPose', page('<!-- @dsCard group="Settings" height=1240 width=1720 subtitle="A hand pose: a bar a finger and the palm in the row; an editor of each finger&#39;s joints, shown on the player&#39;s own hand" -->', 'Hand pose', hp_body))
print('ok hand pose')

# ---------------- Search: the tree's header is the field; a query filters the tree and shows the results as a page ----------------
SEARCH = '<svg class="mc-ico" viewBox="0 0 24 24"><circle cx="10.5" cy="10.5" r="6"/><path d="M15 15 L20 20"/></svg>'

def hit(text, q):
    """Underline where each word of the query starts a word of the name."""
    import re as _re
    for w in q.split():
        text = _re.sub(r'(?i)\b(' + _re.escape(w) + ')', r'<span class="mc-hit">\1</span>', text, count=1)
    return text

def search_field(state='', query='', count=None, clear_state='', mark=None, mods=11):
    """Idle it says what it searches; typing, the query and a caret; with a query, the count of results and Clear."""
    q = query or f'Search {mods} mods'
    cnt = f'<span class="count t-detail">{count}</span>' if count is not None else ''
    clr = f'<span class="clear{" " + clear_state if clear_state else ""}">{X}</span>' if query else ''
    cls = 'mc-search' + (f' {state}' if state else '') + (' has-query' if query else '')
    mk = f'<span class="mc-mark" style="left:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return f'<div class="{cls} {"t-body" if query else "t-detail"}" style="position:relative">{SEARCH}<span class="q">{q}</span>{cnt}{clr}{mk}</div>'

def s_row(name, q, kind='mod', count=None, state='', depth=None, arrow=None, mark=None):
    if depth is None:
        depth = 1 if kind == 'page' else 0
    a = {'r': ARROW_R, 'd': ARROW_D, None: ''}[arrow]
    cls = 'mc-tree-row t-body' + (f' d{depth}' if depth else '') + (f' {state}' if state else '')
    c = f'<span class="count t-detail">{count}</span>' if count else ''
    mk = f'<span class="mc-mark" style="right:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return f'<div class="{cls}"><span class="arrow">{a}</span><span class="name">{hit(name, q)}</span>{c}{mk}</div>'

def nav_search(field, rows, height=None):
    st = f' style="height:{height}px"' if height else ''
    return (f'<div class="mc-nav"{st}><div class="mc-nav-head is-search">{field}</div>'
            f'<div class="mc-nav-rows"><div class="list">{rows}</div></div></div>')

Q = 'hol'
s_tree = ''.join([
    s_row('FRIK', Q, arrow='d'),
    s_row('Weapons', Q, 'page', 1),
    s_row('Immersive Flashlight VR', Q, arrow='d'),
    s_row('Body stow', Q, 'page', 2, state='is-hover', mark=(40, 20)),
    s_row('Virtual Holsters', Q, count=4, arrow='r'),
])
s_results = ''.join([
    pagehelp(f'7 settings match “{Q}”. Change them here, or go to their page in the tree.'),
    section('FRIK » Weapons'),
    row(hit('Two-handed hold', Q), toggle(True)),
    section('Immersive Flashlight VR » Body stow'),
    row(hit('Holster the light on the hip', Q), toggle(False)),
    row(hit('Hip holster offset', Q), slider('6.0', 30)),
    section('Virtual Holsters'),
    row(hit('Enable holsters', Q), toggle(True)),
    row(hit('Holster size', Q), slider('1.00', 50)),
    row(hit('Left hip holster', Q), toggle(True)),
    row(hit('Holster haptic', Q), combo('Tick')),
])
fs_body = f'''<div class="mc-world mc-stack" style="gap:16px">
<div class="mc-panel">
{crumbs([R, ('Search', 'current')], alone=False)}
<div class="mc-body">
{nav_search(search_field('is-typing', Q, 7), s_tree)}
<div class="mc-content">
<div class="mc-scroll"><div class="rows">{s_results}</div><div class="mc-scrollbar"><i style="top:0;height:84%"></i></div></div>
</div>
</div>
<div class="mc-footer">{helpline('Body stow', '2 settings of Immersive Flashlight VR match. Press it to go to the page.')}{btn('Discard', '', 'is-disabled')}{btn('Save', 'is-primary', 'is-disabled')}</div>
</div>
<div class="mc-keyboard t-body" style="width:1152px">SteamVR's keyboard, shown while the field is typed in: each key filters the results</div>
</div>'''
write('FullPanelSearch', page('<!-- @dsCard group="Panels" height=900 width=1200 subtitle="Searching: the tree has the mods and pages that match, the content has the settings that match, each editable where it is" -->', 'Full panel, search', fs_body))

s_none = ''.join([pagehelp('Nothing matches “holz”. Search finds the names of mods, pages, sections and settings.')])
sc_body = f'''<div class="mc-world"><div class="mc-sheet" style="display:grid;grid-template-columns:336px 336px 336px;gap:8px 24px;align-items:start;width:1104px">
<span class="t-detail mc-muted">Idle: it says what it searches</span>
<span class="t-detail mc-muted">Under the ray</span>
<span class="t-detail mc-muted">Typing: the query, and Clear</span>
{nav_search(search_field(), flat_mods()[:0] + ''.join(tree_row(m, arrow='r') for m in ('Baka Fullscreen Pip-Boy', 'Better Locational Damage', 'Companion Command')), 236)}
{nav_search(search_field('is-hover', mark=(150, 18)), ''.join(tree_row(m, arrow='r') for m in ('Baka Fullscreen Pip-Boy', 'Better Locational Damage', 'Companion Command')), 236)}
{nav_search(search_field('is-typing', 'hol', 7), ''.join([s_row('FRIK', Q, arrow='d'), s_row('Weapons', Q, 'page', 1), s_row('Immersive Flashlight VR', Q, arrow='d')]), 236)}
<span class="t-detail mc-muted">Keyboard closed, a result&#39;s page selected</span>
<span class="t-detail mc-muted">The ray on Clear</span>
<span class="t-detail mc-muted">Nothing matches</span>
{nav_search(search_field('', 'hol', 7), ''.join([s_row('FRIK', Q, arrow='r'), s_row('Immersive Flashlight VR', Q, arrow='d'), s_row('Body stow', Q, 'page', 2, state='is-selected')]), 236)}
{nav_search(search_field('', 'hol', 7, clear_state='is-hover', mark=(262, 18)), ''.join([s_row('FRIK', Q, arrow='d'), s_row('Weapons', Q, 'page', 1), s_row('Immersive Flashlight VR', Q, arrow='d')]), 236)}
{nav_search(search_field('is-typing', 'holz', 0), '<div class="mc-nav-empty t-body">No mod matches</div>', 236)}
</div></div>'''
write('Search', page('<!-- @dsCard group="Navigation" height=640 width=1160 subtitle="The tree&#39;s header is the field: idle, under the ray, typing, a result selected, Clear, nothing found" -->', 'Search', sc_body))
print('ok search')

# ---------------- RowWarning: a setting's or a button's warning, which blocks nothing ----------------
CAUTION = ('<svg class="mc-caution" viewBox="0 0 20 18"><path class="tri" d="M10 1.5 L18.5 16.5 H1.5 Z"/>'
           '<rect class="bang" x="9" y="6" width="2" height="6"/><rect class="bang" x="9" y="13" width="2" height="2"/></svg>')

def warnline(name, warning, help_):
    return f'<div class="help t-body"><span class="name">{name}:</span> {CAUTION}<span class="caution">{warning}</span> {help_}</div>'

rw_body = f"""<div class="mc-world mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The mark after the name, and after the pending mark: a hollow amber triangle. The invalid mark takes its place while the value is invalid</span>
{h_sheet([
    ('No warning', row('Player height', slider('1.30 m', 60))),
    ('Warning', row(f'Player height{CAUTION}', slider('1.45 m', 76, 'is-pending', 'is-pending', True), gutter=DOT)),
    ('Under the ray', row(f'Player height{CAUTION}', slider('1.45 m', 76, 'is-pending', 'is-pending', True), 'is-hover', gutter=DOT, mark=(560, 22))),
    ('Pending, restart', row(f'Shadow size{CAUTION}', choice_stepper('4096', True, True), gutter=RESTART)),
    ('A button', row(f'Rebuild the holsters{CAUTION}', '<div class="main"><span class="mc-btn t-body">Run</span></div>' + reset_btn(False))),
    ('Invalid wins', row('Player height', slider('1.60 m', 100, 'is-invalid', 'is-invalid', True), 'is-invalid', gutter=WARN)),
])}
<div class="mc-sheet" style="width:1120px;padding:0"><div class="mc-footer">{warnline('Player height', 'Above 1.4 m your view can be in the ceiling.', 'Adjusts the estimated player height.')}<span class="mc-btn t-body">Discard</span><span class="mc-btn t-body is-primary">Save</span></div></div>
</div>"""
write('RowWarning', page('<!-- @dsCard group="Settings" height=620 width=1170 subtitle="A warning while its condition holds: a hollow amber mark after the name, its text first in the help line; Save is not blocked" -->', 'Row warning', rw_body))

def ro_slider(value, pct):
    return slider(value, pct)

ro_body = f"""<div class="mc-world mc-stack" style="gap:16px">
<span class="t-detail mc-caption">A value the mod writes for the player to read: its control as it is, the row disabled, no Reset</span>
{h_sheet([
    ('A number', row('Measured height', slider('1.78 m', 64), 'is-readonly')),
    ('A switch', row('Calibrated', toggle(True), 'is-readonly')),
    ('A text', row('Last calibration', '<div class="main"><span class="mc-textbox t-body"><span>Sanctuary Hills, day 12</span></span></div>' + reset_btn(False), 'is-readonly')),
    ('No value yet', row('Arm length', '<div class="main"><span class="mc-track is-empty"><i class="fill"></i><i class="grab"></i></span><span class="mc-value"></span></div>' + reset_btn(False), 'is-readonly')),
    ('Under the ray', row('Measured height', slider('1.78 m', 64), 'is-readonly is-hover', mark=(560, 22))),
])}
<div class="mc-sheet" style="width:1120px;padding:0"><div class="mc-footer">{helpline('Measured height', '(Read-only) The height FRIK measured at the last calibration. Calibrate to measure it again.')}<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span></div></div>
</div>"""
write('ReadOnly', page('<!-- @dsCard group="Settings" height=560 width=1170 subtitle="A value the mod writes: shown in its control, the row disabled, (Read-only) first in its help, no Reset" -->', 'Read-only', ro_body))
print('ok warning, read-only')

# ---------------- Hotkey: an MCM hotkey's key, shown and not edited ----------------
def keybox(name):
    """The key in a box as wide as its name, and at least 96 px, where a control ends, before the empty Reset slot."""
    return f'<div class="main"><span class="mc-keybox t-body">{name}</span></div>{reset_btn(False)}'

hk_body = f"""<div class="mc-world mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The key MCM has bound to the hotkey, in a box: the row is always disabled, as MCM binds the key and runs the hotkey</span>
{h_sheet([
    ('A key', row('Toggle the flashlight', keybox('F'))),
    ('Modifiers', row('Cycle the beam', keybox('Ctrl-Shift-F5'))),
    ('No key', row('Quick light', keybox('None'))),
    ('Under the ray', row('Toggle the flashlight', keybox('F'), 'is-hover', mark=(560, 22))),
    ('Not available', row('Strobe', keybox('None'))),
])}
<div class="mc-sheet mc-stack" style="width:1120px;padding:0;gap:0">
<div class="mc-footer">{helpline('Toggle the flashlight', '(Read-only) Turns the flashlight on and off. Bind a key to it in MCM&#39;s menu.')}<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span></div>
<div class="mc-footer">{helpline('Strobe', 'Not available: the hotkey strobe is not in the keybinds.json of the mod.')}<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span></div>
</div>
</div>"""
hk_body = hk_body.replace('class="mc-row ', 'class="mc-row is-readonly ').replace('class="mc-row is-readonly "', 'class="mc-row is-readonly"')
write('Hotkey', page('<!-- @dsCard group="Settings" height=620 width=1170 subtitle="An MCM hotkey: its key in a box at least 96 px wide, the row disabled with the hover look under the ray, (Read-only) first in its help" -->', 'Hotkey', hk_body))

# ---------------- UnsupportedControl: a row of a control the menu does not have ----------------
def ctl_type(name):
    """What the row is, where its control would be: the type as its config writes it, in the muted color."""
    return f'<div class="main"><span class="mc-type t-body">{name}</span></div>{reset_btn(False)}'

uc_body = f"""<div class="mc-world mc-stack" style="gap:16px">
<span class="t-detail mc-caption">A control the menu does not have: its text, and its type where the control would be. The row is always disabled</span>
{h_sheet([
    ('positioner', row('Holster position', ctl_type('positioner'))),
    ('keyinput', row('Melee key', ctl_type('keyinput'))),
    ('customClipLoader', row('Hand animations', ctl_type('customClipLoader'))),
    ('No type', row('Advanced options', ctl_type('unknown'))),
    ('Under the ray', row('Holster position', ctl_type('positioner'), 'is-hover', mark=(560, 22))),
])}
<div class="mc-sheet mc-stack" style="width:1120px;padding:0;gap:0">
<div class="mc-footer">{helpline('Holster position', 'Not available: the menu has no positioner control.')}<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span></div>
<div class="mc-footer">{helpline('Advanced options', 'Not available: the row has no type of control.')}<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span></div>
</div>
</div>"""
uc_body = uc_body.replace('class="mc-row ', 'class="mc-row is-readonly ').replace('class="mc-row is-readonly "', 'class="mc-row is-readonly"')
write('UnsupportedControl', page('<!-- @dsCard group="Settings" height=620 width=1170 subtitle="A control the menu does not have: its type in muted text, the row disabled with the hover look under the ray; its help says the menu has no such control" -->', 'Control the menu does not have', uc_body))
print('ok hotkey, unsupported control')

# ---------------- NumberDisplay: decimals, a unit, a percentage, and a value too wide for its box ----------------
def slider_v(value, pct, cls=''):
    return (f'<div class="main"><span class="mc-track"><i class="fill" style="width:{pct*2.6:.0f}px"></i>'
            f'<i class="grab" style="left:{10+pct*2.6:.0f}px"></i></span>'
            f'<span class="mc-value {cls}">{value}</span></div>{reset_btn(False)}')

nd_body = f"""<div class="mc-world mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The value box shows the number as display says: its decimals, its unit, or a percentage</span>
{h_sheet([
    ('Decimals of step', row('Brightness', slider_v('1.25', 50))),
    ('Whole number', row('Hours per battery', num_stepper('8'))),
    ('decimals: 1', row('Reach', slider_v('12.0', 40))),
    ('unit: m', row('Player height', slider_v('1.78 m', 64))),
    ('unit: °', row('Beam angle', slider_v('40°', 30))),
    ('percent', row('Ray opacity', slider_v('85%', 85))),
    ('Negative', row('Hip offset', slider_v('-0.50', 40))),
    ('Too wide: t-detail', row('Fade time', slider_v('12000 ms', 60, 'is-small'))),
    ('Still too wide: no unit', row('Draw distance', slider_v('250000', 70, 'is-small'))),
    ('Stepper with unit', row('Battery life', num_stepper('8 h'))),
])}
<div class="mc-sheet" style="width:1120px;padding:0"><div class="mc-footer">{helpline('Ray opacity', 'How solid the ray is. 85% (217 of 255). (Default: 100%)')}<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span></div></div>
</div>"""
write('NumberDisplay', page('<!-- @dsCard group="Settings" height=800 width=1170 subtitle="A number as display says: decimals, a unit, a percentage of its range; a value too wide for its box" -->', 'Number display', nd_body))
print('ok number display')

# ---------------- ButtonRow: a button in a row, running and its result; a confirmation, dangerous or not ----------------
SPIN = '<svg class="mc-ico spin" viewBox="0 0 24 24"><path d="M12 3 A9 9 0 1 1 3 12"/></svg>'

def run_btn(label='Run', state=''):
    return f'<div class="main"><span class="mc-btn t-body is-run{" " + state if state else ""}">{label}</span></div>{reset_btn(False)}'

DONE = ('<svg class="mc-done" viewBox="0 0 20 20"><circle class="ring" cx="10" cy="10" r="8.5"/>'
        '<rect class="i" x="9" y="8.5" width="2" height="6.5"/><circle class="i" cx="10" cy="5.8" r="1.3"/></svg>')

def doneline(name, result, help_):
    """The help line of a button whose action worked: the info mark and the result in green, then the row's help."""
    return f'<div class="help t-body"><span class="name">{name}:</span> {DONE}<span class="done">{result}</span> {help_}</div>'

br_states = h_sheet([
    ('Run', row('Hide the HUD', run_btn())),
    ('buttonText', row('Calibrate height', run_btn('Calibrate'))),
    ('Under the ray', row('Calibrate height', run_btn('Calibrate'), 'is-hover', mark=(620, 22))),
    ('Running', row('Calibrate height', run_btn(f'{SPIN}Running', 'is-running'))),
    ('Done, not seen', row('Calibrate height', run_btn('Calibrate'), gutter=DONE)),
    ('Done, ray on it', row('Calibrate height', run_btn('Calibrate'), 'is-hover', gutter=DONE, mark=(620, 22))),
    ('Done, seen', row('Calibrate height', run_btn('Calibrate'))),
    ('Failed', row('Calibrate height', run_btn('Calibrate'), gutter=WARN)),
    ('No client', row('Rebuild the holsters', run_btn('Rebuild'), 'is-disabled')),
])
dlg = lambda title, msg, ok, danger, without=False: (f'<div class="mc-sheet mc-scrim" style="width:820px;height:300px;padding:0">'
    f'<div class="mc-dialog{" is-danger" if danger else ""}"><div class="t-title">{title}</div><div class="t-body">{msg}</div>'
    f'<div class="actions">{btn("Cancel", "is-quiet")}{btn("Run without saving") if without else ""}{btn(ok, "is-danger" if danger else "is-primary")}</div></div></div>')
br_body = f"""<div class="mc-world" style="display:grid;grid-template-columns:908px 820px;gap:16px 24px;align-items:start">
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The row: Run, or the button's text, in the control column; running; a mark after the name for its result, which is in the help line</span>
{br_states}
<div class="mc-sheet mc-stack" style="width:908px;padding:0;gap:0">
<div class="mc-footer">{helpline('Calibrate height', 'Measures your height and updates the calibration. Stand straight first.')}</div>
<div class="mc-footer">{doneline('Calibrate height', 'Done: 1.78 m.', 'Measures your height and updates the calibration. Stand straight first.')}</div>
<div class="mc-footer">{helpline('Calibrate height', 'Stand still, then try again.', error=True)}</div>
</div>
</div>
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">A confirmation, as the config spec writes it</span>
{dlg('Reset the calibration?', 'This discards the current body calibration. FRIK measures you again the next time you calibrate.', 'Reset', False)}
<span class="t-detail mc-caption">dangerous: the confirming button and the frame in red</span>
{dlg('Delete every holster?', 'Every holster you placed is removed, with the weapons in them returned to your inventory. This cannot be undone.', 'Delete', True)}
<span class="t-detail mc-caption">A script's button while its mod has unsaved changes: Save and run, or Run without saving</span>
{dlg('Unsaved changes', 'You have 2 unsaved changes, and the button&#39;s script reads the saved values. Save them before it runs?', 'Save and run', False, True)}
</div>
</div>"""
write('ButtonRow', page('<!-- @dsCard group="Actions" height=1190 width=1800 subtitle="A row that runs an action: Run or its text, running, its result marked in the gutter and said in the help line; a confirmation, and a dangerous one" -->', 'Button row', br_body))
print('ok button row')

# ---------------- RestartNotice: a setting that needs a restart, and the Save that wrote one ----------------
RTAG = '<span class="mc-tag t-detail">Restart</span>'
rn_body = f"""<div class="mc-world mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The row: changed, its pending mark is the Restart mark, and the status counts it (see TakesEffect)</span>
<div class="mc-sheet mc-stack" style="width:1120px;gap:12px">
<div style="width:772px">{row('Shadow size', choice_stepper('4096', True, True), 'is-hover', gutter=RESTART, mark=(560, 22))}{row('Brightness', slider('1.25', 75, 'is-pending', 'is-pending', True), gutter=SAVE)}</div>
{footer(DOT + '<span class="mc-amber">2 unsaved changes, 1 takes effect after a restart</span>', btn('Discard') + btn('Save', 'is-primary'))}
<span class="t-detail mc-muted">The ray on it: the help line's tag</span>
<div class="mc-footer">{helpline('Shadow size', 'How sharp the beam&#39;s shadows are. Larger costs frame time. (Default: 2048)', tag=ptag(RESTART, 'Restart'))}{btn('Discard')}{btn('Save', 'is-primary')}</div>
<span class="t-detail mc-muted">After the Save: the status says it until the next change of the mod</span>
{footer(RTAG + '<span>Saved. 1 change takes effect after the game is restarted.</span>', btn('Discard', '', 'is-disabled') + btn('Save', 'is-primary', 'is-disabled'))}
<span class="t-detail mc-muted">A Save with no restart</span>
{footer('<span class="mc-muted">Saved</span>', btn('Discard', '', 'is-disabled') + btn('Save', 'is-primary', 'is-disabled'))}
</div>
<span class="t-detail mc-caption">The menu's front page keeps it under Notices until the game is restarted</span>
<div class="mc-sheet" style="width:772px">{section('Notices')}<div class="mc-notice t-body">{RTAG}<span>Restart the game: Immersive Flashlight VR has 1 saved change that takes effect then.</span></div>{pagehelp('Sim Settlements 2 is not available: its plugin SS2.esm is not loaded.')}</div>
</div>"""
write('RestartNotice', page('<!-- @dsCard group="Actions" height=910 width=1170 subtitle="A change that takes effect after a restart: its mark, its tag, the status after the Save that wrote it, and the front page notice" -->', 'Restart notice', rn_body))
print('ok restart')

# ---------------- TakesEffect: when a pending change takes effect, as its config spec says ----------------
te_rows = h_sheet([
    ('Not changed', row('Brightness', slider('1.00', 50))),
    ('Not known', row('Brightness', slider('1.25', 75, 'is-pending', 'is-pending', True), gutter=DOT)),
    ('Live', row('Brightness', slider('1.25', 75, 'is-pending', 'is-pending', True), gutter=LIVE)),
    ('After Save', row('Holster size', slider('0.80', 40, 'is-pending', 'is-pending', True), gutter=SAVE)),
    ('After a restart', row('Shadow size', choice_stepper('4096', True, True), gutter=RESTART)),
    ('And a warning', row(f'Shadow size{CAUTION}', choice_stepper('4096', True, True), gutter=RESTART)),
])
te_body = f"""<div class="mc-world mc-stack" style="gap:16px">
<span class="t-detail mc-caption">A changed row: its pending mark is the mark of when the change takes effect, in amber, or the plain dot where its mod does not say</span>
{te_rows}
<span class="t-detail mc-caption">The help line: the tag of the pending change, in amber, before the row's help. A row that is not changed, or whose mod does not say, has none</span>
<div class="mc-sheet mc-stack" style="width:1120px;padding:0;gap:0">
<div class="mc-footer">{helpline('Brightness', 'How bright the beam is. (Default: 1.00)', tag=ptag(LIVE, 'Live'))}{btn('Discard')}{btn('Save', 'is-primary')}</div>
<div class="mc-footer">{helpline('Holster size', 'How large the holsters are drawn. (Default: 1.00)', tag=ptag(SAVE, 'Save'))}{btn('Discard')}{btn('Save', 'is-primary')}</div>
<div class="mc-footer">{helpline('Shadow size', 'How sharp the beam&#39;s shadows are. Larger costs frame time. (Default: 2048)', tag=ptag(RESTART, 'Restart'))}{btn('Discard')}{btn('Save', 'is-primary')}</div>
<div class="mc-footer">{helpline('Brightness', 'How bright the beam is.')}{btn('Discard')}{btn('Save', 'is-primary')}</div>
</div>
</div>"""
write('TakesEffect', page('<!-- @dsCard group="Settings" height=900 width=1170 subtitle="When a pending change takes effect: its mark on a changed row in place of the pending dot, and its tag in the help line" -->', 'When a change takes effect', te_body))
print('ok takes effect')

# ---------------- TextInput: a field that shows the text, typed on SteamVR's keyboard ----------------
PEN = '<svg class="mc-ico pen" viewBox="0 0 24 24"><path d="M4 20 L5 15 L15.5 4.5 L19.5 8.5 L9 19 Z"/><path d="M13.5 6.5 L17.5 10.5"/></svg>'

def field(text, state='', num=False, length=None, show_reset=False):
    cls = 'mc-field' + (' is-num' if num else '') + (f' {state}' if state else '') + ('' if text else ' is-empty')
    shown = text if text else 'Empty'
    tail = f'<span class="len t-detail">{length}</span>' if length else ('' if 'is-editing' in state else PEN)
    return f'<div class="main"><span class="{cls}"><span class="txt t-body">{shown}</span>{tail}</span></div>{reset_btn(show_reset)}'

ti_states = h_sheet([
    ('Idle', row('Settlement name', field('Sanctuary Hills'))),
    ('Under the ray', row('Settlement name', field('Sanctuary Hills'), 'is-hover', mark=(560, 22))),
    ('Typing', row('Settlement name', field('Sanctuary Hil', 'is-editing', length='12 / 32'), 'is-active')),
    ('Pending', row('Settlement name', field('Red Rocket', 'is-pending', show_reset=True), gutter=DOT)),
    ('Empty', row('Greeting', field(''))),
    ('Too long', row('Settlement name', field('The Commonwealth Minutemen headquarters', 'is-invalid', show_reset=True), 'is-invalid', gutter=WARN)),
    ('A number', row('Max settlers', field('20', num=True))),
    ('Number, out of range', row('Max settlers', field('250', 'is-invalid', num=True, show_reset=True), 'is-invalid', gutter=WARN)),
    ('Disabled', row('Settlement name', field('Sanctuary Hills'), 'is-disabled')),
])
ti_body = f"""<div class="mc-world mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The field shows the text and a pencil; a press shows SteamVR's keyboard, and the text changes in the field as it is typed</span>
{ti_states}
<div class="mc-sheet" style="width:1120px;padding:0"><div class="mc-footer">{helpline('Settlement name', 'Typing: Done keeps it, Cancel puts back “Sanctuary Hills”. Up to 32 characters.')}<span class="mc-btn t-body">Discard</span><span class="mc-btn t-body is-primary">Save</span></div></div>
<div class="mc-keyboard t-body" style="width:1120px">SteamVR's keyboard, while the field is typed in: Done and Cancel are its own</div>
</div>"""
write('TextInput', page('<!-- @dsCard group="Settings" height=860 width=1170 subtitle="A text or a typed number: the field, typing on SteamVR&#39;s keyboard with the count of characters, pending, empty, invalid" -->', 'Text input', ti_body))
print('ok text input')

# ---------------- TextRows: a text, html or plain; a spacer; an image ----------------
LOGO = ('<svg width="512" height="128" viewBox="0 0 512 128"><defs><linearGradient id="lg" x1="0" x2="1"><stop offset="0" stop-color="#1b3a2a"/>'
        '<stop offset="1" stop-color="#0a1f15"/></linearGradient></defs><rect width="512" height="128" rx="6" fill="url(#lg)"/>'
        '<circle cx="64" cy="64" r="34" fill="none" stroke="#e8d27a" stroke-width="6"/><path d="M64 38 V64 L82 76" stroke="#e8d27a" stroke-width="6" fill="none"/>'
        '<text x="118" y="76" font-family="Roboto" font-size="31" fill="#e8d27a">IMMERSIVE FLASHLIGHT</text></svg>')
tr_page = ''.join([
    f'<div class="mc-text t-body">{"A text is a paragraph in the muted color, wrapped to the width of the rows. It holds no value, and the ray does nothing on it."}</div>',
    section('Calibration'),
    f'<div class="mc-text t-body"><p style="text-align:center;font-size:25.2px;line-height:30px">Calibration</p>'
    f'<p>Stand straight and look ahead. <span style="color:#FFFFFF">Do not move</span> until it is done. <b>Bold</b>, <i>italic</i> and <u>underlined</u> as MCM writes them.</p></div>',
    '<div class="mc-spacer is-shown" style="height:21px" data-l="spacer, 1 line"></div>',
    row('Calibrate height', run_btn('Calibrate')),
    '<div class="mc-spacer is-shown" style="height:42px" data-l="spacer, numLines 2"></div>',
    f'<div class="mc-image">{LOGO}</div>',
    f'<div class="mc-image"><div class="missing t-detail" style="width:384px;height:96px">Missing: MyMod/config/banner.dds</div></div>',
])
tr_body = f"""<div class="mc-world mc-stack" style="gap:16px">
<span class="t-detail mc-caption">A text, plain and with tags; spacers (outlined here only); an image, and a missing one</span>
<div class="mc-sheet" style="width:772px">{tr_page}</div>
</div>"""
write('TextRows', page('<!-- @dsCard group="Settings" height=800 width=830 subtitle="Rows with no value: a text, plain or with MCM&#39;s tags; a spacer of one line or more; an image, and a missing one" -->', 'Text, spacer and image', tr_body))
print('ok text rows')

# ---------------- FileChoice: a choice of the files in a folder, with a texture's thumbnail ----------------
def gobo(kind):
    """A stand-in for a gobo texture: how the light falls through it, white on black."""
    g = {'round': '<circle cx="20" cy="20" r="15" fill="url(#gr)"/>',
         'incandescent': '<circle cx="20" cy="20" r="16" fill="url(#gr)"/><circle cx="20" cy="20" r="7" fill="none" stroke="#000" stroke-opacity=".35" stroke-width="2"/>',
         'pa': '<circle cx="20" cy="20" r="18" fill="url(#gr)"/><circle cx="20" cy="20" r="11" fill="none" stroke="#fff" stroke-opacity=".5" stroke-width="2"/>',
         'led': ''.join(f'<circle cx="{x}" cy="{y}" r="4" fill="url(#gr)"/>' for x in (12, 20, 28) for y in (12, 20, 28)),
         'dirty': '<circle cx="20" cy="20" r="15" fill="url(#gr)"/><path d="M8 26 Q18 20 32 28 L32 34 L8 34Z" fill="#000" fill-opacity=".45"/>',
         'small': '<circle cx="20" cy="20" r="9" fill="url(#gr)"/>'}[kind]
    return (f'<span class="mc-thumb"><svg viewBox="0 0 40 40"><defs><radialGradient id="gr"><stop offset="0" stop-color="#fff"/>'
            f'<stop offset=".7" stop-color="#d8d8c8"/><stop offset="1" stop-color="#000" stop-opacity="0"/></radialGradient></defs>'
            f'<rect width="40" height="40" fill="#000"/>{g}</svg></span>')

GOBOS = [('1_Clean', 'round'), ('2_LED_Array', 'led'), ('3_Dirty_Lens', 'dirty'), ('4_Spot', 'small'), ('5_F4_PA_Bright', 'pa'),
         ('6_Incandescent', 'incandescent'), ('7_Round_Small', 'small')]
def file_ctl(name, kind, state='', show_reset=False, open_=False, ext='.DDS'):
    cls = 'mc-combo is-file' + (f' {state}' if state else '') + (' is-open' if open_ else '')
    th = '' if state == 'is-invalid' or kind is None else gobo(kind)
    return f'<div class="main"><span class="{cls}">{th}<span class="nm">{name}<span class="ext">{ext}</span></span>{ARROW_D}</span></div>{reset_btn(show_reset)}'

def f_option(name, kind, current=False, hover=False, mark=None):
    mk = f'<span class="mc-mark" style="position:relative;margin-left:60px"></span>' if mark else ''
    ck = CHECK if current else '<span class="check"></span>'
    return f'<div class="mc-option is-file{" is-current" if current else ""}{" is-hover" if hover else ""}">{ck}{gobo(kind)}<span>{name}<span class="ext">.DDS</span></span>{mk}</div>'

fc_states = h_sheet([
    ('A texture', row('Gobo on the head', file_ctl('6_Incandescent', 'incandescent'))),
    ('Under the ray', row('Gobo in the hand', file_ctl('6_Incandescent', 'incandescent'), 'is-hover', mark=(560, 22))),
    ('Pending', row('Gobo on the head', file_ctl('2_LED_Array', 'led', 'is-pending', True), gutter=DOT)),
    ('Not a texture', row('Voice pack', file_ctl('Codsworth', None, ext='.json'))),
    ('Not in the folder', row('Gobo on a weapon', file_ctl('9_Laser', None, 'is-invalid', True), 'is-invalid', gutter=WARN)),
    ('Empty folder', row('Gobo in power armor', file_ctl('No files', None, ext=''), 'is-disabled')),
])
fc_pop = (f'<div class="mc-popup has-scroll t-body" style="position:absolute;left:390px;top:112px;width:392px;z-index:2">'
          f'<div class="folder t-detail"><span>Textures\\ImmersiveFlashlightVR\\Gobos</span><span>7 files</span></div>'
          + ''.join(f_option(n, k, current=(n == '6_Incandescent'), hover=(n == '2_LED_Array'), mark=(n == '2_LED_Array')) for n, k in GOBOS[:5])
          + '<div class="mc-scrollbar"><i style="top:0;height:82%"></i></div></div>')
fc_rows = ''.join([
    section('On the head'),
    row('Gobo on the head', file_ctl('2_LED_Array', 'led', 'is-pending', True, open_=True), 'is-active', gutter=DOT),
    row('Brightness', slider('1.10', 45)),
    row('Reach', slider('2000', 30)),
    row('Beam angle', slider('90°', 60)),
    row('Light color', color_ctl((235, 224, 190))),
    section('In the hand'),
    row('Gobo in the hand', file_ctl('6_Incandescent', 'incandescent')),
    row('Brightness', slider('1.40', 60)),
])
fc_body = f"""<div class="mc-world" style="display:grid;grid-template-columns:908px;gap:16px">
<span class="t-detail mc-caption">The row: the file's name, its extension muted, and for a texture its thumbnail</span>
{fc_states}
<span class="t-detail mc-caption">The list: the folder and how many files, each with its thumbnail; a pick is shown on the light at once</span>
<div class="mc-stack" style="gap:0"><div class="mc-sheet" style="width:772px;height:520px;position:relative;border-radius:8px 8px 0 0;overflow:hidden">{fc_rows}{fc_pop}</div>
<div class="mc-sheet" style="width:908px;padding:0;border-radius:0 8px 8px 8px"><div class="mc-footer">{helpline('2_LED_Array.DDS', 'Textures\\ImmersiveFlashlightVR\\Gobos. Pick it, and the light on your head shows it.')}</div></div></div>
</div>"""
write('FileChoice', page('<!-- @dsCard group="Settings" height=1100 width=960 subtitle="A choice of the files in a folder: the name, a texture&#39;s thumbnail, the folder&#39;s list; not in the folder; an empty folder" -->', 'Choice of files', fc_body))
print('ok file choice')

# ---------------- Transform: pos, rot and scale; steppers with a step size, and tuning in the world ----------------
def fnum(v, deg=False):
    t = f'{v:.2f}'.rstrip('0').rstrip('.') if v != int(v) else f'{int(v)}'
    return t + ('°' if deg else '')

def xform_ctl(t, state='', show_reset=False, text=None, open_=False):
    cls = 'mc-combo is-xform' + (f' {state}' if state else '') + (' is-open' if open_ else '')
    if text is not None:
        lines = f'<span class="t-body">{text}</span>'
    else:
        p, r, s_ = t.get('pos', [0, 0, 0]), t.get('rot', [0, 0, 0]), t.get('scale', 1)
        m = lambda k, txt: txt if k in t else f'<span class="mc-muted">{txt}</span>'
        lines = (f'<span class="t-detail"><span class="k">pos</span>{m("pos", ", ".join(fnum(v) for v in p))}{"" if s_ == 1 else f"   ×{fnum(s_)}"}</span>'
                 f'<span class="t-detail"><span class="k">rot</span>{m("rot", ", ".join(fnum(v, True) for v in r))}</span>')
    return f'<div class="main"><span class="{cls}"><span class="lines">{lines}</span>{ARROW_D}</span></div>{reset_btn(show_reset)}'

def axis(label, v, state='', hover=False, mark=None, deg=False, unset=False):
    mk = f'<span class="mc-mark" style="left:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    vs = ' is-unset' if unset else (f' {state}' if state else '')
    return (f'<div class="mc-axis{" is-hover" if hover else ""}"><span class="lbl t-detail">{label}</span>'
            f'<span class="mc-btn is-icon">{MINUS}</span><span class="mc-value t-body{vs}">{fnum(v, deg)}</span>'
            f'<span class="mc-btn is-icon">{PLUS}</span>{mk}</div>')

def xform_editor(name, t, changed=(), hover=None, mark=None, unset=(), style=''):
    p, r, s_ = t.get('pos', [0, 0, 0]), t.get('rot', [0, 0, 0]), t.get('scale', 1)
    st = lambda k: 'is-pending' if k in changed else ''
    head = (f'<div class="head"><span class="grow t-body">{name}</span><span class="t-detail mc-muted">Step</span>'
            f'<span class="mc-seg t-detail"><span>Fine</span><span class="is-on">Normal</span><span>Coarse</span></span></div>')
    left = (f'<div class="mc-stack"><span class="grp t-detail">Position</span>'
            + ''.join(axis(l, p[i], st(l), hover == l, mark if hover == l else None, unset=l in unset) for i, l in enumerate(('X', 'Y', 'Z')))
            + axis('Scale', s_, st('Scale'), hover == 'Scale', unset='Scale' in unset) + '</div>')
    right = (f'<div class="mc-stack"><span class="grp t-detail">Rotation</span>'
             + ''.join(axis(l, r[i], st(l), hover == l, mark if hover == l else None, True, l in unset) for i, l in enumerate(('Heading', 'Roll', 'Attitude')))
             + '</div>')
    foot = (f'<div class="foot"><span class="mc-btn t-body">In the world</span><span class="grow"></span>'
            f'<span class="mc-btn t-body is-quiet">Cancel</span><span class="mc-btn t-body is-primary">Done</span></div>')
    return f'<div class="mc-editor mc-xformedit"{style}>{head}<div class="body">{left}{right}</div>{foot}</div>'

MESH = {'pos': [9.37, -5.86, -5.13], 'rot': [20.72, 0.0, 90.0], 'scale': 1.5}
MESH_NEW = {'pos': [9.37, -4.86, -5.13], 'rot': [20.72, 0.0, 90.0], 'scale': 1.5}
TILT = {'rot': [0.0, -5.0, 0.0], 'scale': 1.0}
xf_states = h_sheet([
    ('Idle', row('Flashlight mesh', xform_ctl(MESH))),
    ('Keys left out', row('Head light tilt', xform_ctl({'rot': [0.0, -5.0, 0.0]}))),
    ('Under the ray', row('Flashlight mesh', xform_ctl(MESH), 'is-hover', mark=(560, 22))),
    ('Pending', row('Flashlight mesh', xform_ctl(MESH_NEW, 'is-pending', True), gutter=DOT)),
    ('Invalid', row('Weapon mount', xform_ctl({}, 'is-invalid', True, text='{ pos = [0, 0], scale = 1 }'), 'is-invalid', gutter=WARN)),
])
xf_rows = ''.join([
    section('Hand mount'),
    row('Flashlight mesh', xform_ctl(MESH_NEW, 'is-pending', True, open_=True), 'is-active', gutter=DOT),
    row('Overhand mesh', xform_ctl({'pos': [6.57, -5.47, 0.56], 'rot': [-90.0, 0.0, 0.0], 'scale': 1.5})),
    row('Light offset', xform_ctl({'pos': [0, 0, 2.0]})),
    row('Mount on the off hand', toggle(True)),
    row('Forward grip pose', pose_ctl(POSE)),
    row('Overhand grip pose', pose_ctl(POSE_OVER)),
    row('Switch grip with', combo('Grip, double press')),
    row('Mount height', slider('4.0', 40)),
])
xf_page = f"""<div class="mc-stack" style="gap:0"><div class="mc-sheet" style="width:772px;height:531px;position:relative;border-radius:8px 8px 0 0;overflow:hidden">
{xf_rows}
{xform_editor('Flashlight mesh', MESH_NEW, changed=('Y',), hover='Y', mark=(140, 22), style=' style="left:27px;top:20px"')}
</div>
<div class="mc-sheet" style="width:1120px;padding:0;border-radius:0 8px 8px 8px"><div class="mc-footer">{helpline('Y', 'Moves the flashlight mesh along Y by 0.1 a step. The thumbstick steps it. Your flashlight shows it.')}<span class="mc-btn t-body">Discard</span><span class="mc-btn t-body is-primary">Save</span></div></div>
</div>"""
xf_left_out = xform_editor('Head light tilt', {'pos': [0, 0, 0], 'rot': [0.0, -5.0, 0.0], 'scale': 1}, unset=('X', 'Y', 'Z', 'Scale'), style=' style="position:static"')
xf_body = f"""<div class="mc-world" style="display:grid;grid-template-columns:908px 717px;gap:16px 24px;align-items:start">
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">The row: position and scale on one line, rotation on the next</span>
{xf_states}
</div>
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">Keys the value leaves out, in muted: no move, a scale of 1</span>
{xf_left_out}
</div>
<div class="mc-stack" style="gap:16px;grid-column:1 / span 2">
<span class="t-detail mc-caption">The editor over its row, where the pane has no room under it: a stepper for each axis, a step size, and In the world</span>
{xf_page}
</div>
</div>"""
write('Transform', page('<!-- @dsCard group="Settings" height=1180 width=1720 subtitle="A transform: position, rotation and scale in the row; an editor of steppers with a step size, and In the world" -->', 'Transform', xf_body))
print('ok transform')

# ---------------- ActivationSphere: a section of the TOML drawn as one group, its sphere lit in the world ----------------
SPHERE = ('<svg class="sphere" viewBox="0 0 20 20"><defs><radialGradient id="sg"><stop offset="0" stop-color="#0afa78" stop-opacity=".9"/>'
          '<stop offset="1" stop-color="#0afa78" stop-opacity=".15"/></radialGradient></defs>'
          '<circle class="halo" cx="10" cy="10" r="9.5" fill="none" stroke="#0afa78" stroke-opacity=".5" opacity="0"/>'
          '<circle cx="10" cy="10" r="7" fill="url(#sg)" stroke="#0afa78" stroke-width="1.5"/></svg>')

def sphere_head(name, lit=False):
    live = '<span class="live t-detail">Lit in the world</span>' if lit else ''
    return f'<div class="mc-section t-body is-sphere{" is-lit" if lit else ""}">{SPHERE}<span>{name}</span>{live}</div>'

def same_as_zone():
    return (f'<div class="main"><span class="mc-combo is-xform"><span class="lines"><span class="t-body same">As Zone</span></span>{ARROW_D}</span></div>{reset_btn(False)}')

BODY_ZONE = {'pos': [3.91, 7.11, 0.67], 'scale': 20.0}
BODY_PRIMARY = {'hand': 'offhand', 'type': 'tap', 'button': 'trigger', 'suppress': True}
BODY_SECONDARY = {'hand': 'primary', 'type': 'tap', 'button': 'trigger', 'suppress': True}
HEAD_PRIMARY = {'hand': 'offhand', 'type': 'tap', 'button': 'trigger', 'suppress': True}
HEAD_SECONDARY = {'hand': 'offhand', 'type': 'longpress', 'button': 'trigger', 'suppress': True}
as_rows = ''.join([
    pagehelp('Where your hand reaches to take the light, and what a press there does. A sphere is lit in the world while the ray is on its rows.'),
    sphere_head('Body grab', lit=True),
    row('Zone', xform_ctl(BODY_ZONE), 'is-hover', mark=(560, 22)),
    row('Zone in power armor', xform_ctl({'pos': [4.01, 10.21, -4.6], 'scale': 24.0})),
    sub(section('Actions'), ''.join([
        row('Off hand', binding_ctl(BODY_PRIMARY)),
        row('Primary hand', binding_ctl(BODY_SECONDARY)),
        row('Haptic on entering', haptic('Tick')),
        row('Haptic on an action', haptic('DoubleClick')),
    ])),
    sub(collapse('Icon and sphere', unsaved=0)),
    sphere_head('Head'),
    row('Zone', xform_ctl({'pos': [0.28, 6.74, 4.15], 'rot': [-1.09, -0.07, 3.86], 'scale': 18.0})),
    row('Zone in power armor', same_as_zone()),
])
icon_rows = ''.join([
    sub(collapse('Icon and sphere', open_=True), ''.join([
        row('Show the icon', combo('When inside')),
        row('Icon', file_ctl('activation-icon-flashlight', None, ext='.dds')),
        row('Icon color', color_ctl((255, 255, 255, 200))),
        row('Icon size', slider('2.0', 40)),
        row('Show the sphere', combo('Never')),
        row('Sphere style', combo('Cyan, subtle'), 'is-disabled'),
        row('Sphere size', slider('1.00', 50), 'is-disabled'),
    ])),
])
as_body = f"""<div class="mc-world" style="display:grid;grid-template-columns:772px 772px;gap:16px 24px;align-items:start">
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">A page of spheres: each a section with the sphere&#39;s mark; the ray on its rows lights it in the world</span>
<div class="mc-sheet" style="width:772px">{as_rows}</div>
</div>
<div class="mc-stack" style="gap:16px">
<span class="t-detail mc-caption">Its look, folded by default: the icon and the sphere, each with when it shows</span>
<div class="mc-sheet" style="width:772px">{icon_rows}</div>
<div class="mc-sheet" style="width:772px;padding:0"><div class="mc-footer">{helpline('Zone', 'Where the off hand reaches to take the light from your chest. Its sphere is lit in the world while the ray is here.')}</div></div>
</div>
</div>"""
write('ActivationSphere', page('<!-- @dsCard group="Settings" height=830 width=1620 subtitle="An activation sphere: a section of the mod&#39;s TOML as one group, its zone, actions and haptics, its look folded; lit in the world while the ray is on it" -->', 'Activation sphere', as_body))
print('ok sphere')

# ---------------- Developer: what a config spec's author sees with Developer tools on ----------------
GO = f'<span class="mc-go">{ARROW_R}</span>'
CAUTION_D = ('<svg class="mc-caution" viewBox="0 0 20 18"><path class="tri" d="M10 1.5 L18.5 16.5 H1.5 Z"/>'
             '<rect class="bang" x="9" y="6" width="2" height="6"/><rect class="bang" x="9" y="13" width="2" height="2"/></svg>')
INFO = ('<svg class="mc-done" viewBox="0 0 20 20"><circle class="ring" cx="10" cy="10" r="8.5"/>'
        '<rect class="i" x="9" y="8.5" width="2" height="6.5"/><circle class="i" cx="10" cy="5.8" r="1.3"/></svg>')

def problem_counts(errors=0, warnings=0):
    out = ''
    if errors:
        out += f'{WARN}<span class="mc-red">{errors} error{"s" if errors > 1 else ""}</span>'
    if warnings:
        out += f'{CAUTION_D}<span class="mc-amber">{warnings} warning{"s" if warnings > 1 else ""}</span>'
    return out

def counts(errors=0, warnings=0, live=False, status=None):
    """What a config spec row says in the control column: its live copy, then its errors and its warnings, each by its
    mark and in words; or that it is not loaded, or not available."""
    out = '<span class="mc-tag t-detail">Live copy</span>' if live else ''
    if status == 'not-loaded':
        out += f'{WARN}<span class="mc-red">Not loaded</span>'
    elif status == 'not-available':
        out += '<span class="mc-muted">Not available</span>'
    elif not errors and not warnings:
        out += '<span class="mc-muted">No problems</span>'
    else:
        out += problem_counts(errors, warnings)
    return f'<div class="main"><span class="mc-counts t-body">{out}</span></div>{GO}'

def pkg_row(name, state='', mark=None, **kw):
    return row(name, counts(**kw), state, mark=mark)

def dev_fold(title, open_=True, errors=0, warnings=0, hover=False, mark=None):
    """A heading that folds, as SectionHeader's: folded, it says at the end of its rule the problems it hides."""
    marks = f'<span class="marks t-detail mc-counts">{problem_counts(errors, warnings)}</span>' if not open_ and (errors or warnings) else ''
    mk = f'<span class="mc-mark" style="left:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return (f'<div class="mc-collapse t-body{" is-hover" if hover else ""}">{ARROW_D if open_ else ARROW_R}'
            f'<span>{title}</span><span class="line"></span>{marks}{mk}</div>')

def show_row(label, shown, total, state='', mark=None):
    """The filter of the Developer section, its first row: a stepper that is written at once."""
    return row(f'Show<span class="mc-tag t-detail">{shown} of {total}</span>', choice_stepper(label), state, mark=mark)

def diag(kind, msg, go=True, state='', mark=None):
    """A problem of the config spec, set in under its heading as a row: its message on one line. Its heading says whether
    it is an error or a warning, and where it is, is in the help line. One that names a row is pressed to go to it."""
    mk = f'<span class="mc-mark" style="left:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return (f'<div class="mc-diag{" " + state if state else ""}"><span class="msg t-body">{msg}</span>'
            f'<span class="go">{ARROW_R if go else ""}</span>{mk}</div>')

D_DUP = ('error', 'Two settings have the id beamAngle: the second is left out.')
D_STORE = ('error', 'Flicker has no storage: the row is left out.')
D_FIELD = ('warning', 'Unknown field "visibleWen": it is not read. Did you mean "visibleWhen"?')
D_COND = ('warning', 'The condition of Warm tint names "tint", which no setting has: the row is shown as if it had none.')
D_TOKEN = ('warning', '$IFVR_BeamHelp has no translation and no fallback: it is shown as written.')
D_PARSE = ('error', "The live copy cannot be read: expected ',' or '}'. The menu shows the copy it read last.")
H_DUP = ('Beam angle', 'Two settings have the id beamAngle: the second is left out. Beam › Beam angle · pages[0].content[4]. Goes to the row.')

def footer_help(name, text, **kw):
    return f'<div class="mc-footer">{helpline(name, text, **kw)}</div>'

dv_body = f"""<div class="mc-world mc-stack" style="gap:16px">
<span class="t-detail mc-caption">Mods Config VR's own page: the switch, which config specs the front page lists, and the one config spec it can list alone</span>
{h_sheet([
    ('Off as shipped', row('Developer tools', toggle(False))),
    ('On', row('Developer tools', toggle(True))),
    ('Show', row('Show config specs', choice_stepper('Only one'))),
    ('Config spec', row('Config spec', combo('Immersive Flashlight VR'))),
    ('Not only one', row('Config spec', combo('Immersive Flashlight VR'), 'is-disabled')),
])}
<div class="mc-sheet mc-stack" style="width:1120px;padding:0;gap:0">
{footer_help('Show config specs', 'Which config specs the front page lists: those that need attention (errors, warnings, a live copy, or not loaded), all of them, or only the config spec below.')}
{footer_help('Config spec', 'The config spec the front page lists alone while Show config specs is Only one: the one you are working on, with or without problems.')}
</div>
<span class="t-detail mc-caption">The Developer section's heading and its filter: a stepper that is written at once, as a view's preference</span>
{h_sheet([
    ('Open', dev_fold('Developer')),
    ('Folded', dev_fold('Developer', False, errors=2, warnings=8)),
    ('Needing attention', show_row('Needing attention', 3, 12)),
    ('All', show_row('All', 12, 12)),
    ('Only one', show_row('Immersive Flashlight VR', 1, 12)),
])}
<span class="t-detail mc-caption">A config spec's row: a press opens its report</span>
{h_sheet([
    ('No problems', pkg_row('Mods Config VR')),
    ('Warnings', pkg_row('Showcase', warnings=3)),
    ('Both', pkg_row('Immersive Flashlight VR', errors=2, warnings=5)),
    ('Live copy', pkg_row('Immersive Flashlight VR', errors=2, warnings=5, live=True)),
    ('Under the ray', pkg_row('Immersive Flashlight VR', 'is-hover', mark=(200, 22), errors=2, warnings=5, live=True)),
    ('Not loaded', pkg_row('Virtual Holsters', status='not-loaded')),
    ('Not available', pkg_row('Sim Settlements 2', status='not-available')),
])}
<div class="mc-sheet mc-stack" style="width:1120px;padding:0;gap:0">
{footer_help('Immersive Flashlight VR', 'Read from its live copy. 2 errors: parts of it are left out. 5 warnings. Opens its report.')}
{footer_help('Virtual Holsters', "spec.json cannot be read: line 41, column 7: expected ',' or '}'. Opens its report.", error=True)}
</div>
<span class="t-detail mc-caption">A problem in the report: its message on one line, set in under its heading. Where it is, is in the help line. One that names a row goes to it</span>
<div class="mc-sheet" style="width:908px">
{diag(*D_DUP, state='is-hover', mark=(560, 22))}
{diag(*D_FIELD)}
{diag(*D_COND)}
{diag(*D_PARSE, go=False)}
</div>
<div class="mc-sheet mc-stack" style="width:1120px;padding:0;gap:0">
{footer_help(*H_DUP)}
{footer_help('spec.json', "The live copy cannot be read: expected ',' or '}'. The menu shows the copy it read last. Line 41, column 7.", error=True)}
</div>
</div>"""
write('Developer', page('<!-- @dsCard group="Developer" height=2080 width=1170 subtitle="For a config spec&#39;s author: the switch and the filter, the front page&#39;s Developer section, a config spec&#39;s row, and a problem in its report" -->', 'Developer tools', dv_body))

# ---------------- FullPanelDeveloper: the menu's front page with Developer tools on ----------------
fd_rows = ''.join([
    pagehelp('Settings for the mods you have installed. Pick a mod on the left to see its settings.<br>Version 1.0.0'),
    section('Quick launch'),
    launch_row('FRIK'),
    section('Notices'),
    pagehelp('Sim Settlements 2 is not available: its plugin SS2.esm is not loaded.'),
    dev_fold('Developer'),
    show_row('Needing attention', 3, 12),
    pkg_row('Immersive Flashlight VR', 'is-hover', mark=(200, 22), errors=2, warnings=5, live=True),
    pkg_row('Showcase', warnings=3),
    pkg_row('Virtual Holsters', status='not-loaded'),
])
fd_body = f'''<div class="mc-world">
<div class="mc-panel">
<div class="mc-titlebar"><div class="mc-crumbs t-title"><span class="mc-crumb is-selected">MODS CONFIG VR</span></div>
<span class="mc-btn is-icon t-body">?</span><span class="mc-btn is-icon">{MIN}</span><span class="mc-btn is-icon">{X}</span></div>
<div class="mc-body">
{nav(flat_mods(), '', 11)}
<div class="mc-content">
<div class="mc-scroll"><div class="rows" style="margin-top:-178px">{fd_rows}</div><div class="mc-scrollbar"><i style="top:24%;height:76%"></i></div></div>
</div>
</div>
<div class="mc-footer">{helpline('Immersive Flashlight VR', 'Read from its live copy. 2 errors: parts of it are left out. 5 warnings. Opens its report.')}<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span></div>
</div>
</div>'''
write('FullPanelDeveloper', page('<!-- @dsCard group="Developer" height=768 width=1200 subtitle="The menu&#39;s front page with Developer tools on: a Developer section that folds, its filter, and a row for each config spec it lists" -->', 'Full panel, Developer section', fd_body))

# ---------------- FullPanelConfigSpecReport: a config spec's report, with its live copy ----------------
fr_rows = ''.join([
    pagehelp('Read from the live copy, My Games\\Fallout4VR\\Mods_Config\\ModsConfigVR\\Dev\\ImmersiveFlashlightVR\\spec.json. It is read again each time it is saved.'),
    section('Live copy'),
    row('Read the live copy', toggle(True)),
    row('Reload', run_btn('Reload'), gutter=INFO),
    row('Edit spec.json', run_btn('Open')),
    row('Its folder', run_btn('Show')),
    dev_fold('2 errors'),
    diag(*D_DUP, state='is-hover', mark=(560, 22)),
    diag(*D_STORE),
    dev_fold('5 warnings', False),
])
fr_body = f'''<div class="mc-world">
<div class="mc-panel">
{crumbs([R, ('Developer', ''), ('Immersive Flashlight VR', 'current')], alone=False)}
<div class="mc-body">
{nav(flat_mods(), '', 11)}
<div class="mc-content">
<div class="mc-scroll"><div class="rows" style="margin-top:-84px">{fr_rows}</div><div class="mc-scrollbar"><i style="top:13%;height:87%"></i></div></div>
</div>
</div>
<div class="mc-footer">{helpline(*H_DUP)}<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span></div>
</div>
</div>'''
write('FullPanelConfigSpecReport', page('<!-- @dsCard group="Developer" height=768 width=1200 subtitle="A config spec&#39;s report: its live copy, Reload, the file and its folder, then its errors and its warnings, which fold" -->', 'Full panel, config spec report', fr_body))
print('ok developer')
