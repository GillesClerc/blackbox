#!/usr/bin/env python3
"""Extracteur de netlist depuis des fichiers .kicad_sch (KiCad 9/10).

Pas de kicad-cli dans le container : ce script reconstruit la connectivite
a partir des wires, junctions, global labels et pins de symboles.

Usage : kicad_netlist.py <dir> [--net NOM] [--ref REF] [--check]
"""
import sys, math, re, os, glob
from collections import defaultdict

# ---------------------------------------------------------------- s-expressions

def parse_sexp(text):
    tokens = re.findall(r'"(?:[^"\\]|\\.)*"|\(|\)|[^\s()]+', text)
    stack, cur = [], []
    for t in tokens:
        if t == '(':
            stack.append(cur); cur = []
        elif t == ')':
            done = cur; cur = stack.pop(); cur.append(done)
        elif t.startswith('"'):
            cur.append(t[1:-1].replace('\\"', '"').replace('\\\\', '\\'))
        else:
            cur.append(t)
    return cur

def find_all(node, key):
    return [c for c in node if isinstance(c, list) and c and c[0] == key]

def find_one(node, key):
    r = find_all(node, key)
    return r[0] if r else None

def num(v):
    return float(v)

# ---------------------------------------------------------------- geometrie

Q = 100.0  # quantification 0.01 mm

def q(pt):
    return (round(pt[0] * Q) / Q, round(pt[1] * Q) / Q)

def transform(px, py, sx, sy, angle, mx, my):
    """Pin (px,py) du repere symbole (Y vers le haut) -> coord. feuille (Y vers le bas)."""
    if mx:   # (mirror x) : miroir autour de l'axe X horizontal
        py = -py
    if my:   # (mirror y) : miroir autour de l'axe Y vertical
        px = -px
    a = math.radians(angle)
    ca, sa = math.cos(a), math.sin(a)
    dx = px * ca - py * sa
    dy = px * sa + py * ca
    return (sx + dx, sy - dy)

def on_segment(p, a, b, eps=1e-4):
    """p est-il sur le segment [a,b] (extremites incluses) ?"""
    cross = (b[0]-a[0])*(p[1]-a[1]) - (b[1]-a[1])*(p[0]-a[0])
    if abs(cross) > eps:
        return False
    dot = (p[0]-a[0])*(b[0]-a[0]) + (p[1]-a[1])*(b[1]-a[1])
    if dot < -eps:
        return False
    if dot > (b[0]-a[0])**2 + (b[1]-a[1])**2 + eps:
        return False
    return True

# ---------------------------------------------------------------- union-find

class UF:
    def __init__(self):
        self.p = {}
    def add(self, x):
        self.p.setdefault(x, x)
    def find(self, x):
        self.add(x)
        while self.p[x] != x:
            self.p[x] = self.p[self.p[x]]
            x = self.p[x]
        return x
    def union(self, a, b):
        ra, rb = self.find(a), self.find(b)
        if ra != rb:
            self.p[rb] = ra

# ---------------------------------------------------------------- lecture feuille

class Sheet:
    def __init__(self, path):
        self.path = path
        self.name = os.path.basename(path).replace('.kicad_sch', '')
        root = parse_sexp(open(path, encoding='utf-8').read())[0]
        self.root = root
        self.libpins = self._lib_pins()
        self.symbols = self._symbols()
        self.wires = self._wires()
        self.junctions = self._junctions()
        self.labels = self._labels()

    def _lib_pins(self):
        """lib_id -> {unit: [(number, name, x, y)]}"""
        out = {}
        libs = find_one(self.root, 'lib_symbols') or []
        for sym in find_all(libs, 'symbol'):
            lib_id = sym[1]
            units = defaultdict(list)
            for sub in find_all(sym, 'symbol'):
                m = re.search(r'_(\d+)_(\d+)$', sub[1])
                unit = int(m.group(1)) if m else 0
                for pin in find_all(sub, 'pin'):
                    at = find_one(pin, 'at')
                    number = find_one(pin, 'number')[1]
                    pname = find_one(pin, 'name')[1]
                    units[unit].append((number, pname, num(at[1]), num(at[2])))
            out[lib_id] = units
        return out

    def _symbols(self):
        out = []
        for sym in find_all(self.root, 'symbol'):
            lib_id_n = find_one(sym, 'lib_id')
            if not lib_id_n:
                continue
            lib_id = lib_id_n[1]
            at = find_one(sym, 'at')
            sx, sy, angle = num(at[1]), num(at[2]), num(at[3]) if len(at) > 3 else 0.0
            mir = find_one(sym, 'mirror')
            mx = bool(mir and 'x' in mir[1:])
            my = bool(mir and 'y' in mir[1:])
            unit_n = find_one(sym, 'unit')
            unit = int(unit_n[1]) if unit_n else 1
            props = {}
            for p in find_all(sym, 'property'):
                props[p[1]] = p[2]
            ref = props.get('Reference', '?')
            out.append(dict(lib_id=lib_id, ref=ref, value=props.get('Value', ''),
                            fp=props.get('Footprint', ''), props=props,
                            at=(sx, sy), angle=angle, mx=mx, my=my, unit=unit))
        return out

    def _wires(self):
        out = []
        for w in find_all(self.root, 'wire'):
            pts = find_one(w, 'pts')
            xy = [(num(p[1]), num(p[2])) for p in find_all(pts, 'xy')]
            for i in range(len(xy) - 1):
                out.append((q(xy[i]), q(xy[i+1])))
        return out

    def _junctions(self):
        return [q((num(find_one(j, 'at')[1]), num(find_one(j, 'at')[2])))
                for j in find_all(self.root, 'junction')]

    def _labels(self):
        out = []
        for kind in ('global_label', 'label', 'hierarchical_label'):
            for l in find_all(self.root, kind):
                at = find_one(l, 'at')
                out.append((l[1], q((num(at[1]), num(at[2]))), kind))
        return out

# ---------------------------------------------------------------- pins absolues

POWER_PREFIX = ('power:', 'Power:')

def sheet_pins(sh, angle_sign=1.0, swap=False):
    """[(ref, number, pname, point, is_power, value)]"""
    out = []
    for s in sh.symbols:
        units = sh.libpins.get(s['lib_id'], {})
        pins = list(units.get(s['unit'], [])) + list(units.get(0, []))
        for number, pname, px, py in pins:
            pt = transform(px, py, s['at'][0], s['at'][1],
                           s['angle'] * angle_sign, s['mx'], s['my'])
            out.append((s['ref'], number, pname, q(pt),
                        s['lib_id'].startswith(POWER_PREFIX), s['value']))
    return out

# ---------------------------------------------------------------- netlist

def build(sheets, angle_sign=1.0):
    uf = UF()
    node_labels = defaultdict(set)
    pins_at = defaultdict(list)
    stats = dict(pins=0, connected=0)

    for sh in sheets:
        key = lambda p: (sh.name, p)
        # wires
        for a, b in sh.wires:
            uf.union(key(a), key(b))
        # endpoint d'un wire tombant sur un autre segment -> connecte
        endpoints = set()
        for a, b in sh.wires:
            endpoints.add(a); endpoints.add(b)
        for p in endpoints:
            for a, b in sh.wires:
                if p != a and p != b and on_segment(p, a, b):
                    uf.union(key(p), key(a))
        # junctions : relient tous les wires qui les traversent
        for j in sh.junctions:
            for a, b in sh.wires:
                if on_segment(j, a, b):
                    uf.union(key(j), key(a))
        # labels
        for name, pt, kind in sh.labels:
            uf.union(key(pt), key(pt))
            attached = False
            for a, b in sh.wires:
                if on_segment(pt, a, b):
                    uf.union(key(pt), key(a)); attached = True
            node_labels[uf.find(key(pt))].add(name)
        # pins
        for ref, number, pname, pt, is_power, value in sheet_pins(sh, angle_sign):
            stats['pins'] += 1
            uf.add(key(pt))
            touched = False
            for a, b in sh.wires:
                if on_segment(pt, a, b):
                    uf.union(key(pt), key(a)); touched = True
            for name, lpt, kind in sh.labels:
                if lpt == pt:
                    touched = True
            if touched:
                stats['connected'] += 1
            if is_power:
                node_labels[uf.find(key(pt))].add(value)
            else:
                pins_at[uf.find(key(pt))].append((ref, number, pname, sh.name))

    # fusion inter-feuilles par nom de label global
    by_name = defaultdict(list)
    for root, names in node_labels.items():
        for n in names:
            by_name[n].append(root)
    for n, roots in by_name.items():
        for r in roots[1:]:
            uf.union(roots[0], r)

    nets = defaultdict(list)
    names = defaultdict(set)
    for root, pins in pins_at.items():
        nets[uf.find(root)].extend(pins)
    for root, nms in node_labels.items():
        names[uf.find(root)] |= nms

    out = {}
    anon = 0
    for root, pins in nets.items():
        nm = sorted(names.get(root, []))
        if nm:
            label = '/'.join(nm)
        else:
            anon += 1
            label = f'(no-name-{anon})'
        out.setdefault(label, []).extend(pins)
    # nets nommes sans pin (labels seuls)
    for root, nms in names.items():
        label = '/'.join(sorted(nms))
        out.setdefault(label, [])
    return out, stats

# ---------------------------------------------------------------- main

def main():
    d = sys.argv[1] if len(sys.argv) > 1 else '.'
    files = sorted(glob.glob(os.path.join(d, '*.kicad_sch')))
    sheets = [Sheet(f) for f in files]
    best = None
    for sign in (1.0, -1.0):
        nets, stats = build(sheets, sign)
        rate = stats['connected'] / max(stats['pins'], 1)
        if best is None or rate > best[0]:
            best = (rate, sign, nets, stats)
    rate, sign, nets, stats = best
    args = sys.argv[2:]
    print(f"# feuilles : {', '.join(os.path.basename(f) for f in files)}")
    print(f"# pins : {stats['pins']}, raccordees : {stats['connected']} "
          f"({rate*100:.1f}%), convention angle : {'+' if sign>0 else '-'}")
    print(f"# nets : {len(nets)}")
    if '--net' in args:
        pat = args[args.index('--net') + 1].upper()
        for name in sorted(nets):
            if pat in name.upper():
                pins = nets[name]
                print(f"\n{name} ({len(pins)} pins)")
                for ref, number, pname, sheet in sorted(pins):
                    print(f"   {ref:10s} pin {number:>4s} {pname:<12s} [{sheet}]")
    if '--ref' in args:
        want = args[args.index('--ref') + 1].upper()
        for name in sorted(nets):
            for ref, number, pname, sheet in sorted(nets[name]):
                if ref.upper() == want:
                    print(f"{ref:8s} pin {number:>4s} {pname:<14s} -> {name}   [{sheet}]")
    if '--all' in args:
        for name in sorted(nets):
            pins = nets[name]
            print(f"\n{name} ({len(pins)})")
            for ref, number, pname, sheet in sorted(pins):
                print(f"   {ref:10s} {number:>4s} {pname:<12s} [{sheet}]")

if __name__ == '__main__':
    main()
