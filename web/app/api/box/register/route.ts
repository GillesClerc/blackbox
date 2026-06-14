import type { NextRequest } from "next/server";
import { createClient } from "@/lib/supabase/server";
import { createAdminClient } from "@/lib/supabase/admin";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

const BOX_UID_RE = /^[A-Za-z0-9-]{4,64}$/;
const MAX_DEVICES = 3;

// POST /api/box/register { box_uid, name? }
// Auth : utilisateur Supabase connecté (cookie de session). Revendique une box
// sur son compte. Option A (claim simple) : aucune preuve que la box est en
// possession de l'utilisateur — cf. dette « option B » dans CLAUDE.md.
export async function POST(request: NextRequest) {
  let body: { box_uid?: string; name?: string };
  try {
    body = await request.json();
  } catch {
    return Response.json({ error: "json invalide" }, { status: 400 });
  }

  const { box_uid: boxUid, name } = body;
  if (!boxUid || !BOX_UID_RE.test(boxUid)) {
    return Response.json({ error: "box_uid invalide" }, { status: 400 });
  }

  const supabase = await createClient();
  const {
    data: { user },
  } = await supabase.auth.getUser();
  if (!user) {
    return Response.json({ error: "non authentifié" }, { status: 401 });
  }

  const admin = createAdminClient();

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
