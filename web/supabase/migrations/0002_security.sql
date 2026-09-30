-- 0002_security — corrections issues du relevé du 2026-09-30.
-- À passer en STAGING puis en PRODUCTION (SQL Editor de Studio), en une fois.
-- Principe : la clé anon est publique (elle est dans le navigateur) ; tout ce qui n'est
-- pas explicitement autorisé ici doit être refusé aux rôles anon et authenticated. Le
-- serveur (routes /api/box/*) utilise service_role, qui n'est pas concerné.

begin;

-- 1. box_challenges : RLS était DÉSACTIVÉE avec tous les droits pour anon → n'importe qui,
--    avec la clé publique, pouvait lire, consommer ou effacer les nonces (déni de service
--    de l'authentification des box). Table réservée au serveur.
alter table public.box_challenges enable row level security;
revoke all on public.box_challenges from anon, authenticated;

-- 2. Tables réservées au serveur : aucun accès client.
revoke all on public.device_scenarios from anon, authenticated;
revoke all on public.firmware_releases from anon, authenticated;

-- 3. devices : la policy « own devices » (FOR ALL, sans WITH CHECK) laissait un utilisateur
--    connecté INSÉRER une box avec n'importe quel box_uid directement par l'API Supabase,
--    en contournant la preuve de possession BLE de /api/box/register (option B), ou changer
--    le box_uid d'une de ses box. Désormais : lecture seule de ses propres box.
drop policy if exists "own devices" on public.devices;
create policy "devices: lecture par le proprietaire" on public.devices
  for select to authenticated using ((select auth.uid()) = owner_id);
revoke insert, update, delete, truncate, references, trigger
  on public.devices from anon, authenticated;
create index if not exists devices_owner_id_idx on public.devices (owner_id);

-- 4. profiles : lecture de son propre profil ; la création passe par le trigger.
drop policy if exists "own profile" on public.profiles;
create policy "profiles: lecture de son profil" on public.profiles
  for select to authenticated using ((select auth.uid()) = id);
revoke insert, update, delete, truncate, references, trigger
  on public.profiles from anon, authenticated;

-- 5. scenarios : lecture publique des histoires actives, aucune écriture client
--    (déjà bloquée par RLS, retirée aussi des droits par défense en profondeur).
revoke insert, update, delete, truncate, references, trigger
  on public.scenarios from anon, authenticated;

-- 6. waitlist : insertion seule, aussi pour un visiteur connecté (la policy ne couvrait
--    que anon : un utilisateur connecté voyait son inscription échouer). Aucune lecture.
drop policy if exists "waitlist insert only" on public.waitlist;
create policy "waitlist: insertion seule" on public.waitlist
  for insert to anon, authenticated with check (true);
revoke select, update, delete, truncate, references, trigger
  on public.waitlist from anon, authenticated;

-- 7. handle_new_user : SECURITY DEFINER sans search_path figé (détournable par un objet
--    homonyme dans un schéma du search_path). Noms qualifiés + search_path vide.
create or replace function public.handle_new_user()
returns trigger language plpgsql security definer set search_path = '' as $function$
begin
  insert into public.profiles (id, email) values (new.id, new.email);
  return new;
end;
$function$;

-- 8. Colonnes booléennes jamais nulles. Le code filtre `used = false` et `active = true` :
--    une valeur NULL se comporte aujourd'hui comme « utilisé » / « inactif » — on garde ce
--    sens (NULL → true pour used, NULL → false pour active), sans rien réactiver.
update public.box_challenges set used = true where used is null;
alter table public.box_challenges alter column used set not null;
update public.scenarios set active = false where active is null;
alter table public.scenarios alter column active set not null;
update public.firmware_releases set active = false where active is null;
alter table public.firmware_releases alter column active set not null;

-- 9. firmware_releases.sha256 obligatoire (l'OTA F6 vérifie l'image) — seulement si aucune
--    ligne ne l'a vide ; sinon, avertissement et on laisse la colonne telle quelle.
do $$
begin
  if exists (select 1 from public.firmware_releases where sha256 is null) then
    raise warning 'firmware_releases : des lignes sans sha256, NOT NULL non appliqué';
  else
    alter table public.firmware_releases alter column sha256 set not null;
  end if;
end $$;

commit;
