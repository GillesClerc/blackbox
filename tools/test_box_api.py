#!/usr/bin/env python3
"""Simule une box EscapeBox contre l'API web (challenge -> auth -> sync).

Reproduit la crypto du firmware sans rien embarquer : le secret par box est
dérivé du master serveur via HKDF-SHA256, exactement comme web/lib/box-auth.ts.

  box_secret = HKDF-SHA256(master, salt="", info="escapebox:<box_uid>", 32)
  challenge_response = HMAC-SHA256(box_secret, "<box_uid>:<challenge>").hex()

Usage:
  BOX_MASTER_SECRET=<hex> python3 tools/test_box_api.py \
      --base https://box.agill.es \
      --box-uid ESP32S3-TEST-0001 \
      [--firmware 1.0.0]

Le master n'est JAMAIS commité : passe-le par variable d'environnement.
"""
import argparse
import hashlib
import hmac
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request

HASH_LEN = 32  # SHA-256


def hkdf_sha256(ikm: bytes, salt: bytes, info: bytes, length: int) -> bytes:
    """RFC 5869, conforme à Node crypto.hkdfSync('sha256', ...)."""
    if not salt:
        salt = b"\x00" * HASH_LEN
    prk = hmac.new(salt, ikm, hashlib.sha256).digest()  # Extract
    okm = b""
    t = b""
    counter = 1
    while len(okm) < length:
        t = hmac.new(prk, t + info + bytes([counter]), hashlib.sha256).digest()
        okm += t
        counter += 1
    return okm[:length]


def box_secret(master: str, box_uid: str) -> bytes:
    return hkdf_sha256(
        master.encode(), b"", f"escapebox:{box_uid}".encode(), 32
    )


def box_hmac(master: str, box_uid: str, challenge: str) -> str:
    secret = box_secret(master, box_uid)
    return hmac.new(
        secret, f"{box_uid}:{challenge}".encode(), hashlib.sha256
    ).hexdigest()


def http_json(method: str, url: str, body: dict | None = None,
              headers: dict | None = None) -> tuple[int, dict]:
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Content-Type", "application/json")
    for k, v in (headers or {}).items():
        req.add_header(k, v)
    try:
        with urllib.request.urlopen(req) as resp:
            return resp.status, json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read().decode() or "{}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default="http://localhost:3000")
    ap.add_argument("--box-uid", required=True)
    ap.add_argument("--firmware", default="1.0.0",
                    help="version firmware rapportée au sync")
    args = ap.parse_args()

    master = os.environ.get("BOX_MASTER_SECRET")
    if not master:
        print("ERREUR : BOX_MASTER_SECRET absent de l'environnement",
              file=sys.stderr)
        return 2

    base = args.base.rstrip("/")
    uid = args.box_uid

    # 1) GET challenge
    q = urllib.parse.urlencode({"box_uid": uid})
    status, body = http_json("GET", f"{base}/api/box/challenge?{q}")
    print(f"[challenge] {status} {body}")
    if status != 200 or "challenge" not in body:
        return 1
    challenge = body["challenge"]

    # 2) POST auth (preuve HMAC)
    resp = box_hmac(master, uid, challenge)
    status, body = http_json("POST", f"{base}/api/box/auth", {
        "box_uid": uid,
        "challenge": challenge,
        "challenge_response": resp,
    })
    print(f"[auth] {status} {body}")
    if status != 200 or "token" not in body:
        return 1
    token = body["token"]

    # 3) GET sync (Bearer JWT box)
    q = urllib.parse.urlencode({"firmware_version": args.firmware})
    status, body = http_json(
        "GET", f"{base}/api/box/sync?{q}",
        headers={"Authorization": f"Bearer {token}"},
    )
    print(f"[sync] {status} {json.dumps(body, indent=2, ensure_ascii=False)}")
    return 0 if status == 200 else 1


if __name__ == "__main__":
    sys.exit(main())
