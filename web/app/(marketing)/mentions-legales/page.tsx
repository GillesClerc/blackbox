import type { Metadata } from "next";
import { Todo } from "@/components/site/todo";

export const metadata: Metadata = { title: "Mentions légales" };

const dt = "font-mono text-[0.68rem] tracking-[0.22em] text-muted-foreground";

export default function LegalPage() {
  return (
    <main className="flex-1">
      <article className="mx-auto w-full max-w-3xl px-6 pb-24 pt-14 md:pt-20">
        <p className="font-mono text-[0.7rem] tracking-[0.24em] text-brass">MENTIONS LÉGALES</p>
        <h1 className="mt-5 font-display text-[clamp(2.2rem,5vw,3.4rem)] leading-tight">Qui est derrière la box.</h1>
        <dl className="mt-10 flex flex-col gap-6">
          <div>
            <dt className={dt}>ÉDITEUR</dt>
            <dd className="mt-1">
              <Todo>nom ou raison sociale, adresse</Todo>
            </dd>
          </div>
          <div>
            <dt className={dt}>CONTACT</dt>
            <dd className="mt-1">
              <Todo>adresse e-mail</Todo>
            </dd>
          </div>
          <div>
            <dt className={dt}>HÉBERGEMENT</dt>
            <dd className="mt-1">
              <Todo>prestataire et pays</Todo>
            </dd>
          </div>
          <div>
            <dt className={dt}>PROPRIÉTÉ</dt>
            <dd className="mt-1 leading-relaxed text-foreground/80">
              Textes, illustrations et code du site : tous droits réservés. EscapeBox est un nom de travail, susceptible de
              changer.
            </dd>
          </div>
        </dl>
      </article>
    </main>
  );
}
