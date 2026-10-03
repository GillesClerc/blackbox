import type { Metadata } from "next";
import { Figtree, Gloock } from "next/font/google";
import { STEPS, STORY } from "@/components/proto/content";
import { ProtoBar } from "@/components/proto/proto-bar";
import { ProtoSignup } from "@/components/proto/signup";
import { VeilleeV2Hero } from "@/components/proto/veillee2/hero";
import { Ink, Lamp } from "@/components/proto/veillee2/lamp";
import { Riddles } from "@/components/proto/veillee2/riddles";

// Piste E « Veillée v2 » : la Veillée, pour un public adulte. Visage plus discret
// qui change de personnage, lampe sur toute la page (souris, ou faisceau qu'on
// fait défiler sur mobile), box en 3D présentée par des énigmes.
const display = Gloock({ subsets: ["latin"], weight: "400", variable: "--font-e-display" });
const body = Figtree({ subsets: ["latin"], variable: "--font-e-body" });

export const metadata: Metadata = { title: "Piste E · Veillée v2" };

// Une soirée réelle : l'heure encode l'ordre des étapes.
const HOURS = ["20 h 30", "20 h 34", "21 h 12"];
const h2 = "font-(family-name:--font-e-display) text-[clamp(2.5rem,5.6vw,4.8rem)] leading-[0.95] tracking-[-0.015em] text-balance";
const input =
  "h-13 flex-1 rounded-full border border-[#f4e7d3]/20 bg-[#f4e7d3]/[0.06] px-5 text-[#f4e7d3] placeholder:text-[#f4e7d3]/40 focus:border-[#ffc27a] focus:outline-none";
const button = "h-13 rounded-full bg-[#ffc27a] px-6 font-semibold text-[#1c1411] transition hover:bg-[#ffd39b]";

export default function VeilleeV2() {
  return (
    <div
      className={`${display.variable} ${body.variable} min-h-screen bg-[#1c1411] font-(family-name:--font-e-body) text-[#f4e7d3] selection:bg-[#ffc27a] selection:text-[#1c1411]`}
    >
      <Lamp />
      <header className="mx-auto flex max-w-6xl items-center justify-between px-6 py-6">
        <span className="font-(family-name:--font-e-display) text-2xl">EscapeBox</span>
        <nav className="flex gap-6 text-sm text-[#f4e7d3]/70">
          <a href="#faces" className="hover:text-[#ffc27a]">La box</a>
          <a href="#histoire" className="hover:text-[#ffc27a]">Histoires</a>
          <a href="#" className="hidden hover:text-[#ffc27a] sm:inline">Se connecter</a>
        </nav>
      </header>

      <VeilleeV2Hero />

      {/* ── Une soirée ─────────────────────────────────────────────── */}
      <section className="relative border-t border-[#f4e7d3]/10">
        <Ink
          notes={[
            { t: "elle compte les minutes", x: "70%", y: "14%", r: 5 },
            { t: "souvenez-vous de 21 h 12", x: "8%", y: "86%", r: -4, size: "1.2rem" },
          ]}
        />
        <div className="relative mx-auto max-w-6xl px-6 py-24 md:py-28">
          <h2 className={`${h2} max-w-3xl`}>Une soirée, trois moments.</h2>
          <ol className="mt-14 grid gap-12 md:grid-cols-3">
            {STEPS.map((st, i) => (
              <li key={st.title}>
                <p className="font-(family-name:--font-e-display) text-4xl text-[#ffc27a]">{HOURS[i]}</p>
                <h3 className="mt-5 text-xl font-semibold">{st.title}</h3>
                <p className="mt-3 leading-relaxed text-[#f4e7d3]/70">{st.text}</p>
              </li>
            ))}
          </ol>
        </div>
      </section>

      {/* ── Six faces, six énigmes ─────────────────────────────────── */}
      <section id="faces" className="bg-[#140e0c]">
        <div className="mx-auto max-w-6xl px-6 pt-24 md:pt-28">
          <div className="grid gap-6 md:grid-cols-[1.5fr_1fr] md:items-end">
            <h2 className={h2}>Six faces. Six énigmes.</h2>
            <p className="max-w-md text-lg leading-relaxed text-[#f4e7d3]/70 md:justify-self-end">
              Elle ne se présente pas : elle se laisse deviner. Chaque face a son chiffre, chaque chiffre son énigme.
            </p>
          </div>
        </div>
        <div className="mx-auto max-w-6xl px-6 pb-16 pt-10 md:pb-24">
          <Riddles />
        </div>
      </section>

      {/* ── Une histoire ───────────────────────────────────────────── */}
      <section id="histoire" className="relative bg-[#3a1d2a]">
        <Ink notes={[{ t: "42 – 1 = ?", x: "80%", y: "18%", r: 7, size: "1.8rem" }]} />
        <div className="relative mx-auto grid max-w-6xl gap-10 px-6 py-24 md:grid-cols-[auto_1fr] md:items-center md:gap-20 md:py-28">
          <p className="font-(family-name:--font-e-display) text-[clamp(5.5rem,16vw,13rem)] leading-none text-[#ffc27a]/90">
            {STORY.year}
          </p>
          <div>
            <p className="text-[0.75rem] font-semibold uppercase tracking-[0.3em] text-[#ffc27a]/80">Première histoire</p>
            <h2 className="mt-4 font-(family-name:--font-e-display) text-[clamp(2.1rem,4.6vw,3.8rem)] leading-[1] text-balance">
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
      <section className="relative">
        <Ink notes={[{ t: "elle vous a vu", x: "12%", y: "20%", r: -6, size: "1.5rem" }]} />
        <div className="relative mx-auto max-w-6xl px-6 pb-36 pt-24 text-center md:pt-28">
          <h2 className="font-(family-name:--font-e-display) text-[clamp(2.8rem,7vw,6rem)] leading-[0.95] tracking-[-0.015em]">
            Elle dort encore.
          </h2>
          <p className="mx-auto mt-6 max-w-lg text-lg leading-relaxed text-[#f4e7d3]/70">
            Les premières box sont à l&apos;atelier. Laissez votre adresse : vous serez parmi les premiers à la voir ouvrir
            les yeux.
          </p>
          <ProtoSignup
            className="mx-auto mt-10 max-w-md text-left"
            inputClassName={input}
            buttonClassName={button}
            noteClassName="mt-3 px-5 text-sm text-[#f4e7d3]/55"
          />
        </div>
      </section>

      <ProtoBar current="veillee-v2" />
    </div>
  );
}
