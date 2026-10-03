"use client";

import { Eye, Eyes } from "@/components/eyes/eyes";
import { EYE_CATALOG, type EyeId } from "@/components/eyes/catalog.gen";

// Planche des personnages disponibles (formes d'œil Uncanny Eyes), pour choisir.
const HUES = [0, 150, 210];

export default function Yeux() {
  return (
    <main className="min-h-screen bg-[#1c1411] p-8 text-[#f4e7d3]" style={{ fontFamily: "system-ui, sans-serif" }}>
      <h1 className="text-2xl font-semibold">Personnages — formes d&apos;œil et teintes</h1>
      <div className="mt-8 grid gap-8 sm:grid-cols-2 lg:grid-cols-3">
        {(Object.keys(EYE_CATALOG) as EyeId[]).flatMap((id) =>
          HUES.map((hue) => (
            <div key={`${id}-${hue}`} className="rounded-xl bg-black/30 p-4">
              <p className="text-sm opacity-70">
                {id} · teinte {hue}°
              </p>
              <Eyes character={{ eye: id, hue }}>
                <div className="mt-3 flex gap-4">
                  <Eye side={0} className="w-28" />
                  <Eye side={1} className="w-28" />
                </div>
              </Eyes>
            </div>
          ))
        )}
      </div>
    </main>
  );
}
