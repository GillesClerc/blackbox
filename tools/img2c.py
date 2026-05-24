#!/usr/bin/env python3
"""
Convertit une image en tableau C RGB565 pour l'ILI9488 (320x480).

Usage:
    python3 tools/img2c.py image.jpg
    python3 tools/img2c.py image.jpg firmware/main/test_image.h

L'image est recadrée (crop centré) puis redimensionnée à 320x480.
La sortie est un .h avec const uint16_t img_<nom>[153600].

Taille flash : 320x480x2 = 307 200 bytes (~300 KB).
"""

import sys
from pathlib import Path

try:
    from PIL import Image
    import numpy as np
except ImportError:
    print("Installer les dépendances : pip install Pillow numpy")
    sys.exit(1)

W, H = 320, 480


def center_crop(img):
    src_ratio = img.width / img.height
    dst_ratio = W / H
    if src_ratio > dst_ratio:
        new_w = int(img.height * dst_ratio)
        off = (img.width - new_w) // 2
        img = img.crop((off, 0, off + new_w, img.height))
    else:
        new_h = int(img.width / dst_ratio)
        off = (img.height - new_h) // 2
        img = img.crop((0, off, img.width, off + new_h))
    return img


def convert(input_path, output_path=None):
    img = Image.open(input_path).convert('RGB')
    img = center_crop(img)
    img = img.resize((W, H), Image.LANCZOS)

    px = np.array(img, dtype=np.uint16)
    r = px[:, :, 0] >> 3   # 5 bits
    g = px[:, :, 1] >> 2   # 6 bits
    b = px[:, :, 2] >> 3   # 5 bits
    rgb565 = ((r << 11) | (g << 5) | b).flatten()

    stem = Path(input_path).stem.replace('-', '_').replace(' ', '_')
    if output_path is None:
        output_path = Path(input_path).parent / f"{stem}.h"

    with open(output_path, 'w') as f:
        f.write(f"// Généré par img2c.py — {W}x{H} RGB565\n")
        f.write(f"// Source : {Path(input_path).name}\n")
        f.write(f"#pragma once\n#include <stdint.h>\n\n")
        f.write(f"#define IMG_{stem.upper()}_W  {W}\n")
        f.write(f"#define IMG_{stem.upper()}_H  {H}\n\n")
        f.write(f"static const uint16_t img_{stem}[{W * H}] = {{\n")
        for i in range(0, len(rgb565), 12):
            row = rgb565[i:i + 12]
            f.write("    " + ", ".join(f"0x{v:04X}" for v in row) + ",\n")
        f.write("};\n")

    size_kb = W * H * 2 / 1024
    print(f"OK  {W}x{H}  →  {output_path}  ({size_kb:.0f} KB flash)")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    out = sys.argv[2] if len(sys.argv) > 2 else None
    convert(sys.argv[1], out)
