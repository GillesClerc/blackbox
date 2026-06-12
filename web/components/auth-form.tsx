"use client";

import { useActionState } from "react";
import { signIn, signUp, type AuthState } from "@/app/(auth)/actions";

const initialState: AuthState = { status: "idle" };

const FIELD_CLASSES =
  "h-12 w-full rounded-lg border border-input bg-night/60 px-4 text-base text-foreground placeholder:text-muted-foreground focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-ring";

export function AuthForm({ mode }: { mode: "login" | "register" }) {
  const [state, formAction, pending] = useActionState(
    mode === "login" ? signIn : signUp,
    initialState
  );

  return (
    <form action={formAction} className="flex flex-col gap-4">
      <div>
        <label
          htmlFor="email"
          className="mb-2 block font-mono text-xs tracking-[0.2em] text-muted-foreground"
        >
          E-MAIL
        </label>
        <input
          id="email"
          name="email"
          type="email"
          required
          autoComplete="email"
          placeholder="vous@exemple.ch"
          suppressHydrationWarning
          className={FIELD_CLASSES}
        />
      </div>
      <div>
        <label
          htmlFor="password"
          className="mb-2 block font-mono text-xs tracking-[0.2em] text-muted-foreground"
        >
          MOT DE PASSE
        </label>
        <input
          id="password"
          name="password"
          type="password"
          required
          minLength={8}
          autoComplete={mode === "login" ? "current-password" : "new-password"}
          placeholder={mode === "register" ? "8 caractères minimum" : "••••••••"}
          suppressHydrationWarning
          className={FIELD_CLASSES}
        />
      </div>
      <button
        type="submit"
        disabled={pending}
        className="mt-2 h-12 rounded-lg bg-primary px-6 font-mono text-sm font-bold tracking-wide text-primary-foreground transition-colors hover:bg-[#f0b558] disabled:opacity-60 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-ring"
      >
        {pending
          ? "Un instant…"
          : mode === "login"
            ? "Se connecter"
            : "Créer le compte"}
      </button>
      <p
        className="min-h-5 text-sm text-destructive"
        role="alert"
        aria-live="polite"
      >
        {state.status === "error" ? state.message : ""}
      </p>
    </form>
  );
}
