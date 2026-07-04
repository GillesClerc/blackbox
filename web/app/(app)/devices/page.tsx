import type { Metadata } from "next";
import Link from "next/link";
import { createClient } from "@/lib/supabase/server";

export const metadata: Metadata = {
  title: "Mes box — EscapeBox",
};

// Liste des box du compte (RLS « own devices » : le client session suffit).
export default async function DevicesPage() {
  const supabase = await createClient();
  const { data: devices } = await supabase
    .from("devices")
    .select("id, box_uid, name, firmware_version, last_sync_at")
    .order("created_at", { ascending: true });

  const fmt = new Intl.DateTimeFormat("fr-CH", {
    dateStyle: "medium",
    timeStyle: "short",
  });

  return (
    <main className="mx-auto w-full max-w-5xl flex-1 px-6 py-12">
      <div className="flex items-end justify-between">
        <div>
          <p className="font-mono text-xs tracking-[0.25em] text-glow">
            VOS BOX
          </p>
          <h1 className="mt-3 font-display text-2xl font-medium">Mes box</h1>
        </div>
        <Link
          href="/devices/add"
          className="rounded-lg bg-foreground px-4 py-2 text-sm font-medium text-background transition-opacity hover:opacity-85"
        >
          Ajouter une box
        </Link>
      </div>

      {!devices?.length ? (
        <div className="mt-8 max-w-md rounded-2xl border border-border bg-card p-6 text-sm text-muted-foreground">
          Aucune box rattachée à ce compte pour l&apos;instant. Allumez votre
          box, ouvrez sa fenêtre d&apos;appairage depuis son menu, puis cliquez
          sur « Ajouter une box ».
        </div>
      ) : (
        <div className="mt-8 grid gap-4 sm:grid-cols-2">
          {devices.map((d) => (
            <div
              key={d.id}
              className="rounded-2xl border border-border bg-card p-6"
            >
              <p className="text-lg font-medium">{d.name ?? "EscapeBox"}</p>
              <dl className="mt-4 flex flex-col gap-3">
                <div>
                  <dt className="font-mono text-xs tracking-[0.2em] text-muted-foreground">
                    IDENTIFIANT
                  </dt>
                  <dd className="mt-1 font-mono text-sm">{d.box_uid}</dd>
                </div>
                <div>
                  <dt className="font-mono text-xs tracking-[0.2em] text-muted-foreground">
                    FIRMWARE
                  </dt>
                  <dd className="mt-1 text-sm">
                    {d.firmware_version ?? "inconnu"}
                  </dd>
                </div>
                <div>
                  <dt className="font-mono text-xs tracking-[0.2em] text-muted-foreground">
                    DERNIÈRE SYNCHRO
                  </dt>
                  <dd className="mt-1 text-sm">
                    {d.last_sync_at
                      ? fmt.format(new Date(d.last_sync_at))
                      : "jamais"}
                  </dd>
                </div>
              </dl>
            </div>
          ))}
        </div>
      )}
    </main>
  );
}
