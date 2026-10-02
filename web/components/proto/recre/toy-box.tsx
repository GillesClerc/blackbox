"use client";

import { useEffect, useRef, useState } from "react";
import { Eye, Eyes, Mouth } from "@/components/eyes/eyes";
import { MOUTH_LINES, POKE_LINES } from "../content";
import s from "./recre.module.css";

const INK = "#3a2340";

// La box en jouet : cube en aplats, gros trait prune. Les vrais yeux du firmware
// dans des écrans ronds. Elle respire, penche vers le pointeur, saute quand on la touche.
export function ToyBox() {
  const [hop, setHop] = useState(0);
  const tiltRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const move = (e: PointerEvent) => {
      const el = tiltRef.current;
      if (!el) return;
      const r = el.getBoundingClientRect();
      const dx = (e.clientX - (r.left + r.width / 2)) / window.innerWidth;
      el.style.rotate = `${Math.max(-6, Math.min(6, dx * 14))}deg`;
    };
    window.addEventListener("pointermove", move, { passive: true });
    return () => window.removeEventListener("pointermove", move);
  }, []);

  return (
    <Eyes pokeLines={POKE_LINES} reach={0.45}>
      <div
        className="relative mx-auto w-full max-w-[460px]"
        onPointerDown={() => setHop((h) => h + 1)}
      >
        <svg viewBox="0 0 400 420" className="absolute inset-0 h-full w-full overflow-visible" aria-hidden="true">
          <ellipse className={s.shadow} cx="205" cy="398" rx="150" ry="15" fill={INK} />
        </svg>
        <div ref={tiltRef} className={s.tilt}>
          <div className={s.bob}>
            <div key={hop} className={hop ? s.hop : undefined}>
              <div className="relative aspect-[400/420]">
                <svg viewBox="0 0 400 420" className="absolute inset-0 h-full w-full overflow-visible" aria-hidden="true">
                  <g stroke={INK} strokeWidth="8" strokeLinejoin="round" strokeLinecap="round">
                    {/* dessus : la voix (grille du haut-parleur) */}
                    <path d="M60 130 L120 70 L360 70 L300 130 Z" fill="#8ad9b8" />
                    {/* côté : le tableau de bord */}
                    <path d="M300 130 L360 70 L360 310 L300 370 Z" fill="#c9472c" />
                    {/* devant : le visage */}
                    <rect x="60" y="130" width="240" height="240" rx="6" fill="#f0603f" />
                  </g>
                  <g transform="matrix(1 0 1 -1 60 130)" fill={INK}>
                    {Array.from({ length: 5 }, (_, r) =>
                      Array.from({ length: 9 }, (_, c) => (
                        <circle key={`${r}-${c}`} cx={58 + c * 14 - r * 2} cy={14 + r * 8} r="2.6" />
                      ))
                    )}
                  </g>
                  <g transform="matrix(1 -1 0 1 300 130)" stroke={INK} strokeLinecap="round">
                    <line x1="18" y1="34" x2="18" y2="120" strokeWidth="5" />
                    <line x1="40" y1="34" x2="40" y2="120" strokeWidth="5" />
                    <rect x="10" y="58" width="16" height="10" rx="3" fill="#fff6e5" strokeWidth="4" />
                    <rect x="32" y="92" width="16" height="10" rx="3" fill="#fff6e5" strokeWidth="4" />
                    <circle cx="29" cy="165" r="15" fill="#ffc93c" strokeWidth="5" />
                    <line x1="29" y1="165" x2="29" y2="152" strokeWidth="5" />
                    <circle cx="29" cy="205" r="5" fill="#8ad9b8" strokeWidth="4" />
                  </g>
                  {/* reflets de jouet */}
                  <path d="M76 150 L76 196" stroke="#fff6e5" strokeOpacity="0.55" strokeWidth="7" strokeLinecap="round" />
                  <path d="M76 212 L76 222" stroke="#fff6e5" strokeOpacity="0.55" strokeWidth="7" strokeLinecap="round" />
                  {/* lunettes des écrans */}
                  <circle cx="130" cy="215" r="49" fill={INK} />
                  <circle cx="230" cy="215" r="49" fill={INK} />
                  <rect x="122" y="286" width="116" height="44" rx="12" fill={INK} />
                  {/* étiquette */}
                  <g transform="rotate(-9 104 352)">
                    <rect x="66" y="340" width="82" height="24" rx="5" fill="#fff6e5" stroke={INK} strokeWidth="3.5" />
                    <text x="107" y="357" textAnchor="middle" fontSize="11" fontWeight="800" fill={INK} letterSpacing="1">
                      SOUFFLEZ ↗
                    </text>
                  </g>
                </svg>
                <Eye side={0} className={`${s.eye} absolute! left-[22%] top-[41.2%] w-[21%]`} />
                <Eye side={1} className={`${s.eye} absolute! left-[47%] top-[41.2%] w-[21%]`} />
                <Mouth
                  lines={MOUTH_LINES}
                  className="absolute left-[31.5%] top-[68.8%] grid h-[9.5%] w-[28%] place-items-center overflow-hidden rounded-[10px] bg-black px-2"
                  textClassName="text-center text-[clamp(0.6rem,1.3vw,0.9rem)] font-bold leading-tight text-[#ffe9a8]"
                />
              </div>
            </div>
          </div>
        </div>
      </div>
    </Eyes>
  );
}
