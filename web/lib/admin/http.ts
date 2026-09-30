import type { Result } from "./inventory";

export const UUID_RE = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

export async function jsonBody(request: Request): Promise<unknown | Response> {
  try {
    return await request.json();
  } catch {
    return Response.json({ errors: ["JSON invalide"] }, { status: 400 });
  }
}

export function reply<T>(r: Result<T>, okStatus = 200): Response {
  return r.ok
    ? Response.json(r.data ?? { ok: true }, { status: okStatus })
    : Response.json({ errors: r.errors }, { status: r.status });
}

export const badId = () => Response.json({ errors: ["identifiant invalide"] }, { status: 400 });
