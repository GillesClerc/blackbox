#!/usr/bin/env python3
"""Convertit les textures « Uncanny Eyes » du firmware (defaultEye.h, Adafruit, MIT)
en PNG pour le site : le rendu navigateur (web/components/eyes/) reprend le même
algorithme que firmware/components/ui_manager/eyes_anim.c, pixel pour pixel.

  sclera.png  200×200 RGB   blanc de l'œil (RGB565 → RGB888)
  iris.png    256×64  RGB   iris déplié en polaire
  polar.png   80×80   RGB   LUT polaire : R = distance (7 bits), G = angle >> 1, B = angle & 1
  lids.png    128×128 RGB   R = seuil paupière haute, G = seuil paupière basse
                            (variante non symétrique, celle compilée par le firmware)

  python3 tools/eye_assets_web.py
"""
import os
import re
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "firmware/components/ui_manager/data/defaultEye.h")
OUT = os.path.join(ROOT, "web/public/eyes")


def arrays(text):
    """Toutes les déclarations `const uintN_t nom[...] = { ... };`, dans l'ordre."""
    for m in re.finditer(r"const uint(8|16)_t (\w+)\[[^\]]*\][^=]*=\s*\{(.*?)\};", text, re.S):
        body = re.sub(r"//[^\n]*", "", m.group(3))
        yield m.group(2), [int(v, 16) for v in re.findall(r"0[xX][0-9a-fA-F]+", body)]


def rgb565(p):
    r, g, b = (p >> 11) & 0x1F, (p >> 5) & 0x3F, p & 0x1F
    return (r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31


def write_png(path, w, h, pixels):
    raw = b"".join(b"\x00" + bytes(c for px in pixels[y * w:(y + 1) * w] for c in px) for y in range(h))
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    print(f"{os.path.relpath(path, ROOT)}  {w}×{h}  {os.path.getsize(path)} o")


def main():
    found = {}
    for name, vals in arrays(open(SRC).read()):
        found.setdefault(name, []).append(vals)
    sclera, iris, polar = found["sclera"][0], found["iris"][0], found["polar"][0]
    upper, lower = found["upper"][-1], found["lower"][-1]   # #else : paupières non symétriques
    assert len(sclera) == 200 * 200 and len(iris) == 256 * 64 and len(polar) == 80 * 80
    assert len(upper) == len(lower) == 128 * 128
    os.makedirs(OUT, exist_ok=True)
    write_png(os.path.join(OUT, "sclera.png"), 200, 200, [rgb565(p) for p in sclera])
    write_png(os.path.join(OUT, "iris.png"), 256, 64, [rgb565(p) for p in iris])
    write_png(os.path.join(OUT, "polar.png"), 80, 80,
              [(p & 0x7F, (p >> 7) >> 1, (p >> 7) & 1) for p in polar])
    write_png(os.path.join(OUT, "lids.png"), 128, 128, [(u, l, 0) for u, l in zip(upper, lower)])


if __name__ == "__main__":
    main()
