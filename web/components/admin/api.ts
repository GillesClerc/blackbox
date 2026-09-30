// Appels aux routes /api/admin/* depuis le navigateur (session par cookies).
export type ApiResult<T = unknown> = { ok: true; data: T } | { ok: false; errors: string[] };

export async function adminApi<T = unknown>(
  url: string,
  init: { method: string; json?: unknown; form?: FormData }
): Promise<ApiResult<T>> {
  try {
    const res = await fetch(url, {
      method: init.method,
      headers: init.json !== undefined ? { "Content-Type": "application/json" } : undefined,
      body: init.form ?? (init.json !== undefined ? JSON.stringify(init.json) : undefined),
    });
    const body = await res.json().catch(() => ({}));
    if (!res.ok) return { ok: false, errors: body.errors ?? [body.error ?? `erreur ${res.status}`] };
    return { ok: true, data: body as T };
  } catch {
    return { ok: false, errors: ["le serveur ne répond pas"] };
  }
}
