#!/usr/bin/env python3
"""Cycle complet du back-office par l'API, côté admin ET côté box, sur une instance
(staging) : création d'une histoire jetable, dépôt de versions (zip), publication,
retour arrière, licences, et vérification de ce que reçoit la box à chaque étape.

Variables d'environnement :
  SUPABASE_URL, SUPABASE_KEY         base de l'instance (clé servant d'apikey pour la connexion)
  ADMIN_EMAIL, ADMIN_PASSWORD        compte avec profiles.role = 'admin'
  USER_EMAIL, USER_PASSWORD          compte non admin (doit être refusé)
  BOX_MASTER_SECRET, BOX_UID         box de test existante
  BOX_OWNER_EMAIL                    propriétaire de cette box (reçoit la licence)

  python3 tools/test_admin_e2e.py https://tbox.agill.es
"""
import hashlib
import io
import json
import os
import secrets
import sys
import urllib.error
import urllib.request
import uuid
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from box_crypto import box_hmac  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PKG = os.path.join(ROOT, "web", "scenario-packages", "capitaine_verdier")
BASE = sys.argv[1].rstrip("/")
E = os.environ

fails = 0


def ok(name, cond, extra=""):
    global fails
    print(("PASS" if cond else "FAIL"), name, ("— " + str(extra)) if extra else "")
    fails += 0 if cond else 1


def http(method, url, body=None, headers=None, raw=None):
    h = dict(headers or {})
    data = raw
    if body is not None:
        data = json.dumps(body).encode()
        h["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=data, headers=h, method=method)
    try:
        with urllib.request.urlopen(req, timeout=120) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def login(email, password):
    st, b = http("POST", E["SUPABASE_URL"].rstrip("/") + "/auth/v1/token?grant_type=password",
                 {"email": email, "password": password}, {"apikey": E["SUPABASE_KEY"]})
    assert st == 200, f"connexion {email} : {st} {b[:200]}"
    return json.loads(b)["access_token"]


def api(method, path, token=None, body=None):
    h = {"Authorization": f"Bearer {token}"} if token else {}
    st, b = http(method, BASE + path, body, h)
    try:
        return st, json.loads(b or b"{}")
    except ValueError:
        return st, {"raw": b[:200]}


def upload(token, scenario_id, files, notes):
    zbuf = io.BytesIO()
    with zipfile.ZipFile(zbuf, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in files.items():
            z.writestr(f"histoire/{name}", data)   # dossier racine : doit être retiré
    boundary = "----e2e" + uuid.uuid4().hex
    parts = [
        f"--{boundary}\r\nContent-Disposition: form-data; name=\"notes\"\r\n\r\n{notes}\r\n".encode(),
        (f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; filename=\"h.zip\"\r\n"
         "Content-Type: application/zip\r\n\r\n").encode() + zbuf.getvalue() + b"\r\n",
        f"--{boundary}--\r\n".encode(),
    ]
    st, b = http("POST", f"{BASE}/api/admin/scenarios/{scenario_id}/versions", raw=b"".join(parts),
                 headers={"Authorization": f"Bearer {token}",
                          "Content-Type": f"multipart/form-data; boundary={boundary}"})
    return st, json.loads(b or b"{}")


def box_sync():
    uid, master = E["BOX_UID"], E["BOX_MASTER_SECRET"]
    st, b = http("GET", f"{BASE}/api/box/challenge?box_uid={uid}")
    ch = json.loads(b)["challenge"]
    st, b = http("POST", BASE + "/api/box/auth",
                 {"box_uid": uid, "challenge": ch, "challenge_response": box_hmac(master, uid, ch, "auth")})
    tok = json.loads(b)["token"]
    st, b = http("GET", BASE + "/api/box/sync?firmware_version=0.1.0",
                 headers={"Authorization": f"Bearer {tok}"})
    return tok, {s["slug"]: s for s in json.loads(b)["scenarios"]}


def box_fetch(tok, pkg_path, rel):
    return http("GET", f"{BASE}{pkg_path}/{rel}", headers={"Authorization": f"Bearer {tok}"})


def main():
    admin = login(E["ADMIN_EMAIL"], E["ADMIN_PASSWORD"])
    user = login(E["USER_EMAIL"], E["USER_PASSWORD"])
    slug = "e2e_" + secrets.token_hex(4)
    fiche = {"slug": slug, "title": "Histoire de test E2E", "theme": "test", "ambiance": "automatique",
             "min_players": 2, "max_players": 5, "price_chf": 0}

    # ── Accès ────────────────────────────────────────────────────────────────
    ok("sans compte → 401", api("POST", "/api/admin/scenarios", None, fiche)[0] == 401)
    ok("compte non admin → 403", api("POST", "/api/admin/scenarios", user, fiche)[0] == 403)

    # ── Fiche ────────────────────────────────────────────────────────────────
    st, r = api("POST", "/api/admin/scenarios", admin, {**fiche, "max_players": 1})
    ok("fiche incohérente (joueurs) refusée", st == 400, r.get("errors"))
    st, r = api("POST", "/api/admin/scenarios", admin, fiche)
    ok("création de l'histoire (brouillon)", st == 201, r)
    sid = r["id"]
    ok("identifiant déjà pris → 409", api("POST", "/api/admin/scenarios", admin, fiche)[0] == 409)
    st, r = api("PATCH", f"/api/admin/scenarios/{sid}", admin, {"status": "published"})
    ok("publier l'histoire sans version refusé", st == 400, r.get("errors"))

    # ── Versions ─────────────────────────────────────────────────────────────
    files_v1 = {rel: open(os.path.join(PKG, rel), "rb").read()
                for rel in ("scenario.json", "ambient.mp3", "audio/intro_ambient.mp3")}
    st, r = upload(admin, sid, {"scenario.json": b'{"steps": []}'}, "invalide")
    ok("package invalide refusé avec un message", st == 400 and any("steps" in e for e in r.get("errors", [])), r)
    st, r = upload(admin, sid, files_v1, "v1 e2e")
    ok("dépôt v1", st == 201 and r.get("version") == 1, r if st != 201 else "")
    v1 = r.get("id")
    ref = json.load(open(os.path.join(PKG, "manifest.json")))
    ok("manifest v1 = fichiers de référence (sha256)",
       [(f["path"], f["sha256"]) for f in r["manifest"]["files"]] == [(f["path"], f["sha256"]) for f in ref["files"]])
    ok("publication v1", api("POST", f"/api/admin/versions/{v1}/publish", admin)[0] == 200)
    ok("publication de l'histoire", api("PATCH", f"/api/admin/scenarios/{sid}", admin, {"status": "published"})[0] == 200)

    # ── Licence + box ────────────────────────────────────────────────────────
    st, r = api("POST", f"/api/admin/scenarios/{sid}/licenses", admin, {"email": "inconnu-e2e@agill.es"})
    ok("licence vers un compte inconnu → 404", st == 404)
    tok, sync = box_sync()
    ok("sans licence : histoire absente de la synchro", slug not in sync)
    st, r = api("POST", f"/api/admin/scenarios/{sid}/licenses", admin, {"email": E["BOX_OWNER_EMAIL"], "note": "e2e"})
    ok("licence offerte au propriétaire de la box", st == 201, r)
    lic = r.get("id")
    tok, sync = box_sync()
    ok("la box reçoit l'histoire en v1", sync.get(slug, {}).get("version") == 1, sync.get(slug))
    s = sync[slug]
    st, man = box_fetch(tok, s["package_path"], "manifest.json")
    man = json.loads(man)
    ok("manifest servi depuis Storage (v1)", st == 200 and man["version"] == 1)
    bad = [f["path"] for f in man["files"]
           if hashlib.sha256(box_fetch(tok, s["package_path"], f["path"])[1]).hexdigest() != f["sha256"]]
    ok("tous les fichiers servis sont intègres", not bad, bad)

    # v2 : seul scenario.json change
    scen = json.loads(files_v1["scenario.json"])
    scen["_e2e"] = "v2"
    files_v2 = {**files_v1, "scenario.json": json.dumps(scen).encode()}
    st, r = upload(admin, sid, files_v2, "v2 e2e")
    ok("dépôt v2", st == 201 and r.get("version") == 2, r if st != 201 else "")
    v2 = r.get("id")
    changed = {f["path"] for f in r["manifest"]["files"]} - set()
    m1 = {f["path"]: f["sha256"] for f in man["files"]}
    diff = sorted(p for p, h in ((f["path"], f["sha256"]) for f in r["manifest"]["files"]) if m1.get(p) != h)
    ok("v2 : seul scenario.json diffère (la box ne retéléchargera que lui)", diff == ["scenario.json"], diff)
    tok, sync = box_sync()
    ok("v2 en brouillon : la box reste en v1", sync[slug]["version"] == 1)
    ok("publication v2", api("POST", f"/api/admin/versions/{v2}/publish", admin)[0] == 200)
    tok, sync = box_sync()
    ok("la box reçoit la v2", sync[slug]["version"] == 2)
    st, m = box_fetch(tok, sync[slug]["package_path"], "manifest.json")
    ok("manifest v2 servi", json.loads(m)["version"] == 2)

    # retour arrière
    ok("retour à la v1", api("POST", f"/api/admin/versions/{v1}/publish", admin)[0] == 200)
    tok, sync = box_sync()
    ok("la box repasse en v1", sync[slug]["version"] == 1)

    # révocation + nettoyage
    ok("révocation de la licence", api("POST", f"/api/admin/licenses/{lic}/revoke", admin, {"reason": "e2e"})[0] == 200)
    ok("révocation déjà faite → 404", api("POST", f"/api/admin/licenses/{lic}/revoke", admin)[0] == 404)
    tok, sync = box_sync()
    ok("licence révoquée : histoire absente", slug not in sync)
    ok("archivage de l'histoire de test", api("PATCH", f"/api/admin/scenarios/{sid}", admin, {"status": "archived"})[0] == 200)

    print("TOUT PASSE" if not fails else f"{fails} ÉCHEC(S)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
