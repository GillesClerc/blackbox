// Mention à compléter, volontairement visible (site non indexé, en préparation).
export function Todo({ children }: { children: React.ReactNode }) {
  return <span className="rounded-sm bg-brass/15 px-1.5 py-0.5 font-mono text-[0.8em] text-brass">[à compléter : {children}]</span>;
}
