import type { MetadataRoute } from "next";
import { siteBlockCrawl } from "@/lib/indexing";

// Dynamique : SITE_BLOCK_CRAWL est lu à la requête, pas figé au build. (Sous Coolify,
// une nouvelle valeur n'arrive au serveur qu'après un Redeploy, pas un Restart.)
export const dynamic = "force-dynamic";

export default function robots(): MetadataRoute.Robots {
  if (siteBlockCrawl()) {
    return { rules: { userAgent: "*", disallow: "/" } };
  }
  // Les espaces privés et l'API box n'ont rien à faire dans un index. La
  // désindexation de la prod (SITE_NOINDEX) passe par l'en-tête, pas par ici.
  return {
    rules: {
      userAgent: "*",
      allow: "/",
      disallow: ["/api/", "/account", "/devices", "/library", "/checkout", "/admin"],
    },
  };
}
