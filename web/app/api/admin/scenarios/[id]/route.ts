import { adminOrError } from "@/lib/admin/auth";
import { badId, jsonBody, reply, UUID_RE } from "@/lib/admin/http";
import { updateScenario } from "@/lib/admin/inventory";
import { createAdminClient } from "@/lib/supabase/admin";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

// PATCH /api/admin/scenarios/<id> — modifier la fiche / le statut.
export async function PATCH(request: Request, ctx: { params: Promise<{ id: string }> }) {
  const who = await adminOrError(request);
  if (who instanceof Response) return who;
  const { id } = await ctx.params;
  if (!UUID_RE.test(id)) return badId();
  const body = await jsonBody(request);
  if (body instanceof Response) return body;
  return reply(await updateScenario(createAdminClient(), id, body));
}
