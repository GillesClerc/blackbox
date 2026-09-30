"use client";

import { useRouter } from "next/navigation";
import { useState } from "react";
import { adminApi } from "./api";
import { Errors } from "./errors";

export type ScenarioFormValues = {
  slug?: string;
  title: string;
  summary: string | null;
  description: string | null;
  theme: string | null;
  ambiance: string | null;
  difficulty: number | null;
  duration_min: number | null;
  min_players: number | null;
  max_players: number | null;
  min_age: number | null;
  language: string;
  price_chf: number;
  status?: string;
};

const EMPTY: ScenarioFormValues = {
  slug: "", title: "", summary: null, description: null, theme: null, ambiance: null,
  difficulty: null, duration_min: null, min_players: null, max_players: null, min_age: null,
  language: "fr", price_chf: 0,
};

const input =
  "mt-1 w-full rounded-lg border border-border bg-background px-3 py-2 text-sm focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-ring";
const label = "font-mono text-xs tracking-[0.2em] text-muted-foreground";

// Création (sans id) ou modification (avec id) de la fiche d'une histoire.
export function ScenarioForm({ id, initial }: { id?: string; initial?: ScenarioFormValues }) {
  const router = useRouter();
  const [v, setV] = useState<ScenarioFormValues>(initial ?? EMPTY);
  const [errors, setErrors] = useState<string[]>([]);
  const [saved, setSaved] = useState(false);
  const [busy, setBusy] = useState(false);

  const text = (k: keyof ScenarioFormValues) => ({
    value: (v[k] as string | null) ?? "",
    onChange: (e: React.ChangeEvent<HTMLInputElement | HTMLTextAreaElement | HTMLSelectElement>) =>
      setV({ ...v, [k]: e.target.value === "" ? null : e.target.value }),
  });
  const num = (k: keyof ScenarioFormValues) => ({
    type: "number" as const,
    value: v[k] === null || v[k] === undefined ? "" : String(v[k]),
    onChange: (e: React.ChangeEvent<HTMLInputElement>) =>
      setV({ ...v, [k]: e.target.value === "" ? null : Number(e.target.value) }),
  });

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    setBusy(true);
    setErrors([]);
    setSaved(false);
    const payload = { ...v, price_chf: Number(v.price_chf ?? 0) };
    if (id) delete payload.slug;
    delete payload.status;
    const r = id
      ? await adminApi(`/api/admin/scenarios/${id}`, { method: "PATCH", json: payload })
      : await adminApi<{ id: string }>("/api/admin/scenarios", { method: "POST", json: payload });
    setBusy(false);
    if (!r.ok) return setErrors(r.errors);
    if (!id) router.push(`/admin/scenarios/${(r.data as { id: string }).id}`);
    else {
      setSaved(true);
      router.refresh();
    }
  }

  return (
    <form onSubmit={submit} className="grid gap-4 sm:grid-cols-2">
      {!id && (
        <label className="sm:col-span-2">
          <span className={label}>IDENTIFIANT (DÉFINITIF)</span>
          <input className={input} required pattern="[a-z0-9_-]{1,64}" placeholder="capitaine_verdier" {...text("slug")} />
        </label>
      )}
      <label className="sm:col-span-2">
        <span className={label}>TITRE</span>
        <input className={input} required maxLength={120} {...text("title")} />
      </label>
      <label className="sm:col-span-2">
        <span className={label}>PITCH (280 CAR. MAX)</span>
        <input className={input} maxLength={280} {...text("summary")} />
      </label>
      <label className="sm:col-span-2">
        <span className={label}>DESCRIPTION</span>
        <textarea className={input} rows={4} maxLength={4000} {...text("description")} />
      </label>
      <label>
        <span className={label}>THÈME</span>
        <input className={input} maxLength={80} placeholder="pirates" {...text("theme")} />
      </label>
      <label>
        <span className={label}>AMBIANCE</span>
        <input className={input} maxLength={160} placeholder="mystérieuse et espiègle" {...text("ambiance")} />
      </label>
      <label>
        <span className={label}>DIFFICULTÉ (1-5)</span>
        <input className={input} min={1} max={5} {...num("difficulty")} />
      </label>
      <label>
        <span className={label}>DURÉE (MIN)</span>
        <input className={input} min={5} max={600} {...num("duration_min")} />
      </label>
      <label>
        <span className={label}>JOUEURS MIN</span>
        <input className={input} min={1} max={20} {...num("min_players")} />
      </label>
      <label>
        <span className={label}>JOUEURS MAX</span>
        <input className={input} min={1} max={20} {...num("max_players")} />
      </label>
      <label>
        <span className={label}>ÂGE MINIMUM</span>
        <input className={input} min={0} max={99} {...num("min_age")} />
      </label>
      <label>
        <span className={label}>LANGUE</span>
        <select className={input} {...text("language")}>
          <option value="fr">Français</option>
          <option value="de">Deutsch</option>
          <option value="en">English</option>
          <option value="it">Italiano</option>
        </select>
      </label>
      <label>
        <span className={label}>PRIX (CHF)</span>
        <input className={input} min={0} max={1000} step="0.5" {...num("price_chf")} />
      </label>
      <div className="sm:col-span-2">
        <Errors errors={errors} />
        <div className="mt-2 flex items-center gap-3">
          <button
            type="submit"
            disabled={busy}
            className="rounded-lg bg-foreground px-4 py-2 text-sm font-medium text-background transition-opacity hover:opacity-85 disabled:opacity-50"
          >
            {busy ? "Enregistrement…" : id ? "Enregistrer la fiche" : "Créer l'histoire"}
          </button>
          {saved && <span className="text-sm text-muted-foreground">Fiche enregistrée.</span>}
        </div>
      </div>
    </form>
  );
}
