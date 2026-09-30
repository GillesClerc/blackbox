import Link from "next/link";

export function SiteFooter() {
  return (
    <footer className="mt-auto bg-walnut text-[#efe7da]">
      <div className="mx-auto flex w-full max-w-6xl flex-col gap-6 px-6 py-10 sm:flex-row sm:items-end sm:justify-between">
        <div>
          <p className="font-display text-xl">EscapeBox</p>
          <p className="mt-1 max-w-xs text-sm text-[#efe7da]/70">
            Une boîte d&apos;escape game à poser au milieu de la table. En préparation, en Suisse.
          </p>
        </div>
        <nav aria-label="Informations" className="flex flex-wrap gap-x-5 gap-y-2 text-sm text-[#efe7da]/80">
          <Link href="/faq" className="hover:text-white focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-[#efe7da]">
            Questions
          </Link>
          <Link href="/confidentialite" className="hover:text-white focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-[#efe7da]">
            Confidentialité
          </Link>
          <Link href="/mentions-legales" className="hover:text-white focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-[#efe7da]">
            Mentions légales
          </Link>
        </nav>
      </div>
    </footer>
  );
}
