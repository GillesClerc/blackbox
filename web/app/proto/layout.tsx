import type { Metadata } from "next";

// Pistes de design (roadmap E3, deuxième tour) : pages d'essai, jamais indexées.
export const metadata: Metadata = {
  title: "Pistes de design",
  robots: { index: false, follow: false },
};

export default function ProtoLayout({ children }: { children: React.ReactNode }) {
  return <div className="overflow-x-clip">{children}</div>;
}
