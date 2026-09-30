import type { MetadataRoute } from "next";
import { siteNoIndex } from "@/lib/indexing";

// Dynamique : SITE_NOINDEX est lu à la requête, un changement de variable dans
// Coolify ne demande qu'un redémarrage, pas un nouveau build.
export const dynamic = "force-dynamic";

export default function robots(): MetadataRoute.Robots {
  if (siteNoIndex()) {
    return { rules: { userAgent: "*", disallow: "/" } };
  }
  // Les espaces privés et l'API box n'ont rien à faire dans un index.
  return {
    rules: {
      userAgent: "*",
      allow: "/",
      disallow: ["/api/", "/account", "/devices", "/library", "/checkout", "/admin"],
    },
  };
}
