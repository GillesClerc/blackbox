#!/usr/bin/env python3
"""Package un scénario pour la livraison cloud EscapeBox.

Copie les fichiers d'un dossier source vers <out>/<slug>/ et génère
manifest.json (version, total_bytes, sha256 + taille par fichier).
Le manifest ne doit JAMAIS être écrit à la main : c'est le contrat
d'intégrité que le firmware vérifie fichier par fichier.

Usage:
  python3 tools/package_scenario.py <src_dir> --slug capitaine_verdier \
      [--version N] [--out web/scenario-packages]

Sans --version : version du manifest de destination existant + 1 (ou 1).

Règles (alignées sur le firmware, cloud_client.c) :
  - chemins relatifs, profondeur <= 2 sous-dossiers, charset [A-Za-z0-9._-],
    aucun segment commençant par '.'
  - scenario.json obligatoire à la racine du package
  - MP3 : 44100 Hz attendu (contrainte du mixer F4) — warning sinon
"""
import argparse
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

SEGMENT_RE = re.compile(r"^[A-Za-z0-9_-][A-Za-z0-9._-]*$")
SLUG_RE = re.compile(r"^[a-z0-9_-]{1,64}$")
MAX_FILE_BYTES = 32 * 1024 * 1024
MAX_FILES = 128

# Index sample-rate MPEG1 Layer III (datasheet du format, suffisant ici)
MP3_RATES_V1 = {0: 44100, 1: 48000, 2: 32000}


def mp3_sample_rate(path: Path) -> int | None:
    """Sample rate du premier frame MPEG trouvé (None si introuvable)."""
    data = path.read_bytes()
    pos = 0
    if data[:3] == b"ID3":  # saute le tag ID3v2 (taille syncsafe)
        size = (data[6] << 21) | (data[7] << 14) | (data[8] << 7) | data[9]
        pos = 10 + size
    for i in range(pos, min(len(data) - 4, pos + 65536)):
        if data[i] == 0xFF and (data[i + 1] & 0xE0) == 0xE0:
            version = (data[i + 1] >> 3) & 0x03  # 3 = MPEG1
            rate_idx = (data[i + 2] >> 2) & 0x03
            if rate_idx == 3:
                continue
            rate = MP3_RATES_V1.get(rate_idx)
            if rate is None:
                continue
            if version == 2:  # MPEG2 : rates / 2
                rate //= 2
            elif version == 0:  # MPEG2.5 : rates / 4
                rate //= 4
            return rate
    return None


def rel_path_ok(rel: str) -> bool:
    parts = rel.split("/")
    if len(parts) > 3:  # profondeur <= 2 sous-dossiers
        return False
    return all(SEGMENT_RE.match(p) for p in parts)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("src", type=Path, help="dossier source du scénario")
    ap.add_argument("--slug", required=True)
    ap.add_argument("--version", type=int, default=None)
    ap.add_argument("--out", type=Path, default=Path("web/scenario-packages"))
    args = ap.parse_args()

    if not SLUG_RE.match(args.slug):
        print(f"slug invalide : {args.slug}", file=sys.stderr)
        return 1
    if not (args.src / "scenario.json").is_file():
        print(f"scenario.json manquant dans {args.src}", file=sys.stderr)
        return 1

    dest = args.out / args.slug
    version = args.version
    if version is None:
        version = 1
        old = dest / "manifest.json"
        if old.is_file():
            version = json.loads(old.read_text())["version"] + 1

    files = sorted(
        p for p in args.src.rglob("*")
        if p.is_file() and p.name != "manifest.json"
    )
    if len(files) > MAX_FILES:
        print(f"trop de fichiers ({len(files)} > {MAX_FILES})", file=sys.stderr)
        return 1

    entries = []
    for p in files:
        rel = p.relative_to(args.src).as_posix()
        if not rel_path_ok(rel):
            print(f"chemin refusé : {rel}", file=sys.stderr)
            return 1
        size = p.stat().st_size
        if size > MAX_FILE_BYTES:
            print(f"fichier trop gros ({size} o) : {rel}", file=sys.stderr)
            return 1
        if p.suffix.lower() == ".mp3":
            rate = mp3_sample_rate(p)
            if rate != 44100:
                print(f"⚠ {rel} : {rate or '?'} Hz (44100 attendu — mixer F4)")
        entries.append({
            "path": rel,
            "bytes": size,
            "sha256": hashlib.sha256(p.read_bytes()).hexdigest(),
        })

    dest.mkdir(parents=True, exist_ok=True)
    for p, e in zip(files, entries):
        target = dest / e["path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(p, target)

    manifest = {
        "slug": args.slug,
        "version": version,
        "total_bytes": sum(e["bytes"] for e in entries),
        "files": entries,
    }
    (dest / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n"
    )
    print(f"package {args.slug} v{version} : {len(entries)} fichier(s), "
          f"{manifest['total_bytes']} octets → {dest}")
    print("→ penser à bumper scenarios.version en DB au déploiement")
    return 0


if __name__ == "__main__":
    sys.exit(main())
