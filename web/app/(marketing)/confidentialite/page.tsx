import type { Metadata } from "next";
import { Todo } from "@/components/site/todo";

export const metadata: Metadata = { title: "Confidentialité" };

const h2 = "mt-10 font-display text-2xl";
const p = "mt-3 leading-relaxed text-foreground/80";

export default function PrivacyPage() {
  return (
    <main className="flex-1">
      <article className="mx-auto w-full max-w-3xl px-6 pb-24 pt-14 md:pt-20">
        <p className="font-mono text-[0.7rem] tracking-[0.24em] text-brass">CONFIDENTIALITÉ</p>
        <h1 className="mt-5 font-display text-[clamp(2.2rem,5vw,3.4rem)] leading-tight">Vos données, en clair.</h1>
        <p className={p}>
          Cette page décrit les données traitées par EscapeBox, conformément à la loi fédérale sur la protection des données
          (nLPD) et, pour les personnes dans l&apos;Union européenne, au RGPD.
        </p>

        <h2 className={h2}>Ce que nous enregistrons</h2>
        <ul className="mt-3 list-disc space-y-2 pl-5 leading-relaxed text-foreground/80">
          <li>
            <strong>Liste d&apos;attente</strong> : votre adresse e-mail et la date d&apos;inscription, pour vous prévenir de la
            sortie.
          </li>
          <li>
            <strong>Compte</strong> : votre adresse e-mail et un mot de passe (stocké sous forme d&apos;empreinte irréversible, jamais en
            clair).
          </li>
          <li>
            <strong>Box reliées à votre compte</strong> : l&apos;identifiant technique de chaque box, son nom, la version de
            son logiciel et la date de sa dernière connexion.
          </li>
          <li>
            <strong>Histoires</strong> : les histoires associées à votre compte.
          </li>
        </ul>
        <p className={p}>
          La box n&apos;envoie ni son, ni image, ni ce qui se passe pendant la partie. Ses capteurs servent au jeu, sur place.
        </p>

        <h2 className={h2}>Ce que nous ne faisons pas</h2>
        <p className={p}>
          Pas de publicité, pas de mesure d&apos;audience, pas de revente. Les seuls cookies sont ceux qui maintiennent votre
          session quand vous êtes connecté.
        </p>

        <h2 className={h2}>Où sont les données</h2>
        <p className={p}>
          Sur des serveurs gérés par l&apos;éditeur du site. Lieu d&apos;hébergement : <Todo>pays et prestataire</Todo>.
        </p>

        <h2 className={h2}>Vos droits</h2>
        <p className={p}>
          Vous pouvez demander à consulter, corriger ou supprimer vos données, ou vous désinscrire de la liste d&apos;attente,
          en écrivant à <Todo>adresse de contact</Todo>.
        </p>
        <p className="mt-10 text-sm text-muted-foreground">Dernière mise à jour : 1er octobre 2026.</p>
      </article>
    </main>
  );
}
