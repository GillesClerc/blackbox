import type { NextRequest } from "next/server";
import { createClient } from "@/lib/supabase/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { verifyBoxHmac } from "@/lib/box-auth";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

const BOX_UID_RE = /^[A-Za-z0-9-]{4,64}$/;
const MAX_DEVICES = 3;

// POST /api/box/register { box_uid, name?, challenge, challenge_response }
// Auth : utilisateur Supabase connecté (cookie de session). Option B (preuve
// de possession) : la box a signé le challenge via BLE pendant l'appairage
// (hal_box_auth_sign, purpose "register") — impossible de revendiquer un UID
// qu'on n'a pas en main. Même mécanique anti-replay que /api/box/auth (nonce
// consommé) ; la signature "register" n'est pas acceptée par /api/box/auth.
export async function POST(request: NextRequest) {
  let body: {
    box_uid?: string;
    name?: string;
    challenge?: string;
    challenge_response?: string;
  };
  try {
    body = await request.json();
  } catch {
    return Response.json({ error: "json invalide" }, { status: 400 });
  }

  const { box_uid: boxUid, name, challenge, challenge_response: response } = body;
  if (!boxUid || !BOX_UID_RE.test(boxUid)) {
    return Response.json({ error: "box_uid invalide" }, { status: 400 });
  }
  if (!challenge || !response) {
    return Response.json(
      { error: "challenge et challenge_response requis (preuve de possession)" },
      { status: 400 }
    );
  }

  const supabase = await createClient();
  const {
    data: { user },
  } = await supabase.auth.getUser();
  if (!user) {
    return Response.json({ error: "non authentifié" }, { status: 401 });
  }

  const admin = createAdminClient();

  // Nonce valide : émis pour cette box, non consommé, non expiré.
  const { data: row } = await admin
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

  if (!verifyBoxHmac("register", boxUid, challenge, response)) {
    return Response.json({ error: "preuve de possession invalide" }, { status: 401 });
  }

  // Consomme le nonce de façon atomique (anti-replay, même sous concurrence).
  const { data: consumed } = await admin
    .from("box_challenges")
    .update({ used: true })
    .eq("challenge", challenge)
    .eq("used", false)
    .select("challenge");

  if (!consumed || consumed.length === 0) {
    return Response.json({ error: "challenge déjà consommé" }, { status: 401 });
  }

  // Box déjà enregistrée ? Idempotent si c'est déjà la sienne, sinon conflit.
  const { data: existing } = await admin
    .from("devices")
    .select("id, owner_id")
    .eq("box_uid", boxUid)
    .maybeSingle();

  if (existing) {
    if (existing.owner_id === user.id) {
      return Response.json({ device_id: existing.id, already_owned: true });
    }
    return Response.json(
      { error: "box déjà enregistrée sur un autre compte" },
      { status: 409 }
    );
  }

  // WB-04 : max 3 box par compte, vérifié ici (pas de contrainte SQL pour
  // laisser le service_role réassigner en SAV).
  const { count } = await admin
    .from("devices")
    .select("id", { count: "exact", head: true })
    .eq("owner_id", user.id);

  if ((count ?? 0) >= MAX_DEVICES) {
    return Response.json(
      { error: `limite de ${MAX_DEVICES} box atteinte` },
      { status: 409 }
    );
  }

  const { data: device, error } = await admin
    .from("devices")
    .insert({
      box_uid: boxUid,
      owner_id: user.id,
      name: name?.slice(0, 64) || "EscapeBox",
    })
    .select("id")
    .single();

  if (error || !device) {
    return Response.json({ error: "enregistrement échoué" }, { status: 500 });
  }

  return Response.json({ device_id: device.id });
}
