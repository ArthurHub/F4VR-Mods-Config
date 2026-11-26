"""Writes src/Theme.h: the menu's colors, sizes and font sizes, and the framework's theme they make, from the design
system.

The values come from project/tokens.json. designTheme() sets every field of the framework's imgui::Theme from a
token, by THEME_FIELDS below; the fields are read from the framework's ImGuiTheme.h, so a field the table lacks, or
one it names that the framework has not, stops the generator. The framework sets ImGui's style from the theme; the
tables of the "ImGui style" section of project/README.md say which token each ImGuiStyle field and ImGuiCol_ gets,
and a value written in the first table that differs from its token stops the generator, as does an unknown token.

The panel draws in the theme that is set, read as look() and colors() (src/ui/Drawing.h), so devbench's theme action
tunes all of it: a file of src/ other than Theme.h that names the constant of a token the theme has a field for stops
the generator too, with where it does. The tokens of the mod's own layout, which no theme sets, stay constants.

Usage: python tools/gen_theme.py           write src/Theme.h
       python tools/gen_theme.py --check   only report whether it is out of date (exit code 1)
"""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent / 'project'
ROOT = HERE.parents[3]  # tools, design-system, design, docs
OUT = ROOT / 'src' / 'Theme.h'
FRAMEWORK_THEME = ROOT / 'external' / 'F4VR-CommonFramework' / 'src' / 'imgui' / 'ImGuiTheme.h'

# Each field of imgui::Theme and the token it is set from: a color token for a color, a size or a type style for a
# float, and ('units', token) for a size in vrui units, the token's px over the 48 px of a vrui unit.
THEME_FIELDS = {
    'accent': 'green',
    'muted': 'green-muted',
    'track': 'track',
    'hover': 'hover',
    'pressed': 'pressed',
    'glow': 'glow',
    'rule': 'rule',
    'frameBorder': 'frame-border',
    'onAccent': 'on-green',
    'warning': 'amber',
    'onWarning': 'on-amber',
    'error': 'red',
    'onError': 'on-red',
    'popup': 'popup',
    'scrim': 'scrim',
    'glass': 'glass',
    'plate': 'plate',
    'important': 'accent',
    'controlHeight': 'control-height',
    'frameRounding': 'radius-frame',
    'frameBorderSize': 'border-frame',
    'popupRounding': 'radius-popup',
    'titleFontSize': 't-title',
    'bodyFontSize': 't-body',
    'detailFontSize': 't-detail',
    'scrollbarSize': 'scrollbar-size',
    'grabWidth': 'grab-width',
    'grabRounding': 'radius-grab',
    'disabledOpacity': 'opacity-disabled',
    'space1': 'space-1',
    'space2': 'space-2',
    'space3': 'space-3',
    'space4': 'space-4',
    'space6': 'space-6',
    'space8': 'space-8',
    'space12': 'space-12',
    'panelBorderUnits': ('units', 'border-panel'),
    'panelCornerUnits': ('units', 'radius-panel'),
    'panelPaddingUnits': ('units', 'panel-padding'),
}

# the size of each type of a field of imgui::Theme: render::Color is four floats
FIELD_SIZES = {'render::Color': 16, 'float': 4}

# ImGuiStyle fields that are an ImVec2; every other field in the table is a float
VEC2_FIELDS = {'WindowPadding', 'FramePadding', 'ItemSpacing', 'ItemInnerSpacing', 'TouchExtraPadding',
               'SeparatorTextAlign', 'SeparatorTextPadding', 'SelectableTextAlign', 'ButtonTextAlign',
               'WindowTitleAlign', 'WindowMinSize', 'CellPadding', 'DisplaySafeAreaPadding'}


def fail(msg):
    sys.exit(f'gen_theme: {msg}')


def const_name(token):
    return token.upper().replace('-', '_')


def flt(v):
    s = f'{v:.7g}'
    if '.' not in s and 'e' not in s:
        s += '.0'
    return s + 'f'


def summary(usage, limit=150):
    """The first sentence of a token's usage, for the comment beside its constant."""
    text = usage.split('. ')[0].rstrip('.')
    if len(text) > limit:
        text = text[:limit].rsplit(' ', 1)[0] + '...'
    return text


def ticked(cell):
    return re.findall(r'`([^`]+)`', cell)


def numbers(cell):
    return [float(n) for n in re.findall(r'-?\d+(?:\.\d+)?', cell)]


def tables(section):
    """The rows of each Markdown table in the section, as lists of cells, without the header and the rule."""
    out, cur = [], []
    for line in section.splitlines() + ['']:
        if line.startswith('|'):
            cur.append([c.strip() for c in line.strip().strip('|').split('|')])
        elif cur:
            out.append(cur[2:])
            cur = []
    return out


# ---- tokens
tokens = json.loads((PROJECT / 'tokens.json').read_text(encoding='utf-8'))
theme_id = tokens['color']['themes'][0]['id']
colors = {}
for t in tokens['color']['tokens']:
    v = t['value'][theme_id] if isinstance(t['value'], dict) else t['value']
    h = v.lstrip('#')
    rgba = [int(h[i:i + 2], 16) for i in (0, 2, 4)] + [int(h[6:8], 16) if len(h) == 8 else 255]
    colors[t['name']] = (rgba, t.get('usage', ''))
sizes = {}
for fam in ('spacing', 'radius', 'size', 'border', 'opacity'):
    for t in tokens[fam]['tokens']:
        sizes[t['name']] = (float(str(t['value']).removesuffix('px')), t.get('usage', ''), fam)
styles = [s for g in tokens['type']['groups'] for s in g['styles']]

# ---- the mapping, from the README
readme = (PROJECT / 'README.md').read_text(encoding='utf-8')
if '\n## ImGui style\n' not in readme:
    fail('project/README.md has no "## ImGui style" section')
section = readme.split('\n## ImGui style\n', 1)[1].split('\n## ', 1)[0]
style_rows, color_rows = (tables(section) + [[], []])[:2]
if not style_rows or not color_rows:
    fail('the "ImGui style" section needs its two tables: ImGuiStyle fields, then ImGuiCol_ entries')

assignments = []  # (field, C++ expression)
for cells in style_rows:
    fields, written, toks = ticked(cells[0]), numbers(cells[1]), ticked(cells[2]) if len(cells) > 2 else []
    for t in toks:
        if t not in sizes:
            fail(f'unknown token `{t}` for {", ".join(fields)}')
    if toks:
        # the value column is written once for all the fields of the row, or once for each
        per_field = [[sizes[t][0] for t in ((toks * 2)[:2] if f in VEC2_FIELDS else toks)] for f in fields]
        if written and written != per_field[0] and written != sum(per_field, []):
            fail(f'README writes {cells[1]} for {", ".join(fields)}, but {", ".join(toks)} gives {per_field[0]}')
        for f in fields:
            if f in VEC2_FIELDS:
                x, y = (toks * 2)[:2]
                assignments.append((f, f'ImVec2({const_name(x)}, {const_name(y)})'))
            else:
                if len(toks) != 1:
                    fail(f'{f} is a float but has {len(toks)} tokens')
                assignments.append((f, const_name(toks[0])))
    else:
        pairs = re.findall(r'\(([^)]*)\)', cells[1])
        if pairs:
            if len(pairs) != len(fields):
                fail(f'{", ".join(fields)}: {len(fields)} fields, {len(pairs)} values')
            for f, p in zip(fields, pairs):
                x, y = numbers(p)
                assignments.append((f, f'ImVec2({flt(x)}, {flt(y)})'))
        elif len(written) == 1:
            for f in fields:
                assignments.append((f, flt(written[0])))
        else:
            fail(f'no token or value for {", ".join(fields)}')

for cells in color_rows:
    fields, what = ticked(cells[0]), cells[1]
    if not what.lower().startswith('transparent'):
        toks = ticked(what)
        if not toks or toks[0] not in colors:
            fail(f'no known color token for {", ".join(fields)}: {what}')

# ---- the framework's theme
if not FRAMEWORK_THEME.exists():
    fail(f'{FRAMEWORK_THEME} is missing: update the framework submodule')
framework = FRAMEWORK_THEME.read_text(encoding='utf-8')
body = re.search(r'struct Theme\s*\{(.*?)\n    \};', framework, re.S)
if not body:
    fail(f'no struct Theme in {FRAMEWORK_THEME}')
theme_fields = re.findall(r'^\s*(render::Color|float)\s+(\w+)\s*=', body.group(1), re.M)
if not theme_fields:
    fail(f'no fields read from struct Theme in {FRAMEWORK_THEME}')
missing = [name for _, name in theme_fields if name not in THEME_FIELDS]
if missing:
    fail(f'imgui::Theme has fields with no token in THEME_FIELDS: {", ".join(missing)}')
unknown = sorted(set(THEME_FIELDS) - {name for _, name in theme_fields})
if unknown:
    fail(f'THEME_FIELDS names fields imgui::Theme does not have: {", ".join(unknown)}')
font_tokens = {s['name']: 'FONT_SIZE_' + s['name'].removeprefix('t-').upper().replace('-', '_') for s in styles}
theme_assignments = []  # (field, C++ expression)
for kind, name in theme_fields:
    source = THEME_FIELDS[name]
    if kind == 'render::Color':
        if not isinstance(source, str) or source not in colors:
            fail(f'{name} is a color, but `{source}` is no color token')
        theme_assignments.append((name, f'toColor(color::{const_name(source)})'))
    elif isinstance(source, tuple):
        if source[0] != 'units' or source[1] not in sizes:
            fail(f'{name}: `{source}` is not (\'units\', a size token)')
        theme_assignments.append((name, f'{const_name(source[1])} / 48.0f'))
    elif source in sizes:
        theme_assignments.append((name, const_name(source)))
    elif source in font_tokens:
        theme_assignments.append((name, font_tokens[source]))
    else:
        fail(f'{name} is a float, but `{source}` is no size token or type style')
theme_size = sum(FIELD_SIZES[kind] for kind, _ in theme_fields)

# ---- the header
L = []
w = L.append
w('// Generated from the design system in docs/design/design-system by docs/design/design-system/tools/gen_theme.py. Do not edit it:')
w('// change project/tokens.json, or the tables of the "ImGui style" section of project/README.md, and run the generator.')
w('// pre-commit runs it on every commit that changes the design system.')
w('#pragma once')
w('')
w('#include <imgui.h>')
w('')
w('#include "imgui/ImGuiTheme.h"')
w('')
w('// clang-format off')
w('namespace mods_config::theme')
w('{')
w('    // Colors, as ImU32 for ImDrawList. ImGui::ColorConvertU32ToFloat4 makes the ImVec4 an ImGuiStyle takes.')
w('    namespace color')
w('    {')
width = max(len(const_name(n)) for n in colors)
for name, (rgba, usage) in colors.items():
    w(f'        constexpr ImU32 {const_name(name):<{width}} = IM_COL32({rgba[0]:>3}, {rgba[1]:>3}, {rgba[2]:>3}, {rgba[3]:>3}); // {summary(usage)}')
w('    }')
heads = {'spacing': 'Spacing, in the panel\'s layout px: 48 px is one vrui unit.', 'radius': 'Corner radii, in layout px.',
         'size': 'Sizes, in layout px.', 'border': 'Border widths, in layout px.', 'opacity': 'Opacity.'}
width = max(len(const_name(n)) for n in sizes)
fam_prev = None
for name, (v, usage, fam) in sizes.items():
    if fam != fam_prev:
        w('')
        w(f'    // {heads[fam]}')
        fam_prev = fam
    w(f'    constexpr float {const_name(name):<{width}} = {flt(v) + ";":<9} // {summary(usage)}')
w('')
w('    // Font sizes for ImGui::PushFont(nullptr, size) around the call, and PopFont after it. t-body is the canvas font\'s.')
for s in styles:
    n = font_tokens[s['name']]
    px = float(s['lineHeight'].removesuffix('px'))
    w(f'    constexpr float {n:<17} = {flt(px) + ";":<11} // {px:g} px')
w('')
w('    /**')
w('     * A color token as the framework\'s color.')
w('     */')
w('    constexpr f4cf::render::Color toColor(const ImU32 color)')
w('    {')
w('        return f4cf::render::Color::rgba(static_cast<int>((color >> IM_COL32_R_SHIFT) & 0xFF), static_cast<int>((color >> IM_COL32_G_SHIFT) & 0xFF),')
w('            static_cast<int>((color >> IM_COL32_B_SHIFT) & 0xFF), static_cast<int>((color >> IM_COL32_A_SHIFT) & 0xFF));')
w('    }')
w('')
w('    /**')
w('     * The design system as the framework\'s theme, every field from its token: the controls, ImGui\'s style, the')
w('     * keyboard and the panel\'s edge are drawn in it. A panel\'s sizes in vrui units are their px over the 48 of a unit.')
w('     */')
w('    inline f4cf::imgui::Theme designTheme()')
w('    {')
w('        return {')
width = max(len(f) for f, _ in theme_assignments)
for f, expr in theme_assignments:
    w(f'            .{f:<{width}} = {expr},')
w('        };')
w('    }')
w('')
w('    // A framework whose Theme has another field than the generator read stops the build here, so the mod never takes')
w('    // a default for it: run the generator.')
w(f'    static_assert(sizeof(f4cf::imgui::Theme) == {theme_size}, "imgui::Theme has changed: run docs/design/design-system/tools/gen_theme.py");')
w('}')
w('')  # clang-format puts a blank line here
w('// clang-format on')
text = '\r\n'.join(L) + '\r\n'  # CRLF, as .clang-format has it for src/

old = OUT.read_bytes().decode('utf-8') if OUT.exists() else None
if '--check' in sys.argv[1:]:
    if old != text:
        sys.exit(f'gen_theme: {OUT} is out of date: run python docs/design/design-system/tools/gen_theme.py')
elif old != text:
    OUT.write_bytes(text.encode('utf-8'))
    print(f'wrote {OUT}')

# ---- the drawing reads the theme, not the constants of its fields
field_constants = set()
for name, source in THEME_FIELDS.items():
    if isinstance(source, tuple):
        continue  # a panel's edge, corner and padding in vrui units: the panel's style reads them
    if source in colors:
        field_constants.add('color::' + const_name(source))
    elif source in sizes:
        field_constants.add(const_name(source))
    else:
        field_constants.add(font_tokens[source])
# read of fields too: a disabled row's color is the accent at the disabled opacity, a row's pitch a control's height
# and space-2
field_constants |= {'color::DISABLED', 'ROW_PITCH'}
reads = []
for path in sorted((ROOT / 'src').rglob('*')):
    if path.suffix not in ('.h', '.cpp') or path == OUT:
        continue
    for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        for name in re.findall(r'\btheme::((?:color::)?[A-Z][A-Z0-9_]*)\b', line):
            if name in field_constants:
                reads.append(f'{path.relative_to(ROOT).as_posix()}:{number}: theme::{name}')
if reads:
    fail('the drawing reads the theme as look() and colors() (src/ui/Drawing.h), not these constants of its fields:\n  '
         + '\n  '.join(reads))
