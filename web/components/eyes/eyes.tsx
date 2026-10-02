"use client";

import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from "react";
import { EyesEngine, loadEyeAssets, SCREEN, type Mood } from "./engine";

// Le visage vivant : <Eyes> anime une paire d'yeux (moteur du firmware) et
// <Eye side={0|1}> en est un écran rond, placé où l'on veut dans la page.
// Le pointeur sert de regard à suivre et de lampe : le pointer sur un œil
// contracte la pupille, comme le capteur de lumière de la vraie box.

type Api = {
  register: (side: 0 | 1, c: HTMLCanvasElement | null) => void;
  poke: (side: 0 | 1) => void;
  blink: () => void;
  setMood: (m: Mood | null) => void;
  say: (text: string) => void;
  speech: { text: string; id: number } | null;
};

const Ctx = createContext<Api | null>(null);
export const useEyes = () => useContext(Ctx);

export type EyesProps = {
  children: ReactNode;
  mood?: Mood;
  /** Couleur des paupières (noir = écran éteint, comme sur la box). */
  lidColor?: string;
  /** Répliques quand on clique un œil. */
  pokeLines?: string[];
  /** Portée du regard : distance (en fraction de l'écran) où il atteint le bord de l'œil. */
  reach?: number;
};

// Rendu ×2 (256 px par œil) : paupières lisses à la taille où le site les montre.
const RENDER_SCALE = 2;

const prefersReducedMotion = () =>
  typeof window !== "undefined" && window.matchMedia("(prefers-reduced-motion: reduce)").matches;

export function Eyes({ children, mood = "neutre", lidColor = "#000000", pokeLines = [], reach = 0.38 }: EyesProps) {
  const canvases = useRef<(HTMLCanvasElement | null)[]>([null, null]);
  const engine = useRef<EyesEngine | null>(null);
  const moodRef = useRef<Mood>(mood);
  const override = useRef<{ mood: Mood; until: number } | null>(null);
  const [speech, setSpeech] = useState<Api["speech"]>(null);
  const pokeIdx = useRef(0);

  useEffect(() => {
    moodRef.current = mood;
  }, [mood]);
  useEffect(() => {
    engine.current?.setLidColor(lidColor);
  }, [lidColor]);

  const say = useCallback((text: string) => setSpeech({ text, id: Date.now() }), []);

  const flash = useCallback((m: Mood, ms: number) => {
    override.current = { mood: m, until: performance.now() + ms };
  }, []);

  const api = useMemo<Api>(
    () => ({
      register: (side, c) => {
        canvases.current[side] = c;
      },
      poke: (side) => {
        const e = engine.current;
        if (!e) return;
        e.blink(performance.now(), side, 2.2);
        flash(side === 0 ? "mefiante" : "contente", 1400);
        if (pokeLines.length) {
          say(pokeLines[pokeIdx.current % pokeLines.length]);
          pokeIdx.current++;
        }
      },
      blink: () => engine.current?.blink(performance.now()),
      setMood: (m) => {
        override.current = m ? { mood: m, until: Infinity } : null;
      },
      say,
      speech,
    }),
    [flash, pokeLines, say, speech]
  );

  useEffect(() => {
    let raf = 0;
    let alive = true;
    let visible = true;
    const reduced = prefersReducedMotion();
    let buffers: ImageData[] = [];
    let views: Uint32Array[] = [];
    let lastMove = performance.now();
    let pointer: { x: number; y: number } | null = null;
    let asleep = false;

    const centerOf = () => {
      const rects = canvases.current.filter(Boolean).map((c) => c!.getBoundingClientRect());
      if (!rects.length) return null;
      const cx = rects.reduce((s, r) => s + r.left + r.width / 2, 0) / rects.length;
      const cy = rects.reduce((s, r) => s + r.top + r.height / 2, 0) / rects.length;
      return { cx, cy, rects };
    };

    const draw = (now: number) => {
      const e = engine.current;
      if (!e) return;
      canvases.current.forEach((c, side) => {
        if (!c) return;
        e.render(side as 0 | 1, now, views[side]);
        c.getContext("2d")!.putImageData(buffers[side], 0, 0);
      });
    };

    const tick = (now: number) => {
      if (!alive) return;
      const e = engine.current!;
      // Regard et lampe
      const c = centerOf();
      if (pointer && c && now - lastMove < 3500) {
        const span = Math.max(window.innerWidth, window.innerHeight) * reach;
        e.setGaze({ x: (pointer.x - c.cx) / span, y: (pointer.y - c.cy) / span });
        const near = Math.min(
          ...c.rects.map((r) => Math.hypot(pointer!.x - (r.left + r.width / 2), pointer!.y - (r.top + r.height / 2)) / (r.width * 0.9))
        );
        e.setLight(1 - Math.min(1, near));
      } else {
        e.setGaze(null);
        e.setLight(0);
      }
      // Humeur : réaction ponctuelle > endormissement > humeur de la page
      const o = override.current;
      if (o && now > o.until) override.current = null;
      if (!asleep && now - lastMove > 22_000) asleep = true;
      e.setMood(override.current?.mood ?? (asleep ? "endormie" : moodRef.current));
      e.step(now);
      draw(now);
      raf = visible ? requestAnimationFrame(tick) : 0;
    };

    const onMove = (ev: PointerEvent) => {
      pointer = { x: ev.clientX, y: ev.clientY };
      lastMove = performance.now();
      if (asleep) {
        asleep = false;
        flash("surprise", 700);
        engine.current?.blink(lastMove);
      }
    };
    const onScroll = () => {
      lastMove = performance.now();
      asleep = false;
    };

    const io = new IntersectionObserver((entries) => {
      visible = entries.some((en) => en.isIntersecting) && document.visibilityState === "visible";
      if (visible && !raf && engine.current && !reduced) raf = requestAnimationFrame(tick);
    });
    const onVis = () => {
      visible = document.visibilityState === "visible";
      if (visible && !raf && engine.current && !reduced) raf = requestAnimationFrame(tick);
    };

    loadEyeAssets().then((assets) => {
      if (!alive) return;
      engine.current = new EyesEngine(assets, lidColor, RENDER_SCALE);
      buffers = [0, 1].map(() => new ImageData(engine.current!.size, engine.current!.size));
      views = buffers.map((b) => new Uint32Array(b.data.buffer));
      if (reduced) {
        // Pas d'animation : un regard posé, droit devant.
        engine.current.autoBlink = false;
        engine.current.setGaze({ x: 0, y: 0 });
        for (let i = 0; i < 40; i++) engine.current.step(performance.now());
        draw(performance.now());
        return;
      }
      window.addEventListener("pointermove", onMove, { passive: true });
      window.addEventListener("pointerdown", onMove, { passive: true });
      window.addEventListener("scroll", onScroll, { passive: true });
      document.addEventListener("visibilitychange", onVis);
      canvases.current.forEach((cv) => cv && io.observe(cv));
      raf = requestAnimationFrame(tick);
    });

    return () => {
      alive = false;
      cancelAnimationFrame(raf);
      io.disconnect();
      window.removeEventListener("pointermove", onMove);
      window.removeEventListener("pointerdown", onMove);
      window.removeEventListener("scroll", onScroll);
      document.removeEventListener("visibilitychange", onVis);
    };
    // lidColor initial seulement : les changements passent par setLidColor
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [flash, reach]);

  return <Ctx.Provider value={api}>{children}</Ctx.Provider>;
}

/** Un écran rond. Le parent fixe la taille ; le canvas remplit et se découpe en disque. */
export function Eye({ side, className = "", label }: { side: 0 | 1; className?: string; label?: string }) {
  const api = useEyes();
  const ref = useCallback((c: HTMLCanvasElement | null) => api?.register(side, c), [api, side]);
  return (
    <button
      type="button"
      onClick={() => api?.poke(side)}
      aria-label={label ?? (side === 0 ? "Œil gauche de la box" : "Œil droit de la box")}
      className={`relative block aspect-square cursor-pointer overflow-hidden rounded-full bg-black focus-visible:outline-3 focus-visible:outline-offset-4 ${className}`}
    >
      {/* Comme sur la box : l'image de l'œil ne remplit pas tout l'écran rond. */}
      <canvas
        ref={ref}
        width={SCREEN * RENDER_SCALE}
        height={SCREEN * RENDER_SCALE}
        className="absolute inset-[8%] h-[84%] w-[84%]"
      />
      {/* reflet de la vitre */}
      <span
        aria-hidden="true"
        className="pointer-events-none absolute inset-0 rounded-full"
        style={{
          background:
            "radial-gradient(circle at 32% 26%, rgba(255,255,255,0.22), rgba(255,255,255,0) 22%), radial-gradient(circle at 50% 50%, rgba(0,0,0,0) 62%, rgba(0,0,0,0.55) 100%)",
        }}
      />
    </button>
  );
}

/**
 * L'écran-bouche : le texte arrive mot à mot (la vraie box parle ainsi), puis
 * s'efface. Sans réplique en cours, il fait défiler `lines`.
 */
export function Mouth({
  lines,
  className = "",
  textClassName = "",
  wordMs = 170,
  holdMs = 2600,
}: {
  lines: string[];
  className?: string;
  textClassName?: string;
  wordMs?: number;
  holdMs?: number;
}) {
  const api = useEyes();
  const [shown, setShown] = useState(lines[0] ?? "");
  // Au repos, la première réplique est déjà affichée en entier.
  const [count, setCount] = useState(() => (lines[0] ?? "").split(" ").length);
  const idx = useRef(0);
  const speechId = useRef<number | null>(null);
  const reduced = useRef(false);

  useEffect(() => {
    reduced.current = prefersReducedMotion();
    if (reduced.current) setCount(Infinity);
  }, []);

  // Une réplique (clic sur un œil) interrompt le défilement.
  useEffect(() => {
    if (api?.speech && api.speech.id !== speechId.current) {
      speechId.current = api.speech.id;
      setShown(api.speech.text);
      setCount(reduced.current ? Infinity : 0);
    }
  }, [api?.speech]);

  useEffect(() => {
    if (reduced.current) return;
    const words = shown.split(" ").length;
    const t =
      count < words
        ? setTimeout(() => setCount((c) => c + 1), count === 0 ? 450 : wordMs)
        : setTimeout(() => {
            idx.current = (idx.current + 1) % Math.max(1, lines.length);
            setShown(lines[idx.current] ?? "");
            setCount(0);
          }, holdMs);
    return () => clearTimeout(t);
  }, [count, shown, lines, wordMs, holdMs]);

  const words = shown.split(" ");
  return (
    <div className={className}>
      <span className="sr-only">{shown}</span>
      <p aria-hidden="true" className={textClassName}>
        {words.map((w, i) => (
          <span key={i} style={{ opacity: i < count ? 1 : 0 }}>
            {w}
            {i < words.length - 1 ? " " : ""}
          </span>
        ))}
      </p>
    </div>
  );
}
