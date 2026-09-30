import Link from "next/link";
import { redirect } from "next/navigation";
import { createClient } from "@/lib/supabase/server";
import { signOut } from "@/app/(auth)/actions";
import { Logo } from "@/components/site/logo";

export default async function AppLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  const supabase = await createClient();
  const {
    data: { user },
  } = await supabase.auth.getUser();

  if (!user) {
    redirect("/login");
  }

  // Lien ADMIN seulement pour les administrateurs (RLS : lecture de son profil).
  const { data: profile } = await supabase
    .from("profiles")
    .select("role")
    .eq("id", user.id)
    .maybeSingle();
  const isAdmin = profile?.role === "admin";

  const link =
    "rounded-sm px-2 py-1 text-sm text-foreground/75 transition-colors hover:text-foreground focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-brass";

  return (
    <div className="flex min-h-full flex-1 flex-col">
      <header className="border-b border-foreground/10">
        <div className="mx-auto flex w-full max-w-5xl flex-wrap items-center justify-between gap-x-6 gap-y-3 px-6 py-4">
          <Logo />
          <nav aria-label="Espace joueur" className="flex flex-wrap items-center gap-x-4 gap-y-1">
            <Link href="/devices" className={link}>
              Mes box
            </Link>
            <Link href="/account" className={link}>
              Compte
            </Link>
            {isAdmin && (
              <Link href="/admin" className={`${link} font-semibold text-brass`}>
                Admin
              </Link>
            )}
            <form action={signOut}>
              <button type="submit" className={link}>
                Se déconnecter
              </button>
            </form>
          </nav>
        </div>
      </header>
      {children}
    </div>
  );
}
