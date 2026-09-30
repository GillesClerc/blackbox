import type { Metadata } from "next";
import Link from "next/link";
import { AuthForm } from "@/components/auth-form";
import { Logo } from "@/components/site/logo";

export const metadata: Metadata = {
  title: "Créer un compte",
};

export default function RegisterPage() {
  return (
    <main className="flex flex-1 items-center justify-center px-6 py-16">
      <div className="w-full max-w-sm">
        <Logo />
        <div className="mt-6 rounded-lg border border-foreground/15 bg-card p-8">
          <p className="font-mono text-[0.7rem] tracking-[0.24em] text-brass">
            PREMIÈRE RENCONTRE
          </p>
          <h1 className="mt-3 font-display text-3xl leading-tight">
            Créez votre compte joueur
          </h1>
          <div className="mt-6">
            <AuthForm mode="register" />
          </div>
        </div>
        <p className="mt-4 text-center text-sm text-muted-foreground">
          Déjà un compte ?{" "}
          <Link
            href="/login"
            className="font-semibold text-brass underline-offset-4 hover:underline"
          >
            Se connecter
          </Link>
        </p>
      </div>
    </main>
  );
}
