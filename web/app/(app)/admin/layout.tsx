import { checkAdmin } from "@/lib/admin/auth";

// Garde du back-office : réservé à profiles.role = 'admin' (la connexion est
// déjà exigée par le layout (app) et par proxy.ts).
export default async function AdminLayout({ children }: { children: React.ReactNode }) {
  const check = await checkAdmin();
  if (!check.ok) {
    return (
      <main className="mx-auto w-full max-w-5xl flex-1 px-6 py-12">
        <p className="font-mono text-xs tracking-[0.25em] text-glow">ACCÈS RÉSERVÉ</p>
        <h1 className="mt-3 font-display text-2xl font-medium">Cette porte reste fermée</h1>
        <p className="mt-4 max-w-md text-sm text-muted-foreground">
          Le back-office est réservé aux administrateurs.
        </p>
      </main>
    );
  }
  return <>{children}</>;
}
