import type { NextRequest } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { bearerToken, verifyBoxJwt } from "@/lib/box-auth";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

const VERSION_RE = /^\d+\.\d+\.\d+$/;

// Compare deux versions semver "x.y.z". >0 si a>b, <0 si a<b, 0 si égales.
function compareVersions(a: string, b: string): number {
  const pa = a.split(".").map(Number);
  const pb = b.split(".").map(Number);
  for (let i = 0; i < 3; i++) {
    if (pa[i] !== pb[i]) return pa[i] - pb[i];
  }
  return 0;
}

// GET /api/box/sync?firmware_version=1.2.3
// Auth : header "Authorization: Bearer <box-jwt>". Met à jour last_sync_at +
// firmware_version rapporté, puis renvoie les scénarios installés sur la box
// (modèle pull) et un bloc OTA si une release plus récente est active.
export async function GET(request: NextRequest) {
  const token = bearerToken(request.headers.get("authorization"));
  if (!token) {
    return Response.json({ error: "token box requis" }, { status: 401 });
  }

  let payload;
  try {
    payload = await verifyBoxJwt(token);
  } catch {
    return Response.json({ error: "token invalide ou expiré" }, { status: 401 });
  }

  const deviceId = payload.device_id;
  const reported = request.nextUrl.searchParams.get("firmware_version");
  const firmwareVersion = reported && VERSION_RE.test(reported) ? reported : null;

  const supabase = createAdminClient();

  await supabase
    .from("devices")
    .update({
      last_sync_at: new Date().toISOString(),
      ...(firmwareVersion ? { firmware_version: firmwareVersion } : {}),
    })
    .eq("id", deviceId);

  // Scénarios installés (modèle pull : le webhook Stripe a inséré le droit).
  const { data: rows } = await supabase
    .from("device_scenarios")
    .select(
      "installed_at, scenarios(id, slug, title, scenario_path, active)"
    )
    .eq("device_id", deviceId);

  const scenarios = (rows ?? [])
    .map((r) => {
      const s = r.scenarios as unknown as {
        id: string;
        slug: string;
        title: string;
        scenario_path: string | null;
        active: boolean | null;
      } | null;
      if (!s || s.active === false) return null;
      return {
        id: s.id,
        slug: s.slug,
        title: s.title,
        scenario_path: s.scenario_path,
        installed_at: r.installed_at,
      };
    })
    .filter((s): s is NonNullable<typeof s> => s !== null);

  // OTA : release la plus récente active sur le canal stable. Le tri se fait en
  // JS (compareVersions) car un ORDER BY texte trierait "1.10.0" < "1.9.0".
  const { data: releases } = await supabase
    .from("firmware_releases")
    .select("version, url, sha256, notes")
    .eq("active", true)
    .eq("channel", "stable");

  const latest = (releases ?? [])
    .filter((r) => VERSION_RE.test(r.version))
    .sort((a, b) => compareVersions(b.version, a.version))[0];

  let firmwareUpdate: {
    version: string;
    url: string;
    sha256: string | null;
    notes: string | null;
  } | null = null;

  if (
    latest &&
    (!firmwareVersion || compareVersions(latest.version, firmwareVersion) > 0)
  ) {
    firmwareUpdate = {
      version: latest.version,
      url: latest.url,
      sha256: latest.sha256 ?? null,
      notes: latest.notes ?? null,
    };
  }

  return Response.json({
    scenarios,
    firmware_update: firmwareUpdate,
    server_time: Math.floor(Date.now() / 1000),
  });
}
