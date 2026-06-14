#!/usr/bin/env python3
"""Provisionne une box EscapeBox : dérive son secret et l'écrit dans NVS.

Le secret n'est jamais embarqué dans le firmware ni commité. Cet outil, exécuté
une fois par box sur le poste de Gilles, le dérive du BOX_MASTER_SECRET serveur
et le pousse dans la partition NVS (namespace "box_creds") que lit hal_box_auth.

  box_uid    = ESP32S3-XXXX-XXXX  (depuis la MAC eFuse, lue par esptool)
  box_secret = HKDF-SHA256(BOX_MASTER_SECRET, "escapebox:<box_uid>", 32)

Usage typique (dry-run, ne touche pas la box) :
  BOX_MASTER_SECRET=<hex> python3 tools/provision_box.py --port /dev/ttyACM0

Pour écrire réellement la NVS de la box :
  BOX_MASTER_SECRET=<hex> python3 tools/provision_box.py --port /dev/ttyACM0 --flash

⚠ --flash réécrit toute la partition nvs (0x9000) : les autres namespaces
  (config volume/luminosité, futurs identifiants WiFi) sont effacés. À faire au
  premier provisioning, avant toute config. Ensuite, enregistre le box_uid
  affiché sur ton compte (POST /api/box/register ou Supabase Studio).
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from box_crypto import box_secret, box_uid_from_mac  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PARTITIONS_CSV = os.path.join(REPO, "firmware", "partitions.csv")
MAC_RE = re.compile(r"([0-9a-fA-F]{2}(?::[0-9a-fA-F]{2}){5})")


def nvs_partition_offset_size() -> tuple[int, int]:
    """Lit l'offset et la taille de la partition nvs depuis partitions.csv."""
    with open(PARTITIONS_CSV) as f:
        for line in f:
            line = line.strip()
            if line.startswith("#") or not line:
                continue
            cols = [c.strip() for c in line.split(",")]
            if len(cols) >= 5 and cols[0] == "nvs":
                return int(cols[3], 0), int(cols[4], 0)
    return 0x9000, 0x6000  # défauts table OTA EscapeBox


def find_nvs_gen() -> str:
    idf = os.environ.get("IDF_PATH", "/opt/esp/idf")
    path = os.path.join(
        idf, "components", "nvs_flash", "nvs_partition_generator",
        "nvs_partition_gen.py",
    )
    if not os.path.exists(path):
        sys.exit(f"ERREUR : nvs_partition_gen.py introuvable ({path}). "
                 "Active l'environnement ESP-IDF (IDF_PATH).")
    return path


def read_mac(port: str) -> bytes:
    out = subprocess.run(
        [sys.executable, "-m", "esptool", "--chip", "esp32s3", "-p", port,
         "read_mac"],
        capture_output=True, text=True,
    )
    if out.returncode != 0:
        sys.exit(f"ERREUR : lecture MAC échouée sur {port}\n{out.stderr}")
    macs = MAC_RE.findall(out.stdout)
    if not macs:
        sys.exit(f"ERREUR : MAC introuvable dans la sortie esptool :\n{out.stdout}")
    return bytes(int(b, 16) for b in macs[0].split(":"))


def make_nvs_bin(box_uid: str, secret: bytes, size: int, out_bin: str) -> None:
    csv = (
        "key,type,encoding,value\n"
        "box_creds,namespace,,\n"
        f"box_uid,data,string,{box_uid}\n"
        f"box_secret,data,hex2bin,{secret.hex()}\n"
    )
    with tempfile.NamedTemporaryFile("w", suffix=".csv", delete=False) as f:
        f.write(csv)
        csv_path = f.name
    try:
        gen = find_nvs_gen()
        r = subprocess.run(
            [sys.executable, gen, "generate", csv_path, out_bin, hex(size)],
            capture_output=True, text=True,
        )
        if r.returncode != 0:
            sys.exit(f"ERREUR : nvs_partition_gen a échoué :\n{r.stdout}\n{r.stderr}")
    finally:
        os.unlink(csv_path)


def flash_nvs(port: str, offset: int, out_bin: str) -> None:
    r = subprocess.run(
        [sys.executable, "-m", "esptool", "--chip", "esp32s3", "-p", port,
         "write_flash", hex(offset), out_bin],
    )
    if r.returncode != 0:
        sys.exit("ERREUR : flash de la partition nvs échoué")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--uid", help="force le box_uid (saute la lecture MAC ; "
                                   "utile en dry-run sans box branchée)")
    ap.add_argument("--flash", action="store_true",
                    help="écrit réellement la NVS (sinon dry-run)")
    ap.add_argument("--show-secret", action="store_true",
                    help="affiche le secret dérivé en clair (sensible)")
    args = ap.parse_args()

    master = os.environ.get("BOX_MASTER_SECRET")
    if not master:
        print("ERREUR : BOX_MASTER_SECRET absent de l'environnement",
              file=sys.stderr)
        return 2

    if args.uid:
        box_uid = args.uid
    else:
        mac = read_mac(args.port)
        box_uid = box_uid_from_mac(mac)
        print(f"MAC lue        : {mac.hex(':')}")

    secret = box_secret(master, box_uid)
    print(f"box_uid        : {box_uid}")
    if args.show_secret:
        print(f"box_secret     : {secret.hex()}")
    else:
        print(f"box_secret     : {secret[:2].hex()}…{secret[-2:].hex()} "
              "(32 octets, --show-secret pour le voir)")

    offset, size = nvs_partition_offset_size()
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        out_bin = f.name
    try:
        make_nvs_bin(box_uid, secret, size, out_bin)
        print(f"image NVS      : {os.path.getsize(out_bin)} octets "
              f"(partition nvs @ {hex(offset)}, taille {hex(size)})")

        if args.flash:
            print("⚠ écriture de la partition nvs (efface les autres namespaces)…")
            flash_nvs(args.port, offset, out_bin)
            print("✓ box provisionnée. Redémarre-la : le log doit afficher "
                  f"« box provisionnée: {box_uid} ».")
            print(f"→ enregistre maintenant {box_uid} sur ton compte "
                  "(POST /api/box/register ou Supabase Studio).")
        else:
            print("\nDRY-RUN : rien n'a été écrit sur la box. "
                  "Relance avec --flash pour provisionner.")
    finally:
        os.unlink(out_bin)
    return 0


if __name__ == "__main__":
    sys.exit(main())
