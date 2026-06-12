"use client";

import { useEffect, useRef, useState } from "react";

const TAGLINE = "Ouvrez l'œil.";
const WAKE_DELAY_MS = 600;
const TYPE_START_MS = 1400;
const TYPE_INTERVAL_MS = 90;

function Eye({
  offset,
  closed,
}: {
  offset: { x: number; y: number };
  closed: boolean;
}) {
  return (
    <div
      className="relative size-24 sm:size-32 rounded-full overflow-hidden border-4 border-[#222a3d] bg-night shadow-[0_0_40px_-6px_rgba(232,163,61,0.3),inset_0_0_18px_rgba(0,0,0,0.9)]"
      aria-hidden="true"
    >
      {/* sclère */}
      <div
        className="absolute inset-[5%] rounded-full"
        style={{
          background:
            "radial-gradient(circle at 50% 42%, #ece6d6 0 58%, #d3ccb8 76%, #998f78 100%)",
          boxShadow: "inset 0 4px 10px rgba(40,30,10,0.35)",
        }}
      >
        {/* iris */}
        <div
          className="eye-iris absolute left-1/2 top-1/2 size-[60%] rounded-full"
          style={{
            transform: `translate(calc(-50% + ${offset.x}px), calc(-50% + ${offset.y}px))`,
            background: [
              "radial-gradient(circle at 36% 34%, rgba(255,255,255,0.25), transparent 42%)",
              "radial-gradient(circle, transparent 0 52%, rgba(42,29,8,0.9) 92%)",
              "repeating-conic-gradient(from 0deg, #c9882c 0deg 7deg, #8a5a1a 7deg 14deg)",
              "radial-gradient(circle, #e8a33d 0 40%, #b97c24 100%)",
            ].join(", "),
            boxShadow:
              "0 0 16px 1px rgba(232,163,61,0.35), inset 0 0 6px rgba(0,0,0,0.5)",
          }}
        >
          {/* pupille */}
          <div className="eye-pupil absolute left-1/2 top-1/2 size-[44%] -translate-x-1/2 -translate-y-1/2 rounded-full bg-[#0a0703] shadow-[inset_0_0_5px_rgba(0,0,0,1)]" />
          {/* reflets — placés bas pour rester visibles sous la paupière */}
          <div className="absolute left-[22%] top-[30%] size-[16%] rounded-full bg-white/75 blur-[1px]" />
          <div className="absolute left-[62%] top-[60%] size-[9%] rounded-full bg-white/35 blur-[1px]" />
        </div>
      </div>
      {/* paupières */}
      <div
        className="eye-lid-top absolute -left-[8%] -right-[8%] -top-full h-full bg-[#1a2130]"
        data-closed={closed}
        style={{ borderRadius: "0 0 50% 50% / 0 0 32% 32%" }}
      />
      <div
        className="eye-lid-bottom absolute -bottom-full -left-[8%] -right-[8%] h-full bg-[#1a2130]"
        data-closed={closed}
        style={{ borderRadius: "50% 50% 0 0 / 24% 24% 0 0" }}
      />
    </div>
  );
}

export function BoxFace() {
  const faceRef = useRef<HTMLDivElement>(null);
  const lastMoveRef = useRef(0);
  const [reduced, setReduced] = useState(false);
  const [awake, setAwake] = useState(false);
  const [blinking, setBlinking] = useState(false);
  const [wink, setWink] = useState<"left" | "right" | null>(null);
  const [typed, setTyped] = useState(0);
  // regard en coin par défaut : elle ne vous fixe pas, elle vous jauge
  const [offset, setOffset] = useState({ x: 6, y: 2 });

  useEffect(() => {
    const mq = window.matchMedia("(prefers-reduced-motion: reduce)");
    setReduced(mq.matches);
    if (mq.matches) {
      setAwake(true);
      setTyped(TAGLINE.length);
    }
  }, []);

  useEffect(() => {
    if (reduced) return;
    const wake = setTimeout(() => setAwake(true), WAKE_DELAY_MS);

    let blinkTimer: ReturnType<typeof setTimeout>;
    const scheduleBlink = (delay: number) => {
      blinkTimer = setTimeout(() => {
        setBlinking(true);
        setTimeout(() => {
          setBlinking(false);
          scheduleBlink(
            Math.random() < 0.25 ? 350 : 2800 + Math.random() * 4200
          );
        }, 150);
      }, delay);
    };
    scheduleBlink(3200);

    const typeStart = setTimeout(() => {
      const typeTimer = setInterval(() => {
        setTyped((n) => {
          if (n >= TAGLINE.length) {
            clearInterval(typeTimer);
            return n;
          }
          return n + 1;
        });
      }, TYPE_INTERVAL_MS);
    }, TYPE_START_MS);

    const onMove = (e: MouseEvent) => {
      const face = faceRef.current;
      if (!face) return;
      lastMoveRef.current = Date.now();
      const r = face.getBoundingClientRect();
      const dx = e.clientX - (r.left + r.width / 2);
      const dy = e.clientY - (r.top + r.height / 2);
      const max = 10;
      const len = Math.hypot(dx, dy) || 1;
      const k = Math.min(1, len / 320);
      setOffset({ x: (dx / len) * max * k, y: (dy / len) * max * k });
    };
    window.addEventListener("mousemove", onMove);

    // micro-saccades latérales quand le curseur ne bouge plus : regards en coin
    const saccade = setInterval(() => {
      if (Date.now() - lastMoveRef.current < 2200) return;
      const side = Math.random() < 0.5 ? -1 : 1;
      setOffset({
        x: side * (4 + Math.random() * 6),
        y: (Math.random() - 0.4) * 5,
      });
    }, 2600);

    // clins d'œil occasionnels
    const winkTimer = setInterval(() => {
      setWink(Math.random() < 0.5 ? "left" : "right");
      setTimeout(() => setWink(null), 300);
    }, 14000 + Math.random() * 6000);

    return () => {
      clearTimeout(wake);
      clearTimeout(blinkTimer);
      clearTimeout(typeStart);
      clearInterval(saccade);
      clearInterval(winkTimer);
      window.removeEventListener("mousemove", onMove);
    };
  }, [reduced]);

  const doneTyping = typed >= TAGLINE.length;

  // clin d'œil complice juste après que la bouche a fini d'écrire le tagline
  useEffect(() => {
    if (reduced || !doneTyping) return;
    const t = setTimeout(() => {
      setWink("right");
      setTimeout(() => setWink(null), 320);
    }, 500);
    return () => clearTimeout(t);
  }, [doneTyping, reduced]);

  const closed = !awake || blinking;

  return (
    <div className="mx-auto w-fit" style={{ perspective: "1100px" }}>
      <div
        ref={faceRef}
        className="rounded-[2rem] border border-border bg-gradient-to-b from-[#1d2435] to-[#131927] px-8 py-9 sm:px-12 sm:py-11 shadow-[0_30px_70px_-18px_rgba(0,0,0,0.85)]"
        style={{ transform: "rotateX(7deg)" }}
        role="img"
        aria-label="La façade d'EscapeBox : deux yeux animés au regard malicieux et une bouche à encre électronique qui affiche « Ouvrez l'œil. »"
      >
        <div className="flex items-center justify-center gap-8 sm:gap-12">
          <Eye offset={offset} closed={closed || wink === "left"} />
          <Eye offset={offset} closed={closed || wink === "right"} />
        </div>
        <div
          className="mx-auto mt-8 flex h-16 w-56 sm:h-20 sm:w-72 items-center justify-center rounded-md bg-eink-paper shadow-[inset_0_2px_10px_rgba(0,0,0,0.35)]"
          style={{ transform: "rotateX(4deg)" }}
          aria-hidden="true"
        >
          <span className="font-mono text-sm sm:text-base font-bold tracking-wide text-eink-ink">
            {TAGLINE.slice(0, typed)}
            {!doneTyping && !reduced && (
              <span className="eink-caret inline-block w-[0.6em] -mb-0.5 h-[1em] translate-y-[0.15em] bg-eink-ink" />
            )}
          </span>
        </div>
      </div>
    </div>
  );
}
