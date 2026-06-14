import {
  Pointer,
  Nfc,
  SunMedium,
  Rotate3d,
  Mic,
  Thermometer,
  Magnet,
  CircleHelp,
} from "lucide-react";
import { BoxFace } from "@/components/box-face";
import { WaitlistForm } from "@/components/waitlist-form";

const STEPS = [
  {
    n: "01",
    title: "Posez-la au milieu de la table",
    text: "Pas d'application, pas d'écran de téléphone. La boîte est le jeu : elle s'allume, ouvre les yeux et donne le ton.",
  },
  {
    n: "02",
    title: "Choisissez un scénario",
    text: "La boîte le télécharge en Wi-Fi et se transforme : nouvelles voix, nouvelles lumières, nouveaux accessoires à manipuler.",
  },
  {
    n: "03",
    title: "Percez ses secrets",
    text: "Une vingtaine de minutes, à 2-4 joueurs, autour d'un objet qui réagit à tout ce que vous faites.",
  },
];

const SENSES = [
  {
    icon: Pointer,
    code: "TOUCHER",
    text: "Des zones sensibles, quelque part sur la coque. À vous de les trouver.",
  },
  {
    icon: Nfc,
    code: "OBJETS",
    text: "Elle reconnaît certains objets posés sur elle. Lesquels — c'est une autre histoire.",
  },
  {
    icon: SunMedium,
    code: "LUMIÈRE",
    text: "Le noir complet lui parle. Une lampe braquée sur elle aussi.",
  },
  {
    icon: Rotate3d,
    code: "MOUVEMENT",
    text: "Inclinez, retournez, secouez. Elle sait toujours dans quel sens elle est.",
  },
  {
    icon: Mic,
    code: "SON",
    text: "Elle écoute. Certains secrets ne se livrent qu'à ceux qui se taisent.",
  },
  {
    icon: Thermometer,
    code: "CHALEUR",
    text: "La chaleur d'une paume posée au bon endroit ne lui échappe pas.",
  },
  {
    icon: Magnet,
    code: "MAGNÉTISME",
    text: "Quelque chose en elle réagit aux aimants. Reste à savoir où.",
  },
  {
    icon: CircleHelp,
    code: "???",
    text: "Certains sens ne se découvrent qu'en jouant.",
    locked: true,
  },
];

export default function Home() {
  return (
    <main className="flex-1">
      {/* ── Header ──────────────────────────────────────────────── */}
      <header className="mx-auto flex w-full max-w-5xl items-center justify-between px-6 py-6">
        <p className="font-mono text-sm font-bold tracking-widest">
          <span className="text-primary">◉◉</span> ESCAPEBOX
        </p>
        <a
          href="#waitlist"
          className="rounded-md px-3 py-2 font-mono text-xs tracking-wider text-muted-foreground transition-colors hover:text-foreground focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-ring"
        >
          REJOINDRE LA LISTE
        </a>
      </header>

      {/* ── Hero ────────────────────────────────────────────────── */}
      <section className="mx-auto w-full max-w-5xl px-6 pb-24 pt-10 sm:pt-16">
        <BoxFace />
        <div className="mx-auto mt-12 flex max-w-2xl flex-col items-center text-center">
          <p className="font-mono text-xs tracking-[0.25em] text-glow">
            ESCAPE GAME DE TABLE · 2-4 JOUEURS
          </p>
          <h1 className="mt-4 font-display text-3xl font-bold leading-tight sm:text-5xl">
            Une énigme avec un visage
          </h1>
          <p className="mt-5 max-w-xl text-base leading-relaxed text-muted-foreground sm:text-lg">
            EscapeBox est une boîte d&apos;escape game à poser au milieu de la
            table. Elle vous observe, vous écoute et garde ses secrets — à vous
            de les percer.
          </p>
          <div className="mt-8 flex w-full justify-center">
            <WaitlistForm id="email-hero" />
          </div>
        </div>
      </section>

      {/* ── Comment ça joue ─────────────────────────────────────── */}
      <section className="border-t border-border bg-card/40">
        <div className="mx-auto w-full max-w-5xl px-6 py-20">
          <p className="font-mono text-xs tracking-[0.25em] text-muted-foreground">
            COMMENT ÇA JOUE
          </p>
          <div className="mt-10 grid gap-10 sm:grid-cols-3">
            {STEPS.map((s) => (
              <div key={s.n}>
                <p className="font-mono text-sm font-bold text-primary">
                  {s.n}
                </p>
                <h2 className="mt-3 font-display text-lg font-medium leading-snug">
                  {s.title}
                </h2>
                <p className="mt-3 text-sm leading-relaxed text-muted-foreground">
                  {s.text}
                </p>
              </div>
            ))}
          </div>
        </div>
      </section>

      {/* ── Les sens de la boîte ────────────────────────────────── */}
      <section className="mx-auto w-full max-w-5xl px-6 py-20">
        <p className="font-mono text-xs tracking-[0.25em] text-muted-foreground">
          ELLE N&apos;A PAS QUE DES YEUX
        </p>
        <h2 className="mt-4 max-w-2xl font-display text-2xl font-medium leading-snug sm:text-3xl">
          Plus de sens que vous n&apos;en soupçonnez
        </h2>
        <div className="mt-10 grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
          {SENSES.map((s) => (
            <div
              key={s.code}
              className={
                s.locked
                  ? "rounded-xl border border-dashed border-primary/30 bg-transparent p-5"
                  : "rounded-xl border border-border bg-card p-5"
              }
            >
              <div className="flex items-center gap-3">
                <s.icon
                  className={
                    s.locked ? "size-4 text-primary" : "size-4 text-glow"
                  }
                  aria-hidden="true"
                />
                <p className="font-mono text-xs font-bold tracking-[0.2em] text-foreground">
                  {s.code}
                </p>
              </div>
              <p className="mt-3 text-sm leading-relaxed text-muted-foreground">
                {s.text}
              </p>
            </div>
          ))}
        </div>
      </section>

      {/* ── Scénario vedette ────────────────────────────────────── */}
      <section className="border-t border-border bg-card/40">
        <div className="mx-auto w-full max-w-5xl px-6 py-20">
          <div className="mx-auto max-w-2xl rounded-2xl border border-border bg-night/50 p-8 sm:p-10">
            <p className="font-mono text-xs tracking-[0.25em] text-glow">
              PREMIER SCÉNARIO
            </p>
            <h2 className="mt-4 font-display text-2xl font-medium leading-snug sm:text-3xl">
              Le Mystère du Capitaine Verdier
            </h2>
            <p className="mt-5 leading-relaxed text-muted-foreground">
              1957. Le sous-marin Verdier disparaît en mer du Nord avec
              quarante-deux hommes à bord. Soixante-dix ans plus tard, sa
              balise de détresse vient de se réactiver. Posez le médaillon du
              capitaine sur la boîte — et remontez le fil de sa dernière
              plongée.
            </p>
            <p className="mt-6 font-mono text-xs tracking-[0.2em] text-muted-foreground">
              20 MIN · 2-4 JOUEURS · MÉDAILLON LAITON INCLUS
            </p>
          </div>
        </div>
      </section>

      {/* ── Waitlist finale ─────────────────────────────────────── */}
      <section id="waitlist" className="mx-auto w-full max-w-5xl px-6 py-24">
        <div className="mx-auto flex max-w-2xl flex-col items-center text-center">
          <h2 className="font-display text-2xl font-bold sm:text-4xl">
            Ouvrez l&apos;œil.
          </h2>
          <p className="mt-4 max-w-md text-muted-foreground">
            Les premières boîtes partiront en petite série. Laissez votre
            adresse pour être prévenu avant tout le monde.
          </p>
          <div className="mt-8 flex w-full justify-center">
            <WaitlistForm id="email-footer" />
          </div>
        </div>
      </section>

      <footer className="border-t border-border">
        <div className="mx-auto flex w-full max-w-5xl items-center justify-between px-6 py-8">
          <p className="font-mono text-xs tracking-widest text-muted-foreground">
            <span className="text-primary">◉◉</span> ESCAPEBOX © 2026
          </p>
          <p className="font-mono text-xs text-muted-foreground">
            Conçue et assemblée en Suisse
          </p>
        </div>
      </footer>
    </main>
  );
}
