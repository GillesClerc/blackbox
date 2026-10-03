"use client";

import { useEffect, useRef, useState } from "react";
import { Eye, Eyes, Mouth } from "@/components/eyes/eyes";
import { POKE_LINES } from "../content";
import { Ink } from "./lamp";
import s from "./veillee2.module.css";

// Six faces, six énigmes. Rien n'est dit de ce que fait chaque face : chacune se
// présente par une devinette. Le chiffre gravé relie l'énigme à sa face ; la box
// tourne vers la face de l'énigme qu'on lit (au défilement ou au clic).
const RIDDLES = [
  {
    n: "I",
    face: "front",
    rx: -10, ry: -14,
    text: "J'ai deux yeux et je n'ai jamais dormi. Votre souffle me trouble, votre lumière m'éblouit.",
    ink: "regardez-la dans les yeux",
  },
  {
    n: "II",
    face: "top",
    rx: -56, ry: -14,
    text: "Je parle sans lèvres. Ce que je garde, seul un objet bavard posé sur moi saura le lire.",
    ink: "elle aime qu'on lui parle",
  },
  {
    n: "III",
    face: "right",
    rx: -10, ry: -100,
    text: "On me pousse, on me tourne, on me bascule. Une seule combinaison m'apaise, et elle change à chaque histoire.",
    ink: "tout est question de réglage",
  },
  {
    n: "IV",
    face: "left",
    rx: -10, ry: 80,
    text: "Effleurez-moi : je frémis. Approchez sans toucher : je le sais déjà.",
    ink: "plus près…",
  },
  {
    n: "V",
    face: "back",
    rx: -10, ry: 166,
    text: "Je n'attends qu'une chose. Pas la vôtre, pas n'importe laquelle : la bonne.",
    ink: "la bonne chose",
  },
  {
    n: "VI",
    face: "bottom",
    rx: 56, ry: -14,
    text: "Personne ne me regarde, et pourtant j'entends tout ce qui se dit autour de la table. Retournez-moi, si vous osez.",
    ink: "retournez-la",
  },
] as const;

// Veinage du noyer : bruit SVG étiré.
const GRAIN = `url("data:image/svg+xml;utf8,${encodeURIComponent(
  "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'><filter id='g'><feTurbulence type='fractalNoise' baseFrequency='0.008 0.22' numOctaves='3' seed='7'/><feColorMatrix values='0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  0 0 0 0.85 0'/></filter><rect width='100%' height='100%' filter='url(#g)'/></svg>"
)}")`;

export function Riddles() {
  const [active, setActive] = useState(0);
  const cubeRef = useRef<HTMLDivElement>(null);
  const rot = useRef({ rx: RIDDLES[0].rx, ry: RIDDLES[0].ry });
  const items = useRef<(HTMLElement | null)[]>([]);

  // L'énigme au milieu de l'écran devient l'énigme active.
  useEffect(() => {
    const io = new IntersectionObserver(
      (entries) => {
        for (const e of entries) if (e.isIntersecting) setActive(Number((e.target as HTMLElement).dataset.i));
      },
      { rootMargin: "-45% 0px -45% 0px" }
    );
    items.current.forEach((el) => el && io.observe(el));
    return () => io.disconnect();
  }, []);

  // La box tourne vers la face de l'énigme active, et se balance au repos.
  useEffect(() => {
    const reduced = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    let raf = 0;
    const tick = (t: number) => {
      const tg = RIDDLES[active];
      const r = rot.current;
      // chemin le plus court
      const ry = tg.ry + Math.round((r.ry - tg.ry) / 360) * 360;
      const k = reduced ? 1 : 0.07;
      r.rx += (tg.rx - r.rx) * k;
      r.ry += (ry - r.ry) * k;
      const sway = reduced ? 0 : Math.sin(t / 1700) * 5;
      if (cubeRef.current) cubeRef.current.style.transform = `rotateX(${r.rx}deg) rotateY(${r.ry + sway}deg)`;
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, [active]);

  const faceProps = (face: string) => ({
    "data-active": RIDDLES[active].face === face,
  });
  const numeral = (n: string) => <span className={s.numeral}>{n}</span>;

  return (
    <div className="grid gap-x-16 md:grid-cols-[minmax(0,1fr)_minmax(0,1.15fr)]">
      {/* la box, collée en haut pendant qu'on lit */}
      <div className="sticky top-0 z-10 -mx-6 bg-[#140e0c]/95 px-6 pb-4 pt-12 backdrop-blur md:top-[16vh] md:mx-0 md:self-start md:bg-transparent md:p-0 md:pt-6 md:backdrop-blur-none">
        <div className={`${s.stage} flex flex-col items-center`}>
          <div className={s.scene} style={{ "--grain": GRAIN } as React.CSSProperties}>
            <Eyes pokeLines={POKE_LINES} reach={0.5}>
              <div ref={cubeRef} className={s.cube}>
                <div className={`${s.face} ${s.front}`} {...faceProps("front")}>
                  <div className={s.panel}>
                    <Eye side={0} className="absolute! left-[10%] top-[16%] w-[35%] shadow-[0_0_0_2px_#000]" />
                    <Eye side={1} className="absolute! right-[10%] top-[16%] w-[35%] shadow-[0_0_0_2px_#000]" />
                    <Mouth
                      lines={["Devinez."]}
                      holdMs={60_000}
                      className="absolute bottom-[13%] left-[25%] grid h-[17%] w-[50%] place-items-center overflow-hidden rounded-[6px] bg-black px-1"
                      textClassName="text-center text-[clamp(0.5rem,0.9vw,0.8rem)] font-semibold text-[#ffdcae]"
                    />
                  </div>
                </div>
                <div className={`${s.face} ${s.top}`} {...faceProps("top")}>{numeral("II")}</div>
                <div className={`${s.face} ${s.right}`} {...faceProps("right")}>{numeral("III")}</div>
                <div className={`${s.face} ${s.left}`} {...faceProps("left")}>{numeral("IV")}</div>
                <div className={`${s.face} ${s.back}`} {...faceProps("back")}>{numeral("V")}</div>
                <div className={`${s.face} ${s.bottom}`} {...faceProps("bottom")}>{numeral("VI")}</div>
              </div>
            </Eyes>
          </div>
          <p className={`${s.label} font-(family-name:--font-e-display) text-lg text-[#ffc27a]`}>
            Face {RIDDLES[active].n}
          </p>
        </div>
      </div>

      {/* les énigmes */}
      <ol className="md:py-[8vh]">
        {RIDDLES.map((r, i) => (
          <li
            key={r.n}
            ref={(el) => {
              items.current[i] = el;
            }}
            data-i={i}
            data-active={i === active}
            className={`${s.riddle} relative py-12 md:flex md:min-h-[52vh] md:flex-col md:justify-center md:py-0`}
          >
            <Ink notes={[{ t: r.ink, x: "52%", y: "78%", r: -4, size: "1.3rem" }]} />
            <button onClick={() => setActive(i)} className="relative text-left focus-visible:outline-2 focus-visible:outline-offset-8 focus-visible:outline-[#ffc27a]">
              <span className="text-[0.75rem] font-semibold uppercase tracking-[0.3em] text-[#e8743b]">Énigme {r.n}</span>
              <span className="mt-4 block font-(family-name:--font-e-display) text-[clamp(1.6rem,2.8vw,2.4rem)] leading-[1.18] text-balance">
                {r.text}
              </span>
            </button>
          </li>
        ))}
      </ol>
    </div>
  );
}
