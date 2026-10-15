"""Writes src/Theme.h: the menu's colors, sizes, font scales and ImGui style, from the design system.

The values come from project/tokens.json. Which ImGuiStyle field and which ImGuiCol_ takes which token is read
from the tables in the "ImGui style" section of project/README.md, so the document and the code cannot disagree:
a value written in the first table that differs from its token stops the generator, as does an unknown token.

Usage: python tools/gen_theme.py           write src/Theme.h
       python tools/gen_theme.py --check   only report whether it is out of date (exit code 1)
"""
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent / 'project'
OUT = HERE.parents[3] / 'src' / 'Theme.h'  # tools, design-system, design, docs

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
base = next(float(s['lineHeight'].removesuffix('px')) for s in styles if s['name'] == 't-body')

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

color_assignments = []  # (ImGuiCol_ name, token or None for transparent)
for cells in color_rows:
    fields, what = ticked(cells[0]), cells[1]
    if what.lower().startswith('transparent'):
        tok = None
    else:
        toks = ticked(what)
        if not toks or toks[0] not in colors:
            fail(f'no known color token for {", ".join(fields)}: {what}')
        tok = toks[0]
    color_assignments += [(f, tok) for f in fields]

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
w('    // The size the canvas font is loaded at, for imgui::setFontSizePixels: t-body.')
w(f'    constexpr float FONT_SIZE         = {flt(base) + ";":<11} // {base:g} px')
w('')
w(f'    // Font scales for ImGui::SetWindowFontScale, from the {base:g} px font. Reset to 1 after the call.')
for s in styles:
    n = 'FONT_SCALE_' + s['name'].removeprefix('t-').upper().replace('-', '_')
    px = float(s['lineHeight'].removesuffix('px'))
    w(f'    constexpr float {n:<17} = {flt(px / base) + ";":<11} // {px:g} px')
w('')
w('    /**')
w('     * Sets the menu\'s sizes and colors on an ImGui style. Call it once, before the first canvas draws.')
w('     */')
w('    inline void applyStyle(ImGuiStyle& style)')
w('    {')
for f, expr in assignments:
    w(f'        style.{f} = {expr};')
w('')
for f, tok in color_assignments:
    val = 'ImVec4(0.0f, 0.0f, 0.0f, 0.0f)' if tok is None else f'ImGui::ColorConvertU32ToFloat4(color::{const_name(tok)})'
    w(f'        style.Colors[ImGuiCol_{f}] = {val};')
w('    }')
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
