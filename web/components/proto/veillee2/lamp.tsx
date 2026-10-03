"use client";

import { useEffect } from "react";
import { LAMP_EVENT } from "@/components/eyes/eyes";
import s from "./veillee2.module.css";

// La lampe de toute la page. Sur ordinateur, elle suit la souris. Sur mobile
// (pas de survol), c'est un faisceau posé au milieu de l'écran qui balaie
// doucement de gauche à droite : on fait défiler la page dessous pour révéler
// l'encre invisible ; un doigt posé l'attire un instant.
// Chaque calque [data-ink] reçoit la position de la lampe dans son propre repère.
export function Lamp() {
  useEffect(() => {
    const fine = window.matchMedia("(hover: hover) and (pointer: fine)").matches;
    const reduced = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    const root = document.documentElement;
    let x = -1000,
      y = -1000;
    let touch: { x: number; y: number; until: number } | null = null;
    let raf = 0;
    let dirty = true;

    const apply = () => {
      root.style.setProperty("--lamp-x", `${x}px`);
      root.style.setProperty("--lamp-y", `${y}px`);
      document.querySelectorAll<HTMLElement>("[data-ink]").forEach((el) => {
        const r = el.getBoundingClientRect();
        if (r.bottom < -300 || r.top > window.innerHeight + 300) return;
        el.style.setProperty("--lx", `${x - r.left}px`);
        el.style.setProperty("--ly", `${y - r.top}px`);
      });
      window.dispatchEvent(new CustomEvent(LAMP_EVENT, { detail: { x, y } }));
    };

    const tick = (t: number) => {
      if (!fine) {
        if (touch && t < touch.until) {
          x += (touch.x - x) * 0.25;
          y += (touch.y - y) * 0.25;
        } else {
          touch = null;
          const tx = window.innerWidth * (0.5 + (reduced ? 0 : Math.sin(t / 2400) * 0.3));
          const ty = window.innerHeight * 0.42;
          x = x < -500 ? tx : x + (tx - x) * 0.08;
          y = y < -500 ? ty : y + (ty - y) * 0.08;
        }
        dirty = true;
      }
      if (dirty) {
        apply();
        dirty = false;
      }
      raf = requestAnimationFrame(tick);
    };

    const onPointer = (e: PointerEvent) => {
      if (fine) {
        x = e.clientX;
        y = e.clientY;
        dirty = true;
      } else {
        touch = { x: e.clientX, y: e.clientY, until: performance.now() + 2500 };
      }
    };
    const onScroll = () => {
      dirty = true;
    };
    window.addEventListener("pointermove", onPointer, { passive: true });
    window.addEventListener("pointerdown", onPointer, { passive: true });
    window.addEventListener("scroll", onScroll, { passive: true });
    raf = requestAnimationFrame(tick);
    return () => {
      cancelAnimationFrame(raf);
      window.removeEventListener("pointermove", onPointer);
      window.removeEventListener("pointerdown", onPointer);
      window.removeEventListener("scroll", onScroll);
    };
  }, []);

  return <div className={s.glow} aria-hidden="true" />;
}

/** Notes à l'encre invisible : position en % du bloc parent (qui doit être relative). */
export function Ink({
  notes,
  className = "",
}: {
  notes: { t: string; x: string; y: string; r?: number; size?: string }[];
  className?: string;
}) {
  return (
    <div data-ink className={`${s.ink} ${className}`} aria-hidden="true">
      {notes.map((n) => (
        <span key={n.t} style={{ left: n.x, top: n.y, rotate: `${n.r ?? 0}deg`, fontSize: n.size ?? "1.4rem" }}>
          {n.t}
        </span>
      ))}
    </div>
  );
}
