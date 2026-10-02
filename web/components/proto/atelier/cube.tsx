"use client";

import { useEffect, useRef, useState } from "react";
import { Eye, Eyes, Mouth } from "@/components/eyes/eyes";
import { MOUTH_LINES, POKE_LINES } from "../content";
import s from "./atelier.module.css";

// Veinage du bois : bruit SVG étiré (feTurbulence), posé en calque sur la teinte.
const grain = (alpha: number, fx = 0.008, fy = 0.22) =>
  `url("data:image/svg+xml;utf8,${encodeURIComponent(
    `<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'><filter id='g'><feTurbulence type='fractalNoise' baseFrequency='${fx} ${fy}' numOctaves='3' seed='7'/><feColorMatrix values='0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  0 0 0 ${alpha} 0'/></filter><rect width='100%' height='100%' filter='url(#g)'/></svg>`
  )}")`;

export const FINISHES = {
  lite: { label: "Lite", detail: "MDF teinté", wood: "#d6ae80", grain: grain(0.5, 0.01, 0.3) },
  pro: { label: "Pro", detail: "Noyer, ardoise, laiton", wood: "#5c3b26", grain: grain(0.9) },
} as const;
type Finish = keyof typeof FINISHES;

// Orientation qui amène chaque face devant (rotateX, rotateY), avec un léger
// trois-quarts pour garder le volume.
const VIEWS = {
  devant: { label: "Visage", rx: -14, ry: -26 },
  dessus: { label: "Voix", rx: -62, ry: -18 },
  "cote-1": { label: "Tableau de bord", rx: -14, ry: -98 },
  "cote-2": { label: "Toucher", rx: -14, ry: 82 },
  "cote-3": { label: "Mystère", rx: -14, ry: 162 },
  dessous: { label: "Halo", rx: 64, ry: -18 },
} as const;
type View = keyof typeof VIEWS;

export function AtelierCube() {
  const [finish, setFinish] = useState<Finish>("pro");
  const [view, setView] = useState<View>("devant");
  const cubeRef = useRef<HTMLDivElement>(null);
  const rot = useRef({ rx: -14, ry: -26 });
  const target = useRef<{ rx: number; ry: number } | null>(null);
  const drag = useRef<{ x: number; y: number } | null>(null);
  const lastTouch = useRef(0);

  useEffect(() => {
    const reduced = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    let raf = 0;
    const tick = (t: number) => {
      const r = rot.current;
      const tg = target.current;
      if (tg) {
        const k = reduced ? 1 : 0.09;
        r.rx += (tg.rx - r.rx) * k;
        r.ry += (tg.ry - r.ry) * k;
        if (Math.abs(tg.rx - r.rx) < 0.1 && Math.abs(tg.ry - r.ry) < 0.1) target.current = null;
      }
      // au repos, elle se balance doucement
      const sway = !reduced && !drag.current && !tg && t - lastTouch.current > 2500 ? Math.sin(t / 1600) * 7 : 0;
      if (cubeRef.current) {
        cubeRef.current.style.transform = `rotateX(${r.rx}deg) rotateY(${r.ry + sway}deg)`;
      }
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, []);

  const goTo = (v: View) => {
    setView(v);
    const { rx, ry } = VIEWS[v];
    // chemin le plus court
    const cur = rot.current.ry;
    const turns = Math.round((cur - ry) / 360);
    target.current = { rx, ry: ry + turns * 360 };
  };

  const f = FINISHES[finish];
  const vars = { "--wood": f.wood, "--grain": f.grain } as React.CSSProperties;

  return (
    <div className="flex flex-col items-center">
      <div
        className={s.scene}
        onPointerDown={(e) => {
          if ((e.target as HTMLElement).closest("button")) return; // clic sur un œil : pas de rotation
          drag.current = { x: e.clientX, y: e.clientY };
          target.current = null;
          (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
        }}
        onPointerMove={(e) => {
          if (!drag.current) return;
          rot.current.ry += (e.clientX - drag.current.x) * 0.45;
          rot.current.rx = Math.max(-80, Math.min(80, rot.current.rx - (e.clientY - drag.current.y) * 0.45));
          drag.current = { x: e.clientX, y: e.clientY };
          lastTouch.current = performance.now();
        }}
        onPointerUp={() => {
          drag.current = null;
          lastTouch.current = performance.now();
        }}
      >
        <Eyes pokeLines={POKE_LINES} reach={0.45}>
          <div ref={cubeRef} className={`${s.cube} ${finish === "pro" ? s.pro : s.lite}`} style={vars}>
            {/* devant : le visage */}
            <div className={`${s.face} ${s.front}`}>
              <div className={s.panel} style={finish === "lite" ? { background: "rgba(0,0,0,0.06)" } : undefined}>
                <Eye side={0} className="absolute! left-[11%] top-[17%] w-[34%] shadow-[0_0_0_3px_#111]" />
                <Eye side={1} className="absolute! right-[11%] top-[17%] w-[34%] shadow-[0_0_0_3px_#111]" />
                <Mouth
                  lines={MOUTH_LINES}
                  className="absolute bottom-[14%] left-[24%] grid h-[17%] w-[52%] place-items-center overflow-hidden rounded-[8px] bg-black px-2 shadow-[0_0_0_3px_#111]"
                  textClassName="text-center text-[clamp(0.55rem,1vw,0.85rem)] font-medium leading-tight text-[#ffe2b0]"
                />
              </div>
            </div>
            {/* dessus : la voix */}
            <div className={`${s.face} ${s.top}`}>
              <div className="absolute inset-[22%] grid grid-cols-7 place-items-center">
                {Array.from({ length: 49 }, (_, i) => (
                  <span key={i} className="size-[22%] rounded-full bg-black/70 shadow-[0_1px_0_rgba(255,255,255,0.15)]" />
                ))}
              </div>
            </div>
            {/* côté 1 : le tableau de bord */}
            <div className={`${s.face} ${s.right}`}>
              <div className={`${s.panel} flex items-center justify-around`}>
                {[38, 64, 22].map((v, i) => (
                  <div key={i} className="relative h-[70%] w-[6%] rounded-full bg-black/60">
                    <span
                      className="absolute left-1/2 h-[12%] w-[260%] -translate-x-1/2 rounded-[3px] bg-[#c9a15a] shadow"
                      style={{ top: `${v}%` }}
                    />
                  </div>
                ))}
                <div className="size-[22%] rounded-full bg-[#c9a15a] shadow-[inset_0_-4px_6px_rgba(0,0,0,0.35),0_3px_6px_rgba(0,0,0,0.4)]" />
              </div>
            </div>
            {/* côté 2 : le toucher */}
            <div className={`${s.face} ${s.left}`}>
              <div className="absolute inset-[24%] grid grid-cols-2 gap-[18%]">
                {[0, 1, 2, 3].map((i) => (
                  <span key={i} className="rounded-full border border-black/30 shadow-[inset_0_1px_3px_rgba(0,0,0,0.35)]" />
                ))}
              </div>
            </div>
            {/* côté 3 : la face mystère */}
            <div className={`${s.face} ${s.back}`}>
              <div className="absolute inset-[26%] grid place-items-center rounded-[10%] shadow-[inset_0_3px_10px_rgba(0,0,0,0.55)]">
                <span className="font-(family-name:--font-d-display) text-[calc(var(--s)*0.22)] text-black/45">?</span>
              </div>
            </div>
            {/* dessous : le halo */}
            <div className={`${s.face} ${s.bottom}`}>
              <div className={s.halo} />
            </div>
          </div>
        </Eyes>
      </div>
      <div aria-hidden="true" className={s.shadow} style={{ "--s": "clamp(210px, 27vw, 330px)" } as React.CSSProperties} />

      {/* finitions */}
      <div className="mt-8 flex gap-2" role="radiogroup" aria-label="Finition">
        {(Object.keys(FINISHES) as Finish[]).map((k) => (
          <button
            key={k}
            role="radio"
            aria-checked={finish === k}
            onClick={() => setFinish(k)}
            className={`flex items-center gap-2.5 rounded-full border py-1.5 pl-1.5 pr-4 text-sm transition ${
              finish === k ? "border-[#e0a458] bg-[#e0a458]/12 text-[#f1e9da]" : "border-[#f1e9da]/20 text-[#f1e9da]/65 hover:border-[#f1e9da]/50"
            }`}
          >
            <span
              className="size-7 rounded-full shadow-[inset_0_0_0_1px_rgba(0,0,0,0.3)]"
              style={{
                background: `${FINISHES[k].grain}, ${FINISHES[k].wood}`,
                boxShadow: k === "pro" ? "inset 0 0 0 3px #c9a15a" : undefined,
              }}
            />
            <span>
              <b className="font-semibold">{FINISHES[k].label}</b> · {FINISHES[k].detail}
            </span>
          </button>
        ))}
      </div>

      {/* faire tourner vers une face */}
      <div className="mt-5 flex max-w-md flex-wrap justify-center gap-x-4 gap-y-1 text-sm text-[#f1e9da]/60">
        {(Object.keys(VIEWS) as View[]).map((v) => (
          <button
            key={v}
            onClick={() => goTo(v)}
            className={`underline-offset-4 hover:text-[#f1e9da] hover:underline ${view === v ? "text-[#e0a458]" : ""}`}
          >
            {VIEWS[v].label}
          </button>
        ))}
      </div>
      <p className="mt-3 text-xs italic text-[#f1e9da]/45">ou attrapez-la pour la tourner</p>
    </div>
  );
}
