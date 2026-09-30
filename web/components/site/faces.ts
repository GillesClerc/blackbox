// Les six faces de la box (docs/escapebox-fsd.md §2.2.2c). Le texte suggère sans
// tout dire (direction « Ouvrez l'œil ») : aucune fiche technique ici.
export type Face = {
  id: string;
  name: string;
  title: string;
  teaser: string;
  more: string;
};

export const FACES: Face[] = [
  {
    id: "dessus",
    name: "DESSUS",
    title: "La voix",
    teaser: "Elle raconte, chuchote, s'impatiente.",
    more: "C'est par là qu'elle vous parle. Et elle y garde un secret que seul un téléphone posé au bon moment saura lire.",
  },
  {
    id: "cote-1",
    name: "CÔTÉ 1",
    title: "Le tableau de bord",
    teaser: "Des curseurs, des molettes, des interrupteurs.",
    more: "Tout se règle, tout se combine. Certains réglages ouvrent des portes, d'autres vous font tourner en rond — à vous de voir lesquels.",
  },
  {
    id: "devant",
    name: "DEVANT",
    title: "Le visage",
    teaser: "Elle vous regarde, et pas seulement.",
    more: "Deux yeux qui suivent, une bouche qui écrit, une lueur qui change d'humeur. Elle voit la lumière, sent votre souffle et sa chaleur.",
  },
  {
    id: "cote-2",
    name: "CÔTÉ 2",
    title: "Le toucher",
    teaser: "Effleurez. Ou ne touchez pas du tout.",
    more: "Des zones sensibles, cachées dans la paroi. Et une qui sent votre main avant même qu'elle ne la touche.",
  },
  {
    id: "cote-3",
    name: "CÔTÉ 3",
    title: "La face mystère",
    teaser: "Il suffit d'y poser la bonne chose.",
    more: "Laquelle ? C'est toute la question. Elle, elle le sait tout de suite.",
  },
  {
    id: "dessous",
    name: "DESSOUS",
    title: "Le halo",
    teaser: "Elle s'éclaire par en dessous.",
    more: "Elle écoute ce qui se dit autour de la table, et sait toujours dans quel sens on la tient. Retournez-la, inclinez-la : elle a son avis.",
  },
];
