import { createClient } from "@supabase/supabase-js";

// Client service_role : bypass la RLS. Réservé aux route handlers serveur des
// box (la box n'est pas un utilisateur Supabase, elle s'authentifie par JWT box).
// Ne JAMAIS importer ce module dans un composant client.
// La vérification des env est paresseuse (dans la fonction) et non au chargement
// du module : sinon `next build` plante en collectant la page data sans secrets.
export function createAdminClient() {
  const url = process.env.NEXT_PUBLIC_SUPABASE_URL;
  const serviceRoleKey = process.env.SUPABASE_SERVICE_ROLE_KEY;
  if (!url || !serviceRoleKey) {
    throw new Error(
      "NEXT_PUBLIC_SUPABASE_URL ou SUPABASE_SERVICE_ROLE_KEY manquant"
    );
  }
  return createClient(url, serviceRoleKey, {
    auth: { autoRefreshToken: false, persistSession: false },
  });
}
