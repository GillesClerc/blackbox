"use client";

import { useRouter } from "next/navigation";
import { useState } from "react";
import { adminApi } from "./api";
import { Errors } from "./errors";

// Offrir l'histoire à un compte existant (par son e-mail).
export function LicenseGrant({ scenarioId }: { scenarioId: string }) {
  const router = useRouter();
  const [email, setEmail] = useState("");
  const [note, setNote] = useState("");
  const [errors, setErrors] = useState<string[]>([]);
  const [busy, setBusy] = useState(false);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    setBusy(true);
    setErrors([]);
    const r = await adminApi(`/api/admin/scenarios/${scenarioId}/licenses`, {
      method: "POST",
      json: { email: email.trim(), source: "gift", note: note.trim() || null },
    });
    setBusy(false);
    if (!r.ok) return setErrors(r.errors);
    setEmail("");
    setNote("");
    router.refresh();
  }

  return (
    <form onSubmit={submit} className="flex flex-col gap-3 sm:flex-row sm:items-start">
      <input
        type="email"
        required
        value={email}
        onChange={(e) => setEmail(e.target.value)}
        placeholder="adresse@du.compte"
        className="rounded-lg border border-border bg-background px-3 py-2 text-sm sm:w-64"
      />
      <input
        value={note}
        onChange={(e) => setNote(e.target.value)}
        placeholder="Note (facultatif)"
        maxLength={500}
        className="rounded-lg border border-border bg-background px-3 py-2 text-sm sm:flex-1"
      />
      <button
        type="submit"
        disabled={busy}
        className="rounded-lg bg-foreground px-4 py-2 text-sm font-medium text-background transition-opacity hover:opacity-85 disabled:opacity-50"
      >
        Offrir l&apos;histoire
      </button>
      <div className="sm:basis-full">
        <Errors errors={errors} />
      </div>
    </form>
  );
}
