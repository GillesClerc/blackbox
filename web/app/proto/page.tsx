import Link from "next/link";
import { PISTES } from "@/components/proto/content";

// Sommaire des pistes : neutre exprès, pour ne favoriser aucune direction.
const WHY: Record<string, string> = {
  veillee:
    "Un soir de jeux, lumière basse. La box s'efface dans la pénombre, il ne reste que son visage. Le pointeur devient une lampe : braquée sur ses yeux, elle contracte les pupilles et révèle des notes à l'encre invisible.",
  recre:
    "L'étagère à jeux de société. La box en personnage illustré, aplats francs et gros trait, qui sautille et réagit. Le contraste avec ses yeux réalistes la rend drôle et un peu troublante.",
  affiche:
    "Une affiche suisse, rouge et crème. Pas de box : le titre est le visage. Les O de « OUVREZ L'ŒIL » sont ses yeux, et la bouche parle sous le titre.",
  "veillee-v2":
    "La Veillée retenue, pour un public adulte. Visage plus discret qui change de personnage (forme de pupille, couleur d'iris). La lampe parcourt toute la page : à la souris, ou sur mobile un faisceau sous lequel on fait défiler. Plus bas, la box en 3D se présente par six énigmes.",
  atelier:
    "L'objet d'artisan qu'on garde sur l'étagère. Une vraie box en 3D, qu'on fait tourner, avec la finition au choix (Lite ou Pro), puisque la matière n'est pas encore arrêtée.",
};

export default function ProtoIndex() {
  return (
    <main className="mx-auto w-full max-w-3xl px-6 py-16" style={{ fontFamily: "system-ui, sans-serif" }}>
      <h1 className="text-3xl font-bold">Pistes de design — tour 1</h1>
      <p className="mt-3 text-neutral-600">
        Quatre directions pour l&apos;accueil. Mêmes textes partout, seul le style change. Les yeux sont ceux du firmware
        (Uncanny Eyes), rendus à l&apos;identique.
      </p>
      <ul className="mt-10 grid gap-4">
        {PISTES.map((p) => (
          <li key={p.slug}>
            <Link
              href={`/proto/${p.slug}`}
              className="block rounded-xl border border-neutral-300 bg-white p-5 transition hover:border-neutral-900"
            >
              <p className="text-sm text-neutral-500">
                Piste {p.letter} · {p.box}
              </p>
              <p className="mt-1 text-xl font-semibold text-neutral-900">{p.name}</p>
              <p className="mt-2 leading-relaxed text-neutral-700">{WHY[p.slug]}</p>
            </Link>
          </li>
        ))}
      </ul>
      <p className="mt-10 text-neutral-600">
        <Link href="/proto/yeux" className="underline underline-offset-4">
          Planche des personnages
        </Link>{" "}
        : toutes les formes d&apos;œil disponibles, en trois teintes.
      </p>
    </main>
  );
}
