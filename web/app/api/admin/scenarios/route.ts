import { adminOrError } from "@/lib/admin/auth";
import { jsonBody, reply } from "@/lib/admin/http";
import { createScenario } from "@/lib/admin/inventory";
import { createAdminClient } from "@/lib/supabase/admin";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

// POST /api/admin/scenarios — créer une histoire (brouillon).
export async function POST(request: Request) {
  const who = await adminOrError(request);
  if (who instanceof Response) return who;
  const body = await jsonBody(request);
  if (body instanceof Response) return body;
  return reply(await createScenario(createAdminClient(), body), 201);
}
