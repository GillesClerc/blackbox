#!/usr/bin/env python3
"""Parcours complet d'une box simulée contre une instance web :
challenge → auth → sync → manifest → un fichier (sha256), plus les refus (404/403/401).

  BOX_MASTER_SECRET=<secret de l'instance> python3 tools/test_box_e2e.py \
      https://tbox.agill.es ESP32S3-TEST-0001 capitaine_verdier   # histoires attendues
  ... ESP32S3-TEST-0001 -                                          # aucune attendue

La box doit exister en base et appartenir à un compte (voir docs/plans/staging-coolify.md).
"""
import json, os, sys, urllib.request, urllib.error, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from box_crypto import box_hmac  # noqa

base, uid = sys.argv[1].rstrip("/"), sys.argv[2]
expected = [] if len(sys.argv) < 4 or sys.argv[3] == "-" else sys.argv[3].split(",")
master = os.environ["BOX_MASTER_SECRET"]

def call(method, path, body=None, token=None):
    h = {"Content-Type": "application/json"}
    if token: h["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(base + path, data=json.dumps(body).encode() if body else None, headers=h, method=method)
    try:
        with urllib.request.urlopen(req, timeout=30) as r: return r.status, r.read()
    except urllib.error.HTTPError as e: return e.code, e.read()

fails = 0
def ok(name, cond, extra=""):
    global fails
    print(("PASS" if cond else "FAIL"), name, ("— " + extra) if extra else "")
    fails += 0 if cond else 1

st, b = call("GET", f"/api/box/challenge?box_uid={uid}"); ch = json.loads(b)["challenge"]
st, b = call("POST", "/api/box/auth", {"box_uid": uid, "challenge": ch, "challenge_response": box_hmac(master, uid, ch, "auth")})
ok("auth → JWT", st == 200, str(st)); tok = json.loads(b).get("token")
st, b = call("GET", "/api/box/sync?firmware_version=0.1.0", token=tok)
sync = json.loads(b); slugs = [s["slug"] for s in sync.get("scenarios", [])]
ok("sync 200", st == 200)
ok("histoires synchronisées", slugs == expected, f"{slugs} (attendu {expected})")
for s in sync.get("scenarios", []):
    ok(f"{s['slug']} : champs firmware présents", all(k in s for k in ("slug", "package_path", "version")), json.dumps(s))
    st, b = call("GET", s["package_path"] + "/manifest.json", token=tok)
    ok(f"{s['slug']} : manifest 200", st == 200, str(st))
    if st == 200:
        man = json.loads(b)
        ok(f"{s['slug']} : version manifest = version sync", man.get("version") == s["version"], f"{man.get('version')} / {s['version']}")
        f0 = sorted(man["files"], key=lambda f: f["bytes"])[0]
        st, data = call("GET", s["package_path"] + "/" + f0["path"], token=tok)
        ok(f"{s['slug']} : fichier {f0['path']} intègre", st == 200 and hashlib.sha256(data).hexdigest() == f0["sha256"])
# accès à un slug non licencié / inconnu
st, _ = call("GET", "/api/box/pkg/histoire_inconnue/manifest.json", token=tok)
ok("slug inconnu → 404", st == 404, str(st))
if not expected:
    st, _ = call("GET", "/api/box/pkg/capitaine_verdier/manifest.json", token=tok)
    ok("histoire publiée sans licence → 403", st == 403, str(st))
st, _ = call("GET", "/api/box/sync", token="invalide")
ok("sync sans JWT valide → 401", st == 401, str(st))
print("TOUT PASSE" if not fails else f"{fails} ÉCHEC(S)")
sys.exit(1 if fails else 0)
