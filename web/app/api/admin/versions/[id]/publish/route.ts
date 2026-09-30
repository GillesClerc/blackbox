import { adminOrError } from "@/lib/admin/auth";
import { badId, reply, UUID_RE } from "@/lib/admin/http";
import { publishVersion } from "@/lib/admin/inventory";
import { createAdminClient } from "@/lib/supabase/admin";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

// POST /api/admin/versions/<id>/publish — rend la version courante (atomique) ;
// sert aussi à revenir à une version précédente.
export async function POST(request: Request, ctx: { params: Promise<{ id: string }> }) {
  const who = await adminOrError(request);
  if (who instanceof Response) return who;
  const { id } = await ctx.params;
  if (!UUID_RE.test(id)) return badId();
  return reply(await publishVersion(createAdminClient(), id));
}
