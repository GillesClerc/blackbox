export function Errors({ errors }: { errors: string[] }) {
  if (!errors.length) return null;
  return (
    <ul role="alert" className="mt-3 flex flex-col gap-1 rounded-lg border border-destructive/40 bg-destructive/10 p-3 text-sm text-destructive">
      {errors.map((e) => (
        <li key={e}>{e}</li>
      ))}
    </ul>
  );
}
