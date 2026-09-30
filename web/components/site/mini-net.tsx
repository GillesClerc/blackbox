// Petit patron du cube qui situe une face (repère visuel, décoratif).
const CELLS: [string, number, number][] = [
  ["dessus", 1, 0],
  ["cote-1", 0, 1],
  ["devant", 1, 1],
  ["cote-2", 2, 1],
  ["cote-3", 3, 1],
  ["dessous", 1, 2],
];

export function MiniNet({ active }: { active: string }) {
  return (
    <svg viewBox="0 0 49 37" className="h-9 w-12" aria-hidden="true">
      {CELLS.map(([id, x, y]) => (
        <rect
          key={id}
          x={x * 12 + 0.5}
          y={y * 12 + 0.5}
          width="11"
          height="11"
          rx="1.5"
          className={id === active ? "fill-brass" : "fill-none stroke-foreground/35"}
          strokeWidth="1"
        />
      ))}
    </svg>
  );
}
