"use client";

import { useState } from "react";

// Champ d'inscription des pistes : aspect seulement, rien n'est envoyé.
export function ProtoSignup({
  inputClassName,
  buttonClassName,
  className = "",
  noteClassName = "",
  label = "Me prévenir",
}: {
  inputClassName: string;
  buttonClassName: string;
  className?: string;
  noteClassName?: string;
  label?: string;
}) {
  const [sent, setSent] = useState(false);
  return (
    <form
      className={className}
      onSubmit={(e) => {
        e.preventDefault();
        setSent(true);
      }}
    >
      <div className="flex flex-col gap-3 sm:flex-row">
        <label className="sr-only" htmlFor="proto-email">
          Adresse e-mail
        </label>
        <input id="proto-email" type="email" required placeholder="vous@exemple.ch" className={`min-h-12 min-w-0 ${inputClassName.replace(/\bflex-1\b/, "sm:flex-1")}`} />
        <button type="submit" className={`shrink-0 whitespace-nowrap ${buttonClassName}`}>
          {label}
        </button>
      </div>
      <p className={noteClassName} role="status">
        {sent ? "Page d'essai : rien n'a été envoyé." : "Pas encore en vente. Une adresse suffit, elle vous fera signe."}
      </p>
    </form>
  );
}
