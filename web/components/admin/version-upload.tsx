"use client";

import { useRouter } from "next/navigation";
import { useState } from "react";
import { adminApi } from "./api";
import { Errors } from "./errors";

// Dépôt d'un zip du dossier de l'histoire → nouvelle version en brouillon.
export function VersionUpload({ scenarioId }: { scenarioId: string }) {
  const router = useRouter();
  const [errors, setErrors] = useState<string[]>([]);
  const [warnings, setWarnings] = useState<string[]>([]);
  const [done, setDone] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  async function submit(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    const form = new FormData(e.currentTarget);
    setBusy(true);
    setErrors([]);
    setWarnings([]);
    setDone(null);
    const r = await adminApi<{ version: number; warnings: string[] }>(
      `/api/admin/scenarios/${scenarioId}/versions`,
      { method: "POST", form }
    );
    setBusy(false);
    if (!r.ok) return setErrors(r.errors);
    setWarnings(r.data.warnings);
    setDone(`Version ${r.data.version} déposée en brouillon.`);
    (e.target as HTMLFormElement).reset();
    router.refresh();
  }

  return (
    <form onSubmit={submit} className="flex flex-col gap-3 rounded-2xl border border-border bg-card p-5">
      <p className="text-sm text-muted-foreground">
        Zip du dossier de l&apos;histoire : <span className="font-mono">scenario.json</span> à la racine, sons en MP3
        44,1 kHz (<span className="font-mono">ambient.mp3</span>, <span className="font-mono">audio/…</span>). Le
        manifest est calculé ici. 100 Mo maximum.
      </p>
      <input
        name="file"
        type="file"
        accept=".zip,application/zip"
        required
        className="text-sm file:mr-3 file:rounded-lg file:border-0 file:bg-secondary file:px-3 file:py-2 file:text-sm"
      />
      <input
        name="notes"
        placeholder="Notes de version (facultatif)"
        maxLength={500}
        className="rounded-lg border border-border bg-background px-3 py-2 text-sm"
      />
      <div>
        <button
          type="submit"
          disabled={busy}
          className="rounded-lg bg-foreground px-4 py-2 text-sm font-medium text-background transition-opacity hover:opacity-85 disabled:opacity-50"
        >
          {busy ? "Vérification et envoi…" : "Déposer une nouvelle version"}
        </button>
      </div>
      <Errors errors={errors} />
      {warnings.length > 0 && (
        <ul className="rounded-lg border border-border bg-secondary/40 p-3 text-sm">
          {warnings.map((w) => (
            <li key={w}>⚠ {w}</li>
          ))}
        </ul>
      )}
      {done && <p className="text-sm">{done}</p>}
    </form>
  );
}
