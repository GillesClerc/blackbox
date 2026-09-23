"""Crypto box EscapeBox — source unique, partagée par les outils Python.

Doit rester rigoureusement aligné sur web/lib/box-auth.ts :
  box_secret = HKDF-SHA256(master, salt="", info="escapebox:<box_uid>", 32)
  challenge_response = HMAC-SHA256(box_secret,
                                   "<purpose>:<box_uid>:<challenge>").hex()
  purpose ∈ {"auth", "register"} — séparation de domaine : une preuve
  d'appairage (signée via BLE, canal non authentifié) ne vaut pas auth cloud.

Le master (BOX_MASTER_SECRET) n'est jamais embarqué ni commité.
"""
import hashlib
import hmac

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
    """Secret 32 octets dérivé du master pour cette box."""
    return hkdf_sha256(
        master.encode(), b"", f"escapebox:{box_uid}".encode(), 32
    )


PURPOSES = ("auth", "register")


def box_hmac(master: str, box_uid: str, challenge: str,
             purpose: str = "auth") -> str:
    """Réponse HMAC au challenge, en hex.

    purpose="auth" : ce que la box renvoie à /api/box/auth ;
    purpose="register" : preuve de possession pour /api/box/register.
    """
    if purpose not in PURPOSES:
        raise ValueError(f"purpose invalide : {purpose}")
    secret = box_secret(master, box_uid)
    return hmac.new(
        secret, f"{purpose}:{box_uid}:{challenge}".encode(), hashlib.sha256
    ).hexdigest()


def box_uid_from_mac(mac: bytes) -> str:
    """Forme le box_uid 'ESP32S3-XXXX-XXXX' à partir de la MAC (6 octets).

    Utilise les 4 derniers octets : stable, unique par puce, et lisible.
    """
    if len(mac) != 6:
        raise ValueError("MAC doit faire 6 octets")
    return f"ESP32S3-{mac[2]:02X}{mac[3]:02X}-{mac[4]:02X}{mac[5]:02X}"
