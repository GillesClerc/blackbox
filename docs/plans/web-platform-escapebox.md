# Plan — Web Platform EscapeBox

## Contexte

Construire la plateforme web en parallèle du hardware.
Stack : Next.js 14 App Router · Tailwind · shadcn/ui · Supabase self-hosted (Coolify) · Stripe test mode · Email auth uniquement.
Déploiement : `box.agill.es` via Coolify (GitHub → build auto, base dir `web/`).
Monorepo : `web/` dans ce repo git à côté de `firmware/`.

Source : `docs/plans/web-platform-escapebox.md`

---

## Phase 0 — Infrastructure Coolify (manuel, ~1h)

À faire dans l'interface Coolify avant d'écrire le code.

### 0.1 Supabase self-hosted
1. Coolify → New Resource → Supabase (template one-click)
2. Sous-domaine : `db.box.agill.es`
3. Récupérer : `SUPABASE_URL`, `ANON_KEY`, `SERVICE_ROLE_KEY`
4. Studio : `https://db.box.agill.es/dashboard`

### 0.2 Service Next.js
1. Coolify → New Resource → Application → GitHub
2. Repo `GillesClerc/blackbox`, branch `master`, **base directory `web/`**
3. Build pack : Nixpacks
4. Domaine `box.agill.es`, SSL auto

Variables d'env à remplir dans Coolify :
```env
NEXT_PUBLIC_SUPABASE_URL=https://db.box.agill.es
NEXT_PUBLIC_SUPABASE_ANON_KEY=eyJ...
SUPABASE_SERVICE_ROLE_KEY=eyJ...
NEXT_PUBLIC_STRIPE_PUBLISHABLE_KEY=pk_test_...
STRIPE_SECRET_KEY=sk_test_...
STRIPE_WEBHOOK_SECRET=whsec_...
BOX_SHARED_SECRET=<openssl rand -hex 32>
NEXT_PUBLIC_APP_URL=https://box.agill.es
```

### 0.3 Stripe test mode
1. stripe.com → Test mode → Créer produit "Capitaine Verdier" prix fixe CHF
2. Récupérer `pk_test_` / `sk_test_`
3. Webhook → `https://box.agill.es/api/webhooks/stripe`

---

## Phase 1 — Foundation (Jours 1-2)

### 1.1 Init projet
```bash
cd /workspaces/blackbox
npx create-next-app@latest web \
  --typescript --tailwind --eslint --app \
  --no-src-dir --import-alias "@/*" \
  --no-git
cd web
npx shadcn@latest init   # style: default, dark, CSS variables: yes
```
> `--no-git` est intentionnel : le monorepo a déjà un `.git` à la racine, on ne veut pas de repo imbriqué. `--eslint` doit être explicite sinon le prompt peut bloquer en CI/non-interactif.

### 1.2 Dépendances
```bash
npm install @supabase/supabase-js @supabase/ssr \
            stripe @stripe/stripe-js \
            jose js-yaml zod lucide-react
```
> `jose` signe/vérifie les JWT box sans dépendance native Node.js (compatible Edge Runtime).

### 1.3 Arborescence complète

```
web/
  app/
    layout.tsx                    # Root layout — dark theme, Inter font
    page.tsx                      # Landing (/)
    score/
      [token]/page.tsx            # Score public (QR → navigateur, SANS auth)
    (auth)/
      login/page.tsx
      register/page.tsx
    (app)/
      layout.tsx                  # Shell authentifié (sidebar/nav)
      shop/page.tsx               # Catalogue scénarios
      library/page.tsx            # Scénarios achetés
      devices/page.tsx            # Mes boxes
      account/page.tsx            # Compte + déconnexion
    studio/
      simulate/page.tsx           # Simulateur YAML
    api/
      box/
        auth/route.ts             # POST — HMAC challenge/response → JWT
        sync/route.ts             # GET  — liste scénarios autorisés
        session/route.ts          # POST — upload score + retourne score_url
      checkout/route.ts           # POST — crée Stripe checkout session
      webhooks/
        stripe/route.ts           # POST — Stripe events → création licence
  components/
    ui/                           # shadcn/ui (auto-généré)
    ScenarioCard.tsx
    ScenarioPlayer.tsx            # simulateur
    DeviceCard.tsx
  lib/
    supabase/
      client.ts                   # createBrowserClient
      server.ts                   # createServerClient (Server Components + Route Handlers)
    stripe.ts                     # instance Stripe singleton
    scenario-engine.ts            # port JS du moteur YAML firmware
    box-auth.ts                   # HMAC sign/verify + JWT box (jose)
  middleware.ts                   # Refresh session Supabase + protection routes
  public/
    scenarios/
      capitaine-verdier.yaml      # copié depuis firmware/scenarios/
  .env.local
  next.config.ts
```

### 1.4 Snippets clés

**`lib/supabase/server.ts`**
```typescript
import { createServerClient } from '@supabase/ssr'
import { cookies } from 'next/headers'

export async function createClient() {
  const cookieStore = await cookies()
  return createServerClient(
    process.env.NEXT_PUBLIC_SUPABASE_URL!,
    process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY!,
    {
      cookies: {
        getAll: () => cookieStore.getAll(),
        setAll: (cookiesToSet) => {
          try {
            cookiesToSet.forEach(({ name, value, options }) =>
              cookieStore.set(name, value, options)
            )
          } catch {
            // Server Component read-only — ignoré, middleware gère le refresh
          }
        },
      },
    }
  )
}
```

**`lib/supabase/client.ts`**
```typescript
import { createBrowserClient } from '@supabase/ssr'

export function createClient() {
  return createBrowserClient(
    process.env.NEXT_PUBLIC_SUPABASE_URL!,
    process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY!
  )
}
```

**`middleware.ts`**
```typescript
import { createServerClient } from '@supabase/ssr'
import { NextResponse, type NextRequest } from 'next/server'

export async function middleware(request: NextRequest) {
  let supabaseResponse = NextResponse.next({ request })

  const supabase = createServerClient(
    process.env.NEXT_PUBLIC_SUPABASE_URL!,
    process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY!,
    {
      cookies: {
        getAll: () => request.cookies.getAll(),
        setAll: (cookiesToSet) => {
          cookiesToSet.forEach(({ name, value }) => request.cookies.set(name, value))
          supabaseResponse = NextResponse.next({ request })
          cookiesToSet.forEach(({ name, value, options }) =>
            supabaseResponse.cookies.set(name, value, options)
          )
        },
      },
    }
  )

  const { data: { user } } = await supabase.auth.getUser()

  if (!user && request.nextUrl.pathname.startsWith('/app')) {
    const url = request.nextUrl.clone()
    url.pathname = '/login'
    return NextResponse.redirect(url)
  }

  return supabaseResponse
}

export const config = {
  matcher: ['/((?!_next/static|_next/image|favicon.ico|api/).*)'],
}
```

**`lib/box-auth.ts`**
```typescript
import { SignJWT, jwtVerify } from 'jose'
import { createHmac, timingSafeEqual } from 'crypto'

const secret = new TextEncoder().encode(process.env.BOX_SHARED_SECRET!)

export function verifyBoxHmac(deviceMac: string, timestamp: number, hmac: string): boolean {
  const expected = createHmac('sha256', process.env.BOX_SHARED_SECRET!)
    .update(`${deviceMac}:${timestamp}`)
    .digest('hex')
  const a = Buffer.from(expected)
  const b = Buffer.from(hmac)
  // timingSafeEqual throws if lengths differ — guard required
  if (a.length !== b.length) return false
  return timingSafeEqual(a, b)
}

export async function signBoxJwt(payload: { deviceMac: string; deviceId: string; ownerId: string }) {
  return new SignJWT({ device_id: payload.deviceId, owner_id: payload.ownerId })
    .setProtectedHeader({ alg: 'HS256' })
    .setSubject(payload.deviceMac)
    .setIssuedAt()
    .setExpirationTime('24h')
    .sign(secret)
}

export async function verifyBoxJwt(token: string) {
  const { payload } = await jwtVerify(token, secret)
  return payload as { sub: string; device_id: string; owner_id: string }
}
```

**`lib/stripe.ts`**
```typescript
import Stripe from 'stripe'
export const stripe = new Stripe(process.env.STRIPE_SECRET_KEY!, { apiVersion: '2024-04-10' })
```

### 1.5 Schema SQL — à coller dans Supabase Studio → SQL Editor

```sql
-- Waitlist
create table public.waitlist (
  id uuid primary key default gen_random_uuid(),
  email text unique not null,
  created_at timestamptz default now()
);

-- Profiles
create table public.profiles (
  id uuid references auth.users(id) on delete cascade primary key,
  email text,
  created_at timestamptz default now()
);
alter table public.profiles enable row level security;
create policy "own profile" on public.profiles for all using (auth.uid() = id);

create or replace function public.handle_new_user()
returns trigger language plpgsql security definer as $$
begin
  insert into public.profiles (id, email) values (new.id, new.email);
  return new;
end;
$$;
create trigger on_auth_user_created
  after insert on auth.users
  for each row execute function public.handle_new_user();

-- Devices
create table public.devices (
  id uuid primary key default gen_random_uuid(),
  owner_id uuid references public.profiles(id) on delete set null,
  device_mac text unique not null,
  name text default 'EscapeBox',
  created_at timestamptz default now()
);
alter table public.devices enable row level security;
create policy "own devices" on public.devices for all using (auth.uid() = owner_id);

-- Scenarios (lecture publique)
create table public.scenarios (
  id uuid primary key default gen_random_uuid(),
  slug text unique not null,
  title text not null,
  description text,
  price_chf numeric(10,2) not null default 0,
  stripe_price_id text,
  cover_url text,
  yaml_path text,
  active boolean default true,
  created_at timestamptz default now()
);
alter table public.scenarios enable row level security;
create policy "public read" on public.scenarios for select using (active = true);

-- Licenses
create table public.licenses (
  id uuid primary key default gen_random_uuid(),
  owner_id uuid references public.profiles(id) on delete cascade,
  scenario_id uuid references public.scenarios(id),
  stripe_session_id text,
  created_at timestamptz default now(),
  unique(owner_id, scenario_id)
);
alter table public.licenses enable row level security;
create policy "own licenses" on public.licenses for all using (auth.uid() = owner_id);

-- Game sessions
create table public.game_sessions (
  id uuid primary key default gen_random_uuid(),
  device_id uuid references public.devices(id),
  scenario_id uuid references public.scenarios(id),
  score integer,
  duration_seconds integer,
  completed boolean default false,
  session_token text unique default encode(gen_random_bytes(16), 'hex'),
  created_at timestamptz default now()
);
alter table public.game_sessions enable row level security;
-- Insertion via service_role (firmware) — pas de policy user

-- Seed
insert into public.scenarios (slug, title, description, price_chf, yaml_path)
values ('capitaine-verdier', 'Capitaine Verdier',
        'Un mystère maritime en 3 énigmes.', 19.00,
        '/scenarios/capitaine-verdier.yaml');
```

---

## Phase 2 — Pages core (Jours 3-5)

### 2.1 Landing `/`
- Hero + CTA "Rejoindre la liste d'attente"
- Formulaire email → `INSERT INTO waitlist (email)` via Server Action
- 3 sections features (Énigmes, Audio, Scores)
- Section "Comment ça marche" : box → app → leaderboard
- Footer

### 2.2 Auth
- `/login` : email + password, lien register
- `/register` : email + password → email de confirmation Supabase
- Redirect post-login → `/app/library`

### 2.3 App shell
- Layout authentifié : nav top (mobile) ou sidebar (desktop)
- Items : Library, Shop, Devices, Account
- Middleware protège `/app/*` → redirect `/login`

### 2.4 Pages app
- `/app/shop` : liste `scenarios` actifs (1 card pour l'instant)
- `/app/library` : `licenses join scenarios` de l'user connecté
- `/app/devices` : liste + formulaire "Ajouter une box" (saisie MAC)
- `/app/account` : email, bouton déconnexion

---

## Phase 3 — API sync box + Stripe (Semaine 2)

### 3.1 `POST /api/box/auth`
```
Body: { device_mac, timestamp, hmac }
→ verifyBoxHmac() + anti-replay (|now - timestamp| < 300s)
→ lookup device dans Supabase (service_role)
→ signBoxJwt() → { token, device_id }
```

### 3.2 `GET /api/box/sync`
```
Header: Authorization: Bearer <token>
→ verifyBoxJwt()
→ SELECT scenarios via licenses du owner
→ { scenarios: [{ slug, title, yaml_url }] }
```

### 3.3 `POST /api/box/session`
```
Header: Authorization: Bearer <token>
Body: { scenario_slug, score, duration_seconds, completed }
→ INSERT game_sessions (service_role)
→ { session_token, score_url: "https://box.agill.es/score/<token>" }
```
> `score_url` est affiché en QR code sur le GC9A01 (face révélation).

### 3.4 `app/score/[token]/page.tsx` — page publique
- Récupère la session via `session_token` (Supabase service_role)
- Affiche : scénario, score, durée, date
- Accessible sans auth (depuis QR code smartphone)

### 3.5 `POST /api/checkout`
```
Body: { scenario_id }
→ stripe.checkout.sessions.create({ mode: 'payment', metadata: { owner_id, scenario_id } })
→ { url } (redirect vers Stripe hosted)
```

### 3.6 `POST /api/webhooks/stripe`
```
Event checkout.session.completed
→ vérifier signature Stripe (STRIPE_WEBHOOK_SECRET)
→ INSERT licenses (owner_id, scenario_id) via service_role
```

---

## Phase 4 — Simulateur YAML (Semaine 3)

### 4.1 `lib/scenario-engine.ts`
Port JS du moteur C (`firmware/components/scenario/`).

```typescript
interface ScenarioState {
  current_state: string
  variables: Record<string, string | number>
  hints_shown: number
  elapsed_seconds: number
}

class ScenarioEngine {
  constructor(yaml: ScenarioYAML) {}
  getState(): ScenarioState
  trigger(event: string, payload?: unknown): ScenarioAction[]
  injectEvent(type: 'nfc' | 'keypad' | 'sensor', value: string): void
  reset(): void
}
```

### 4.2 `app/studio/simulate/page.tsx`
- Zone gauche : upload YAML ou charger "Capitaine Verdier"
- Zone centrale : état courant (state, variables, chrono)
- Zone droite : injecter un événement (NFC uid, code keypad, IMU shake…)
- Log bas : historique transitions + actions
- Bouton Reset

---

## Ordre d'exécution

| Étape | Durée | Livrables |
|-------|-------|-----------|
| Phase 0 — Infra Coolify | 1h (manuel) | Supabase + Next.js live sur box.agill.es |
| Phase 1 — Foundation | 2 jours | Init, auth email, schema DB, premier deploy |
| Phase 2 — Pages core | 3 jours | Landing + waitlist, shop, library, devices |
| Phase 3 — API box + Stripe | 4 jours | HMAC auth, sync, session, score, paiement test |
| Phase 4 — Simulateur | 3 jours | Player YAML complet navigateur |

**Priorité absolue** : Phase 0 + Phase 1 + landing (email waitlist = validation marché FFF dès maintenant).

---

## Vérification par phase

- **Phase 0** : `curl https://db.box.agill.es/rest/v1/` → 200 · `https://box.agill.es` → page Next.js
- **Phase 1** : Register/login email OK · Tables visibles dans Supabase Studio · `git push` → Coolify build vert
- **Phase 2** : Landing affichée · Email waitlist inséré · Login → redirect `/app/library`
- **Phase 3** : `curl -X POST https://box.agill.es/api/box/auth -d '{"device_mac":"AA:BB:CC:DD:EE:FF","timestamp":...,"hmac":"..."}' -H 'Content-Type: application/json'` → `{ token: ... }` · Achat test Stripe → license créée dans Supabase
- **Phase 4** : Charger `capitaine-verdier.yaml` → déclencher events → voir transitions dans le log

---

## Fichiers critiques

| Fichier | Rôle |
|---------|------|
| `web/middleware.ts` | Refresh session + protection routes `/app/*` |
| `web/lib/supabase/server.ts` | Client SSR (async cookies) |
| `web/lib/box-auth.ts` | HMAC verify + JWT sign/verify (jose) |
| `web/app/api/box/auth/route.ts` | Auth firmware |
| `web/app/api/webhooks/stripe/route.ts` | Création licence post-paiement |
| `web/app/score/[token]/page.tsx` | Page score publique (QR code) |
| `web/lib/scenario-engine.ts` | Port JS moteur C |
| `web/public/scenarios/capitaine-verdier.yaml` | Copié depuis `firmware/scenarios/` |
