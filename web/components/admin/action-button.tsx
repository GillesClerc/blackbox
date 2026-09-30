"use client";

import { useRouter } from "next/navigation";
import { useState } from "react";
import { adminApi } from "./api";

// Bouton qui appelle une route admin (POST) puis rafraîchit la page.
export function ActionButton({
  url,
  label,
  confirmText,
  json,
  tone = "default",
}: {
  url: string;
  label: string;
  confirmText?: string;
  json?: unknown;
  tone?: "default" | "danger";
}) {
  const router = useRouter();
  const [armed, setArmed] = useState(false);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  async function run() {
    if (confirmText && !armed) {
      setArmed(true);
      return;
    }
    setBusy(true);
    setError(null);
    const r = await adminApi(url, { method: "POST", json: json ?? {} });
    setBusy(false);
    setArmed(false);
    if (!r.ok) return setError(r.errors.join(" · "));
    router.refresh();
  }

  const cls =
    tone === "danger"
      ? "border-destructive/50 text-destructive hover:bg-destructive/10"
      : "border-border text-foreground hover:bg-secondary";
  return (
    <span className="inline-flex flex-col items-start gap-1">
      <span className="inline-flex gap-2">
        <button
          type="button"
          onClick={run}
          disabled={busy}
          className={`rounded-lg border px-3 py-1.5 text-sm transition-colors disabled:opacity-50 ${cls}`}
        >
          {busy ? "…" : armed ? confirmText : label}
        </button>
        {armed && (
          <button type="button" onClick={() => setArmed(false)} className="text-sm text-muted-foreground hover:text-foreground">
            Annuler
          </button>
        )}
      </span>
      {error && <span role="alert" className="text-xs text-destructive">{error}</span>}
    </span>
  );
}
