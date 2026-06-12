import type { Metadata } from "next";
import { createClient } from "@/lib/supabase/server";

export const metadata: Metadata = {
  title: "Compte — EscapeBox",
};

export default async function AccountPage() {
  const supabase = await createClient();
  const {
    data: { user },
  } = await supabase.auth.getUser();

  const memberSince = user?.created_at
    ? new Intl.DateTimeFormat("fr-CH", { dateStyle: "long" }).format(
        new Date(user.created_at)
      )
    : null;

  return (
    <main className="mx-auto w-full max-w-5xl flex-1 px-6 py-12">
      <p className="font-mono text-xs tracking-[0.25em] text-glow">
        VOTRE COMPTE
      </p>
      <h1 className="mt-3 font-display text-2xl font-medium">
        Bienvenue parmi les joueurs
      </h1>

      <div className="mt-8 max-w-md rounded-2xl border border-border bg-card p-6">
        <dl className="flex flex-col gap-4">
          <div>
            <dt className="font-mono text-xs tracking-[0.2em] text-muted-foreground">
              E-MAIL
            </dt>
            <dd className="mt-1 text-foreground">{user?.email}</dd>
          </div>
          {memberSince && (
            <div>
              <dt className="font-mono text-xs tracking-[0.2em] text-muted-foreground">
                MEMBRE DEPUIS
              </dt>
              <dd className="mt-1 text-foreground">{memberSince}</dd>
            </div>
          )}
        </dl>
      </div>

      <div className="mt-6 max-w-md rounded-2xl border border-dashed border-primary/30 p-6">
        <p className="font-mono text-xs tracking-[0.2em] text-primary">
          BIENTÔT ICI
        </p>
        <p className="mt-2 text-sm leading-relaxed text-muted-foreground">
          Vos box, vos scénarios et vos scores apparaîtront sur cette page.
          La boîte garde encore quelques secrets.
        </p>
      </div>
    </main>
  );
}
