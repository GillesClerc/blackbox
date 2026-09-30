// Désindexation pilotée par l'environnement (lue à chaque requête, pas au build) :
// SITE_NOINDEX=1 sur le staging, et sur la prod tant que la marque n'est pas choisie.
// Effet : en-tête X-Robots-Tag sur toutes les pages (proxy.ts) + robots.txt « Disallow: / ».
export function siteNoIndex(): boolean {
  const v = process.env.SITE_NOINDEX?.trim().toLowerCase();
  return v === "1" || v === "true" || v === "yes";
}

export const NOINDEX_HEADER = "noindex, nofollow, noarchive";
