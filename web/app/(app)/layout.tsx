import Link from "next/link";
import { redirect } from "next/navigation";
import { createClient } from "@/lib/supabase/server";
import { signOut } from "@/app/(auth)/actions";

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

  return (
    <div className="flex min-h-full flex-1 flex-col">
      <header className="border-b border-border">
        <div className="mx-auto flex w-full max-w-5xl items-center justify-between px-6 py-5">
          <Link
            href="/"
            className="font-mono text-sm font-bold tracking-widest transition-colors hover:text-primary"
          >
            <span className="text-primary">◉◉</span> ESCAPEBOX
          </Link>
          <nav className="flex items-center gap-2">
            <Link
              href="/account"
              className="rounded-md px-3 py-2 font-mono text-xs tracking-wider text-muted-foreground transition-colors hover:text-foreground focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-ring"
            >
              COMPTE
            </Link>
            <form action={signOut}>
              <button
                type="submit"
                className="rounded-md px-3 py-2 font-mono text-xs tracking-wider text-muted-foreground transition-colors hover:text-foreground focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-ring"
              >
                DÉCONNEXION
              </button>
            </form>
          </nav>
        </div>
      </header>
      {children}
    </div>
  );
}
