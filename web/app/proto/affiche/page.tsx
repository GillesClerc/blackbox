import type { Metadata } from "next";
import { Instrument_Sans, Unbounded } from "next/font/google";
import { AfficheHeadline } from "@/components/proto/affiche/headline";
import { FACES, STEPS, STORY } from "@/components/proto/content";
import { ProtoBar } from "@/components/proto/proto-bar";
import { ProtoSignup } from "@/components/proto/signup";

// Piste C « Affiche » : une affiche suisse, rouge et crème, typographie massive.
// Pas de box dessinée : le titre est le visage, la grille fait le reste.
const display = Unbounded({ subsets: ["latin"], weight: ["600", "800"], variable: "--font-c-display" });
const body = Instrument_Sans({ subsets: ["latin"], variable: "--font-c-body" });

export const metadata: Metadata = { title: "Piste C · Affiche" };

const RED = "#d9452b";
const label = "text-[0.78rem] font-semibold uppercase tracking-[0.18em]";
const input = "h-13 flex-1 border-2 border-current bg-transparent px-4 text-lg placeholder:opacity-60 focus:outline-none focus:ring-4 focus:ring-current/20";

export default function Affiche() {
  return (
    <div className={`${display.variable} ${body.variable} min-h-screen bg-[#fff0dc] font-(family-name:--font-c-body) text-[#1d1714]`}>
      {/* ── Ouverture : l'affiche ──────────────────────────────────── */}
      <section style={{ background: RED }} className="text-[#fff0dc]">
        <header className="flex items-baseline justify-between px-[4vw] py-5">
          <span className="font-(family-name:--font-c-display) text-lg font-extrabold uppercase tracking-tight">EscapeBox</span>
          <nav className={`flex gap-6 ${label}`}>
            <a href="#faces" className="hover:underline">La box</a>
            <a href="#histoire" className="hover:underline">Histoires</a>
            <a href="#" className="hidden hover:underline sm:inline">Se connecter</a>
          </nav>
        </header>
        <div className="border-t-2 border-[#fff0dc]/40 px-[4vw] pb-16 pt-10 md:pt-14">
          <AfficheHeadline />
          <div className="mt-16 grid gap-10 border-t-2 border-[#fff0dc] pt-6 md:grid-cols-3">
            <p className={label}>
              Escape game de table
              <br />
              Une box, plusieurs histoires
            </p>
            <p className="text-xl leading-snug">
              Une boîte à poser au milieu de la table. Elle vous regarde, vous écoute et garde ses secrets. À vous de les lui
              soutirer.
            </p>
            <ProtoSignup
              className="text-[#fff0dc]"
              inputClassName={input}
              buttonClassName="h-13 bg-[#fff0dc] px-6 text-lg font-semibold text-[#d9452b] hover:bg-white"
              noteClassName="mt-3 text-sm opacity-80"
            />
          </div>
        </div>
      </section>

      {/* ── Six faces : la grille ──────────────────────────────────── */}
      <section id="faces" className="px-[4vw] py-24" style={{ color: RED }}>
        <div className="grid gap-6 md:grid-cols-3">
          <p className={label}>La box</p>
          <h2 className="font-(family-name:--font-c-display) text-[clamp(2.2rem,5vw,4.4rem)] font-extrabold uppercase leading-[0.92] tracking-[-0.03em] md:col-span-2">
            Six faces.
            <br />
            Aucune pour décorer.
          </h2>
        </div>
        <ul className="mt-14 border-t-[3px] border-current">
          {FACES.map((f) => (
            <li key={f.id} className="grid items-baseline gap-2 border-b-[3px] border-current py-6 md:grid-cols-3 md:gap-6">
              <p className={label}>{f.name}</p>
              <h3 className="font-(family-name:--font-c-display) text-[clamp(1.8rem,3.6vw,3.2rem)] font-extrabold uppercase leading-none tracking-[-0.03em]">
                {f.title}
              </h3>
              <p className="text-lg text-[#1d1714]">{f.teaser}</p>
            </li>
          ))}
        </ul>
      </section>

      {/* ── Comment ça joue ────────────────────────────────────────── */}
      <section className="bg-[#1d1714] px-[4vw] py-24 text-[#fff0dc]">
        <p className={label}>Comment ça joue</p>
        <ol className="mt-12 grid gap-12 md:grid-cols-3">
          {STEPS.map((st, i) => (
            <li key={st.title} className="border-t-2 border-[#fff0dc]/30 pt-6">
              <p className="font-(family-name:--font-c-display) text-[clamp(4rem,9vw,8rem)] font-extrabold leading-none" style={{ color: RED }}>
                {i + 1}
              </p>
              <h3 className="mt-6 text-2xl font-semibold">{st.title}</h3>
              <p className="mt-3 text-lg leading-relaxed opacity-75">{st.text}</p>
            </li>
          ))}
        </ol>
      </section>

      {/* ── Une histoire : l'affiche du film ───────────────────────── */}
      <section id="histoire" style={{ background: RED }} className="overflow-hidden px-[4vw] py-20 text-[#fff0dc]">
        <p className={label}>Première histoire</p>
        <p className="mt-4 font-(family-name:--font-c-display) text-[clamp(7rem,33vw,30rem)] font-extrabold leading-[0.8] tracking-[-0.06em]">
          {STORY.year}
        </p>
        <div className="mt-10 grid gap-8 border-t-2 border-[#fff0dc] pt-6 md:grid-cols-3">
          <h2 className="font-(family-name:--font-c-display) text-[clamp(1.8rem,3vw,2.6rem)] font-extrabold uppercase leading-[0.95] tracking-[-0.03em] md:col-span-1">
            {STORY.title}
          </h2>
          <p className="text-xl leading-snug">{STORY.pitch}</p>
          <p className={`${label} md:text-right`}>
            {STORY.duration}
            <br />
            {STORY.players}
          </p>
        </div>
      </section>

      {/* ── Liste d'attente ────────────────────────────────────────── */}
      <section className="px-[4vw] pb-36 pt-24">
        <div className="grid gap-10 md:grid-cols-3">
          <h2
            className="font-(family-name:--font-c-display) text-[clamp(2.6rem,6vw,5.4rem)] font-extrabold uppercase leading-[0.9] tracking-[-0.035em] md:col-span-2"
            style={{ color: RED }}
          >
            Elle dort encore.
          </h2>
          <div className="md:pt-3">
            <p className="text-lg leading-relaxed">
              Les premières box sont à l&apos;atelier. Laissez votre adresse : vous serez parmi les premiers à la voir ouvrir les
              yeux.
            </p>
            <ProtoSignup
              className="mt-6"
              inputClassName={input}
              buttonClassName="h-13 bg-[#d9452b] px-6 text-lg font-semibold text-[#fff0dc] hover:bg-[#c23a22]"
              noteClassName="mt-3 text-sm opacity-70"
            />
          </div>
        </div>
      </section>

      <ProtoBar current="affiche" />
    </div>
  );
}
