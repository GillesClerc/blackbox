import type { NextRequest } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { z } from "zod";
import { signBoxJwt, verifyBoxHmac } from "@/lib/box-auth";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

// Corps strictement typé : un champ non textuel ferait lever verifyBoxHmac
// (Buffer.from) et répondre 500 au lieu de 400.
const bodySchema = z.object({
  box_uid: z.string().regex(/^[A-Za-z0-9-]{4,64}$/),
  challenge: z.string().min(1).max(128),
  challenge_response: z.string().min(1).max(128),
});

// POST /api/box/auth { box_uid, challenge, challenge_response }
// → { token, server_time }. Vérifie le nonce (non utilisé, non expiré) puis le
// HMAC de la box, marque le nonce consommé et émet un JWT box (2h).
export async function POST(request: NextRequest) {
  let raw: unknown;
  try {
    raw = await request.json();
  } catch {
    return Response.json({ error: "json invalide" }, { status: 400 });
  }

  const parsed = bodySchema.safeParse(raw);
  if (!parsed.success) {
    return Response.json(
      { error: "box_uid, challenge et challenge_response requis" },
      { status: 400 }
    );
  }
  const { box_uid: boxUid, challenge, challenge_response: response } = parsed.data;

  const supabase = createAdminClient();

  // Nonce valide : émis pour cette box, non consommé, non expiré.
  const { data: row } = await supabase
    .from("box_challenges")
    .select("challenge")
    .eq("challenge", challenge)
    .eq("box_uid", boxUid)
    .eq("used", false)
    .gt("expires_at", new Date().toISOString())
    .maybeSingle();

  if (!row) {
    return Response.json({ error: "challenge invalide ou expiré" }, { status: 401 });
  }

  if (!verifyBoxHmac("auth", boxUid, challenge, response)) {
    return Response.json({ error: "signature invalide" }, { status: 401 });
  }

  // Consomme le nonce de façon atomique (garde used=false) → anti-replay même
  // sous concurrence : si aucune ligne n'est mise à jour, un autre l'a déjà pris.
  const { data: consumed } = await supabase
    .from("box_challenges")
    .update({ used: true })
    .eq("challenge", challenge)
    .eq("used", false)
    .select("challenge");

  if (!consumed || consumed.length === 0) {
    return Response.json({ error: "challenge déjà consommé" }, { status: 401 });
  }

  // La box doit être enregistrée sur un compte (cf. /api/box/register).
  const { data: device } = await supabase
    .from("devices")
    .select("id, owner_id")
    .eq("box_uid", boxUid)
    .maybeSingle();

  if (!device || !device.owner_id) {
    return Response.json({ error: "box non enregistrée" }, { status: 403 });
  }

  const token = await signBoxJwt({
    boxUid,
    deviceId: device.id,
    ownerId: device.owner_id,
  });

  return Response.json({
    token,
    server_time: Math.floor(Date.now() / 1000),
  });
}
