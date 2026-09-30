import { adminOrError } from "@/lib/admin/auth";
import { badId, reply, UUID_RE } from "@/lib/admin/http";
import { revokeLicense } from "@/lib/admin/inventory";
import { createAdminClient } from "@/lib/supabase/admin";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

// POST /api/admin/licenses/<id>/revoke { reason? } — révoquer une licence.
export async function POST(request: Request, ctx: { params: Promise<{ id: string }> }) {
  const who = await adminOrError(request);
  if (who instanceof Response) return who;
  const { id } = await ctx.params;
  if (!UUID_RE.test(id)) return badId();
  let reason: string | null = null;
  try {
    const b = (await request.json()) as { reason?: unknown };
    reason = typeof b?.reason === "string" ? b.reason : null;
  } catch {
    // corps facultatif
  }
  return reply(await revokeLicense(createAdminClient(), id, reason));
}
