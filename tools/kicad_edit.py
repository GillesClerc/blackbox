#!/usr/bin/env python3
"""Helpers d'edition de fichiers .kicad_sch (KiCad 9/10).

Pas de kicad-cli dans le container : ces fonctions ecrivent directement les
s-expressions. Toute modification doit etre relue avec tools/kicad_netlist.py,
qui reconstruit la connectivite reelle — c'est la seule verification possible
ici (le rendu visuel, lui, se controle en ouvrant KiCad).

Conventions du projet :
- une pin de symbole est placee par son point de connexion (at x y angle) ;
- les feuilles communiquent par global labels, jamais par labels hierarchiques ;
- la grille est 1,27 mm ; les pins de Device:R, Device:C et Device:FerriteBead
  sont a +-3,81 mm de leur centre (donc interchangeables en place).
"""
import re
import uuid

ROOT_UUID = '49e0257f-ff47-4890-a1ac-89438fa62044'
SHEETS = {
    'esp32': '14cfff57-6a31-4c05-af76-83111480b025',
    'audio': '17c39c9a-96dc-41e6-b1fb-7a30f21efb24',
    'connector': '51b01d3e-87dc-456c-a96b-c77ebbca5dc9',
    'power': '85a32c7c-168b-4c08-8a60-f4e86c1b8173',
    'imu': 'a8900e0e-0e6a-45dd-8142-149b90a970ba',
}
PIN_OFFSET = 3.81      # Device:R / C / FerriteBead


def u():
    return str(uuid.uuid4())


def _prop(name, value, x, y, angle=0, hide=False):
    h = '\n\t\t\t(hide yes)' if hide else ''
    return f'''		(property "{name}" "{value}"
			(at {x} {y} {angle}){h}
			(show_name no)
			(do_not_autoplace no)
			(effects
				(font
					(size 1.27 1.27)
				)
			)
		)'''


def symbol(lib_id, ref, value, footprint, at, angle=0, sheet='audio',
           extra_props=(), description='', dnp=False, pins=2, mirror=None,
           pin_names=None):
    """pins : nombre de broches numerotees 1..n, ou pin_names pour des numeros
    explicites (ex. ('1','2','3') ou ('G','D','S'))."""
    x, y = at
    mir = f'\n\t\t(mirror {mirror})' if mirror else ''
    props = [
        _prop('Reference', ref, x + 2.54, y - 1.27, angle),
        _prop('Value', value, x + 2.54, y + 1.27, angle),
        _prop('Footprint', footprint, x, y, angle, hide=True),
        _prop('Datasheet', '', x, y, 0, hide=True),
        _prop('Description', description, x, y, 0, hide=True),
    ]
    for n, v in extra_props:
        props.append(_prop(n, v, x, y, 0, hide=True))
    numbers = pin_names if pin_names else [str(i) for i in range(1, pins + 1)]
    pin_blk = '\n'.join(f'''		(pin "{n}"
			(uuid "{u()}")
		)''' for n in numbers)
    return f'''	(symbol
		(lib_id "{lib_id}")
		(at {x} {y} {angle}){mir}
		(unit 1)
		(body_style 1)
		(exclude_from_sim no)
		(in_bom yes)
		(on_board yes)
		(in_pos_files yes)
		(dnp {"yes" if dnp else "no"})
		(uuid "{u()}")
{chr(10).join(props)}
{pin_blk}
		(instances
			(project "blackbox"
				(path "/{ROOT_UUID}/{SHEETS[sheet]}"
					(reference "{ref}")
					(unit 1)
				)
			)
		)
	)
'''


def wire(p1, p2):
    return f'''	(wire
		(pts
			(xy {p1[0]} {p1[1]}) (xy {p2[0]} {p2[1]})
		)
		(stroke
			(width 0)
			(type default)
		)
		(uuid "{u()}")
	)
'''


def global_label(name, at, angle=0, shape='input'):
    x, y = at
    just = 'right' if angle == 180 else 'left'
    dx = -11.43 if angle == 180 else 11.43
    return f'''	(global_label "{name}"
		(shape {shape})
		(at {x} {y} {angle})
		(fields_autoplaced yes)
		(effects
			(font
				(size 1.27 1.27)
			)
			(justify {just})
		)
		(uuid "{u()}")
		(property "Intersheetrefs" "${{INTERSHEET_REFS}}"
			(at {x + dx} {y} 0)
			(hide yes)
			(show_name no)
			(do_not_autoplace no)
			(effects
				(font
					(size 1.27 1.27)
				)
				(justify {just})
			)
		)
	)
'''


def label(name, at, angle=0):
    x, y = at
    return f'''	(label "{name}"
		(at {x} {y} {angle})
		(fields_autoplaced yes)
		(effects
			(font
				(size 1.27 1.27)
			)
			(justify left bottom)
		)
		(uuid "{u()}")
	)
'''


def junction(at):
    return f'''	(junction
		(at {at[0]} {at[1]})
		(diameter 0)
		(color 0 0 0 0)
		(uuid "{u()}")
	)
'''


# ----------------------------------------------------------------- fichier

def read(path):
    return open(path, encoding='utf-8').read()


def write(path, txt):
    open(path, 'w', encoding='utf-8').write(txt)


def ensure_lib_symbol(txt, lib_id, source_path):
    """Copie la definition d'un lib_symbol depuis une autre feuille si absente."""
    if f'(symbol "{lib_id}"' in txt:
        return txt
    src = read(source_path)
    i = src.find(f'(symbol "{lib_id}"')
    if i < 0:
        raise SystemExit(f'{lib_id} introuvable dans {source_path}')
    start = src.rfind('\t\t', 0, i)
    j = src.find('\n\t\t(symbol "', i)
    k = src.find('\n\t)\n', i)
    end = min(x for x in (j, k) if x > 0)
    blk = src[start:end] + '\n'
    m = re.search(r'\n\t\)\n', txt)          # fermeture de lib_symbols
    return txt[:m.start()] + '\n' + blk.rstrip('\n') + txt[m.start():]


def append(txt, *chunks):
    """Insere des elements avant la parenthese finale du fichier."""
    cut = txt.rstrip()
    assert cut.endswith(')')
    cut = cut[:cut.rfind(')')]
    return cut + ''.join(chunks) + ')\n'


def symbol_block(txt, ref):
    """(debut, fin) du bloc symbole portant cette reference."""
    m = re.search(r'\(property "Reference" "' + re.escape(ref) + r'"', txt)
    if not m:
        raise SystemExit(f'reference {ref} introuvable')
    start = txt.rfind('\n\t(symbol\n', 0, m.start())
    end = txt.find('\n\t(symbol\n', m.start())
    if end < 0:
        end = txt.rfind('\n)')
    return start, end


def retarget(txt, ref, new_ref=None, lib_id=None, value=None, footprint=None):
    """Change en place le lib_id / la valeur / l'empreinte / la reference d'un symbole.

    Sert au remplacement d'un composant par un autre de meme geometrie de pins
    (ferrite -> resistance) : les fils et la connectivite ne bougent pas.
    """
    start, end = symbol_block(txt, ref)
    blk = txt[start:end]
    if lib_id:
        blk = re.sub(r'\(lib_id "[^"]+"\)', f'(lib_id "{lib_id}")', blk, count=1)
    if value is not None:
        blk = re.sub(r'\(property "Value" "[^"]*"', f'(property "Value" "{value}"',
                     blk, count=1)
    if footprint is not None:
        blk = re.sub(r'\(property "Footprint" "[^"]*"',
                     f'(property "Footprint" "{footprint}"', blk, count=1)
    if new_ref:
        blk = blk.replace(f'(property "Reference" "{ref}"',
                          f'(property "Reference" "{new_ref}"')
        blk = blk.replace(f'(reference "{ref}")', f'(reference "{new_ref}")')
    return txt[:start] + blk + txt[end:]


def drop_wire(txt, p1, p2):
    """Supprime le fil reliant exactement ces deux points (dans un sens ou l'autre)."""
    for a, b in ((p1, p2), (p2, p1)):
        pat = (r'\t\(wire\s*\(pts\s*\(xy ' + f'{a[0]} {a[1]}' + r'\)\s*\(xy '
               + f'{b[0]} {b[1]}' + r'\)[\s\S]*?\n\t\)\n')
        new, n = re.subn(pat, '', txt, count=1)
        if n:
            return new
    raise SystemExit(f'fil {p1}-{p2} introuvable')
