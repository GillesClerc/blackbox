import { createReadStream } from "node:fs";
import { stat } from "node:fs/promises";
import path from "node:path";
import { Readable } from "node:stream";
import type { NextRequest } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { bearerToken, verifyBoxJwt } from "@/lib/box-auth";
import { boxPackageAccess } from "@/lib/entitlements";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

// GET /api/box/pkg/<slug>/<fichier...> — livraison contrôlée des packages.
// Auth : JWT box + licence active du propriétaire de la box sur CE slug ; sert la
// version PUBLIÉE (scenarios.current_version_id). Stockage 'repo' : fichiers dans
// web/scenario-packages/<storage_path>/ (hors de public/ → jamais servis en
// statique). Stockage 'bucket' (Supabase Storage) : étape E2.
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

  const access = await boxPackageAccess(createAdminClient(), payload.device_id, slug);
  if (!access.ok) {
    return Response.json({ error: access.error }, { status: access.status });
  }
  if (access.version.storage !== "repo") {
    return Response.json({ error: "stockage non encore pris en charge" }, { status: 501 });
  }

  const pkgDir = path.join(PKG_ROOT, access.version.storage_path);
  const filePath = path.join(pkgDir, ...segments);
  // Ceinture + bretelles : les regex interdisent déjà toute traversée.
  if (!pkgDir.startsWith(PKG_ROOT + path.sep) || !filePath.startsWith(pkgDir + path.sep)) {
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
