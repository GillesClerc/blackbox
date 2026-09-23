#!/usr/bin/env python3
"""Provisionne une box EscapeBox : dérive son secret et l'écrit dans NVS.

Le secret n'est jamais embarqué dans le firmware ni commité. Cet outil, exécuté
une fois par box sur le poste de Gilles, le dérive du BOX_MASTER_SECRET serveur
et le pousse dans la partition NVS dédiée "box_nvs" (namespace "box_creds")
que lit hal_box_auth.

  box_uid    = ESP32S3-XXXX-XXXX  (depuis la MAC eFuse, lue par esptool)
  box_secret = HKDF-SHA256(BOX_MASTER_SECRET, "escapebox:<box_uid>", 32)

Usage typique (dry-run, ne touche pas la box) :
  BOX_MASTER_SECRET=<hex> python3 tools/provision_box.py --port /dev/ttyACM0

Pour écrire réellement l'identité de la box :
  BOX_MASTER_SECRET=<hex> python3 tools/provision_box.py --port /dev/ttyACM0 --flash

--flash n'écrit QUE la partition box_nvs : la NVS applicative (volume,
scénario actif, WiFi) est préservée. Prérequis : la table de partitions
flashée sur la box contient box_nvs (flash complet une fois, cf. CLAUDE.md).
Ensuite, appaire la box depuis /devices/add (preuve de possession BLE).

--wifi-ssid/--wifi-pass (optionnel, dev) : écrit en plus une image de la NVS
applicative ne contenant que wifi_creds — ⚠ efface volume/scénario actif.
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


def partition_offset_size(name: str) -> tuple[int, int]:
    """Lit l'offset et la taille d'une partition depuis partitions.csv."""
    with open(PARTITIONS_CSV) as f:
        for line in f:
            line = line.strip()
            if line.startswith("#") or not line:
                continue
            cols = [c.strip() for c in line.split(",")]
            if len(cols) >= 5 and cols[0] == name:
                return int(cols[3], 0), int(cols[4], 0)
    sys.exit(f"ERREUR : partition '{name}' absente de {PARTITIONS_CSV}")


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


def gen_nvs_bin(csv: str, size: int, out_bin: str) -> None:
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


def creds_csv(box_uid: str, secret: bytes) -> str:
    """Image box_nvs : identité de la box (namespace box_creds)."""
    return (
        "key,type,encoding,value\n"
        "box_creds,namespace,,\n"
        f"box_uid,data,string,{box_uid}\n"
        f"box_secret,data,hex2bin,{secret.hex()}\n"
    )


def wifi_csv(wifi_ssid: str, wifi_pass: str | None) -> str:
    """Image NVS applicative : identifiants WiFi lus par hal_wifi."""
    csv = (
        "key,type,encoding,value\n"
        "wifi_creds,namespace,,\n"
        f"ssid,data,string,{wifi_ssid}\n"
    )
    if wifi_pass:
        csv += f"pass,data,string,{wifi_pass}\n"
    return csv


def flash_nvs(port: str, offset: int, out_bin: str) -> None:
    r = subprocess.run(
        [sys.executable, "-m", "esptool", "--chip", "esp32s3", "-p", port,
         "write_flash", hex(offset), out_bin],
    )
    if r.returncode != 0:
        sys.exit(f"ERREUR : flash de la partition @ {hex(offset)} échoué")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--uid", help="force le box_uid (saute la lecture MAC ; "
                                   "utile en dry-run sans box branchée)")
    ap.add_argument("--flash", action="store_true",
                    help="écrit réellement la NVS (sinon dry-run)")
    ap.add_argument("--show-secret", action="store_true",
                    help="affiche le secret dérivé en clair (sensible)")
    ap.add_argument("--wifi-ssid", help="SSID WiFi à écrire dans la NVS "
                                        "(namespace wifi_creds, lu par hal_wifi)")
    ap.add_argument("--wifi-pass", help="mot de passe WiFi (omis = réseau ouvert)")
    args = ap.parse_args()

    if args.wifi_pass and not args.wifi_ssid:
        print("ERREUR : --wifi-pass nécessite --wifi-ssid", file=sys.stderr)
        return 2

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

    box_off, box_size = partition_offset_size("box_nvs")
    images: list[tuple[str, int, str]] = []  # (libellé, offset, fichier)
    try:
        with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
            creds_bin = f.name
        gen_nvs_bin(creds_csv(box_uid, secret), box_size, creds_bin)
        images.append(("box_nvs", box_off, creds_bin))
        print(f"image box_nvs  : {os.path.getsize(creds_bin)} octets "
              f"(@ {hex(box_off)}, taille {hex(box_size)})")

        if args.wifi_ssid:
            nvs_off, nvs_size = partition_offset_size("nvs")
            with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
                wifi_bin = f.name
            gen_nvs_bin(wifi_csv(args.wifi_ssid, args.wifi_pass), nvs_size,
                        wifi_bin)
            images.append(("nvs (wifi_creds)", nvs_off, wifi_bin))
            print(f"WiFi SSID      : {args.wifi_ssid}"
                  f"{' (réseau ouvert)' if not args.wifi_pass else ''}"
                  f" → nvs @ {hex(nvs_off)}")

        if args.flash:
            for label, off, path in images:
                if label.startswith("nvs"):
                    print("⚠ écriture de la NVS applicative (efface volume, "
                          "scénario actif…)")
                print(f"écriture {label} @ {hex(off)}…")
                flash_nvs(args.port, off, path)
            print("✓ box provisionnée. Redémarre-la : le log doit afficher "
                  f"« box provisionnée: {box_uid} ».")
            print("  (si « table de partitions sans box_nvs » : faire le flash "
                  "complet du CLAUDE.md puis relancer cet outil)")
            print(f"→ appaire maintenant {box_uid} depuis /devices/add.")
        else:
            print("\nDRY-RUN : rien n'a été écrit sur la box. "
                  "Relance avec --flash pour provisionner.")
    finally:
        for _, _, path in images:
            os.unlink(path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
