import Link from "next/link";
import { Logo } from "./logo";

const NAV = [
  { href: "/la-box", label: "La box" },
  { href: "/histoires", label: "Les histoires" },
  { href: "/faq", label: "Questions" },
];

const navLink =
  "rounded-sm px-2 py-1 text-sm text-foreground/75 transition-colors hover:text-foreground focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-brass";

export function SiteHeader({ signedIn }: { signedIn: boolean }) {
  return (
    <header className="border-b border-foreground/10">
      <div className="mx-auto flex w-full max-w-6xl flex-wrap items-center justify-between gap-x-6 gap-y-3 px-6 py-4">
        <Logo />
        <nav aria-label="Navigation principale" className="flex flex-wrap items-center gap-x-4 gap-y-1">
          {NAV.map((n) => (
            <Link key={n.href} href={n.href} className={navLink}>
              {n.label}
            </Link>
          ))}
          <Link
            href={signedIn ? "/devices" : "/login"}
            className="ml-1 rounded-md border border-foreground/20 px-3 py-1.5 text-sm transition-colors hover:border-foreground/50 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-brass"
          >
            {signedIn ? "Mon espace" : "Se connecter"}
          </Link>
        </nav>
      </div>
    </header>
  );
}
