import type { Metadata } from "next";
import { Figtree, Gloock } from "next/font/google";
import { FACES, STEPS, STORY } from "@/components/proto/content";
import { ProtoBar } from "@/components/proto/proto-bar";
import { ProtoSignup } from "@/components/proto/signup";
import { VeilleeHero } from "@/components/proto/veillee/hero";
import s from "@/components/proto/veillee/veillee.module.css";

// Piste A « Veillée » : un soir de jeux, lumière basse. La box disparaît dans la
// pénombre, il ne reste que son visage. Le pointeur est une lampe.
const display = Gloock({ subsets: ["latin"], weight: "400", variable: "--font-a-display" });
const body = Figtree({ subsets: ["latin"], variable: "--font-a-body" });

export const metadata: Metadata = { title: "Piste A · Veillée" };

// Une soirée réelle : l'heure encode l'ordre des étapes.
const HOURS = ["20 h 30", "20 h 34", "21 h 12"];

export default function Veillee() {
  return (
    <div
      className={`${display.variable} ${body.variable} min-h-screen bg-[#1c1411] font-(family-name:--font-a-body) text-[#f4e7d3] selection:bg-[#ffc27a] selection:text-[#1c1411]`}
    >
      <header className="mx-auto flex max-w-6xl items-center justify-between px-6 py-6">
        <span className="font-(family-name:--font-a-display) text-2xl">EscapeBox</span>
        <nav className="flex gap-6 text-sm text-[#f4e7d3]/70">
          <a href="#soiree" className="hover:text-[#ffc27a]">La box</a>
          <a href="#histoire" className="hover:text-[#ffc27a]">Histoires</a>
          <a href="#" className="hidden hover:text-[#ffc27a] sm:inline">Se connecter</a>
        </nav>
      </header>

      <VeilleeHero />

      {/* ── Une soirée ─────────────────────────────────────────────── */}
      <section id="soiree" className="border-t border-[#f4e7d3]/10">
        <div className="mx-auto max-w-6xl px-6 py-28">
          <h2 className="max-w-3xl font-(family-name:--font-a-display) text-[clamp(2.6rem,6vw,5rem)] leading-[0.95] tracking-[-0.015em] text-balance">
            Une soirée, trois moments.
          </h2>
          <ol className="mt-16 grid gap-12 md:grid-cols-3">
            {STEPS.map((st, i) => (
              <li key={st.title}>
                <p className="font-(family-name:--font-a-display) text-4xl text-[#ffc27a]">{HOURS[i]}</p>
                <h3 className="mt-5 text-xl font-semibold">{st.title}</h3>
                <p className="mt-3 leading-relaxed text-[#f4e7d3]/70">{st.text}</p>
              </li>
            ))}
          </ol>
        </div>
      </section>

      {/* ── Six faces ──────────────────────────────────────────────── */}
      <section className="bg-[#140e0c]">
        <div className="mx-auto max-w-6xl px-6 py-28">
          <div className="grid gap-6 md:grid-cols-[1.5fr_1fr] md:items-end">
            <h2 className="font-(family-name:--font-a-display) text-[clamp(2.6rem,6vw,5rem)] leading-[0.95] tracking-[-0.015em]">
              Six faces.<br />Aucune pour décorer.
            </h2>
            <p className="max-w-md text-lg leading-relaxed text-[#f4e7d3]/70 md:justify-self-end">
              Chaque côté cache une façon de jouer. On ne vous dit pas tout : ce serait dommage.
            </p>
          </div>
          <ul className="mt-16 grid border-t border-[#f4e7d3]/12 md:grid-cols-2">
            {FACES.map((f) => (
              <li key={f.id} className={`${s.row} border-b border-[#f4e7d3]/12 px-2 py-7 md:odd:border-r md:odd:pr-10 md:even:pl-10`}>
                <p className="text-[0.72rem] font-semibold uppercase tracking-[0.26em] text-[#e8743b]">{f.name}</p>
                <h3 className="mt-2 font-(family-name:--font-a-display) text-3xl">{f.title}</h3>
                <p className="mt-2 leading-relaxed text-[#f4e7d3]/70">{f.teaser}</p>
              </li>
            ))}
          </ul>
        </div>
      </section>

      {/* ── Une histoire ───────────────────────────────────────────── */}
      <section id="histoire" className="bg-[#3a1d2a]">
        <div className="mx-auto grid max-w-6xl gap-12 px-6 py-28 md:grid-cols-[auto_1fr] md:items-center md:gap-20">
          <p className="font-(family-name:--font-a-display) text-[clamp(6rem,16vw,13rem)] leading-none text-[#ffc27a]/90">
            {STORY.year}
          </p>
          <div>
            <p className="text-[0.78rem] font-semibold uppercase tracking-[0.28em] text-[#ffc27a]/80">Première histoire</p>
            <h2 className="mt-4 font-(family-name:--font-a-display) text-[clamp(2.2rem,4.6vw,3.8rem)] leading-[1] text-balance">
              {STORY.title}
            </h2>
            <p className="mt-6 max-w-xl text-lg leading-relaxed text-[#f4e7d3]/80">{STORY.pitch}</p>
            <p className="mt-8 flex gap-6 text-sm font-semibold text-[#ffc27a]">
              <span>{STORY.duration}</span>
              <span>{STORY.players}</span>
            </p>
          </div>
        </div>
      </section>

      {/* ── Liste d'attente ────────────────────────────────────────── */}
      <section className="mx-auto max-w-6xl px-6 pb-36 pt-28 text-center">
        <h2 className="font-(family-name:--font-a-display) text-[clamp(2.8rem,7vw,6rem)] leading-[0.95] tracking-[-0.015em]">
          Elle dort encore.
        </h2>
        <p className="mx-auto mt-6 max-w-lg text-lg leading-relaxed text-[#f4e7d3]/70">
          Les premières box sont à l&apos;atelier. Laissez votre adresse : vous serez parmi les premiers à la voir ouvrir
          les yeux.
        </p>
        <ProtoSignup
          className="mx-auto mt-10 max-w-md text-left"
          inputClassName="h-13 flex-1 rounded-full border border-[#f4e7d3]/20 bg-[#f4e7d3]/[0.06] px-5 text-[#f4e7d3] placeholder:text-[#f4e7d3]/40 focus:border-[#ffc27a] focus:outline-none"
          buttonClassName="h-13 rounded-full bg-[#ffc27a] px-6 font-semibold text-[#1c1411] transition hover:bg-[#ffd39b]"
          noteClassName="mt-3 px-5 text-sm text-[#f4e7d3]/55"
        />
      </section>

      <ProtoBar current="veillee" />
    </div>
  );
}
