import type { Metadata } from "next";
import { Atkinson_Hyperlegible_Next, Martian_Mono, Young_Serif } from "next/font/google";
import "./globals.css";

// « Ardoise » : titres en Young Serif (chaleureux, un peu excentrique), texte en
// Atkinson Hyperlegible Next (lisible à tout âge), étiquettes et écran-bouche en
// Martian Mono.
const display = Young_Serif({ variable: "--font-young-serif", subsets: ["latin"], weight: "400" });
const body = Atkinson_Hyperlegible_Next({ variable: "--font-atkinson", subsets: ["latin"] });
const mono = Martian_Mono({ variable: "--font-martian", subsets: ["latin"] });

export const metadata: Metadata = {
  title: { default: "EscapeBox — Une énigme avec un visage", template: "%s — EscapeBox" },
  description:
    "Une boîte d'escape game à poser au milieu de la table. Elle vous regarde, vous écoute et garde ses secrets. Rejoignez la liste d'attente.",
};

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return (
    <html
      lang="fr"
      className={`${display.variable} ${body.variable} ${mono.variable} h-full antialiased`}
    >
      <body className="flex min-h-full flex-col">{children}</body>
    </html>
  );
}
