import { createReadStream } from "node:fs";
import { stat } from "node:fs/promises";
import path from "node:path";
import { Readable } from "node:stream";
import type { NextRequest } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { bearerToken, verifyBoxJwt } from "@/lib/box-auth";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

// GET /api/box/pkg/<slug>/<fichier...> — livraison contrôlée des packages.
// Auth : JWT box + droit device_scenarios sur CE slug. Les fichiers vivent
// dans web/scenario-packages/ (hors de public/ → jamais servis en statique).
// ⚠ Nixpacks déploie le repo complet donc fs marche ; si passage un jour en
// `output: standalone`, ajouter outputFileTracingIncludes pour ce dossier.

const SLUG_RE = /^[a-z0-9_-]{1,64}$/i;
// Segment de chemin : jamais de '.' initial → exclut '.', '..' et les cachés.
const SEGMENT_RE = /^[A-Za-z0-9_-][A-Za-z0-9._-]*$/;
const MAX_SEGMENTS = 3; // profondeur <= 2 sous-dossiers (cf. package_scenario.py)

const PKG_ROOT = path.join(process.cwd(), "scenario-packages");

const MIME: Record<string, string> = {
  ".json": "application/json",
  ".mp3": "audio/mpeg",
};

export async function GET(
  request: NextRequest,
  ctx: { params: Promise<{ slug: string; path: string[] }> }
) {
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

  const { slug, path: segments } = await ctx.params;
  if (
    !SLUG_RE.test(slug) ||
    segments.length === 0 ||
    segments.length > MAX_SEGMENTS ||
    !segments.every((s) => SEGMENT_RE.test(s))
  ) {
    return Response.json({ error: "chemin invalide" }, { status: 400 });
  }

  const admin = createAdminClient();

  const { data: scenario } = await admin
    .from("scenarios")
    .select("id, active")
    .eq("slug", slug)
    .maybeSingle();
  if (!scenario || scenario.active === false) {
    return Response.json({ error: "scénario inconnu" }, { status: 404 });
  }

  const { data: right } = await admin
    .from("device_scenarios")
    .select("device_id")
    .eq("device_id", payload.device_id)
    .eq("scenario_id", scenario.id)
    .maybeSingle();
  if (!right) {
    return Response.json({ error: "scénario non licencié pour cette box" }, {
      status: 403,
    });
  }

  const filePath = path.join(PKG_ROOT, slug, ...segments);
  // Ceinture + bretelles : les regex interdisent déjà toute traversée.
  if (!filePath.startsWith(PKG_ROOT + path.sep)) {
    return Response.json({ error: "chemin invalide" }, { status: 400 });
  }

  let info;
  try {
    info = await stat(filePath);
  } catch {
    return Response.json({ error: "fichier absent" }, { status: 404 });
  }
  if (!info.isFile()) {
    return Response.json({ error: "fichier absent" }, { status: 404 });
  }

  const stream = Readable.toWeb(createReadStream(filePath)) as ReadableStream;
  return new Response(stream, {
    headers: {
      "Content-Type":
        MIME[path.extname(filePath).toLowerCase()] ?? "application/octet-stream",
      "Content-Length": String(info.size),
      "Cache-Control": "private, no-store",
    },
  });
}
