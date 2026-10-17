"""Writes components/<Name>/preview.html of every component but Cover, which is written by hand.

The previews are generated: change them here (and the classes in components/bundle.css), not in the HTML.
Usage: python tools/gen_previews.py [project dir]
"""
import os
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

def helpline(name, text, long=False, error=False):
    """The left of the footer while the ray is on a setting: "Name: help". Long help drops to t-detail."""
    cls = 'help ' + ('t-detail is-long' if long else 't-body') + (' is-error' if error else '')
    mark = WARN if error else ''
    return f'<div class="{cls}">{mark}<span class="name">{name}:</span> {text}</div>'

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

def row(name, ctl, state='', gutter='', message='', mark=None):
    msg = f'<small class="t-detail mc-red">{message}</small>' if message else ''
    mk = f'<span class="mc-mark" style="left:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return (f'<div class="mc-row {state}"><span class="gutter">{gutter}</span>'
            f'<span class="name t-body">{name}{msg}</span><div class="ctl t-body">{ctl}</div>{mk}</div>')

def section(title):
    return f'<div class="mc-section t-body">{title}</div>'

def collapse(title, open_=False, hover=False):
    return f'<div class="mc-collapse t-body{" is-hover" if hover else ""}">{ARROW_D if open_ else ARROW_R}<span>{title}</span></div>'

def tree_row(name, kind='mod', state='', arrow=None, mark=None, cat=False):
    a = {'r': ARROW_R, 'd': ARROW_D, None: ''}[arrow]
    cls = 'mc-tree-row t-body' + (' is-page' if kind == 'page' else '') + (' in-cat' if cat else '') + (f' {state}' if state else '')
    arrow_html = f'<span class="arrow">{a}</span>' if kind in ('mod', 'root') else ''
    mk = f'<span class="mc-mark" style="right:{mark[0]}px;top:{mark[1]}px"></span>' if mark else ''
    return f'<div class="{cls}">{arrow_html}<span class="name">{name}</span>{mk}</div>'

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
                pst = 'is-selected' if pg == selected else ''
                if hover and pg == hover[0]: pst, pmk = 'is-hover', hover[1]
                else: pmk = None
                out.append(tree_row(pg, kind='page', state=pst, mark=pmk))
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
write('SettingRow', page('<!-- @dsCard group="Settings" height=660 width=940 subtitle="Idle, hover, focused, active, pending, invalid, disabled, needs a restart" -->', 'Setting row', labeled([
    ('Idle', row('Brightness', slider('1.00', 50))),
    ('Hover', row('Brightness', slider('1.00', 50), 'is-hover', mark=(212, 20))),
    ('Focused', row('Brightness', slider('1.00', 50), 'is-focus')),
    ('Active (drag)', row('Brightness', slider('1.10', 60, 'is-active'), 'is-active')),
    ('Pending', row('Brightness', slider('1.25', 75, 'is-pending', 'is-pending', True), gutter=DOT)),
    ('Invalid', row('Brightness', slider('2.00', 100, 'is-invalid', 'is-invalid', True), 'is-invalid', gutter=WARN, message='Must be 0.50 to 1.50')),
    ('Disabled', row('Beam shadows', toggle(False), 'is-disabled')),
    ('Needs a restart', row('Shadow resolution<span class="mc-tag t-detail">Restart</span>', choice_stepper('2048'))),
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
    ('Out of range', row('Beam angle', slider('140°', 100, 'is-invalid', 'is-invalid', True), 'is-invalid', gutter=WARN, message='Must be 10° to 90°')),
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

# ---------------- SectionHeader ----------------
sh_body = f'''<div class="mc-world"><div class="mc-sheet" style="width:772px">
{section('General')}
{row('Enabled', toggle(True))}
{section('Battery')}
{row('Drain battery', toggle(False))}
{collapse('Advanced')}
{collapse('Advanced', open_=True, hover=True)}
<div style="padding-left:32px">{row('Shadow distance', slider('2048', 50))}</div>
</div></div>'''
write('SectionHeader', page('<!-- @dsCard group="Settings" height=520 width=820 subtitle="SeparatorText headings and the collapsible Advanced section" -->', 'Section header', sh_body))

# ---------------- DescriptionArea: the help line, left of the footer ----------------
def fbar(left, dirty=True):
    b = ('<span class="mc-btn t-body">Discard</span><span class="mc-btn t-body is-primary">Save</span>' if dirty
         else '<span class="mc-btn t-body is-disabled">Discard</span><span class="mc-btn t-body is-primary is-disabled">Save</span>')
    return f'<div class="mc-footer">{left}{b}</div>'
da_body = f'''<div class="mc-world"><div class="mc-sheet mc-stack" style="width:1120px">
<span class="t-detail mc-muted">The ray is on a setting: its help, "Name: help", two lines at t-body</span>
{fbar(helpline('Brightness', "How bright the beam is. 1.00 is the game's own flashlight; above 1.25 the beam starts to wash out close surfaces. Default 1.00."))}
<span class="t-detail mc-muted">Help that needs a third line drops to t-detail; past three lines it ends in an ellipsis</span>
{fbar(helpline('Hide HUD in Pip-Boy', "Hides the compass, health, action points and the ammo counter while the Pip-Boy is raised, and brings them back when it is lowered. Widgets that FallUI HUD does not own, such as quest markers from other mods, keep their own setting. Default off.", long=True))}
<span class="t-detail mc-muted">A validation error comes first</span>
{fbar(helpline('Brightness', "2.00 is out of range. Enter a value from 0.50 to 1.50, or Reset it to 1.00.", error=True))}
<span class="t-detail mc-muted">The ray is off the settings: the session status</span>
<div class="mc-footer"><div class="status t-body"><span class="mc-dot"></span><span class="mc-amber">1 unsaved change</span></div><span class="mc-btn t-body">Discard</span><span class="mc-btn t-body is-primary">Save</span></div>
</div></div>'''
write('DescriptionArea', page('<!-- @dsCard group="Settings" height=520 width=1170 subtitle="Left of the footer: an error, else help for the pointed setting, else the status" -->', 'Help line', da_body))

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
{footer(WARN + '<span class="mc-red">Brightness is invalid. Save shows what to fix.</span>', btn('Discard') + btn('Save'))}
<span class="t-detail mc-muted">An MCM mod: no Save bar</span>
{footer('<span class="mc-muted">Changes are saved as they are made</span>', '')}
</div></div>'''
write('SaveBar', page('<!-- @dsCard group="Actions" height=540 width=1180 subtitle="Clean, pending, blocked, and the status line of an MCM mod" -->', 'Save bar', sb_body))

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
</div>'''
write('ModTree', page('<!-- @dsCard group="Navigation" height=660 width=1110 subtitle="Flat under 30 mods, grouped by category from 30; a bar while the list scrolls" -->', 'Mod tree', mt_body))

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
<div class="item"><div class="t-body" style="display:flex;align-items:center;gap:8px">{DOT}<span>Brightness</span></div>
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
    ('Invalid', row('Press haptic', haptic('none', 'is-invalid', 'is-disabled', True, text='Tik'), 'is-invalid', gutter=WARN, message='Not a haptic pattern')),
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
<div class="item"><div class="t-body" style="display:flex;align-items:center;gap:8px">{DOT}<span>Entry haptic</span></div>
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
