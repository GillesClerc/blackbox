-- 0001_baseline — schéma public de la PRODUCTION tel que relevé le 2026-09-30
-- (web/supabase/snapshot-schema.sql), reproduit à l'identique, défauts compris.
--
-- ⚠ NE PAS REJOUER EN PRODUCTION : la prod est déjà dans cet état (tables créées à la
-- main dans Studio en 2026-06/07). À passer uniquement sur une base vierge (staging).
-- Les corrections de sécurité sont dans 0002_security.sql.

-- ── profiles (1 ligne par utilisateur, créée par trigger) ─────────────────────
create table public.profiles (
  id uuid primary key references auth.users(id) on delete cascade,
  email text,
  created_at timestamptz default now()
);
alter table public.profiles enable row level security;
create policy "own profile" on public.profiles
  for all using (auth.uid() = id);

create or replace function public.handle_new_user()
returns trigger language plpgsql security definer as $function$
begin
  insert into public.profiles (id, email) values (new.id, new.email);
  return new;
end;
$function$;

create trigger on_auth_user_created
  after insert on auth.users
  for each row execute function public.handle_new_user();

-- Fonction présente en prod, utilisée par aucune table (aucune colonne updated_at).
create or replace function public.update_updated_at()
returns trigger language plpgsql as $function$
begin
  new.updated_at = now();
  return new;
end;
$function$;

-- ── devices (box rattachées à un compte) ──────────────────────────────────────
create table public.devices (
  id uuid primary key default gen_random_uuid(),
  owner_id uuid references public.profiles(id) on delete set null,
  box_uid text not null unique,
  name text default 'EscapeBox'::text,
  firmware_version text,
  last_sync_at timestamptz,
  created_at timestamptz default now()
);
alter table public.devices enable row level security;
create policy "own devices" on public.devices
  for all using (auth.uid() = owner_id);

-- ── scenarios (catalogue) ─────────────────────────────────────────────────────
create table public.scenarios (
  id uuid primary key default gen_random_uuid(),
  slug text not null unique,
  title text not null,
  description text,
  difficulty integer check (difficulty >= 1 and difficulty <= 5),
  duration_min integer,
  price_chf numeric not null default 0,
  package_path text,
  active boolean default true,
  created_at timestamptz default now(),
  version integer not null default 1
);
alter table public.scenarios enable row level security;
create policy "public read" on public.scenarios
  for select using (active = true);

-- ── device_scenarios (droit d'une box sur une histoire — seul droit aujourd'hui) ──
create table public.device_scenarios (
  device_id uuid not null references public.devices(id) on delete cascade,
  scenario_id uuid not null references public.scenarios(id),
  installed_at timestamptz default now(),
  primary key (device_id, scenario_id)
);
alter table public.device_scenarios enable row level security;

-- ── box_challenges (nonces à usage unique de l'API box) ───────────────────────
create table public.box_challenges (
  challenge text primary key,
  box_uid text not null,
  expires_at timestamptz not null,
  used boolean default false
);
-- (RLS désactivée en prod : corrigé par 0002)

-- ── firmware_releases (OTA) ───────────────────────────────────────────────────
create table public.firmware_releases (
  id uuid primary key default gen_random_uuid(),
  version text not null,
  channel text not null default 'stable'::text,
  url text not null,
  sha256 text,
  notes text,
  active boolean default true,
  created_at timestamptz default now(),
  unique (version, channel)
);
alter table public.firmware_releases enable row level security;

-- ── waitlist (landing) ────────────────────────────────────────────────────────
create table public.waitlist (
  id uuid primary key default gen_random_uuid(),
  email text not null unique,
  created_at timestamptz default now()
);
alter table public.waitlist enable row level security;
create policy "waitlist insert only" on public.waitlist
  for insert to anon with check (true);
