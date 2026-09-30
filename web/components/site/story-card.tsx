import Link from "next/link";
import { playersLabel, type Story } from "@/lib/catalog";

// Difficulté sur 5, en petites faces de cube (pleine = difficulté atteinte).
export function Difficulty({ level }: { level: number | null }) {
  if (!level) return null;
  return (
    <span className="inline-flex items-center gap-1.5" aria-label={`Difficulté ${level} sur 5`}>
      <span className="flex gap-[3px]" aria-hidden="true">
        {[1, 2, 3, 4, 5].map((i) => (
          <span key={i} className={`size-2.5 rounded-[2px] ${i <= level ? "bg-brass" : "border border-foreground/25"}`} />
        ))}
      </span>
    </span>
  );
}

export function StoryMeta({ story }: { story: Story }) {
  const bits = [
    playersLabel(story),
    story.duration_min ? `${story.duration_min} min` : null,
    story.min_age != null ? `dès ${story.min_age} ans` : null,
  ].filter(Boolean);
  return (
    <p className="flex flex-wrap items-center gap-x-3 gap-y-1 font-mono text-[0.7rem] tracking-wide text-muted-foreground">
      {bits.map((b) => (
        <span key={b as string}>{b}</span>
      ))}
      <Difficulty level={story.difficulty} />
    </p>
  );
}

// Carte « couvercle de boîte de jeu » : thème en bandeau, titre, pitch, repères.
export function StoryCard({ story }: { story: Story }) {
  return (
    <Link
      href={`/histoires/${story.slug}`}
      className="group flex h-full flex-col overflow-hidden rounded-lg border border-foreground/15 bg-card transition-[transform,box-shadow] hover:-translate-y-0.5 hover:shadow-[0_14px_30px_-18px_rgba(29,35,39,0.45)] focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-brass motion-reduce:transition-none motion-reduce:hover:translate-y-0"
    >
      <p className="bg-walnut px-5 py-2 font-mono text-[0.68rem] tracking-[0.22em] text-[#efe7da]">
        {(story.theme ?? "histoire").toUpperCase()}
      </p>
      <div className="flex flex-1 flex-col p-5">
        <h3 className="font-display text-2xl leading-tight group-hover:text-brass">{story.title}</h3>
        {story.summary && <p className="mt-3 text-[0.95rem] leading-relaxed text-foreground/80">{story.summary}</p>}
        <div className="mt-auto pt-6">
          <StoryMeta story={story} />
        </div>
      </div>
    </Link>
  );
}
