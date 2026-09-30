import Link from "next/link";
import { BoxScene } from "@/components/site/box-scene";
import { CubeNet } from "@/components/site/cube-net";
import { StoryCard } from "@/components/site/story-card";
import { WaitlistForm } from "@/components/waitlist-form";
import { listStories } from "@/lib/catalog";

export const dynamic = "force-dynamic";

// Une vraie séquence de jeu : l'ordre compte, d'où la numérotation.
const STEPS = [
  {
    title: "Posez-la au milieu de la table",
    text: "Pas d'appli, pas d'écran de téléphone : la box est le jeu. Elle s'allume, ouvre les yeux et donne le ton.",
  },
  {
    title: "Choisissez une histoire",
    text: "Chaque histoire la transforme : une autre voix, d'autres lumières, d'autres énigmes cachées dans ses faces.",
  },
  {
    title: "Percez ses secrets",
    text: "Tournez-la, touchez-la, parlez-lui. Elle réagit à tout ce que vous faites — et elle se souvient.",
  },
];

const eyebrow = "font-mono text-[0.7rem] tracking-[0.24em] text-brass";

export default async function Home() {
  const stories = (await listStories()).slice(0, 3);

  return (
    <main className="flex-1">
      {/* ── La box, posée sur la table ─────────────────────────────── */}
      <section className="relative">
        <div className="mx-auto grid w-full max-w-6xl items-center gap-12 px-6 pb-4 pt-12 md:grid-cols-[1.05fr_1fr] md:pt-20">
          <div>
            <p className={eyebrow}>ESCAPE GAME DE TABLE</p>
            <h1 className="mt-5 font-display text-[clamp(2.6rem,6.2vw,4.6rem)] leading-[1.02] tracking-[-0.015em] text-balance">
              Une énigme avec un visage.
            </h1>
            <p className="mt-6 max-w-md text-lg leading-relaxed text-foreground/80">
              Une boîte en bois à poser au milieu de la table. Elle vous regarde, vous écoute et garde ses secrets. À vous
              de les lui soutirer.
            </p>
            <div className="mt-9">
              <WaitlistForm id="email-hero" />
              <p className="-mt-1 text-sm text-muted-foreground">Pas encore en vente. Laissez votre adresse, elle vous fera signe.</p>
            </div>
          </div>
          {/* la base du cube repose sur le plateau, son ombre déborde sur le bois */}
          <div className="relative z-10 -mb-10 md:-mb-12">
            <BoxScene />
          </div>
        </div>
        {/* le plateau de la table */}
        <div aria-hidden="true" className="h-4 bg-gradient-to-b from-walnut-light to-walnut shadow-[0_6px_14px_-8px_rgba(29,35,39,0.5)]" />
      </section>

      {/* ── Comment ça joue ────────────────────────────────────────── */}
      <section className="mx-auto w-full max-w-6xl px-6 py-24">
        <p className={eyebrow}>COMMENT ÇA JOUE</p>
        <ol className="mt-10 grid gap-10 md:grid-cols-3">
          {STEPS.map((s, i) => (
            <li key={s.title} className="border-t border-foreground/20 pt-5">
              <p className="font-mono text-sm text-brass">{i + 1}</p>
              <h2 className="mt-3 font-display text-2xl leading-snug">{s.title}</h2>
              <p className="mt-3 leading-relaxed text-foreground/75">{s.text}</p>
            </li>
          ))}
        </ol>
      </section>

      {/* ── Les six faces ──────────────────────────────────────────── */}
      <section className="border-y border-foreground/10 bg-secondary/60">
        <div className="mx-auto w-full max-w-6xl px-6 py-24">
          <div className="max-w-2xl">
            <p className={eyebrow}>SIX FACES</p>
            <h2 className="mt-4 font-display text-[clamp(1.9rem,3.8vw,2.8rem)] leading-tight text-balance">
              Aucune n&apos;est là pour faire joli.
            </h2>
            <p className="mt-4 leading-relaxed text-foreground/75">
              Dépliez la box : chaque face cache une façon de jouer. On ne vous dit pas tout — ce serait dommage.
            </p>
          </div>
          <div className="mt-12">
            <CubeNet />
          </div>
        </div>
      </section>

      {/* ── Les histoires ──────────────────────────────────────────── */}
      <section className="mx-auto w-full max-w-6xl px-6 py-24">
        <div className="flex flex-wrap items-end justify-between gap-4">
          <div className="max-w-xl">
            <p className={eyebrow}>LES HISTOIRES</p>
            <h2 className="mt-4 font-display text-[clamp(1.9rem,3.8vw,2.8rem)] leading-tight">Une box, plusieurs vies.</h2>
            <p className="mt-4 leading-relaxed text-foreground/75">
              Achetée une fois, elle change de peau à chaque histoire. Les nouvelles arrivent par le Wi-Fi ; pendant la
              partie, plus besoin de réseau.
            </p>
          </div>
          <Link href="/histoires" className="text-sm font-semibold text-brass underline-offset-4 hover:underline">
            Toutes les histoires
          </Link>
        </div>
        {stories.length > 0 ? (
          <div className="mt-10 grid gap-6 sm:grid-cols-2 lg:grid-cols-3">
            {stories.map((s) => (
              <StoryCard key={s.slug} story={s} />
            ))}
          </div>
        ) : (
          <p className="mt-10 text-foreground/70">Les premières histoires sont en écriture.</p>
        )}
      </section>

      {/* ── Liste d'attente ────────────────────────────────────────── */}
      <section className="bg-foreground text-background">
        <div className="mx-auto grid w-full max-w-6xl gap-8 px-6 py-20 md:grid-cols-2 md:items-center">
          <div>
            <h2 className="font-display text-[clamp(1.9rem,3.8vw,2.8rem)] leading-tight">Elle n&apos;est pas encore réveillée.</h2>
            <p className="mt-4 max-w-md leading-relaxed text-background/75">
              Les premières box sont en fabrication. Inscrivez-vous : vous serez parmi les premiers à la voir ouvrir les
              yeux.
            </p>
          </div>
          <WaitlistForm id="email-final" tone="dark" />
        </div>
      </section>
    </main>
  );
}
