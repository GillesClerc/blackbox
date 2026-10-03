#!/usr/bin/env python3
"""Convertit des textures « Uncanny Eyes » (Adafruit, MIT) en PNG pour le site.
Le rendu navigateur (web/components/eyes/engine.ts) reprend l'algorithme du
firmware (firmware/components/ui_manager/eyes_anim.c) pour l'œil par défaut,
et celui du croquis Adafruit actuel pour les autres personnages.

Sources :
  default  firmware/components/ui_manager/data/defaultEye.h (celui de la box)
  autres   fichiers graphics/<nom>Eye.h du dépôt github.com/adafruit/Uncanny_Eyes,
           passés en argument (téléchargés à part, non versionnés)

Sortie, par œil, dans web/public/eyes/<id>/ :
  sclera.png  W×W RGB   blanc de l'œil (RGB565 → RGB888)
  iris.png    w×h RGB   iris déplié en polaire
  polar.png   N×N RGB   LUT polaire : R = distance (7 bits), G = angle >> 1, B = angle & 1
  lids.png    128² RGB  R = seuil paupière haute, G = seuil basse (variante non symétrique)
et web/components/eyes/catalog.gen.ts (dimensions, plage de pupille).

  python3 tools/eye_assets_web.py [chemin/vers/dragonEye.h ...]
"""
import os
import re
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIRMWARE_EYE = os.path.join(ROOT, "firmware/components/ui_manager/data/defaultEye.h")
OUT = os.path.join(ROOT, "web/public/eyes")
CATALOG = os.path.join(ROOT, "web/components/eyes/catalog.gen.ts")


def arrays(text):
    """Toutes les déclarations `const uintN_t nom[...]... = { ... };`, dans l'ordre."""
    for m in re.finditer(r"const uint(8|16)_t (\w+)((?:\[[^\]]*\])+)[^=]*=\s*\{(.*?)\};", text, re.S):
        body = re.sub(r"//[^\n]*", "", m.group(4))
        yield m.group(2), [int(v, 16) for v in re.findall(r"0[xX][0-9a-fA-F]+", body)]


def define(text, name, default=None):
    m = re.search(rf"#define\s+{name}\s+(\d+)", text)
    return int(m.group(1)) if m else default


def rgb565(p):
    r, g, b = (p >> 11) & 0x1F, (p >> 5) & 0x3F, p & 0x1F
    return (r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31


def write_png(path, w, h, pixels):
    raw = b"".join(b"\x00" + bytes(c for px in pixels[y * w:(y + 1) * w] for c in px) for y in range(h))
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    return os.path.getsize(path)


def convert(eye_id, src, legacy):
    text = open(src).read()
    found = {}
    for name, vals in arrays(text):
        found.setdefault(name, []).append(vals)
    sw, iw = define(text, "SCLERA_WIDTH"), define(text, "IRIS_WIDTH")
    mw, mh = define(text, "IRIS_MAP_WIDTH"), define(text, "IRIS_MAP_HEIGHT")
    sclera, iris, polar = found["sclera"][0], found["iris"][0], found["polar"][0]
    upper, lower = found["upper"][-1], found["lower"][-1]   # #else : paupières non symétriques
    assert len(sclera) == sw * sw and len(iris) == mw * mh and len(polar) == iw * iw, eye_id
    assert len(upper) == len(lower) == 128 * 128, eye_id
    d = os.path.join(OUT, eye_id)
    os.makedirs(d, exist_ok=True)
    size = write_png(os.path.join(d, "sclera.png"), sw, sw, [rgb565(p) for p in sclera])
    size += write_png(os.path.join(d, "iris.png"), mw, mh, [rgb565(p) for p in iris])
    size += write_png(os.path.join(d, "polar.png"), iw, iw, [(p & 0x7F, (p >> 7) >> 1, (p >> 7) & 1) for p in polar])
    size += write_png(os.path.join(d, "lids.png"), 128, 128, [(u, l, 0) for u, l in zip(upper, lower)])
    # Plage de pupille : le firmware (formule historique) borne à 90–130 ;
    # le croquis Adafruit actuel à 120–720 sauf surcharge dans le fichier de l'œil.
    lo, hi = (90, 130) if legacy else (define(text, "IRIS_MIN", 120), define(text, "IRIS_MAX", 720))
    print(f"{eye_id:12s} sclère {sw}² iris {mw}×{mh} polaire {iw}² pupille {lo}–{hi}  {size // 1024} Ko")
    return {"id": eye_id, "sclera": sw, "iris": iw, "mapW": mw, "mapH": mh, "irisMin": lo, "irisMax": hi,
            "legacy": legacy}


def main():
    eyes = [convert("default", FIRMWARE_EYE, True)]
    for src in sys.argv[1:]:
        eye_id = os.path.basename(src).replace("Eye.h", "").lower()
        eyes.append(convert(eye_id, src, False))
    lines = ["// Généré par tools/eye_assets_web.py — ne pas modifier à la main.",
             "export const EYE_CATALOG = {"]
    for e in eyes:
        lines.append(f"  {e['id']}: {{ sclera: {e['sclera']}, iris: {e['iris']}, mapW: {e['mapW']}, mapH: {e['mapH']}, "
                     f"irisMin: {e['irisMin']}, irisMax: {e['irisMax']}, legacy: {str(e['legacy']).lower()} }},")
    lines += ["} as const;", "export type EyeId = keyof typeof EYE_CATALOG;", ""]
    open(CATALOG, "w").write("\n".join(lines))


if __name__ == "__main__":
    main()
