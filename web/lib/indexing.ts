// Indexation pilotée par l'environnement (lue à chaque requête, pas au build).
//
// SITE_NOINDEX=1     → en-tête X-Robots-Tag « noindex » sur toutes les pages (proxy.ts).
//                      Prod tant que la marque n'est pas choisie. robots.txt reste ouvert :
//                      le robot doit pouvoir charger la page pour lire le noindex (un
//                      Disallow l'en empêcherait, et une URL liée ailleurs pourrait sortir
//                      dans les résultats sans description).
// SITE_BLOCK_CRAWL=1 → robots.txt « Disallow: / ». Staging uniquement (déjà derrière un
//                      mot de passe) ; à ne pas combiner avec SITE_NOINDEX en prod.
function envFlag(name: string): boolean {
  const v = process.env[name]?.trim().toLowerCase();
  return v === "1" || v === "true" || v === "yes";
}

export function siteNoIndex(): boolean {
  return envFlag("SITE_NOINDEX");
}

export function siteBlockCrawl(): boolean {
  return envFlag("SITE_BLOCK_CRAWL");
}

export const NOINDEX_HEADER = "noindex, nofollow, noarchive";
