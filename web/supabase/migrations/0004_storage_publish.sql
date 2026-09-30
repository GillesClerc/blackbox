-- 0004_storage_publish — back-office E2 (validé par Gilles le 2026-09-30).
--   - bucket privé « scenario-packages » : fichiers des versions publiées depuis /admin,
--     rangés sous <slug>/v<version>/… ; aucune policy → accès service_role uniquement
--     (la box passe toujours par /api/box/pkg, jamais directement par Storage) ;
--   - publish_scenario_version() : publication ATOMIQUE d'une version (la version
--     devient courante, l'ancienne courante est retirée), appelée côté serveur.
-- Ordre : STAGING puis PRODUCTION, avant de déployer le code E2.

begin;

insert into storage.buckets (id, name, public, file_size_limit)
values ('scenario-packages', 'scenario-packages', false, 33554432)  -- 32 Mo par fichier
on conflict (id) do nothing;

create or replace function public.publish_scenario_version(p_version_id uuid)
returns void
language plpgsql
security definer
set search_path = ''
as $function$
declare
  v_scenario uuid;
  v_previous uuid;
begin
  select scenario_id into v_scenario
    from public.scenario_versions where id = p_version_id for update;
  if v_scenario is null then
    raise exception 'version inconnue : %', p_version_id;
  end if;

  select current_version_id into v_previous
    from public.scenarios where id = v_scenario for update;

  if v_previous is not null and v_previous <> p_version_id then
    update public.scenario_versions set status = 'retired' where id = v_previous;
  end if;

  update public.scenario_versions
     set status = 'published', published_at = now()
   where id = p_version_id;

  update public.scenarios
     set current_version_id = p_version_id, updated_at = now()
   where id = v_scenario;
end;
$function$;

-- Réservée au serveur (service_role) : jamais appelable par un client.
revoke all on function public.publish_scenario_version(uuid) from public, anon, authenticated;

commit;
