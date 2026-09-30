import type { Metadata } from "next";
import Link from "next/link";
import { notFound } from "next/navigation";
import { Difficulty } from "@/components/site/story-card";
import { WaitlistForm } from "@/components/waitlist-form";
import { getStory, LANGUAGES, playersLabel } from "@/lib/catalog";

export const dynamic = "force-dynamic";

type Props = { params: Promise<{ slug: string }> };

export async function generateMetadata({ params }: Props): Promise<Metadata> {
  const story = await getStory((await params).slug);
  return story ? { title: story.title, description: story.summary ?? undefined } : { title: "Histoire introuvable" };
}

export default async function StoryPage({ params }: Props) {
  const story = await getStory((await params).slug);
  if (!story) notFound();

  const facts: [string, React.ReactNode][] = (
    [
      ["JOUEURS", playersLabel(story)],
      ["DURÉE", story.duration_min ? `${story.duration_min} min` : null],
      ["ÂGE", story.min_age != null ? `dès ${story.min_age} ans` : null],
      ["DIFFICULTÉ", story.difficulty ? <Difficulty level={story.difficulty} /> : null],
      ["LANGUE", LANGUAGES[story.language] ?? story.language],
    ] as [string, React.ReactNode][]
  ).filter(([, v]) => v);

  return (
    <main className="flex-1">
      <section className="bg-walnut text-[#efe7da]">
        <div className="mx-auto w-full max-w-6xl px-6 pb-14 pt-10">
          <Link href="/histoires" className="font-mono text-[0.7rem] tracking-[0.22em] text-[#efe7da]/70 hover:text-white">
            ← LES HISTOIRES
          </Link>
          {story.theme && <p className="mt-8 font-mono text-[0.7rem] tracking-[0.24em] text-iris">{story.theme.toUpperCase()}</p>}
          <h1 className="mt-3 max-w-3xl font-display text-[clamp(2.4rem,6vw,4.4rem)] leading-[1.03] tracking-[-0.015em] text-balance">
            {story.title}
          </h1>
          {story.summary && <p className="mt-5 max-w-xl text-lg leading-relaxed text-[#efe7da]/85">{story.summary}</p>}
        </div>
      </section>

      <section className="mx-auto grid w-full max-w-6xl gap-12 px-6 py-16 md:grid-cols-[1fr_18rem]">
        <div className="max-w-2xl">
          {story.ambiance && (
            <p className="font-display text-2xl leading-snug text-foreground/90">Ambiance : {story.ambiance}.</p>
          )}
          {story.description ? (
            <div className="mt-6 space-y-4 leading-relaxed text-foreground/80">
              {story.description.split(/\n{2,}/).map((para) => (
                <p key={para.slice(0, 32)}>{para}</p>
              ))}
            </div>
          ) : (
            <p className="mt-6 text-foreground/70">Elle préfère que vous découvriez la suite en jouant.</p>
          )}
        </div>
        <aside className="h-fit rounded-lg border border-foreground/15 bg-card p-5">
          <dl className="flex flex-col gap-4">
            {facts.map(([k, v]) => (
              <div key={k}>
                <dt className="font-mono text-[0.66rem] tracking-[0.22em] text-muted-foreground">{k}</dt>
                <dd className="mt-1">{v}</dd>
              </div>
            ))}
          </dl>
          <div className="mt-6 border-t border-foreground/10 pt-5">
            <p className="text-sm text-foreground/75">Pas encore en vente. Soyez prévenu à la sortie :</p>
            <div className="mt-3">
              <WaitlistForm id="email-story" stacked />
            </div>
          </div>
        </aside>
      </section>
    </main>
  );
}
