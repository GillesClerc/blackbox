"use client";

import { useActionState } from "react";
import { joinWaitlist, type WaitlistState } from "@/app/actions";

const initialState: WaitlistState = { status: "idle" };

export function WaitlistForm({ id }: { id?: string }) {
  const [state, formAction, pending] = useActionState(
    joinWaitlist,
    initialState
  );

  if (state.status === "ok") {
    return (
      <p
        className="rounded-lg border border-primary/40 bg-primary/10 px-4 py-3 text-sm text-foreground"
        role="status"
      >
        Vous y êtes. On vous écrit dès que la boîte ouvre les yeux.
      </p>
    );
  }

  return (
    <form action={formAction} className="w-full max-w-md">
      <div className="flex flex-col gap-2 sm:flex-row">
        <label htmlFor={id ?? "email"} className="sr-only">
          Adresse e-mail
        </label>
        <input
          id={id ?? "email"}
          name="email"
          type="email"
          required
          autoComplete="email"
          placeholder="vous@exemple.ch"
          className="h-12 flex-1 rounded-lg border border-input bg-night/60 px-4 text-base text-foreground placeholder:text-muted-foreground focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-ring"
        />
        <button
          type="submit"
          disabled={pending}
          className="h-12 shrink-0 rounded-lg bg-primary px-6 font-mono text-sm font-bold tracking-wide text-primary-foreground transition-colors hover:bg-[#f0b558] disabled:opacity-60 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-ring"
        >
          {pending ? "Inscription…" : "Rejoindre la liste"}
        </button>
      </div>
      <p
        className="mt-2 min-h-5 text-sm text-destructive"
        role="alert"
        aria-live="polite"
      >
        {state.status === "error" ? state.message : ""}
      </p>
    </form>
  );
}
