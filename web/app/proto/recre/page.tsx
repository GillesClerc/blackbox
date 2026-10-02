import type { Metadata } from "next";
import { Bagel_Fat_One, Outfit } from "next/font/google";
import { FACES, STEPS, STORY } from "@/components/proto/content";
import { ProtoBar } from "@/components/proto/proto-bar";
import { ProtoSignup } from "@/components/proto/signup";
import s from "@/components/proto/recre/recre.module.css";
import { ToyBox } from "@/components/proto/recre/toy-box";

// Piste B « Récré » : l'étagère à jeux de société. La box en personnage illustré,
// couleurs franches, gros trait ; ses yeux, eux, sont troublants de réalisme.
const display = Bagel_Fat_One({ subsets: ["latin"], weight: "400", variable: "--font-b-display" });
const body = Outfit({ subsets: ["latin"], variable: "--font-b-body" });

export const metadata: Metadata = { title: "Piste B · Récré" };

const INK = "text-[#3a2340]";
const FACE_COLORS = ["#8ad9b8", "#ffc93c", "#f0603f", "#8ec5ff", "#ff9ec4", "#fff6e5"];

// Faces de dé : l'ordre des étapes compte.
function Die({ n }: { n: number }) {
  const pips: Record<number, number[]> = { 1: [4], 2: [0, 8], 3: [0, 4, 8] };
  return (
    <span className="grid size-14 grid-cols-3 grid-rows-3 gap-0.5 rounded-xl border-4 border-[#3a2340] bg-white p-1.5" aria-label={`Étape ${n}`}>
      {Array.from({ length: 9 }, (_, i) => (
        <span key={i} className={`m-auto size-2 rounded-full ${pips[n].includes(i) ? "bg-[#3a2340]" : ""}`} />
      ))}
    </span>
  );
}

const btn = `${s.btn} h-14 rounded-2xl bg-[#f0603f] px-6 text-lg font-bold text-[#3a2340]`;
const input =
  "h-14 flex-1 rounded-2xl border-4 border-[#3a2340] bg-[#fff6e5] px-5 text-lg text-[#3a2340] placeholder:text-[#3a2340]/45 focus:outline-none focus:ring-4 focus:ring-[#3a2340]/25";

export default function Recre() {
  return (
    <div className={`${display.variable} ${body.variable} min-h-screen bg-[#ffc93c] font-(family-name:--font-b-body) ${INK}`}>
      <header className="mx-auto flex max-w-6xl items-center justify-between px-6 py-6">
        <span className="font-(family-name:--font-b-display) text-3xl">EscapeBox</span>
        <nav className="flex items-center gap-6 text-base font-semibold">
          <a href="#faces" className="hover:underline">La box</a>
          <a href="#histoire" className="hover:underline">Histoires</a>
          <a href="#" className="hidden rounded-full border-[3px] border-[#3a2340] px-4 py-1.5 hover:bg-[#3a2340] hover:text-[#ffc93c] sm:inline">
            Se connecter
          </a>
        </nav>
      </header>

      {/* ── Ouverture ──────────────────────────────────────────────── */}
      <section className="mx-auto grid max-w-6xl items-center gap-10 px-6 pb-24 pt-6 md:grid-cols-[1.1fr_1fr] md:pt-10">
        <div>
          <p className="inline-block rotate-[-2deg] rounded-full border-[3px] border-[#3a2340] bg-[#fff6e5] px-4 py-1 text-sm font-bold uppercase tracking-[0.14em]">
            Escape game de table
          </p>
          <h1 className="mt-6 font-(family-name:--font-b-display) text-[clamp(3.4rem,7.6vw,7rem)] leading-[0.92] tracking-[-0.01em] text-balance">
            Le jeu qui vous <span className="text-[#f0603f] [-webkit-text-stroke:3px_#3a2340] [paint-order:stroke_fill]">regarde</span> jouer.
          </h1>
          <p className="mt-6 max-w-md text-xl leading-relaxed">
            Une boîte d&apos;escape game à poser au milieu de la table. Elle vous regarde, vous écoute et garde ses
            secrets. À vous de les lui soutirer.
          </p>
          <ProtoSignup
            className="mt-8 max-w-lg"
            inputClassName={input}
            buttonClassName={btn}
            noteClassName="mt-3 text-base font-medium opacity-75"
          />
        </div>
        <ToyBox />
      </section>

      {/* ── Comment ça joue ────────────────────────────────────────── */}
      <section className="bg-[#fff6e5]">
        <div className="mx-auto max-w-6xl px-6 py-24">
          <h2 className="font-(family-name:--font-b-display) text-[clamp(2.6rem,5.6vw,4.8rem)] leading-[0.95]">Comment ça joue ?</h2>
          <ol className="mt-12 grid gap-8 md:grid-cols-3">
            {STEPS.map((st, i) => (
              <li key={st.title} className={`${s.card} rounded-3xl bg-white p-7`}>
                <Die n={i + 1} />
                <h3 className="mt-6 font-(family-name:--font-b-display) text-2xl leading-tight">{st.title}</h3>
                <p className="mt-3 text-lg leading-relaxed opacity-80">{st.text}</p>
              </li>
            ))}
          </ol>
        </div>
      </section>

      {/* ── Six faces ──────────────────────────────────────────────── */}
      <section id="faces" className="bg-[#3a2340] text-[#fff6e5]">
        <div className="mx-auto max-w-6xl px-6 py-24">
          <h2 className="max-w-3xl font-(family-name:--font-b-display) text-[clamp(2.6rem,5.6vw,4.8rem)] leading-[0.95] text-balance">
            Six faces, six façons de jouer.
          </h2>
          <p className="mt-5 max-w-xl text-xl leading-relaxed opacity-80">On ne vous dit pas tout : ce serait dommage.</p>
          <ul className="mt-12 grid gap-6 sm:grid-cols-2 lg:grid-cols-3">
            {FACES.map((f, i) => (
              <li key={f.id} className={`${s.card} rounded-3xl p-7 ${INK}`} style={{ background: FACE_COLORS[i], borderColor: "#fff6e5", boxShadow: "6px 6px 0 #fff6e5" }}>
                <p className="inline-block rounded-full bg-[#3a2340] px-3 py-1 text-xs font-bold tracking-[0.16em] text-[#fff6e5]">{f.name}</p>
                <h3 className="mt-5 font-(family-name:--font-b-display) text-3xl">{f.title}</h3>
                <p className="mt-2 text-lg leading-snug">{f.teaser}</p>
              </li>
            ))}
          </ul>
        </div>
      </section>

      {/* ── Une histoire : le couvercle de la boîte ────────────────── */}
      <section id="histoire" className="bg-[#fff6e5]">
        <div className="mx-auto max-w-6xl px-6 py-24">
          <div className={`${s.card} grid overflow-hidden rounded-[2rem] bg-[#24476b] text-[#fff6e5] md:grid-cols-[1fr_1.1fr]`}>
            <div className="relative min-h-64 bg-[#1b3654]">
              <svg viewBox="0 0 400 300" className="absolute inset-0 h-full w-full" aria-hidden="true">
                <path d="M0 70 Q50 55 100 70 T200 70 T300 70 T400 70 V300 H0 Z" fill="#24476b" />
                <g stroke="#3a2340" strokeWidth="6" strokeLinejoin="round">
                  <path d="M70 190 Q70 160 110 160 L290 160 Q340 160 340 190 Q340 220 290 220 L110 220 Q70 220 70 190 Z" fill="#ffc93c" />
                  <path d="M170 160 L180 128 L240 128 L248 160" fill="#ffc93c" />
                  <path d="M210 128 L210 104 L228 104" fill="none" />
                  <path d="M70 190 L40 168 L40 212 Z" fill="#f0603f" />
                </g>
                {[130, 175, 220, 265].map((x) => (
                  <circle key={x} cx={x} cy="190" r="10" fill="#8ec5ff" stroke="#3a2340" strokeWidth="5" />
                ))}
                {[[330, 120, 8], [352, 96, 5], [362, 70, 3]].map(([x, y, r]) => (
                  <circle key={x} cx={x} cy={y} r={r} fill="none" stroke="#fff6e5" strokeOpacity="0.7" strokeWidth="3" />
                ))}
              </svg>
            </div>
            <div className="p-8 md:p-12">
              <p className="text-sm font-bold uppercase tracking-[0.18em] text-[#ffc93c]">Première histoire · {STORY.year}</p>
              <h2 className="mt-4 font-(family-name:--font-b-display) text-[clamp(2.2rem,4.4vw,3.6rem)] leading-[0.98]">{STORY.title}</h2>
              <p className="mt-5 text-lg leading-relaxed opacity-90">{STORY.pitch}</p>
              <div className="mt-8 flex flex-wrap gap-3">
                {[STORY.duration, STORY.players].map((t, i) => (
                  <span key={t} className={`rounded-full border-[3px] border-[#3a2340] px-4 py-1.5 font-bold text-[#3a2340] ${i ? "bg-[#8ad9b8]" : "bg-[#ffc93c]"}`}>
                    {t}
                  </span>
                ))}
              </div>
            </div>
          </div>
        </div>
      </section>

      {/* ── Liste d'attente ────────────────────────────────────────── */}
      <section className="mx-auto max-w-3xl px-6 pb-36 pt-24 text-center">
        <h2 className="font-(family-name:--font-b-display) text-[clamp(3rem,7vw,6rem)] leading-[0.92]">Elle dort encore.</h2>
        <p className="mx-auto mt-5 max-w-lg text-xl leading-relaxed">
          Les premières box sont à l&apos;atelier. Laissez votre adresse : vous serez parmi les premiers à la voir ouvrir
          les yeux.
        </p>
        <ProtoSignup className="mx-auto mt-9 max-w-lg text-left" inputClassName={input} buttonClassName={btn} noteClassName="mt-3 text-base font-medium opacity-75" />
      </section>

      <ProtoBar current="recre" />
    </div>
  );
}
