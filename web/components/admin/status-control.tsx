"use client";

import { useRouter } from "next/navigation";
import { useState } from "react";
import { adminApi } from "./api";
import { Errors } from "./errors";

const LABELS: Record<string, string> = { draft: "Brouillon", published: "Publiée", archived: "Archivée" };

// Statut de l'histoire : seule une histoire publiée est servie aux box et au catalogue.
export function StatusControl({ id, status, hasVersion }: { id: string; status: string; hasVersion: boolean }) {
  const router = useRouter();
  const [errors, setErrors] = useState<string[]>([]);
  const [busy, setBusy] = useState(false);

  async function set(next: string) {
    setBusy(true);
    setErrors([]);
    const r = await adminApi(`/api/admin/scenarios/${id}`, { method: "PATCH", json: { status: next } });
    setBusy(false);
    if (!r.ok) return setErrors(r.errors);
    router.refresh();
  }

  return (
    <div>
      <div className="flex flex-wrap gap-2">
        {Object.entries(LABELS).map(([k, l]) => (
          <button
            key={k}
            type="button"
            disabled={busy || k === status || (k === "published" && !hasVersion)}
            onClick={() => set(k)}
            aria-pressed={k === status}
            className={`rounded-lg border px-3 py-1.5 text-sm transition-colors disabled:cursor-not-allowed ${
              k === status ? "border-primary bg-primary/15 text-foreground" : "border-border text-muted-foreground hover:text-foreground disabled:opacity-40"
            }`}
          >
            {l}
          </button>
        ))}
      </div>
      {!hasVersion && (
        <p className="mt-2 text-xs text-muted-foreground">Publiez une version pour pouvoir publier l&apos;histoire.</p>
      )}
      <Errors errors={errors} />
    </div>
  );
}
