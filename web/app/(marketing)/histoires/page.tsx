import type { Metadata } from "next";
import { StoryCard } from "@/components/site/story-card";
import { WaitlistForm } from "@/components/waitlist-form";
import { listStories } from "@/lib/catalog";

export const metadata: Metadata = {
  title: "Les histoires",
  description: "Les histoires qui transforment la box : thème, ambiance, nombre de joueurs et durée.",
};
export const dynamic = "force-dynamic";

export default async function StoriesPage() {
  const stories = await listStories();
  return (
    <main className="flex-1">
      <section className="mx-auto w-full max-w-6xl px-6 pb-24 pt-14 md:pt-20">
        <p className="font-mono text-[0.7rem] tracking-[0.24em] text-brass">LES HISTOIRES</p>
        <h1 className="mt-5 max-w-3xl font-display text-[clamp(2.4rem,5.6vw,4.2rem)] leading-[1.04] tracking-[-0.015em] text-balance">
          Chaque histoire lui donne une autre personnalité.
        </h1>
        <p className="mt-6 max-w-xl text-lg leading-relaxed text-foreground/80">
          Une histoire s&apos;ajoute à votre compte et arrive sur votre box par le Wi-Fi. Ensuite, elle se joue hors ligne,
          autant de fois que vous voulez.
        </p>

        {stories.length > 0 ? (
          <div className="mt-14 grid gap-6 sm:grid-cols-2 lg:grid-cols-3">
            {stories.map((s) => (
              <StoryCard key={s.slug} story={s} />
            ))}
          </div>
        ) : (
          <div className="mt-14 max-w-lg rounded-lg border border-dashed border-foreground/25 p-6">
            <p className="font-display text-xl">Les premières histoires sont en écriture.</p>
            <p className="mt-2 text-foreground/70">Laissez votre adresse pour être prévenu de leur sortie.</p>
            <div className="mt-5">
              <WaitlistForm id="email-histoires" />
            </div>
          </div>
        )}
      </section>
    </main>
  );
}
