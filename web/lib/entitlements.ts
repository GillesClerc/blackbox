import type { SupabaseClient } from "@supabase/supabase-js";
import type { Database } from "@/lib/database.types";

// Droits d'une box sur les histoires (roadmap E1) : une box reçoit les histoires
// PUBLIÉES pour lesquelles son PROPRIÉTAIRE a une licence active. La licence est
// liée au compte (décision 2026-09-30) : toutes les box du compte en profitent.

type Admin = SupabaseClient<Database>;

export type CurrentVersion = {
  version: number;
  status: string;
  storage: string;
  storage_path: string;
};

export type ScenarioWithVersion = {
  id: string;
  slug: string;
  title: string;
  status: string;
  current_version: CurrentVersion | null;
};

export type LicenseRow = { scenario_id: string; created_at: string };

// Élément de la réponse /api/box/sync — format inchangé depuis F3 : le firmware
// (cloud_client) lit slug, package_path et version.
export type SyncScenario = {
  id: string;
  slug: string;
  title: string;
  package_path: string;
  version: number;
  installed_at: string;
};

// Une histoire est servie si elle est publiée ET a une version courante publiée.
export function isServable(s: ScenarioWithVersion): s is ScenarioWithVersion & {
  current_version: CurrentVersion;
} {
  return (
    s.status === "published" &&
    s.current_version !== null &&
    s.current_version.status === "published"
  );
}

export function packagePath(slug: string): string {
  return `/api/box/pkg/${slug}`;
}

// Pur : construit la liste de synchro à partir des licences actives du
// propriétaire et des histoires correspondantes.
export function buildSyncList(
  licenses: LicenseRow[],
  scenarios: ScenarioWithVersion[]
): SyncScenario[] {
  const grantedAt = new Map(licenses.map((l) => [l.scenario_id, l.created_at]));
  return scenarios
    .filter((s) => grantedAt.has(s.id) && isServable(s))
    .map((s) => ({
      id: s.id,
      slug: s.slug,
      title: s.title,
      package_path: packagePath(s.slug),
      version: s.current_version!.version,
      installed_at: grantedAt.get(s.id)!,
    }))
    .sort((a, b) => a.slug.localeCompare(b.slug));
}

export type PackageAccess =
  | { ok: true; scenarioId: string; version: CurrentVersion }
  | { ok: false; status: 403 | 404; error: string };

// Pur : décision d'accès aux fichiers d'un package.
export function decidePackageAccess(
  scenario: ScenarioWithVersion | null,
  hasActiveLicense: boolean
): PackageAccess {
  if (!scenario || !isServable(scenario)) {
    return { ok: false, status: 404, error: "scénario inconnu" };
  }
  if (!hasActiveLicense) {
    return { ok: false, status: 403, error: "scénario non licencié pour ce compte" };
  }
  return { ok: true, scenarioId: scenario.id, version: scenario.current_version };
}

const SCENARIO_SELECT =
  "id, slug, title, status, current_version:scenario_versions!scenarios_current_version_id_fkey(version, status, storage, storage_path)";

async function deviceOwner(admin: Admin, deviceId: string): Promise<string | null> {
  const { data } = await admin
    .from("devices")
    .select("owner_id")
    .eq("id", deviceId)
    .maybeSingle();
  return data?.owner_id ?? null;
}

// Histoires à synchroniser pour une box.
export async function boxSyncList(admin: Admin, deviceId: string): Promise<SyncScenario[]> {
  const owner = await deviceOwner(admin, deviceId);
  if (!owner) return [];

  const { data: licenses } = await admin
    .from("licenses")
    .select("scenario_id, created_at")
    .eq("user_id", owner)
    .is("revoked_at", null);
  if (!licenses || licenses.length === 0) return [];

  const { data: scenarios } = await admin
    .from("scenarios")
    .select(SCENARIO_SELECT)
    .in(
      "id",
      licenses.map((l) => l.scenario_id)
    );

  return buildSyncList(licenses, (scenarios ?? []) as unknown as ScenarioWithVersion[]);
}

// Accès d'une box aux fichiers du package publié d'une histoire.
export async function boxPackageAccess(
  admin: Admin,
  deviceId: string,
  slug: string
): Promise<PackageAccess> {
  const { data: scenario } = await admin
    .from("scenarios")
    .select(SCENARIO_SELECT)
    .eq("slug", slug)
    .maybeSingle();
  const s = (scenario ?? null) as unknown as ScenarioWithVersion | null;
  if (!s || !isServable(s)) return decidePackageAccess(s, false);

  const owner = await deviceOwner(admin, deviceId);
  let licensed = false;
  if (owner) {
    const { data: lic } = await admin
      .from("licenses")
      .select("id")
      .eq("user_id", owner)
      .eq("scenario_id", s.id)
      .is("revoked_at", null)
      .maybeSingle();
    licensed = !!lic;
  }
  return decidePackageAccess(s, licensed);
}
