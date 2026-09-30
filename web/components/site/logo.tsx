import Link from "next/link";

// Logotype provisoire (marque non choisie) : deux yeux-écrans et le nom.
export function Logo() {
  return (
    <Link
      href="/"
      className="group inline-flex items-center gap-2.5 rounded-sm focus-visible:outline-2 focus-visible:outline-offset-4 focus-visible:outline-brass"
    >
      <span aria-hidden="true" className="flex gap-[3px] rounded-[3px] bg-walnut p-[3px]">
        <span className="grid size-3.5 place-items-center rounded-full bg-screen">
          <span className="size-1.5 rounded-full bg-iris transition-transform group-hover:translate-x-[2px]" />
        </span>
        <span className="grid size-3.5 place-items-center rounded-full bg-screen">
          <span className="size-1.5 rounded-full bg-iris transition-transform group-hover:translate-x-[2px]" />
        </span>
      </span>
      <span className="font-display text-lg leading-none tracking-tight">EscapeBox</span>
    </Link>
  );
}
