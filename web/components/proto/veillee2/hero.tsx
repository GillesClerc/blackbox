"use client";

import { useEffect, useRef, useState } from "react";
import { Eye, Eyes, Mouth, useEyes, type Character } from "@/components/eyes/eyes";
import { POKE_LINES } from "../content";
import { ProtoSignup } from "../signup";
import { Ink } from "./lamp";
import s from "./veillee2.module.css";

// Un visage par histoire : la box change de personnage de temps en temps
// (forme de pupille et couleur d'iris), le temps d'un clignement.
const CHARACTERS: (Character & { name: string; line: string })[] = [
  { name: "L'hôte", eye: "default", line: "Bonsoir. Je vous attendais." },
  { name: "Le dragon", eye: "dragon", line: "Approchez. Si vous l'osez." },
  { name: "L'abysse", eye: "nosclera", hue: 200, line: "Vous entendez la mer ?" },
  { name: "Le faune", eye: "goat", line: "On joue ? Je triche un peu." },
  { name: "L'étrangère", eye: "default", hue: 150, line: "Je ne suis pas d'ici." },
  { name: "La sentinelle", eye: "terminator", line: "Analyse des joueurs en cours." },
  { name: "Le triton", eye: "newt", hue: 150, line: "Glissant, n'est-ce pas ?" },
];
const CYCLE_MS = 9000;

const INK = [
  { t: "le code est sous ses yeux", x: "5%", y: "16%", r: -8, size: "1.4rem" },
  { t: "3 · 7 · 1", x: "80%", y: "12%", r: 6, size: "2.1rem" },
  { t: "← pas par là", x: "3%", y: "58%", r: 4, size: "1.25rem" },
  { t: "elle ment parfois", x: "77%", y: "50%", r: -5, size: "1.35rem" },
  { t: "ne la retournez pas avant minuit", x: "6%", y: "90%", r: -3, size: "1.15rem" },
];

/** La bouche annonce chaque nouveau personnage. */
function Announce({ line }: { line: string }) {
  const api = useEyes();
  const first = useRef(true);
  useEffect(() => {
    if (first.current) {
      first.current = false;
      return;
    }
    const t = setTimeout(() => api?.say(line), 420); // après le clignement
    return () => clearTimeout(t);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [line]);
  return null;
}

export function VeilleeV2Hero() {
  const [idx, setIdx] = useState(0);
  const pausedUntil = useRef(0);

  useEffect(() => {
    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) return;
    const t = setInterval(() => {
      if (performance.now() > pausedUntil.current) setIdx((i) => (i + 1) % CHARACTERS.length);
    }, CYCLE_MS);
    return () => clearInterval(t);
  }, []);

  const c = CHARACTERS[idx];

  return (
    <section className="relative isolate overflow-hidden">
      <div
        aria-hidden="true"
        className="absolute inset-0 -z-10"
        style={{ background: "radial-gradient(1100px 560px at 50% 0%, rgba(232,116,59,0.1), rgba(232,116,59,0) 70%)" }}
      />
      <Ink notes={INK} />

      <div className="relative mx-auto flex max-w-6xl flex-col items-center px-6 pb-20 pt-6 text-center md:pb-24 md:pt-10">
        <p className="text-[0.75rem] font-semibold uppercase tracking-[0.3em] text-[#ffc27a]/75">Escape game de table</p>

        <Eyes pokeLines={POKE_LINES} character={c}>
          <Announce line={c.line} />
          <div className="mt-10 flex items-center gap-[clamp(22px,4vw,56px)]">
            <Eye side={0} className={s.eye} />
            <Eye side={1} className={s.eye} />
          </div>
          <Mouth lines={[CHARACTERS[0].line]} holdMs={60_000} className={`${s.mouth} mt-7`} textClassName={s.mouthText} />
        </Eyes>

        {/* sélecteur de personnage */}
        <div className="mt-7 flex flex-col items-center gap-2.5">
          <p className="text-sm text-[#f4e7d3]/60">
            Ce soir : <span className="font-semibold text-[#ffc27a]">{c.name}</span>
          </p>
          <div className="flex gap-1" role="radiogroup" aria-label="Personnage de la box">
            {CHARACTERS.map((ch, i) => (
              <button
                key={ch.name}
                role="radio"
                aria-checked={i === idx}
                aria-label={ch.name}
                onClick={() => {
                  pausedUntil.current = performance.now() + 30_000;
                  setIdx(i);
                }}
                className="grid size-6 place-items-center rounded-full focus-visible:outline-2 focus-visible:outline-[#ffc27a]"
              >
                <span
                  className={`block rounded-full transition-all ${i === idx ? "size-2.5 bg-[#ffc27a]" : "size-1.5 bg-[#f4e7d3]/35"}`}
                />
              </button>
            ))}
          </div>
        </div>

        <h1 className="mt-12 font-(family-name:--font-e-display) text-[clamp(3.4rem,10vw,9rem)] leading-[0.9] tracking-[-0.02em] text-balance">
          Ouvrez l&apos;œil<span className="text-[#e8743b]">.</span>
        </h1>
        <p className="mt-6 max-w-xl text-[clamp(1.05rem,1.5vw,1.25rem)] leading-relaxed text-[#f4e7d3]/80">
          Une boîte d&apos;escape game pour vos soirées entre amis. Posée au milieu de la table, elle vous regarde, vous
          écoute, et change de visage à chaque histoire.
        </p>
        <ProtoSignup
          className="mt-8 w-full max-w-md text-left"
          inputClassName="h-13 flex-1 rounded-full border border-[#f4e7d3]/20 bg-[#f4e7d3]/[0.06] px-5 text-[#f4e7d3] placeholder:text-[#f4e7d3]/40 focus:border-[#ffc27a] focus:outline-none"
          buttonClassName="h-13 rounded-full bg-[#ffc27a] px-6 font-semibold text-[#1c1411] transition hover:bg-[#ffd39b] focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-[#ffc27a]"
          noteClassName="mt-3 px-5 text-sm text-[#f4e7d3]/55"
        />
        <p className="mt-10 text-sm italic text-[#ffc27a]/65">
          <span className="hidden md:inline">Promenez la lampe sur la page : tout n&apos;est pas écrit à l&apos;encre visible.</span>
          <span className="md:hidden">Faites défiler sous la lampe : tout n&apos;est pas écrit à l&apos;encre visible.</span>
        </p>
      </div>
    </section>
  );
}
