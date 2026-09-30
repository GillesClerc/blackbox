import type { Metadata } from "next";
import Link from "next/link";
import { createAdminClient } from "@/lib/supabase/admin";

export const metadata: Metadata = { title: "Inventaire — EscapeBox" };
export const dynamic = "force-dynamic";

const STATUS: Record<string, string> = { draft: "Brouillon", published: "Publiée", archived: "Archivée" };

// Inventaire des histoires (accès vérifié par admin/layout.tsx).
export default async function AdminPage() {
  const admin = createAdminClient();
  const { data: scenarios } = await admin
    .from("scenarios")
    .select(
      "id, slug, title, status, theme, updated_at, current_version:scenario_versions!scenarios_current_version_id_fkey(version), versions:scenario_versions!scenario_versions_scenario_id_fkey(count), licenses(count)"
    )
    .is("licenses.revoked_at", null)
    .order("title");

  return (
    <main className="mx-auto w-full max-w-5xl flex-1 px-6 py-12">
      <div className="flex items-end justify-between">
        <div>
          <p className="font-mono text-xs tracking-[0.25em] text-glow">BACK-OFFICE</p>
          <h1 className="mt-3 font-display text-2xl font-medium">Inventaire des histoires</h1>
        </div>
        <Link
          href="/admin/scenarios/new"
          className="rounded-lg bg-foreground px-4 py-2 text-sm font-medium text-background transition-opacity hover:opacity-85"
        >
          Nouvelle histoire
        </Link>
      </div>

      {!scenarios?.length ? (
        <p className="mt-8 text-sm text-muted-foreground">Aucune histoire pour l&apos;instant.</p>
      ) : (
        <div className="mt-8 overflow-x-auto rounded-2xl border border-border">
          <table className="w-full text-sm">
            <thead className="bg-card text-left font-mono text-xs tracking-[0.15em] text-muted-foreground">
              <tr>
                <th className="px-4 py-3 font-normal">HISTOIRE</th>
                <th className="px-4 py-3 font-normal">STATUT</th>
                <th className="px-4 py-3 font-normal">VERSION PUBLIÉE</th>
                <th className="px-4 py-3 font-normal">VERSIONS</th>
                <th className="px-4 py-3 font-normal">LICENCES ACTIVES</th>
              </tr>
            </thead>
            <tbody>
              {scenarios.map((s) => {
                const cv = s.current_version as unknown as { version: number } | null;
                const nVersions = (s.versions as unknown as { count: number }[])?.[0]?.count ?? 0;
                const nLicenses = (s.licenses as unknown as { count: number }[])?.[0]?.count ?? 0;
                return (
                  <tr key={s.id} className="border-t border-border">
                    <td className="px-4 py-3">
                      <Link href={`/admin/scenarios/${s.id}`} className="font-medium hover:text-primary">
                        {s.title}
                      </Link>
                      <p className="font-mono text-xs text-muted-foreground">{s.slug}</p>
                    </td>
                    <td className="px-4 py-3">{STATUS[s.status] ?? s.status}</td>
                    <td className="px-4 py-3 tabular-nums">{cv ? `v${cv.version}` : "—"}</td>
                    <td className="px-4 py-3 tabular-nums">{nVersions}</td>
                    <td className="px-4 py-3 tabular-nums">{nLicenses}</td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      )}
    </main>
  );
}
