import type { Metadata } from "next";
import Link from "next/link";
import { AuthForm } from "@/components/auth-form";

export const metadata: Metadata = {
  title: "Connexion — EscapeBox",
};

export default function LoginPage() {
  return (
    <main className="flex flex-1 items-center justify-center px-6 py-16">
      <div className="w-full max-w-sm">
        <Link
          href="/"
          className="font-mono text-xs tracking-widest text-muted-foreground transition-colors hover:text-foreground"
        >
          <span className="text-primary">◉◉</span> ESCAPEBOX
        </Link>
        <div className="mt-6 rounded-2xl border border-border bg-card p-8">
          <p className="font-mono text-xs tracking-[0.25em] text-glow">
            ACCÈS JOUEUR
          </p>
          <h1 className="mt-3 font-display text-xl font-medium">
            Elle vous attend.
          </h1>
          <div className="mt-6">
            <AuthForm mode="login" />
          </div>
        </div>
        <p className="mt-4 text-center text-sm text-muted-foreground">
          Pas encore de compte ?{" "}
          <Link
            href="/register"
            className="text-primary underline-offset-4 hover:underline"
          >
            Créer un compte
          </Link>
        </p>
      </div>
    </main>
  );
}
