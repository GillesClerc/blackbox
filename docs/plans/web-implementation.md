# Web Platform — Guide d'implémentation

*Guide opérationnel uniquement. Spec fonctionnelle dans `docs/escapebox-fsd.md`.*

---

## Phase 0 — Infrastructure Coolify (manuel, ~1h)

### 0.1 Supabase self-hosted
1. Coolify → New Resource → Supabase (template one-click)
2. Sous-domaine : `db.box.agill.es`
3. Récupérer : `SUPABASE_URL`, `ANON_KEY`, `SERVICE_ROLE_KEY`
4. Studio : `https://db.box.agill.es/dashboard`

### 0.2 Service Next.js
1. Coolify → New Resource → Application → GitHub
2. Repo `GillesClerc/blackbox`, branch `master`, **base directory `web/`**
3. Build pack : Nixpacks
4. Domaine `box.agill.es` (dev/FFF) — prod : `escapebox.ch` (Phase 3)
5. SSL auto

Variables d'env :
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

## Phase 1 — Init projet

```bash
cd /workspaces/blackbox
npx create-next-app@latest web \
  --typescript --tailwind --eslint --app \
  --no-src-dir --import-alias "@/*" \
  --no-git
cd web
npx shadcn@latest init   # style: default, dark, CSS variables: yes
```
> `--no-git` : monorepo avec `.git` à la racine — pas de repo imbriqué. `--eslint` doit être explicite sinon le prompt peut bloquer en CI.

```bash
npm install @supabase/supabase-js @supabase/ssr \
            stripe @stripe/stripe-js \
            jose js-yaml zod lucide-react
```
> `jose` signe/vérifie les JWT box sans dépendance native Node.js (compatible Edge Runtime).

---

## Arborescence

```
web/
  app/
    layout.tsx
    page.tsx                                    # Landing (/)
    (marketing)/
      activate/[token]/page.tsx                 # Activation QR code physique
      v/[scenario]/[session]/page.tsx           # Résultat / leaderboard (public, sans auth)
    (auth)/
      login/page.tsx
      register/page.tsx
    (app)/
      layout.tsx                                # Shell authentifié
      shop/page.tsx
      shop/[slug]/page.tsx
      checkout/success/page.tsx
      checkout/cancel/page.tsx
      library/page.tsx
      devices/page.tsx
      devices/add/page.tsx
      scores/page.tsx
      account/page.tsx
    (studio)/
      page.tsx                                  # Guard plan Pro+ (Phase 3)
      new/page.tsx
      [id]/edit/page.tsx
      [id]/simulate/page.tsx
    api/
      box/
        challenge/route.ts                      # GET  — nonce HMAC (usage unique, TTL 60s)
        auth/route.ts                           # POST — HMAC response → JWT 2h
        register/route.ts                       # POST — enregistrement box (BLE provisioning)
        sync/route.ts                           # GET  — scénarios + firmware
        session/route.ts                        # POST — upload score
      checkout/route.ts
      webhooks/
        stripe/route.ts
  components/
    ui/                                         # shadcn/ui (auto-généré)
    ScenarioCard.tsx
    ScenarioPlayer.tsx
    DeviceCard.tsx
  lib/
    supabase/
      client.ts
      server.ts
    stripe.ts
    scenario-engine.ts                          # Port JS du moteur firmware
    box-auth.ts
  middleware.ts
  public/
    scenarios/
      capitaine-verdier.yaml                    # Copié depuis firmware/scenarios/
  .env.local
  next.config.ts
```

---

## Snippets clés

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
  const { pathname } = request.nextUrl

  if (!user && (pathname.startsWith('/app') || pathname.startsWith('/studio'))) {
    const url = request.nextUrl.clone()
    url.pathname = '/login'
    return NextResponse.redirect(url)
  }

  // TODO Phase 3 : vérifier plan Pro+ pour /studio/*

  return supabaseResponse
}

export const config = {
  matcher: ['/((?!_next/static|_next/image|favicon.ico|api/).*)'],
}
```

**`lib/box-auth.ts`**
```typescript
import { SignJWT, jwtVerify } from 'jose'
import { createHmac, timingSafeEqual, randomUUID } from 'crypto'

const secret = new TextEncoder().encode(process.env.BOX_SHARED_SECRET!)

export function verifyBoxHmac(boxUid: string, challenge: string, hmac: string): boolean {
  const expected = createHmac('sha256', process.env.BOX_SHARED_SECRET!)
    .update(`${boxUid}:${challenge}`)
    .digest('hex')
  const a = Buffer.from(expected)
  const b = Buffer.from(hmac)
  if (a.length !== b.length) return false
  return timingSafeEqual(a, b)
}

export async function signBoxJwt(payload: { boxUid: string; deviceId: string; ownerId: string }) {
  return new SignJWT({ device_id: payload.deviceId, owner_id: payload.ownerId })
    .setProtectedHeader({ alg: 'HS256' })
    .setSubject(payload.boxUid)
    .setJti(randomUUID())
    .setIssuedAt()
    .setExpirationTime('2h')
    .sign(secret)
}

export async function verifyBoxJwt(token: string) {
  const { payload } = await jwtVerify(token, secret)
  return payload as { sub: string; device_id: string; owner_id: string; jti: string }
}
```

**`lib/stripe.ts`**
```typescript
import Stripe from 'stripe'
export const stripe = new Stripe(process.env.STRIPE_SECRET_KEY!, { apiVersion: '2024-04-10' })
```

---

## Schema SQL — Supabase Studio → SQL Editor

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
  box_uid text unique not null,       -- eFuse ESP32 (format ESP32S3-XXXX-XXXX)
  name text default 'EscapeBox',
  firmware_version text,
  last_sync_at timestamptz,
  created_at timestamptz default now()
);
alter table public.devices enable row level security;
create policy "own devices" on public.devices for all using (auth.uid() = owner_id);

-- Scenarios
create table public.scenarios (
  id uuid primary key default gen_random_uuid(),
  slug text unique not null,
  title text not null,
  description text,
  difficulty int check (difficulty between 1 and 5),
  duration_min int,
  min_players int default 1,
  max_players int default 6,
  language text default 'fr',
  hardware_required text[],
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
  stripe_payment_intent text,
  purchased_at timestamptz default now(),
  unique(owner_id, scenario_id)
);
alter table public.licenses enable row level security;
create policy "own licenses" on public.licenses for all using (auth.uid() = owner_id);

-- Device <-> Scenario installé
create table public.device_scenarios (
  device_id uuid references public.devices(id) on delete cascade,
  scenario_id uuid references public.scenarios(id),
  installed_at timestamptz default now(),
  primary key (device_id, scenario_id)
);
alter table public.device_scenarios enable row level security;
create policy "own device scenarios" on public.device_scenarios for all using (
  exists (select 1 from public.devices d where d.id = device_id and d.owner_id = auth.uid())
);

-- Game sessions
create table public.game_sessions (
  id uuid primary key default gen_random_uuid(),
  device_id uuid references public.devices(id),
  scenario_id uuid references public.scenarios(id),
  score integer,
  duration_seconds integer,
  hints_used integer default 0,
  completed boolean default false,
  session_token text unique default encode(gen_random_bytes(16), 'hex'),
  played_at timestamptz default now()
);
-- Insertion via service_role (firmware) — pas de RLS user

-- Seed
insert into public.scenarios (slug, title, description, price_chf, difficulty, duration_min, yaml_path)
values ('capitaine-verdier', 'Le Trésor du Capitaine Verdier',
        'Un mystère maritime en 3 énigmes.', 19.00, 3, 60,
        '/scenarios/capitaine-verdier.yaml');
```

---

## Vérification par phase

```bash
# Phase 0
curl https://db.box.agill.es/rest/v1/          # → 200
curl https://box.agill.es                       # → page Next.js

# Phase 1 : register/login OK · tables dans Supabase Studio · git push → Coolify build vert

# Phase 3 — Auth box
curl "https://box.agill.es/api/box/challenge?box_uid=ESP32S3-TEST-0001"
# → { "challenge": "abc123...", "expires_in": 60 }

curl -X POST https://box.agill.es/api/box/auth \
  -H 'Content-Type: application/json' \
  -d '{"box_uid":"ESP32S3-TEST-0001","challenge":"abc123...","challenge_response":"hmac..."}'
# → { "token": "eyJ...", "server_time": 1234567890 }

# Stripe webhook local
stripe listen --forward-to localhost:3000/api/webhooks/stripe
# Achat test (card 4242 4242 4242 4242) → license créée dans Supabase
```

---

## Fichiers critiques

| Fichier | Rôle |
|---------|------|
| `web/middleware.ts` | Refresh session Supabase + protection `/app/*` et `/studio/*` |
| `web/lib/supabase/server.ts` | Client SSR (async cookies) |
| `web/lib/box-auth.ts` | HMAC challenge + JWT 2h (jose) |
| `web/app/api/box/challenge/route.ts` | Nonce anti-replay (usage unique, TTL 60s) |
| `web/app/api/box/auth/route.ts` | Auth firmware → JWT |
| `web/app/api/box/register/route.ts` | Enregistrement box (BLE provisioning) |
| `web/app/api/webhooks/stripe/route.ts` | Création licence post-paiement |
| `web/app/(marketing)/v/[scenario]/[session]/page.tsx` | Page résultat QR (publique) |
| `web/lib/scenario-engine.ts` | Port JS du moteur C firmware |
