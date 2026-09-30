import Link from "next/link";
import { FACES } from "./faces";

// Patron déplié du cube : la vraie disposition des six faces autour de la façade.
//        [dessus]
// [côté 1][devant][côté 2][côté 3]
//        [dessous]
const PLACE: Record<string, string> = {
  dessus: "lg:col-start-2 lg:row-start-1",
  "cote-1": "lg:col-start-1 lg:row-start-2",
  devant: "lg:col-start-2 lg:row-start-2",
  "cote-2": "lg:col-start-3 lg:row-start-2",
  "cote-3": "lg:col-start-4 lg:row-start-2",
  dessous: "lg:col-start-2 lg:row-start-3",
};

export function CubeNet({ linked = true }: { linked?: boolean }) {
  return (
    <ol className="grid gap-px overflow-hidden rounded-lg border border-foreground/15 bg-foreground/15 sm:grid-cols-2 lg:grid-cols-4 lg:grid-rows-3 lg:border-0 lg:bg-transparent lg:gap-0">
      {FACES.map((f) => {
        const front = f.id === "devant";
        const tile = (
          <div
            className={`flex h-full min-h-40 flex-col justify-between p-5 transition-colors ${
              front ? "bg-foreground text-background" : "bg-card group-hover:bg-popover"
            }`}
          >
            <p className={`font-mono text-[0.68rem] tracking-[0.22em] ${front ? "text-iris" : "text-brass"}`}>
              {f.name}
            </p>
            <div className="mt-6">
              <p className="font-display text-xl leading-tight">{f.title}</p>
              <p className={`mt-2 text-sm leading-snug ${front ? "text-background/75" : "text-muted-foreground"}`}>
                {f.teaser}
              </p>
            </div>
          </div>
        );
        return (
          <li key={f.id} className={`group lg:border lg:border-foreground/15 lg:-ml-px lg:-mt-px ${PLACE[f.id]}`}>
            {linked ? (
              <Link
                href={`/la-box#${f.id}`}
                className="block h-full focus-visible:outline-2 focus-visible:-outline-offset-2 focus-visible:outline-brass"
              >
                {tile}
              </Link>
            ) : (
              tile
            )}
          </li>
        );
      })}
    </ol>
  );
}
