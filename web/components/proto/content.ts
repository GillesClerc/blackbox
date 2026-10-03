// Contenu commun aux pistes : mêmes textes partout, seul le style change.
export { FACES } from "@/components/site/faces";

export const PISTES = [
  { slug: "veillee", letter: "A", name: "Veillée", box: "visage seul" },
  { slug: "recre", letter: "B", name: "Récré", box: "box illustrée" },
  { slug: "affiche", letter: "C", name: "Affiche", box: "pas de box" },
  { slug: "atelier", letter: "D", name: "Atelier", box: "box en 3D" },
  { slug: "veillee-v2", letter: "E", name: "Veillée v2", box: "visage + box en 3D" },
] as const;

export const MOUTH_LINES = [
  "Bonsoir.",
  "Je vous attendais.",
  "Approchez un peu…",
  "Vous ne trouverez jamais.",
];

export const POKE_LINES = ["Hé !", "Aïe.", "Ça chatouille.", "Encore ?", "Je vous ai à l'œil."];

export const STEPS = [
  {
    title: "Posez-la au milieu de la table",
    text: "Pas d'appli, pas de téléphone : la box est le jeu. Elle s'allume, ouvre les yeux et donne le ton.",
  },
  {
    title: "Choisissez une histoire",
    text: "Chaque histoire la transforme : une autre voix, d'autres lumières, d'autres énigmes cachées dans ses faces.",
  },
  {
    title: "Percez ses secrets",
    text: "Tournez-la, touchez-la, soufflez-lui dessus. Elle réagit à tout, et elle se souvient.",
  },
];

export const STORY = {
  title: "Le Mystère du Capitaine Verdier",
  year: "1957",
  pitch:
    "Le sous-marin Verdier disparaît en mer du Nord avec 42 hommes à bord. Aujourd'hui, sa balise de détresse vient de s'activer. Vous avez vingt minutes.",
  duration: "20 min",
  players: "2 à 4 joueurs",
};
