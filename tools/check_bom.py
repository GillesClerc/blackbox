#!/usr/bin/env python3
"""Confronte la BOM CSV au schema KiCad et signale les ecarts.

La BOM a derive du schema par le passe (elle attribuait J10 a la batterie alors
que le schema utilise J2), d'ou ce controle systematique. Il verifie :

  - toute reference du schema est presente dans la BOM (et l'inverse, en
    ignorant les cartes autres que Main) ;
  - la quantite declaree correspond au nombre de references du groupe ;
  - la valeur et l'empreinte de la BOM correspondent a celles du schema ;
  - les champs LCSC manquants sont listes (dette connue).

Usage : tools/check_bom.py [hardware/main] [BOM.csv]
"""
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kicad_netlist import Sheet  # noqa: E402

MULT = {'p': 1e-12, 'n': 1e-9, 'u': 1e-6, 'm': 1e-3, '': 1, 'k': 1e3, 'M': 1e6, 'meg': 1e6}


def norm_value(txt):
    """« 4u7 », « 4.7uF 10V ceramique », « 1k5 » -> une valeur numerique comparable.

    Le schema utilise la notation europeenne (4u7), la BOM une description
    litterale : sans normalisation, tout diverge pour rien.
    """
    t = txt.strip().replace('ohm', '').replace('Ohm', '')
    m = re.match(r'^(\d+)([pnumkM])(\d+)', t)          # 4u7, 1k5, 2n2
    if m:
        return float(f'{m.group(1)}.{m.group(3)}') * MULT[m.group(2)]
    m = re.match(r'^(\d+(?:\.\d+)?)\s*([pnumkM]|meg)?[FH]?\b', t)   # 4.7uF, 100nF, 10k
    if m:
        return float(m.group(1)) * MULT.get(m.group(2) or '', 1)
    return None


SHEETS = ('power', 'esp32', 'audio', 'connector', 'imu')
# lignes de la BOM qui ne concernent pas la carte Main
OTHER_BOARDS = ('face avant', 'satellite', 'cote 2', 'cote 1', 'cote 3',
                'dessus', 'devant', 'batterie', 'boitier')


def load_schematic(d):
    """ref -> (valeur, empreinte)"""
    out = {}
    for f in SHEETS:
        for s in Sheet(os.path.join(d, f + '.kicad_sch')).symbols:
            if s['lib_id'].startswith('power:'):
                continue
            out[s['ref']] = (s['value'], s['fp'])
    return out


def load_bom(path):
    """ref -> (ligne, valeur, empreinte, lcsc, qty, board)"""
    out, rows = {}, []
    with open(path, encoding='utf-8') as fh:
        for i, row in enumerate(csv.reader(fh), 1):
            if not row or row[0] == 'Designator':
                continue
            rows.append((i, row))
            refs = [r for r in re.split(r'[,\s]+', row[0].strip('"')) if r and r != '—']
            for r in refs:
                out[r] = dict(line=i, value=row[2] if len(row) > 2 else '',
                              fp=row[3] if len(row) > 3 else '',
                              lcsc=row[4] if len(row) > 4 else '',
                              qty=row[5] if len(row) > 5 else '',
                              board=row[6] if len(row) > 6 else '',
                              nrefs=len(refs))
    return out, rows


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else 'hardware/main'
    bom_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(d, 'BOM', '02-bom-lcsc.csv')
    sch = load_schematic(d)
    bom, rows = load_bom(bom_path)
    problems = 0

    missing = sorted(r for r in sch if r not in bom)
    if missing:
        problems += len(missing)
        print(f'❌ {len(missing)} references du schema absentes de la BOM :')
        for r in missing:
            print(f'     {r:14s} {sch[r][0]!r} {sch[r][1]}')

    extra = sorted(r for r in bom if r not in sch
                   and not any(b in bom[r]['board'].lower() for b in OTHER_BOARDS))
    if extra:
        print(f'\n⚠  {len(extra)} references de la BOM (carte Main) absentes du schema :')
        for r in extra:
            print(f'     {r:14s} ligne {bom[r]["line"]}, board={bom[r]["board"]!r}')

    # quantites
    bad_qty = []
    for i, row in rows:
        refs = [r for r in re.split(r'[,\s]+', row[0].strip('"')) if r and r != '—']
        if not refs or len(row) < 6 or not row[5].strip().isdigit():
            continue
        if int(row[5]) != len(refs):
            bad_qty.append((i, row[0], len(refs), row[5]))
    if bad_qty:
        problems += len(bad_qty)
        print(f'\n❌ {len(bad_qty)} quantites incoherentes :')
        for i, des, n, q in bad_qty:
            print(f'     ligne {i}: {des!r} -> {n} references mais Qty={q}')

    # valeurs et empreintes
    diff_val, diff_fp = [], []
    for r, (val, fp) in sorted(sch.items()):
        if r not in bom:
            continue
        b = bom[r]
        if (b['nrefs'] == 1 and b['value'].strip() and val.strip()
                and not r.startswith('J')):     # les connecteurs portent une description
            a, c = norm_value(b['value']), norm_value(val)
            # comparaison relative : 0.1u et 100nF donnent des flottants differents
            if a and c and abs(a - c) > 1e-9 * max(a, c):
                diff_val.append((r, val, b['value']))
        # la BOM abrege les empreintes (« JST-SH-4P » vs le chemin KiCad complet) :
        # on ne compare que la taille de boitier, seul indice fiable des deux cotes
        size = re.compile(r'\b(0201|0402|0603|0805|1206|1210|2010|2512|'
                          r'SOT-?23-?\d?|SOT-?89|SOIC-?8|SO-?8|SMA|SMB|TSSOP-?\d+)\b', re.I)
        a = {m.group(1).lower().replace('-', '') for m in size.finditer(fp)}
        c = {m.group(1).lower().replace('-', '') for m in size.finditer(b['fp'])}
        if a and c and not (a & c):
            diff_fp.append((r, fp, b['fp']))
    if diff_val:
        print(f'\n⚠  {len(diff_val)} valeurs divergentes (schema vs BOM) :')
        for r, a, b in diff_val:
            print(f'     {r:14s} schema={a!r}  bom={b!r}')
    if diff_fp:
        print(f'\n⚠  {len(diff_fp)} empreintes divergentes (schema vs BOM) :')
        for r, a, b in diff_fp[:20]:
            print(f'     {r:14s} schema={a}  bom={b!r}')

    no_lcsc = sorted(r for r in sch if r in bom and not bom[r]['lcsc'].strip('— ').strip())
    print(f'\nℹ  {len(no_lcsc)} references sans champ LCSC (dette connue) :')
    print('     ' + ', '.join(no_lcsc[:30]) + (' ...' if len(no_lcsc) > 30 else ''))

    print(f'\n{len(sch)} symboles au schema, {len(bom)} references en BOM.')
    print('RESULTAT :', 'OK' if problems == 0 else f'{problems} probleme(s) bloquant(s)')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
