// Rejoue toutes les migrations dans un vrai PostgreSQL (PGlite, WebAssembly), avec
// des données façon prod insérées après 0001, puis vérifie la reprise et la RLS vue
// d'un client. Filet de sécurité avant de passer une migration en staging puis en prod.
import { readFileSync } from "node:fs";
import path from "node:path";
import { PGlite } from "@electric-sql/pglite";
import { beforeAll, describe, expect, it } from "vitest";

const MIG = path.join(__dirname, "migrations");
const U1 = "11111111-1111-1111-1111-111111111111"; // compte avec une box
const U2 = "22222222-2222-2222-2222-222222222222"; // autre compte

let db: PGlite;
const rows = async <T = Record<string, unknown>>(sql: string) =>
  (await db.query<T>(sql)).rows;

// Exécute une requête avec le rôle d'un client Supabase (anon / authenticated).
async function asClient(role: "anon" | "authenticated", uid: string | null, sql: string) {
  await db.exec(`set role ${role}; select set_config('request.jwt.claim.sub', '${uid ?? ""}', false);`);
  try {
    return { rows: (await db.query(sql)).rows as Record<string, unknown>[], error: null };
  } catch (e) {
    return { rows: null, error: (e as Error).message };
  } finally {
    await db.exec("reset role;");
  }
}

beforeAll(async () => {
  db = new PGlite();
  // Bouchons de ce que fournit Supabase : schéma auth, auth.uid(), rôles, droits par défaut.
  await db.exec(`
    create role anon nologin; create role authenticated nologin; create role service_role nologin bypassrls;
    create schema auth;
    create table auth.users (id uuid primary key, email text);
    create function auth.uid() returns uuid language sql stable as
      $$ select nullif(current_setting('request.jwt.claim.sub', true), '')::uuid $$;
    grant usage on schema public, auth to anon, authenticated, service_role;
    grant execute on function auth.uid() to anon, authenticated;
    alter default privileges in schema public grant all on tables to anon, authenticated, service_role;
  `);
  await db.exec(readFileSync(path.join(MIG, "0001_baseline.sql"), "utf8"));
  // État de la prod au 2026-09-30 : une histoire active packagée, une inactive, un
  // brouillon sans package, une box rattachée et une box orpheline.
  await db.exec(`
    insert into auth.users values ('${U1}','u1@test.ch'), ('${U2}','u2@test.ch');
    insert into public.scenarios (slug, title, version, package_path, active) values
      ('capitaine_verdier','Capitaine Verdier',4,'/api/box/pkg/capitaine_verdier',true),
      ('vieux_test','Ancien test',2,'/api/box/pkg/vieux_test',false),
      ('sans_package','Brouillon',1,null,true);
    insert into public.devices (box_uid, owner_id) values ('ESP32S3-8FF7-D684','${U1}'), ('ESP32S3-ORPH-0001', null);
    insert into public.device_scenarios (device_id, scenario_id)
      select d.id, s.id from public.devices d, public.scenarios s where s.slug in ('capitaine_verdier','vieux_test');
  `);
  for (const f of ["0002_security.sql", "0003_inventory.sql"]) {
    await db.exec(readFileSync(path.join(MIG, f), "utf8"));
  }
}, 60_000);

describe("0003 — reprise des données", () => {
  it("chaque histoire packagée devient une version, statut déduit de active", async () => {
    const r = await rows<{ slug: string; status: string; cv: number | null; vs: string | null }>(`
      select s.slug, s.status, v.version cv, v.status vs
      from public.scenarios s left join public.scenario_versions v on v.id = s.current_version_id
      order by s.slug`);
    expect(r).toEqual([
      { slug: "capitaine_verdier", status: "published", cv: 4, vs: "published" },
      { slug: "sans_package", status: "draft", cv: null, vs: null },
      { slug: "vieux_test", status: "archived", cv: 2, vs: "retired" },
    ]);
  });

  it("les droits par box deviennent des licences du propriétaire (box orpheline ignorée)", async () => {
    const r = await rows<{ user_id: string; slug: string; source: string }>(`
      select l.user_id, s.slug, l.source from public.licenses l
      join public.scenarios s on s.id = l.scenario_id order by s.slug`);
    expect(r).toEqual([
      { user_id: U1, slug: "capitaine_verdier", source: "admin" },
      { user_id: U1, slug: "vieux_test", source: "admin" },
    ]);
  });

  it("les anciennes colonnes restent (compatibilité du code déployé)", async () => {
    const r = await rows<{ n: number }>(
      "select count(*)::int n from public.scenarios where package_path is not null and version >= 1 and active is not null");
    expect(r[0].n).toBe(2);
  });

  it("une seule licence active par compte et histoire, nouvelle possible après révocation", async () => {
    const grant = `insert into public.licenses (user_id, scenario_id, source)
                   select '${U1}', id, 'gift' from public.scenarios where slug='vieux_test'`;
    await expect(db.exec(grant)).rejects.toThrow();
    await db.exec(`update public.licenses set revoked_at = now()
                   where user_id='${U1}' and scenario_id=(select id from public.scenarios where slug='vieux_test')`);
    await expect(db.exec(grant)).resolves.toBeDefined();
  });

  it("requête de synchro : licences actives du propriétaire × versions publiées", async () => {
    const r = await rows(`
      select s.slug, v.version from public.devices d
      join public.licenses l on l.user_id = d.owner_id and l.revoked_at is null
      join public.scenarios s on s.id = l.scenario_id and s.status = 'published'
      join public.scenario_versions v on v.id = s.current_version_id and v.status = 'published'
      where d.box_uid = 'ESP32S3-8FF7-D684'`);
    expect(r).toEqual([{ slug: "capitaine_verdier", version: 4 }]);
  });
});

describe("RLS vue d'un client", () => {
  it("un compte ne lit que ses licences", async () => {
    expect((await asClient("authenticated", U1, "select count(*)::int n from public.licenses")).rows?.[0].n).toBeGreaterThan(0);
    expect((await asClient("authenticated", U2, "select count(*)::int n from public.licenses")).rows?.[0].n).toBe(0);
  });

  it("anon : aucune licence, aucune version, aucun défi de box", async () => {
    for (const t of ["licenses", "scenario_versions", "box_challenges", "device_scenarios"]) {
      expect((await asClient("anon", null, `select 1 from public.${t}`)).error).toMatch(/permission denied/);
    }
  });

  it("catalogue public : uniquement les histoires publiées", async () => {
    const r = await asClient("anon", null, "select slug from public.scenarios order by slug");
    expect(r.rows).toEqual([{ slug: "capitaine_verdier" }]);
  });

  it("un client ne peut ni s'octroyer une licence, ni se donner le rôle admin, ni créer une box", async () => {
    const lic = await asClient("authenticated", U1, `insert into public.licenses (user_id, scenario_id, source)
      select '${U1}', id, 'gift' from public.scenarios where slug='sans_package'`);
    expect(lic.error).toMatch(/permission denied/);
    expect((await asClient("authenticated", U1, "update public.profiles set role='admin'")).error).toMatch(/permission denied/);
    const box = await asClient("authenticated", U1, `insert into public.devices (box_uid, owner_id) values ('ESP32S3-SQUAT-0001', '${U1}')`);
    expect(box.error).toMatch(/permission denied/);
  });
});
