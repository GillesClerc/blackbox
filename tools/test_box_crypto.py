#!/usr/bin/env python3
"""Vecteurs de référence de la crypto box (hors ligne, sans secret réel).

Le vecteur attendu a été produit par web/lib/box-auth.ts (Node) : ce test
détecte toute dérive entre box_crypto.py et le serveur. Le firmware
(hal_box_auth.c) signe le même message "<purpose>:<box_uid>:<challenge>".

  python3 tools/test_box_crypto.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from box_crypto import box_hmac, box_uid_from_mac  # noqa: E402

MASTER = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
UID = "ESP32S3-8FF7-D684"
CHALLENGE = "deadbeef" * 8
EXPECTED_AUTH = "590412840de9d81a82914e06be8b24c5fa08c65cd74e5f1d0c68d11bf90fe07d"


def main() -> int:
    checks = [
        ("vecteur auth == serveur Node",
         box_hmac(MASTER, UID, CHALLENGE, "auth") == EXPECTED_AUTH),
        ("register != auth (séparation de domaine)",
         box_hmac(MASTER, UID, CHALLENGE, "register") != EXPECTED_AUTH),
        ("box_uid depuis MAC",
         box_uid_from_mac(bytes.fromhex("a4cb8ff7d684")) == UID),
    ]
    try:
        box_hmac(MASTER, UID, CHALLENGE, "autre")
        checks.append(("purpose inconnu refusé", False))
    except ValueError:
        checks.append(("purpose inconnu refusé", True))

    for name, ok in checks:
        print(("PASS" if ok else "FAIL"), name)
    return 0 if all(ok for _, ok in checks) else 1


if __name__ == "__main__":
    sys.exit(main())
