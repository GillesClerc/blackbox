import type { Metadata } from "next";
import Link from "next/link";

export const metadata: Metadata = {
  title: "Questions",
  description: "Comment on joue, ce qu'il faut pour jouer, comment arrivent les histoires.",
};

const QA: { q: string; a: React.ReactNode }[] = [
  {
    q: "C'est quoi, exactement ?",
    a: "Un escape game de table en forme de cube. La box est à la fois le décor, le maître du jeu et l'énigme : elle raconte l'histoire, réagit à ce que vous faites et ne livre ses secrets qu'aux joueurs attentifs.",
  },
  {
    q: "Faut-il un téléphone pour jouer ?",
    a: "Non. Pendant la partie, tout se passe avec la box. Un navigateur sert seulement une fois, pour la relier à votre compte et à votre Wi-Fi.",
  },
  {
    q: "Faut-il internet pendant la partie ?",
    a: "Non. Le Wi-Fi sert à recevoir les histoires ; une fois reçues, elles se jouent hors ligne.",
  },
  {
    q: "Combien de joueurs, à partir de quel âge, pour combien de temps ?",
    a: (
      <>
        Cela dépend de l&apos;histoire : chaque fiche l&apos;indique.{" "}
        <Link href="/histoires" className="text-brass underline underline-offset-4">
          Voir les histoires
        </Link>
        .
      </>
    ),
  },
  {
    q: "Comment ajoute-t-on des histoires ?",
    a: "Une histoire s'ajoute à votre compte. Toutes les box reliées à ce compte la reçoivent automatiquement à leur prochaine connexion.",
  },
  {
    q: "Elle fonctionne sur batterie ?",
    a: "Oui, elle a une batterie rechargeable par USB-C. Pas de câble au milieu de la table pendant la partie.",
  },
  {
    q: "Quand pourra-t-on l'acheter, et à quel prix ?",
    a: "Les premières box sont en fabrication et le prix n'est pas encore fixé. Inscrivez-vous sur la liste d'attente : vous serez prévenu avant tout le monde.",
  },
  {
    q: "Que faites-vous de mes données ?",
    a: (
      <>
        Le strict nécessaire, et rien n&apos;est revendu.{" "}
        <Link href="/confidentialite" className="text-brass underline underline-offset-4">
          Tout est expliqué ici
        </Link>
        .
      </>
    ),
  },
];

export default function FaqPage() {
  return (
    <main className="flex-1">
      <section className="mx-auto w-full max-w-3xl px-6 pb-24 pt-14 md:pt-20">
        <p className="font-mono text-[0.7rem] tracking-[0.24em] text-brass">QUESTIONS</p>
        <h1 className="mt-5 font-display text-[clamp(2.4rem,5.6vw,4rem)] leading-[1.04] tracking-[-0.015em]">
          Ce qu&apos;on nous demande.
        </h1>
        <div className="mt-12 divide-y divide-foreground/15 border-y border-foreground/15">
          {QA.map(({ q, a }) => (
            <details key={q} className="group py-5">
              <summary className="flex cursor-pointer list-none items-start justify-between gap-6 rounded-sm font-display text-xl leading-snug focus-visible:outline-2 focus-visible:outline-offset-4 focus-visible:outline-brass [&::-webkit-details-marker]:hidden">
                {q}
                <span aria-hidden="true" className="mt-1 font-mono text-base text-brass transition-transform group-open:rotate-45 motion-reduce:transition-none">
                  +
                </span>
              </summary>
              <div className="mt-3 max-w-2xl leading-relaxed text-foreground/75">{a}</div>
            </details>
          ))}
        </div>
      </section>
    </main>
  );
}
