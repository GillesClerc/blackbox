import type { Metadata } from "next";
import Link from "next/link";
import { notFound } from "next/navigation";
import { ActionButton } from "@/components/admin/action-button";
import { LicenseGrant } from "@/components/admin/license-grant";
import { ScenarioForm } from "@/components/admin/scenario-form";
import { StatusControl } from "@/components/admin/status-control";
import { VersionUpload } from "@/components/admin/version-upload";
import { UUID_RE } from "@/lib/admin/http";
import { createAdminClient } from "@/lib/supabase/admin";

export const metadata: Metadata = { title: "Histoire — EscapeBox" };
export const dynamic = "force-dynamic";

const VSTATUS: Record<string, string> = { draft: "Brouillon", published: "Publiée", retired: "Retirée" };
const SOURCE: Record<string, string> = { purchase: "Achat", gift: "Cadeau", admin: "Admin", code: "Code" };

const fmt = new Intl.DateTimeFormat("fr-CH", { dateStyle: "medium", timeStyle: "short" });
const size = (b: number | null) => (b == null ? "—" : b > 1e6 ? `${(b / 1e6).toFixed(1)} Mo` : `${Math.ceil(b / 1e3)} Ko`);

export default async function ScenarioAdminPage({ params }: { params: Promise<{ id: string }> }) {
  const { id } = await params;
  if (!UUID_RE.test(id)) notFound();
  const admin = createAdminClient();

  const { data: s } = await admin.from("scenarios").select("*").eq("id", id).maybeSingle();
  if (!s) notFound();

  const [{ data: versions }, { data: licenses }] = await Promise.all([
    admin
      .from("scenario_versions")
      .select("id, version, status, total_bytes, storage, notes, created_at, published_at")
      .eq("scenario_id", id)
      .order("version", { ascending: false }),
    admin
      .from("licenses")
      .select("id, source, note, created_at, revoked_at, profile:profiles!licenses_user_id_fkey(email)")
      .eq("scenario_id", id)
      .order("created_at", { ascending: false }),
  ]);

  const section = "mt-12 border-t border-border pt-8";
  const eyebrow = "font-mono text-xs tracking-[0.25em] text-glow";

  return (
    <main className="mx-auto w-full max-w-4xl flex-1 px-6 py-12">
      <Link href="/admin" className="font-mono text-xs tracking-[0.2em] text-muted-foreground hover:text-foreground">
        ← INVENTAIRE
      </Link>
      <h1 className="mt-3 font-display text-2xl font-medium">{s.title}</h1>
      <p className="font-mono text-xs text-muted-foreground">{s.slug}</p>

      <div className="mt-6">
        <StatusControl id={s.id} status={s.status} hasVersion={!!s.current_version_id} />
      </div>

      <section className={section}>
        <p className={eyebrow}>FICHE</p>
        <div className="mt-4">
          <ScenarioForm
            id={s.id}
            initial={{
              title: s.title, summary: s.summary, description: s.description, theme: s.theme,
              ambiance: s.ambiance, difficulty: s.difficulty, duration_min: s.duration_min,
              min_players: s.min_players, max_players: s.max_players, min_age: s.min_age,
              language: s.language, price_chf: Number(s.price_chf),
            }}
          />
        </div>
      </section>

      <section className={section}>
        <p className={eyebrow}>VERSIONS</p>
        <p className="mt-2 text-sm text-muted-foreground">
          La version publiée est celle que reçoivent les box. Republier une ancienne version sert de retour arrière.
        </p>
        <div className="mt-4">
          <VersionUpload scenarioId={s.id} />
        </div>
        <ul className="mt-6 flex flex-col gap-3">
          {(versions ?? []).map((v) => (
            <li key={v.id} className="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-border p-4">
              <div>
                <p className="font-medium">
                  v{v.version}{" "}
                  <span className={`ml-2 text-xs ${v.id === s.current_version_id ? "text-primary" : "text-muted-foreground"}`}>
                    {v.id === s.current_version_id ? "● servie aux box" : VSTATUS[v.status] ?? v.status}
                  </span>
                </p>
                <p className="mt-1 text-xs text-muted-foreground">
                  {size(v.total_bytes)} · déposée le {fmt.format(new Date(v.created_at))}
                  {v.storage === "repo" ? " · fichiers dans le dépôt Git" : ""}
                  {v.notes ? ` · ${v.notes}` : ""}
                </p>
              </div>
              {v.id !== s.current_version_id && (
                <ActionButton
                  url={`/api/admin/versions/${v.id}/publish`}
                  label={v.status === "draft" ? "Publier" : "Revenir à cette version"}
                  confirmText={`Servir la v${v.version} aux box ?`}
                />
              )}
            </li>
          ))}
        </ul>
      </section>

      <section className={section}>
        <p className={eyebrow}>LICENCES</p>
        <p className="mt-2 text-sm text-muted-foreground">
          Une licence est liée à un compte : toutes les box de ce compte reçoivent l&apos;histoire (si elle est publiée).
        </p>
        <div className="mt-4">
          <LicenseGrant scenarioId={s.id} />
        </div>
        <ul className="mt-6 flex flex-col gap-2">
          {(licenses ?? []).map((l) => {
            const email = (l.profile as unknown as { email: string | null } | null)?.email ?? "compte supprimé";
            return (
              <li key={l.id} className="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-border px-4 py-3 text-sm">
                <span className={l.revoked_at ? "text-muted-foreground line-through" : ""}>
                  {email} · {SOURCE[l.source] ?? l.source} · {fmt.format(new Date(l.created_at))}
                  {l.note ? ` · ${l.note}` : ""}
                </span>
                {l.revoked_at ? (
                  <span className="text-xs text-muted-foreground">révoquée</span>
                ) : (
                  <ActionButton
                    url={`/api/admin/licenses/${l.id}/revoke`}
                    label="Révoquer"
                    confirmText="Confirmer la révocation"
                    tone="danger"
                  />
                )}
              </li>
            );
          })}
        </ul>
      </section>
    </main>
  );
}
