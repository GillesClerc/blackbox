import type { Metadata } from "next";
import Link from "next/link";
import { AuthForm } from "@/components/auth-form";
import { Logo } from "@/components/site/logo";

export const metadata: Metadata = {
  title: "Connexion",
};

export default function LoginPage() {
  return (
    <main className="flex flex-1 items-center justify-center px-6 py-16">
      <div className="w-full max-w-sm">
        <Logo />
        <div className="mt-6 rounded-lg border border-foreground/15 bg-card p-8">
          <p className="font-mono text-[0.7rem] tracking-[0.24em] text-brass">
            ACCÈS JOUEUR
          </p>
          <h1 className="mt-3 font-display text-3xl leading-tight">
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
            className="font-semibold text-brass underline-offset-4 hover:underline"
          >
            Créer un compte
          </Link>
        </p>
      </div>
    </main>
  );
}
