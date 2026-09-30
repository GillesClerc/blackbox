import { SiteFooter } from "@/components/site/site-footer";
import { SiteHeader } from "@/components/site/site-header";
import { createClient } from "@/lib/supabase/server";

// Pages publiques : en-tête (« Mon espace » si connecté) et pied de page en noyer.
export default async function MarketingLayout({ children }: { children: React.ReactNode }) {
  let signedIn = false;
  try {
    const supabase = await createClient();
    const { data } = await supabase.auth.getUser();
    signedIn = !!data.user;
  } catch {
    // base injoignable : on affiche simplement « Se connecter »
  }
  return (
    <div className="flex min-h-full flex-1 flex-col">
      <SiteHeader signedIn={signedIn} />
      {children}
      <SiteFooter />
    </div>
  );
}
