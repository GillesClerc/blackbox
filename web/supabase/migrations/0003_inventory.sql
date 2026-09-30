-- 0003_inventory — modèle d'inventaire (roadmap web E1, validé par Gilles le 2026-09-30).
--   scenarios          : fiche catalogue + statut + version publiée (current_version_id)
--   scenario_versions  : un package par version (manifest, emplacement des fichiers)
--   licenses           : droit d'un COMPTE sur une histoire (plus d'une box)
--   device_scenarios   : devient le suivi d'installation (plus un droit)
--   profiles.role      : user / admin
--
-- Purement ADDITIVE : les anciennes colonnes (scenarios.active, .version, .package_path)
-- restent, pour que le code actuel continue de marcher entre ce SQL et le déploiement du
-- nouveau code. Elles seront supprimées par une migration ultérieure, une fois E1 stable.
-- Ordre : STAGING d'abord (SQL puis déploiement tbox), puis PRODUCTION.

begin;

-- ── profiles.role ─────────────────────────────────────────────────────────────
alter table public.profiles
  add column role text not null default 'user'
    check (role in ('user', 'admin'));

-- ── scenarios : fiche catalogue ───────────────────────────────────────────────
alter table public.scenarios
  add column status text not null default 'draft'
    check (status in ('draft', 'published', 'archived')),
  add column summary text,                 -- pitch court (cartes du catalogue)
  add column theme text,                   -- ex. « pirates », « science »
  add column ambiance text,                -- ex. « mystérieuse et drôle »
  add column min_players integer check (min_players >= 1),
  add column max_players integer check (max_players >= 1),
  add column min_age integer check (min_age >= 0),
  add column language text not null default 'fr',
  add column cover_path text,              -- image de couverture (Storage, E2)
  add column updated_at timestamptz not null default now(),
  add constraint scenarios_players_range
    check (min_players is null or max_players is null or max_players >= min_players);

-- ── scenario_versions : un package par version ────────────────────────────────
create table public.scenario_versions (
  id uuid primary key default gen_random_uuid(),
  scenario_id uuid not null references public.scenarios(id) on delete cascade,
  version integer not null check (version >= 1),
  status text not null default 'draft'
    check (status in ('draft', 'published', 'retired')),
  manifest jsonb,                          -- manifest.json du package (sha256 par fichier)
  total_bytes bigint,
  -- Emplacement des fichiers : 'repo' = web/scenario-packages/<storage_path>/ (existant),
  -- 'bucket' = Supabase Storage (étape E2).
  storage text not null default 'repo' check (storage in ('repo', 'bucket')),
  storage_path text not null,
  notes text,                              -- notes de version
  created_by uuid references public.profiles(id) on delete set null,
  created_at timestamptz not null default now(),
  published_at timestamptz,
  unique (scenario_id, version)
);
create index scenario_versions_scenario_idx on public.scenario_versions (scenario_id);
alter table public.scenario_versions enable row level security;
revoke all on public.scenario_versions from anon, authenticated;   -- serveur uniquement

-- Version publiée d'une histoire : une seule source de vérité.
alter table public.scenarios
  add column current_version_id uuid
    references public.scenario_versions(id) on delete set null;

-- Catalogue public : les histoires publiées (remplace le critère active = true).
drop policy if exists "public read" on public.scenarios;
create policy "scenarios: lecture des histoires publiees" on public.scenarios
  for select using (status = 'published');

-- ── licenses : droit d'un compte sur une histoire ─────────────────────────────
create table public.licenses (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references public.profiles(id) on delete cascade,
  scenario_id uuid not null references public.scenarios(id) on delete restrict,
  source text not null check (source in ('purchase', 'gift', 'admin', 'code')),
  stripe_payment_intent text unique,       -- rempli à l'étape E5
  granted_by uuid references public.profiles(id) on delete set null,
  note text,
  created_at timestamptz not null default now(),
  revoked_at timestamptz,
  revoke_reason text
);
-- Au plus une licence active par compte et par histoire.
create unique index licenses_one_active_idx
  on public.licenses (user_id, scenario_id) where revoked_at is null;
create index licenses_scenario_idx on public.licenses (scenario_id);
alter table public.licenses enable row level security;
revoke all on public.licenses from anon, authenticated;
grant select on public.licenses to authenticated;
create policy "licenses: lecture de ses licences" on public.licenses
  for select to authenticated using ((select auth.uid()) = user_id);

-- ── device_scenarios : suivi d'installation ───────────────────────────────────
alter table public.device_scenarios
  add column installed_version integer,
  add column last_seen_at timestamptz;

-- ── Reprise des données ───────────────────────────────────────────────────────
-- Chaque histoire packagée devient sa première version (fichiers dans le dépôt).
insert into public.scenario_versions
  (scenario_id, version, status, storage, storage_path, published_at, notes)
select id, version,
       case when active then 'published' else 'retired' end,
       'repo', slug,
       case when active then now() end,
       'Reprise de l''existant (migration 0003)'
from public.scenarios
where package_path is not null;

update public.scenarios s
   set current_version_id = v.id
  from public.scenario_versions v
 where v.scenario_id = s.id and v.version = s.version;

update public.scenarios
   set status = case
                  when not active then 'archived'
                  when current_version_id is not null then 'published'
                  else 'draft'
                end;

-- Chaque droit par box (d'une box rattachée à un compte) devient une licence du compte.
insert into public.licenses (user_id, scenario_id, source, note)
select distinct d.owner_id, ds.scenario_id, 'admin',
       'Reprise des droits par box (migration 0003)'
from public.device_scenarios ds
join public.devices d on d.id = ds.device_id
where d.owner_id is not null
on conflict (user_id, scenario_id) where revoked_at is null do nothing;

commit;

-- Après coup (hors transaction), se donner le rôle admin :
--   update public.profiles set role = 'admin' where email = '<ton e-mail>';
