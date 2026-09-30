import type { Metadata } from "next";
import { CubeNet } from "@/components/site/cube-net";
import { FACES } from "@/components/site/faces";
import { MiniNet } from "@/components/site/mini-net";
import { WaitlistForm } from "@/components/waitlist-form";

export const metadata: Metadata = {
  title: "La box",
  description: "Un cube de bois et d'ardoise, six faces, un visage. Ce qu'on peut en dire sans gâcher la surprise.",
};

export default function LaBoxPage() {
  return (
    <main className="flex-1">
      <section className="mx-auto w-full max-w-6xl px-6 pb-16 pt-14 md:pt-20">
        <p className="font-mono text-[0.7rem] tracking-[0.24em] text-brass">LA BOX</p>
        <h1 className="mt-5 max-w-3xl font-display text-[clamp(2.4rem,5.6vw,4.2rem)] leading-[1.04] tracking-[-0.015em] text-balance">
          Un cube de bois et d&apos;ardoise. Et quelqu&apos;un dedans.
        </h1>
        <p className="mt-6 max-w-xl text-lg leading-relaxed text-foreground/80">
          Elle tient dans les mains, trône sur une étagère entre deux parties, et ne fonctionne jamais deux fois de la même
          façon. Voici ce qu&apos;on peut en dire sans gâcher la surprise.
        </p>
        <div className="mt-12">
          <CubeNet linked={false} />
        </div>
      </section>

      <div className="border-t border-foreground/10">
        {FACES.map((f) => (
          <section
            key={f.id}
            id={f.id}
            className="scroll-mt-6 border-b border-foreground/10 odd:bg-card/60"
            aria-labelledby={`${f.id}-titre`}
          >
            <div className="mx-auto grid w-full max-w-6xl gap-6 px-6 py-14 md:grid-cols-[12rem_1fr] md:gap-12">
              <div className="flex items-center gap-3 md:flex-col md:items-start">
                <MiniNet active={f.id} />
                <p className="font-mono text-[0.7rem] tracking-[0.24em] text-brass">{f.name}</p>
              </div>
              <div className="max-w-2xl">
                <h2 id={`${f.id}-titre`} className="font-display text-3xl leading-tight">
                  {f.title}
                </h2>
                <p className="mt-2 text-lg text-foreground/80">{f.teaser}</p>
                <p className="mt-4 leading-relaxed text-foreground/70">{f.more}</p>
              </div>
            </div>
          </section>
        ))}
      </div>

      <section className="mx-auto w-full max-w-6xl px-6 py-20">
        <div className="grid gap-8 md:grid-cols-2 md:items-center">
          <div>
            <h2 className="font-display text-[clamp(1.8rem,3.6vw,2.6rem)] leading-tight">Le reste, elle vous le montrera.</h2>
            <p className="mt-4 max-w-md leading-relaxed text-foreground/75">
              Batterie rechargeable, Wi-Fi pour recevoir les histoires, et plus rien de connecté pendant la partie.
            </p>
          </div>
          <WaitlistForm id="email-box" />
        </div>
      </section>
    </main>
  );
}
