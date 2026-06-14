import type { NextRequest } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { newChallenge } from "@/lib/box-auth";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

const BOX_UID_RE = /^[A-Za-z0-9-]{4,64}$/;
const TTL_SECONDS = 60;

// GET /api/box/challenge?box_uid=ESP32S3-XXXX-XXXX
// → { challenge, expires_in }. Nonce usage unique anti-replay (TTL 60s).
export async function GET(request: NextRequest) {
  const boxUid = request.nextUrl.searchParams.get("box_uid");
  if (!boxUid || !BOX_UID_RE.test(boxUid)) {
    return Response.json({ error: "box_uid invalide" }, { status: 400 });
  }

  const supabase = createAdminClient();

  // Purge opportuniste (pas besoin de cron) : nettoie les nonces expirés.
  await supabase
    .from("box_challenges")
    .delete()
    .lt("expires_at", new Date().toISOString());

  const challenge = newChallenge();
  const expiresAt = new Date(Date.now() + TTL_SECONDS * 1000).toISOString();

  const { error } = await supabase
    .from("box_challenges")
    .insert({ challenge, box_uid: boxUid, expires_at: expiresAt });

  if (error) {
    return Response.json({ error: "challenge_failed" }, { status: 500 });
  }

  return Response.json({ challenge, expires_in: TTL_SECONDS });
}
