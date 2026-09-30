#!/usr/bin/env python3
"""Exécute du SQL sur la base de STAGING (tsupabase.agill.es) via /pg/query.

Lit web/.env.staging.local (jamais commité) : STAGING_SUPABASE_URL,
STAGING_SERVICE_ROLE_KEY. Refuse toute URL qui n'est pas celle du staging :
la production ne se modifie que par Gilles, dans Studio.

  python3 tools/staging_sql.py -f web/supabase/migrations/0003_inventory.sql
  python3 tools/staging_sql.py -c "select count(*) from public.licenses"
  python3 tools/staging_sql.py -f web/supabase/snapshot-schema.sql --json
  python3 tools/staging_sql.py --gen-types      # régénère web/lib/database.types.ts
"""
import argparse
import json
import os
import sys
import urllib.error
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENV_FILE = os.path.join(ROOT, "web", ".env.staging.local")
STAGING_HOST = "tsupabase.agill.es"


def load_env() -> dict:
    env = {}
    with open(ENV_FILE, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                env[k.strip()] = v.strip()
    return env


def run_sql(sql: str) -> list:
    env = load_env()
    url = env["STAGING_SUPABASE_URL"].rstrip("/")
    if url != f"https://{STAGING_HOST}":
        sys.exit(f"REFUS : {url} n'est pas le staging ({STAGING_HOST})")
    key = env["STAGING_SERVICE_ROLE_KEY"]
    req = urllib.request.Request(
        f"{url}/pg/query",
        data=json.dumps({"query": sql}).encode(),
        headers={"apikey": key, "Authorization": f"Bearer {key}",
                 "Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return json.loads(r.read() or b"[]")
    except urllib.error.HTTPError as e:
        sys.exit(f"ERREUR {e.code} : {e.read().decode('utf-8', 'replace')[:2000]}")


def gen_types() -> None:
    env = load_env()
    url = env["STAGING_SUPABASE_URL"].rstrip("/")
    if url != f"https://{STAGING_HOST}":
        sys.exit(f"REFUS : {url} n'est pas le staging ({STAGING_HOST})")
    key = env["STAGING_SERVICE_ROLE_KEY"]
    req = urllib.request.Request(
        f"{url}/pg/generators/typescript?included_schemas=public",
        headers={"apikey": key, "Authorization": f"Bearer {key}"})
    with urllib.request.urlopen(req, timeout=60) as r:
        body = r.read().decode()
    try:
        body = json.loads(body).get("types", body)
    except ValueError:
        pass
    out = os.path.join(ROOT, "web", "lib", "database.types.ts")
    with open(out, "w", encoding="utf-8") as f:
        f.write("// Généré depuis la base de staging (postgres-meta) : "
                "python3 tools/staging_sql.py --gen-types\n"
                "// Ne pas modifier à la main — régénérer après chaque migration.\n\n")
        f.write(body)
    print(f"écrit : {out} ({len(body)} caractères)")


def main() -> int:
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("-f", "--file")
    g.add_argument("-c", "--command")
    g.add_argument("--gen-types", action="store_true")
    ap.add_argument("--json", action="store_true", help="sortie JSON brute")
    a = ap.parse_args()
    if a.gen_types:
        gen_types()
        return 0
    sql = open(a.file, encoding="utf-8").read() if a.file else a.command
    rows = run_sql(sql)
    if a.json:
        print(json.dumps(rows, ensure_ascii=False))
    else:
        for r in rows if isinstance(rows, list) else [rows]:
            print(r)
    return 0


if __name__ == "__main__":
    sys.exit(main())
