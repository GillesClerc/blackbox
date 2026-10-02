"use client";

import { Eye, Eyes, Mouth } from "@/components/eyes/eyes";
import { MOUTH_LINES, POKE_LINES } from "../content";

// Les deux O de « OUVREZ L'ŒIL » sont les yeux : l'anneau crème dessine la
// lettre, l'écran rond en est le creux. Le titre entier devient un visage.
function EyeO({ side }: { side: 0 | 1 }) {
  return (
    <span
      className="relative inline-block size-[0.8em] rounded-full border-[0.11em] border-[#fff0dc] bg-black align-baseline"
      style={{ marginBottom: "-0.02em" }}
    >
      <Eye side={side} className="absolute! inset-0 w-full" />
    </span>
  );
}

export function AfficheHeadline() {
  return (
    <Eyes pokeLines={POKE_LINES} reach={0.5}>
      <h1 className="font-(family-name:--font-c-display) font-extrabold uppercase leading-[0.86] tracking-[-0.035em] text-[#fff0dc]">
        <span className="sr-only">Ouvrez l&apos;œil</span>
        <span aria-hidden="true" className="block text-[clamp(3.2rem,16.5vw,9rem)] md:text-[9.9vw]">
          <span className="whitespace-nowrap">
            <EyeO side={0} />
            UVREZ
          </span>{" "}
          <br className="md:hidden" />
          <span className="whitespace-nowrap">
            L&apos;
            <EyeO side={1} />
            EIL
          </span>
        </span>
      </h1>
      <Mouth
        lines={MOUTH_LINES}
        className="mt-8 flex h-14 w-full max-w-md items-center rounded-full bg-[#1d1714] px-6 md:ml-[1vw]"
        textClassName="text-lg font-semibold text-[#fff0dc]"
      />
    </Eyes>
  );
}
