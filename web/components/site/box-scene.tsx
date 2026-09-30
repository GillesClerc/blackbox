"use client";

import { useCallback, useEffect, useRef, useState, useSyncExternalStore } from "react";

// La box, dessinée en perspective cavalière et posée sur la table : un cube de
// noyer, façade en ardoise, deux écrans ronds pour les yeux et un écran-bouche.
// Les yeux suivent le pointeur, clignent, lancent des regards en coin ; un clic
// sur la bouche lui fait lâcher une réplique (elle répond, sans rien dévoiler).

const TAGLINE = "Ouvrez l'œil.";
const QUIPS = ["Vous brûlez.", "Pas si vite.", "Curieux ?", "Chut.", "Encore un peu.", "Je vous vois.", "Approchez.", "Presque."];
const WAKE_DELAY_MS = 700;
const TYPE_START_MS = 1500;
const TYPE_INTERVAL_MS = 85;
const QUIP_REVERT_MS = 4200;

// Géométrie (viewBox 0 0 420 400) : façade 40..320 × 120..400, profondeur (70, -70).
const EYES = [
  { cx: 125, cy: 232 },
  { cx: 235, cy: 232 },
];
const EYE_R = 46;
const MOUTH = { x: 128, y: 304, w: 104, h: 34 };
const pct = (v: number, of: number) => `${(v / of) * 100}%`;

function useReducedMotion() {
  return useSyncExternalStore(
    (cb) => {
      const mq = window.matchMedia("(prefers-reduced-motion: reduce)");
      mq.addEventListener("change", cb);
      return () => mq.removeEventListener("change", cb);
    },
    () => window.matchMedia("(prefers-reduced-motion: reduce)").matches,
    () => false
  );
}

function Eye({
  style,
  offset,
  closed,
  poked,
  onPoke,
  label,
}: {
  style: React.CSSProperties;
  offset: { x: number; y: number };
  closed: boolean;
  poked: boolean;
  onPoke: () => void;
  label: string;
}) {
  return (
    <button
      type="button"
      onClick={onPoke}
      aria-label={label}
      style={style}
      className="absolute cursor-pointer select-none overflow-hidden rounded-full bg-screen shadow-[inset_0_0_0_3px_#0a0e10,inset_0_0_14px_rgba(0,0,0,0.9)] focus-visible:outline-2 focus-visible:outline-offset-4 focus-visible:outline-brass"
    >
      {/* l'œil affiché par l'écran rond */}
      <span
        className="absolute inset-[9%] rounded-full"
        style={{ background: "radial-gradient(circle at 50% 44%, #f2f1ea 0 56%, #cfd2c9 78%, #8d938c 100%)" }}
      >
        <span
          className="eye-iris absolute left-1/2 top-1/2 size-[58%] rounded-full"
          style={{
            transform: `translate(calc(-50% + ${offset.x}%), calc(-50% + ${offset.y}%))`,
            background: [
              "radial-gradient(circle at 35% 32%, rgba(255,255,255,0.35), transparent 40%)",
              "radial-gradient(circle, transparent 0 50%, rgba(6,40,36,0.85) 94%)",
              "repeating-conic-gradient(from 0deg, #3fc7b6 0deg 6deg, #1f8a7d 6deg 12deg)",
            ].join(", "),
            boxShadow: "0 0 18px 2px rgba(47,181,165,0.45)",
          }}
        >
          <span
            className="eye-pupil absolute left-1/2 top-1/2 size-[44%] -translate-x-1/2 -translate-y-1/2 rounded-full bg-[#050807]"
            data-poked={poked}
          />
          <span className="absolute left-[24%] top-[26%] size-[18%] rounded-full bg-white/80" />
        </span>
      </span>
      {/* paupières (rendues par l'écran : même noir que le verre) */}
      <span
        className="eye-lid-top absolute -left-[8%] -right-[8%] -top-full h-full bg-screen"
        data-closed={closed}
        style={{ borderRadius: "0 0 42% 42% / 0 0 22% 22%" }}
      />
      <span
        className="eye-lid-bottom absolute -bottom-full -left-[8%] -right-[8%] h-full bg-screen"
        data-closed={closed}
        style={{ borderRadius: "44% 44% 0 0 / 16% 16% 0 0" }}
      />
    </button>
  );
}

export function BoxScene() {
  const sceneRef = useRef<HTMLDivElement>(null);
  const lastMoveRef = useRef(0);
  const typeTimer = useRef<ReturnType<typeof setInterval> | null>(null);
  const revertTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const pokeTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const reduced = useReducedMotion();

  const [awake, setAwake] = useState(false);
  const [blinking, setBlinking] = useState(false);
  const [wink, setWink] = useState<0 | 1 | null>(null);
  const [poke, setPoke] = useState<0 | 1 | null>(null);
  const [message, setMessage] = useState(TAGLINE);
  const [typed, setTyped] = useState(0);
  // regard en coin au repos : elle ne vous fixe pas, elle vous jauge
  const [offset, setOffset] = useState({ x: 9, y: 3 });

  const typeOut = useCallback(
    (text: string) => {
      if (typeTimer.current) clearInterval(typeTimer.current);
      setMessage(text);
      if (reduced) return setTyped(text.length);
      setTyped(0);
      typeTimer.current = setInterval(() => {
        setTyped((n) => {
          if (n >= text.length) {
            if (typeTimer.current) clearInterval(typeTimer.current);
            typeTimer.current = null;
            return n;
          }
          return n + 1;
        });
      }, TYPE_INTERVAL_MS);
    },
    [reduced]
  );

  useEffect(() => {
    if (reduced) return;
    const wake = setTimeout(() => setAwake(true), WAKE_DELAY_MS);
    const typeStart = setTimeout(() => typeOut(TAGLINE), TYPE_START_MS);

    let blinkTimer: ReturnType<typeof setTimeout>;
    const scheduleBlink = (delay: number) => {
      blinkTimer = setTimeout(() => {
        setBlinking(true);
        setTimeout(() => {
          setBlinking(false);
          scheduleBlink(Math.random() < 0.25 ? 320 : 2600 + Math.random() * 4400);
        }, 140);
      }, delay);
    };
    scheduleBlink(3400);

    const onMove = (e: PointerEvent) => {
      const el = sceneRef.current;
      if (!el) return;
      lastMoveRef.current = Date.now();
      const r = el.getBoundingClientRect();
      const dx = e.clientX - (r.left + r.width * 0.43);
      const dy = e.clientY - (r.top + r.height * 0.58);
      const len = Math.hypot(dx, dy) || 1;
      const k = Math.min(1, len / 360) * 16; // % de déplacement de l'iris
      setOffset({ x: (dx / len) * k, y: (dy / len) * k });
    };
    window.addEventListener("pointermove", onMove);

    // regards en coin quand plus rien ne bouge
    const saccade = setInterval(() => {
      if (Date.now() - lastMoveRef.current < 2400) return;
      const side = Math.random() < 0.5 ? -1 : 1;
      setOffset({ x: side * (6 + Math.random() * 9), y: (Math.random() - 0.4) * 7 });
    }, 2700);

    const winkTimer = setInterval(() => {
      setWink(Math.random() < 0.5 ? 0 : 1);
      setTimeout(() => setWink(null), 300);
    }, 15000 + Math.random() * 6000);

    return () => {
      clearTimeout(wake);
      clearTimeout(typeStart);
      clearTimeout(blinkTimer);
      clearInterval(saccade);
      clearInterval(winkTimer);
      window.removeEventListener("pointermove", onMove);
      if (typeTimer.current) clearInterval(typeTimer.current);
      if (revertTimer.current) clearTimeout(revertTimer.current);
      if (pokeTimer.current) clearTimeout(pokeTimer.current);
    };
  }, [reduced, typeOut]);

  const doneTyping = reduced || typed >= message.length;

  // clin d'œil complice quand la bouche a fini d'écrire
  useEffect(() => {
    if (reduced || !doneTyping) return;
    const t = setTimeout(() => {
      setWink(1);
      setTimeout(() => setWink(null), 320);
    }, 500);
    return () => clearTimeout(t);
  }, [doneTyping, reduced]);

  const pokeEye = (i: 0 | 1) => {
    setPoke(i);
    if (pokeTimer.current) clearTimeout(pokeTimer.current);
    pokeTimer.current = setTimeout(() => setPoke(null), 420);
  };

  const pokeMouth = () => {
    const pool = QUIPS.filter((q) => q !== message);
    typeOut(pool[Math.floor(Math.random() * pool.length)] ?? QUIPS[0]);
    if (revertTimer.current) clearTimeout(revertTimer.current);
    if (!reduced) revertTimer.current = setTimeout(() => typeOut(TAGLINE), QUIP_REVERT_MS);
  };

  const closed = !reduced && (!awake || blinking);
  const shown = reduced ? message : message.slice(0, typed);

  return (
    <div
      ref={sceneRef}
      role="group"
      aria-label="La box : touchez ses yeux ou sa bouche pour la faire réagir."
      className="relative mx-auto w-full max-w-[460px]"
      style={{ aspectRatio: "420 / 430" }}
    >
      <svg viewBox="0 0 420 430" className="absolute inset-0 size-full" aria-hidden="true">
        <defs>
          <linearGradient id="bs-top" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0" stopColor="#9a7652" />
            <stop offset="1" stopColor="#7a5a3f" />
          </linearGradient>
          <linearGradient id="bs-side" x1="0" y1="0" x2="1" y2="0">
            <stop offset="0" stopColor="#4a3426" />
            <stop offset="1" stopColor="#3a291e" />
          </linearGradient>
          <linearGradient id="bs-front" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0" stopColor="#6d4f37" />
            <stop offset="1" stopColor="#5a4130" />
          </linearGradient>
          <linearGradient id="bs-slate" x1="0" y1="0" x2="1" y2="1">
            <stop offset="0" stopColor="#343c41" />
            <stop offset="1" stopColor="#262d31" />
          </linearGradient>
          <radialGradient id="bs-shadow" cx="0.5" cy="0.5" r="0.5">
            <stop offset="0" stopColor="#1d2327" stopOpacity="0.32" />
            <stop offset="1" stopColor="#1d2327" stopOpacity="0" />
          </radialGradient>
        </defs>
        {/* ombre portée sur la table */}
        <ellipse cx="220" cy="404" rx="210" ry="22" fill="url(#bs-shadow)" />
        {/* dessus, flanc, façade */}
        <polygon points="40,120 110,50 390,50 320,120" fill="url(#bs-top)" />
        <polygon points="320,120 390,50 390,330 320,400" fill="url(#bs-side)" />
        <rect x="40" y="120" width="280" height="280" fill="url(#bs-front)" />
        {/* arêtes en laiton (gamme Pro) */}
        <polyline points="40,120 110,50 390,50" fill="none" stroke="#b48a3c" strokeWidth="2" opacity="0.8" />
        <polyline points="320,120 390,50" fill="none" stroke="#b48a3c" strokeWidth="1.5" opacity="0.6" />
        {/* fil du bois sur le dessus */}
        <g stroke="#6a4c34" strokeWidth="1" opacity="0.35" fill="none">
          <path d="M80,92 C160,88 250,96 340,86" />
          <path d="M62,108 C150,103 240,112 330,101" />
          <path d="M98,74 C180,70 270,78 362,68" />
        </g>
        {/* panneau d'ardoise de la façade */}
        <rect x="62" y="142" width="236" height="236" rx="6" fill="url(#bs-slate)" />
        <rect x="62" y="142" width="236" height="236" rx="6" fill="none" stroke="#1b2124" strokeWidth="2" />
        {/* lunettes des écrans */}
        {EYES.map((e) => (
          <circle key={e.cx} cx={e.cx} cy={e.cy} r={EYE_R + 5} fill="#171c1f" />
        ))}
        <rect x={MOUTH.x - 5} y={MOUTH.y - 5} width={MOUTH.w + 10} height={MOUTH.h + 10} rx="5" fill="#171c1f" />
        {/* petite lueur du halo sous la façade */}
        <rect x="60" y="398" width="240" height="3" rx="1.5" fill="#2fb5a5" opacity="0.35" />
      </svg>

      {EYES.map((e, i) => (
        <Eye
          key={e.cx}
          style={{
            left: pct(e.cx - EYE_R, 420),
            top: pct(e.cy - EYE_R, 430),
            width: pct(EYE_R * 2, 420),
            height: pct(EYE_R * 2, 430),
          }}
          offset={offset}
          closed={closed || wink === i || poke === i}
          poked={poke !== null}
          onPoke={() => pokeEye(i as 0 | 1)}
          label={i === 0 ? "Œil gauche — chatouillez-la" : "Œil droit — chatouillez-la"}
        />
      ))}

      <button
        type="button"
        onClick={pokeMouth}
        aria-label="La bouche — faites-la parler"
        className="absolute flex cursor-pointer select-none items-center justify-center rounded-[4px] bg-screen px-1 focus-visible:outline-2 focus-visible:outline-offset-4 focus-visible:outline-brass"
        style={{
          left: pct(MOUTH.x, 420),
          top: pct(MOUTH.y, 430),
          width: pct(MOUTH.w, 420),
          height: pct(MOUTH.h, 430),
        }}
      >
        <span
          aria-live="polite"
          className="whitespace-nowrap font-mono text-[clamp(0.55rem,2.1vw,0.8rem)] font-medium tracking-tight text-eink-ink [text-shadow:0_0_8px_rgba(47,181,165,0.7)]"
        >
          {shown}
          {!doneTyping && <span className="eink-caret ml-px inline-block h-[0.9em] w-[0.5em] translate-y-[0.12em] bg-eink-ink" />}
        </span>
      </button>
    </div>
  );
}
