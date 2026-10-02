import Link from "next/link";
import { PISTES } from "./content";

// Barre flottante pour passer d'une piste à l'autre (pages d'essai seulement).
export function ProtoBar({ current }: { current: string }) {
  return (
    <nav
      aria-label="Pistes de design"
      className="fixed inset-x-0 bottom-3 z-50 mx-auto flex w-fit max-w-[calc(100%-1.5rem)] items-center gap-1 overflow-x-auto rounded-full bg-neutral-950/85 p-1 text-[13px] text-white shadow-lg backdrop-blur"
      style={{ fontFamily: "system-ui, sans-serif" }}
    >
      <Link href="/proto" className="rounded-full px-3 py-1.5 text-white/70 hover:text-white">
        Pistes
      </Link>
      {PISTES.map((p) => (
        <Link
          key={p.slug}
          href={`/proto/${p.slug}`}
          aria-current={p.slug === current ? "page" : undefined}
          className={`whitespace-nowrap rounded-full px-3 py-1.5 ${
            p.slug === current ? "bg-white text-neutral-950" : "text-white/80 hover:bg-white/10"
          }`}
        >
          {p.letter} · {p.name}
        </Link>
      ))}
    </nav>
  );
}
