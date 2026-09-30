"use client";

import { useActionState } from "react";
import { joinWaitlist, type WaitlistState } from "@/app/actions";

const initialState: WaitlistState = { status: "idle" };

export function WaitlistForm({
  id,
  tone = "light",
  stacked = false,
}: {
  id?: string;
  tone?: "light" | "dark";
  stacked?: boolean;
}) {
  const [state, formAction, pending] = useActionState(joinWaitlist, initialState);
  const dark = tone === "dark";

  if (state.status === "ok") {
    return (
      <p
        role="status"
        className={`rounded-md border px-4 py-3 text-sm ${dark ? "border-iris/50 text-[#efe7da]" : "border-brass/40 bg-brass/10"}`}
      >
        C&apos;est noté. On vous écrit le jour où elle ouvre les yeux.
      </p>
    );
  }

  return (
    <form action={formAction} className="w-full max-w-md">
      <div className={`flex flex-col gap-2 ${stacked ? "" : "sm:flex-row"}`}>
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
          suppressHydrationWarning
          className={`h-12 min-h-12 rounded-md border px-4 ${stacked ? "w-full" : "sm:flex-1"} text-base focus-visible:outline-2 focus-visible:outline-offset-2 ${
            dark
              ? "border-[#efe7da]/30 bg-transparent text-[#efe7da] placeholder:text-[#efe7da]/55 focus-visible:outline-iris"
              : "border-input bg-popover placeholder:text-muted-foreground focus-visible:outline-brass"
          }`}
        />
        <button
          type="submit"
          disabled={pending}
          className={`h-12 shrink-0 rounded-md px-5 text-sm font-semibold transition-colors disabled:opacity-60 focus-visible:outline-2 focus-visible:outline-offset-2 ${
            dark
              ? "bg-[#efe7da] text-walnut hover:bg-white focus-visible:outline-iris"
              : "bg-foreground text-background hover:bg-[#33403f] focus-visible:outline-brass"
          }`}
        >
          {pending ? "Inscription…" : "Me prévenir"}
        </button>
      </div>
      <p role="alert" aria-live="polite" className={`mt-2 min-h-5 text-sm ${dark ? "text-[#ffb4a6]" : "text-destructive"}`}>
        {state.status === "error" ? state.message : ""}
      </p>
    </form>
  );
}
