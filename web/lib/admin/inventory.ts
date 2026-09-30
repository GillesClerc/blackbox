import type { SupabaseClient } from "@supabase/supabase-js";
import { z } from "zod";
import type { Database } from "@/lib/database.types";
import { buildPackage, readZip, SLUG_RE, type Manifest } from "@/lib/scenario/package";

// Opérations du back-office sur l'inventaire (roadmap E2). Appelées par les
// route handlers /api/admin/* après checkAdmin(), avec le client service_role.

type Admin = SupabaseClient<Database>;
export const PACKAGES_BUCKET = "scenario-packages";

export type Result<T> = { ok: true; data: T } | { ok: false; status: number; errors: string[] };
const err = (status: number, ...errors: string[]) => ({ ok: false as const, status, errors });

// ── Fiche d'une histoire ─────────────────────────────────────────────────────
const optText = z.string().trim().max(4000).nullable().optional();
const optInt = (min: number, max: number) => z.number().int().min(min).max(max).nullable().optional();

export const scenarioFieldsSchema = z.object({
  title: z.string().trim().min(1, "titre requis").max(120),
  summary: z.string().trim().max(280).nullable().optional(),
  description: optText,
  theme: z.string().trim().max(80).nullable().optional(),
  ambiance: z.string().trim().max(160).nullable().optional(),
  difficulty: optInt(1, 5),
  duration_min: optInt(5, 600),
  min_players: optInt(1, 20),
  max_players: optInt(1, 20),
  min_age: optInt(0, 99),
  language: z.enum(["fr", "de", "en", "it"]).optional(),
  price_chf: z.number().min(0).max(1000).optional(),
  status: z.enum(["draft", "published", "archived"]).optional(),
});
export const createScenarioSchema = scenarioFieldsSchema.extend({
  slug: z.string().regex(SLUG_RE, "identifiant : minuscules, chiffres, - et _ (64 max)"),
});
export type ScenarioFields = z.infer<typeof scenarioFieldsSchema>;

function zodErrors(e: z.ZodError): string[] {
  return e.issues.map((i) => `${i.path.join(".") || "données"} : ${i.message}`);
}

function playersOk(f: ScenarioFields): string | null {
  if (f.min_players != null && f.max_players != null && f.max_players < f.min_players) {
    return "joueurs : le maximum doit être supérieur ou égal au minimum";
  }
  return null;
}

export async function createScenario(admin: Admin, input: unknown): Promise<Result<{ id: string }>> {
  const p = createScenarioSchema.safeParse(input);
  if (!p.success) return err(400, ...zodErrors(p.error));
  const bad = playersOk(p.data);
  if (bad) return err(400, bad);
  // Une histoire naît en brouillon : elle n'est publiée qu'avec une version.
  const { data, error } = await admin
    .from("scenarios")
    .insert({ ...p.data, status: "draft", active: true })
    .select("id")
    .single();
  if (error?.code === "23505") return err(409, `l'identifiant « ${p.data.slug} » existe déjà`);
  if (error || !data) return err(500, "création impossible");
  return { ok: true, data: { id: data.id } };
}

export async function updateScenario(admin: Admin, id: string, input: unknown): Promise<Result<null>> {
  const p = scenarioFieldsSchema.partial().safeParse(input);
  if (!p.success) return err(400, ...zodErrors(p.error));
  const bad = playersOk(p.data as ScenarioFields);
  if (bad) return err(400, bad);
  if (p.data.status === "published") {
    const { data: s } = await admin.from("scenarios").select("current_version_id").eq("id", id).maybeSingle();
    if (!s?.current_version_id) return err(400, "publier une version avant de publier l'histoire");
  }
  const { error, count } = await admin
    .from("scenarios")
    .update({ ...p.data, updated_at: new Date().toISOString() }, { count: "exact" })
    .eq("id", id);
  if (error) return err(500, "enregistrement impossible");
  if (!count) return err(404, "histoire inconnue");
  return { ok: true, data: null };
}

// ── Versions ─────────────────────────────────────────────────────────────────
export async function createVersion(
  admin: Admin,
  scenarioId: string,
  zip: Uint8Array,
  notes: string | null,
  userId: string
): Promise<Result<{ id: string; version: number; warnings: string[]; manifest: Manifest }>> {
  const { data: scenario } = await admin
    .from("scenarios")
    .select("id, slug")
    .eq("id", scenarioId)
    .maybeSingle();
  if (!scenario) return err(404, "histoire inconnue");

  const { data: last } = await admin
    .from("scenario_versions")
    .select("version")
    .eq("scenario_id", scenarioId)
    .order("version", { ascending: false })
    .limit(1)
    .maybeSingle();
  const version = (last?.version ?? 0) + 1;

  let files;
  try {
    files = readZip(zip);
  } catch {
    return err(400, "archive zip illisible");
  }
  const built = buildPackage(scenario.slug, version, files);
  if (!built.ok) return err(400, ...built.errors);

  // Fichiers + manifest dans Storage, sous <slug>/v<version>/.
  const prefix = `${scenario.slug}/v${version}`;
  const bucket = admin.storage.from(PACKAGES_BUCKET);
  const uploaded: string[] = [];
  const manifestBytes = new TextEncoder().encode(JSON.stringify(built.manifest, null, 2) + "\n");
  const toUpload = [
    ...built.files.map((f) => ({ path: f.path, data: f.data })),
    { path: "manifest.json", data: manifestBytes },
  ];
  for (const f of toUpload) {
    const key = `${prefix}/${f.path}`;
    const { error } = await bucket.upload(key, f.data, {
      contentType: f.path.endsWith(".json")
        ? "application/json"
        : f.path.endsWith(".mp3")
          ? "audio/mpeg"
          : "application/octet-stream",
      upsert: false,
    });
    if (error) {
      if (uploaded.length) await bucket.remove(uploaded);
      return err(500, `envoi impossible : ${f.path} (${error.message})`);
    }
    uploaded.push(key);
  }

  const { data: row, error } = await admin
    .from("scenario_versions")
    .insert({
      scenario_id: scenarioId,
      version,
      status: "draft",
      manifest: built.manifest as unknown as Database["public"]["Tables"]["scenario_versions"]["Insert"]["manifest"],
      total_bytes: built.manifest.total_bytes,
      storage: "bucket",
      storage_path: prefix,
      notes: notes?.trim() || null,
      created_by: userId,
    })
    .select("id")
    .single();
  if (error || !row) {
    await bucket.remove(uploaded);
    return err(error?.code === "23505" ? 409 : 500, "enregistrement de la version impossible");
  }
  return { ok: true, data: { id: row.id, version, warnings: built.warnings, manifest: built.manifest } };
}

export async function publishVersion(admin: Admin, versionId: string): Promise<Result<null>> {
  const { error } = await admin.rpc("publish_scenario_version", { p_version_id: versionId });
  if (error) {
    return err(/version inconnue/.test(error.message) ? 404 : 500, "publication impossible");
  }
  return { ok: true, data: null };
}

// ── Licences ─────────────────────────────────────────────────────────────────
const grantSchema = z.object({
  email: z.email(),
  source: z.enum(["gift", "admin"]).default("gift"),
  note: z.string().trim().max(500).nullable().optional(),
});

export async function grantLicense(
  admin: Admin,
  scenarioId: string,
  input: unknown,
  grantedBy: string
): Promise<Result<{ id: string }>> {
  const p = grantSchema.safeParse(input);
  if (!p.success) return err(400, ...zodErrors(p.error));
  const { data: profile } = await admin
    .from("profiles")
    .select("id")
    .eq("email", p.data.email.toLowerCase())
    .maybeSingle();
  if (!profile) return err(404, `aucun compte avec l'adresse ${p.data.email}`);
  const { data, error } = await admin
    .from("licenses")
    .insert({
      user_id: profile.id,
      scenario_id: scenarioId,
      source: p.data.source,
      note: p.data.note ?? null,
      granted_by: grantedBy,
    })
    .select("id")
    .single();
  if (error?.code === "23505") return err(409, "ce compte a déjà une licence active pour cette histoire");
  if (error?.code === "23503") return err(404, "histoire inconnue");
  if (error || !data) return err(500, "octroi impossible");
  return { ok: true, data: { id: data.id } };
}

export async function revokeLicense(admin: Admin, licenseId: string, reason: string | null): Promise<Result<null>> {
  const { error, count } = await admin
    .from("licenses")
    .update({ revoked_at: new Date().toISOString(), revoke_reason: reason?.trim() || null }, { count: "exact" })
    .eq("id", licenseId)
    .is("revoked_at", null);
  if (error) return err(500, "révocation impossible");
  if (!count) return err(404, "licence inconnue ou déjà révoquée");
  return { ok: true, data: null };
}
