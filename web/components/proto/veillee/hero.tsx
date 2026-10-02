"use client";

import { useEffect, useRef } from "react";
import { Eye, Eyes, Mouth } from "@/components/eyes/eyes";
import { MOUTH_LINES, POKE_LINES } from "../content";
import { ProtoSignup } from "../signup";
import s from "./veillee.module.css";

// Encre invisible : des notes de joueurs griffonnées, que seule la lampe révèle.
const INK = [
  { t: "le code est sous ses yeux", x: "6%", y: "18%", r: -8, size: "1.5rem" },
  { t: "3 · 7 · 1", x: "80%", y: "14%", r: 6, size: "2.2rem" },
  { t: "← pas par là", x: "4%", y: "62%", r: 4, size: "1.3rem" },
  { t: "elle ment parfois", x: "76%", y: "56%", r: -5, size: "1.4rem" },
  { t: "soufflez", x: "70%", y: "84%", r: 9, size: "1.7rem" },
  { t: "ne la retournez pas avant minuit", x: "8%", y: "88%", r: -3, size: "1.2rem" },
];

export function VeilleeHero() {
  const ref = useRef<HTMLElement>(null);

  useEffect(() => {
    const el = ref.current;
    if (!el) return;
    let raf = 0;
    const move = (e: PointerEvent) => {
      cancelAnimationFrame(raf);
      raf = requestAnimationFrame(() => {
        const r = el.getBoundingClientRect();
        el.style.setProperty("--lx", `${e.clientX - r.left}px`);
        el.style.setProperty("--ly", `${e.clientY - r.top}px`);
      });
    };
    window.addEventListener("pointermove", move, { passive: true });
    window.addEventListener("pointerdown", move, { passive: true });
    return () => {
      cancelAnimationFrame(raf);
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerdown", move);
    };
  }, []);

  return (
    <section ref={ref} className={s.hero}>
      <div className={s.lamp} aria-hidden="true" />
      <div className={`${s.ink} font-(family-name:--font-a-display)`} aria-hidden="true">
        {INK.map((i) => (
          <span key={i.t} style={{ left: i.x, top: i.y, rotate: `${i.r}deg`, fontSize: i.size }}>
            {i.t}
          </span>
        ))}
      </div>

      <div className="relative z-10 mx-auto flex max-w-6xl flex-col items-center px-6 pb-24 pt-8 text-center md:pt-12">
        <p className="text-[0.78rem] font-semibold uppercase tracking-[0.28em] text-[#ffc27a]/80">
          Escape game de table
        </p>

        <Eyes pokeLines={POKE_LINES}>
          <div className="mt-10 flex items-center gap-[clamp(28px,6vw,90px)] md:mt-14">
            <Eye side={0} className={s.eye} />
            <Eye side={1} className={s.eye} />
          </div>
          <Mouth lines={MOUTH_LINES} className={`${s.mouth} mt-[clamp(26px,4vw,48px)]`} textClassName={s.mouthText} />
        </Eyes>

        <h1 className="mt-14 font-(family-name:--font-a-display) text-[clamp(3.6rem,11vw,10rem)] leading-[0.9] tracking-[-0.02em] text-balance">
          Ouvrez l&apos;œil<span className="text-[#e8743b]">.</span>
        </h1>
        <p className="mt-7 max-w-xl text-[clamp(1.1rem,1.6vw,1.3rem)] leading-relaxed text-[#f4e7d3]/80">
          Une boîte d&apos;escape game à poser au milieu de la table. Elle vous regarde, vous écoute et garde ses secrets
          jusqu&apos;au bout de la soirée.
        </p>
        <ProtoSignup
          className="mt-9 w-full max-w-md text-left"
          inputClassName="h-13 flex-1 rounded-full border border-[#f4e7d3]/20 bg-[#f4e7d3]/[0.06] px-5 text-[#f4e7d3] placeholder:text-[#f4e7d3]/40 focus:border-[#ffc27a] focus:outline-none"
          buttonClassName="h-13 rounded-full bg-[#ffc27a] px-6 font-semibold text-[#1c1411] transition hover:bg-[#ffd39b] focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-[#ffc27a]"
          noteClassName="mt-3 px-5 text-sm text-[#f4e7d3]/55"
        />
        <p className="mt-12 text-sm italic text-[#ffc27a]/70">Braquez la lampe sur ses yeux. Ou cliquez-les, pour voir.</p>
      </div>
    </section>
  );
}
