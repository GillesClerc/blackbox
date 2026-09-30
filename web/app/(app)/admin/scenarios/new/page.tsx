import type { Metadata } from "next";
import Link from "next/link";
import { ScenarioForm } from "@/components/admin/scenario-form";

export const metadata: Metadata = { title: "Nouvelle histoire" };

export default function NewScenarioPage() {
  return (
    <main className="mx-auto w-full max-w-3xl flex-1 px-6 py-12">
      <Link href="/admin" className="font-mono text-xs tracking-[0.2em] text-muted-foreground hover:text-foreground">
        ← INVENTAIRE
      </Link>
      <h1 className="mt-3 font-display text-2xl font-medium">Nouvelle histoire</h1>
      <p className="mt-2 text-sm text-muted-foreground">
        Elle naît en brouillon. Déposez ensuite une version, publiez-la, puis publiez l&apos;histoire.
      </p>
      <div className="mt-8">
        <ScenarioForm />
      </div>
    </main>
  );
}
