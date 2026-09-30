import { createServerClient } from "@supabase/ssr";
import { NextResponse, type NextRequest } from "next/server";
import { NOINDEX_HEADER, siteNoIndex } from "@/lib/indexing";

// Les pages protégées vivent dans le route group (app)/ → servies à la racine
// (/library, /devices, ...). Un route group NE crée PAS de segment d'URL :
// tester startsWith('/app') ne matcherait jamais. On liste les préfixes réels.
const PROTECTED = [
  "/shop",
  "/library",
  "/devices",
  "/scores",
  "/account",
  "/checkout",
  "/studio",
];

export async function proxy(request: NextRequest) {
  let supabaseResponse = NextResponse.next({ request });

  const supabase = createServerClient(
    process.env.NEXT_PUBLIC_SUPABASE_URL!,
    process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY!,
    {
      cookies: {
        getAll: () => request.cookies.getAll(),
        setAll: (cookiesToSet) => {
          cookiesToSet.forEach(({ name, value }) =>
            request.cookies.set(name, value)
          );
          supabaseResponse = NextResponse.next({ request });
          cookiesToSet.forEach(({ name, value, options }) =>
            supabaseResponse.cookies.set(name, value, options)
          );
        },
      },
    }
  );

  const {
    data: { user },
  } = await supabase.auth.getUser();
  const { pathname } = request.nextUrl;

  if (
    !user &&
    PROTECTED.some((p) => pathname === p || pathname.startsWith(p + "/"))
  ) {
    const url = request.nextUrl.clone();
    url.pathname = "/login";
    return withIndexing(NextResponse.redirect(url));
  }

  // TODO Phase 3 : vérifier plan Pro+ pour /studio/*

  return withIndexing(supabaseResponse);
}

// Staging, et prod tant que la marque n'est pas choisie : aucune page indexée
// (SITE_NOINDEX, lu à chaque requête — voir lib/indexing.ts).
function withIndexing(response: NextResponse): NextResponse {
  if (siteNoIndex()) response.headers.set("X-Robots-Tag", NOINDEX_HEADER);
  return response;
}

export const config = {
  matcher: ["/((?!_next/static|_next/image|favicon.ico|api/).*)"],
};
