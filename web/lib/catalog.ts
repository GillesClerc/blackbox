import { createClient } from "@supabase/supabase-js";
import type { Database } from "@/lib/database.types";

// Catalogue public : histoires PUBLIÉES, lues avec la clé publique (RLS
// « lecture des histoires publiées »). Aucune donnée privée ici.

export type Story = {
  slug: string;
  title: string;
  summary: string | null;
  description: string | null;
  theme: string | null;
  ambiance: string | null;
  difficulty: number | null;
  duration_min: number | null;
  min_players: number | null;
  max_players: number | null;
  min_age: number | null;
  language: string;
};

const FIELDS =
  "slug, title, summary, description, theme, ambiance, difficulty, duration_min, min_players, max_players, min_age, language";

function publicClient() {
  const url = process.env.NEXT_PUBLIC_SUPABASE_URL;
  const key = process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY;
  if (!url || !key) return null;
  return createClient<Database>(url, key, { auth: { persistSession: false } });
}

export async function listStories(): Promise<Story[]> {
  const sb = publicClient();
  if (!sb) return [];
  const { data } = await sb.from("scenarios").select(FIELDS).eq("status", "published").order("title");
  return (data ?? []) as Story[];
}

export async function getStory(slug: string): Promise<Story | null> {
  const sb = publicClient();
  if (!sb || !/^[a-z0-9_-]{1,64}$/.test(slug)) return null;
  const { data } = await sb.from("scenarios").select(FIELDS).eq("status", "published").eq("slug", slug).maybeSingle();
  return (data as Story | null) ?? null;
}

export function playersLabel(s: Pick<Story, "min_players" | "max_players">): string | null {
  const { min_players: a, max_players: b } = s;
  if (a && b) return a === b ? `${a} joueurs` : `${a} à ${b} joueurs`;
  if (a) return `dès ${a} joueurs`;
  if (b) return `jusqu'à ${b} joueurs`;
  return null;
}

export const LANGUAGES: Record<string, string> = { fr: "Français", de: "Deutsch", en: "English", it: "Italiano" };
