import type { Metadata } from "next";
import { Libre_Caslon_Display, Work_Sans } from "next/font/google";
import { AtelierCube, FINISHES } from "@/components/proto/atelier/cube";
import { FACES, STEPS, STORY } from "@/components/proto/content";
import { ProtoBar } from "@/components/proto/proto-bar";
import { ProtoSignup } from "@/components/proto/signup";

// Piste D « Atelier » : l'objet d'artisan qu'on garde sur l'étagère. Une vraie box
// en volume, qu'on fait tourner ; la finition se choisit, puisqu'elle n'est pas arrêtée.
const display = Libre_Caslon_Display({ subsets: ["latin"], weight: "400", variable: "--font-d-display" });
const body = Work_Sans({ subsets: ["latin"], variable: "--font-d-body" });

export const metadata: Metadata = { title: "Piste D · Atelier" };

const eyebrow = "text-[0.75rem] font-medium uppercase tracking-[0.22em]";
const h2 = "font-(family-name:--font-d-display) text-[clamp(2.6rem,5.6vw,4.8rem)] leading-[1] tracking-[-0.01em] text-balance";

const FINISH_TEXT = {
  lite: "Les capteurs essentiels, dans un boîtier simple et solide. Pour jouer en famille, sans se ruiner.",
  pro: "Bois massif, façade en ardoise, arêtes de laiton, plateau tournant et tous les capteurs. Pour les passionnés et les collectionneurs.",
};

export default function Atelier() {
  return (
    <div className={`${display.variable} ${body.variable} min-h-screen bg-[#efe7d9] font-(family-name:--font-d-body) text-[#1f2e27]`}>
      {/* ── Ouverture ──────────────────────────────────────────────── */}
      <section className="bg-[#1f2e27] text-[#f1e9da]">
        <header className="mx-auto flex max-w-6xl items-center justify-between px-6 py-6">
          <span className="font-(family-name:--font-d-display) text-2xl">EscapeBox</span>
          <nav className="flex gap-7 text-sm text-[#f1e9da]/70">
            <a href="#finitions" className="hover:text-[#e0a458]">La box</a>
            <a href="#histoire" className="hover:text-[#e0a458]">Histoires</a>
            <a href="#" className="hidden hover:text-[#e0a458] sm:inline">Se connecter</a>
          </nav>
        </header>
        <div className="mx-auto grid max-w-6xl items-center gap-14 px-6 pb-24 pt-8 md:grid-cols-[1.05fr_1fr] md:pt-14">
          <div>
            <p className={`${eyebrow} text-[#e0a458]`}>Escape game de table · design artisanal</p>
            <h1 className="mt-6 font-(family-name:--font-d-display) text-[clamp(3.4rem,7.4vw,6.8rem)] leading-[0.95] tracking-[-0.015em] text-balance">
              Elle attend sur l&apos;étagère.
            </h1>
            <p className="mt-6 max-w-md text-xl leading-relaxed text-[#f1e9da]/80">
              Jusqu&apos;au soir où on la pose au milieu de la table. Alors elle ouvre les yeux, vous écoute, et garde ses
              secrets aussi longtemps qu&apos;elle peut.
            </p>
            <ProtoSignup
              className="mt-9 max-w-md"
              inputClassName="h-13 flex-1 rounded-md border border-[#f1e9da]/25 bg-[#f1e9da]/[0.06] px-4 text-[#f1e9da] placeholder:text-[#f1e9da]/40 focus:border-[#e0a458] focus:outline-none"
              buttonClassName="h-13 rounded-md bg-[#e0a458] px-6 font-semibold text-[#1f2e27] hover:bg-[#ebb673]"
              noteClassName="mt-3 text-sm text-[#f1e9da]/55"
            />
          </div>
          <AtelierCube />
        </div>
      </section>

      {/* ── Deux finitions ─────────────────────────────────────────── */}
      <section id="finitions" className="mx-auto max-w-6xl px-6 py-28">
        <div className="grid gap-6 md:grid-cols-[1.2fr_1fr] md:items-end">
          <h2 className={h2}>Deux finitions, un même caractère.</h2>
          <p className="text-lg leading-relaxed text-[#1f2e27]/75">
            Les finitions sont encore à l&apos;atelier. Ce qui ne change pas : six faces à explorer, et un regard qui vous suit.
          </p>
        </div>
        <div className="mt-14 grid gap-8 md:grid-cols-2">
          {(Object.keys(FINISHES) as (keyof typeof FINISHES)[]).map((k) => (
            <article key={k} className="overflow-hidden rounded-lg bg-[#f7f2e9] shadow-[0_1px_0_rgba(31,46,39,0.08),0_20px_40px_-24px_rgba(31,46,39,0.35)]">
              <div
                className="h-40"
                style={{
                  background: `${FINISHES[k].grain}, ${FINISHES[k].wood}`,
                  boxShadow: k === "pro" ? "inset 0 -6px 0 #c9a15a" : undefined,
                }}
              />
              <div className="p-8">
                <p className={`${eyebrow} text-[#9a6b2f]`}>{FINISHES[k].detail}</p>
                <h3 className="mt-2 font-(family-name:--font-d-display) text-4xl">Box {FINISHES[k].label}</h3>
                <p className="mt-3 leading-relaxed text-[#1f2e27]/75">{FINISH_TEXT[k]}</p>
              </div>
            </article>
          ))}
        </div>
      </section>

      {/* ── Six faces ──────────────────────────────────────────────── */}
      <section className="border-y border-[#1f2e27]/12 bg-[#e6dccb]">
        <div className="mx-auto max-w-6xl px-6 py-28">
          <h2 className={h2}>Six faces. Aucune pour décorer.</h2>
          <dl className="mt-14 grid gap-x-12 gap-y-10 sm:grid-cols-2 lg:grid-cols-3">
            {FACES.map((f) => (
              <div key={f.id} className="border-t border-[#1f2e27]/25 pt-5">
                <dt>
                  <span className={`${eyebrow} text-[#9a6b2f]`}>{f.name}</span>
                  <span className="mt-2 block font-(family-name:--font-d-display) text-3xl">{f.title}</span>
                </dt>
                <dd className="mt-2 leading-relaxed text-[#1f2e27]/75">{f.more}</dd>
              </div>
            ))}
          </dl>
        </div>
      </section>

      {/* ── Comment ça joue ────────────────────────────────────────── */}
      <section className="mx-auto max-w-6xl px-6 py-28">
        <h2 className={h2}>Comment ça joue.</h2>
        <ol className="mt-14 grid gap-12 md:grid-cols-3">
          {STEPS.map((st, i) => (
            <li key={st.title}>
              <p className="font-(family-name:--font-d-display) text-6xl italic text-[#c9a15a]">{["I", "II", "III"][i]}</p>
              <h3 className="mt-4 text-xl font-semibold">{st.title}</h3>
              <p className="mt-3 leading-relaxed text-[#1f2e27]/75">{st.text}</p>
            </li>
          ))}
        </ol>
      </section>

      {/* ── Une histoire ───────────────────────────────────────────── */}
      <section id="histoire" className="bg-[#1f2e27] text-[#f1e9da]">
        <div className="mx-auto grid max-w-6xl gap-10 px-6 py-28 md:grid-cols-[1fr_1.2fr] md:items-center">
          <p className="font-(family-name:--font-d-display) text-[clamp(6rem,15vw,12rem)] leading-none text-[#c9a15a]">{STORY.year}</p>
          <div>
            <p className={`${eyebrow} text-[#e0a458]`}>Première histoire</p>
            <h2 className="mt-4 font-(family-name:--font-d-display) text-[clamp(2.2rem,4.4vw,3.6rem)] leading-[1]">{STORY.title}</h2>
            <p className="mt-6 text-lg leading-relaxed text-[#f1e9da]/80">{STORY.pitch}</p>
            <p className="mt-6 text-sm text-[#e0a458]">
              {STORY.duration} · {STORY.players}
            </p>
          </div>
        </div>
      </section>

      {/* ── Liste d'attente ────────────────────────────────────────── */}
      <section className="mx-auto max-w-2xl px-6 pb-36 pt-28 text-center">
        <h2 className={h2}>Elle dort encore.</h2>
        <p className="mx-auto mt-6 max-w-lg text-lg leading-relaxed text-[#1f2e27]/75">
          Les premières box sont à l&apos;atelier. Laissez votre adresse : vous serez parmi les premiers à la voir ouvrir les
          yeux.
        </p>
        <ProtoSignup
          className="mx-auto mt-9 max-w-md text-left"
          inputClassName="h-13 flex-1 rounded-md border border-[#1f2e27]/25 bg-white/50 px-4 placeholder:text-[#1f2e27]/40 focus:border-[#1f2e27] focus:outline-none"
          buttonClassName="h-13 rounded-md bg-[#1f2e27] px-6 font-semibold text-[#f1e9da] hover:bg-[#2c4237]"
          noteClassName="mt-3 text-sm text-[#1f2e27]/55"
        />
      </section>

      <ProtoBar current="atelier" />
    </div>
  );
}
