import { adminOrError } from "@/lib/admin/auth";
import { badId, reply, UUID_RE } from "@/lib/admin/http";
import { createVersion } from "@/lib/admin/inventory";
import { MAX_TOTAL_BYTES } from "@/lib/scenario/package";
import { createAdminClient } from "@/lib/supabase/admin";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

// POST /api/admin/scenarios/<id>/versions — multipart : file (zip), notes.
// Crée une version en BROUILLON (fichiers dans Storage) ; la publication est à part.
export async function POST(request: Request, ctx: { params: Promise<{ id: string }> }) {
  const who = await adminOrError(request);
  if (who instanceof Response) return who;
  const { id } = await ctx.params;
  if (!UUID_RE.test(id)) return badId();

  const declared = Number(request.headers.get("content-length") ?? 0);
  if (declared > MAX_TOTAL_BYTES + 1024 * 1024) {
    return Response.json({ errors: ["archive trop grosse (100 Mo max)"] }, { status: 413 });
  }
  let form: FormData;
  try {
    form = await request.formData();
  } catch {
    return Response.json({ errors: ["formulaire invalide"] }, { status: 400 });
  }
  const file = form.get("file");
  if (!(file instanceof File) || file.size === 0) {
    return Response.json({ errors: ["archive zip manquante"] }, { status: 400 });
  }
  if (file.size > MAX_TOTAL_BYTES) {
    return Response.json({ errors: ["archive trop grosse (100 Mo max)"] }, { status: 413 });
  }
  const notes = typeof form.get("notes") === "string" ? (form.get("notes") as string) : null;
  const zip = new Uint8Array(await file.arrayBuffer());
  return reply(await createVersion(createAdminClient(), id, zip, notes, who.userId), 201);
}
