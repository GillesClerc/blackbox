import { createAdminClient } from "@/lib/supabase/admin";
import { createClient } from "@/lib/supabase/server";
import { bearerToken } from "@/lib/box-auth";

// Contrôle d'accès du back-office : utilisateur connecté ET profiles.role = 'admin'.
// Identité : session du navigateur (cookies Supabase) ou, pour les tests
// automatisés, en-tête « Authorization: Bearer <jeton d'accès Supabase> ».
// Les écritures passent ensuite par le client service_role (décision E1).

export type AdminCheck =
  | { ok: true; userId: string; email: string | null }
  | { ok: false; status: 401 | 403; error: string };

export async function checkAdmin(authHeader: string | null = null): Promise<AdminCheck> {
  const admin = createAdminClient();
  const token = bearerToken(authHeader);

  let userId: string | null = null;
  let email: string | null = null;
  if (token) {
    const { data } = await admin.auth.getUser(token);
    userId = data.user?.id ?? null;
    email = data.user?.email ?? null;
  } else {
    const supabase = await createClient();
    const { data } = await supabase.auth.getUser();
    userId = data.user?.id ?? null;
    email = data.user?.email ?? null;
  }
  if (!userId) return { ok: false, status: 401, error: "non authentifié" };

  const { data: profile } = await admin
    .from("profiles")
    .select("role")
    .eq("id", userId)
    .maybeSingle();
  if (profile?.role !== "admin") {
    return { ok: false, status: 403, error: "réservé aux administrateurs" };
  }
  return { ok: true, userId, email };
}

// Pour les route handlers : renvoie la réponse d'erreur, ou null si admin.
export async function adminOrError(
  request: Request
): Promise<{ userId: string } | Response> {
  const check = await checkAdmin(request.headers.get("authorization"));
  if (!check.ok) return Response.json({ error: check.error }, { status: check.status });
  return { userId: check.userId };
}
