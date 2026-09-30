-- Relevé complet du schéma public, en UNE requête qui renvoie UN document JSON.
-- Usage : Supabase Studio → SQL Editor → coller, Run, copier la cellule « schema ».
-- Sert à écrire / vérifier les migrations de web/supabase/migrations/ (lecture seule).
select json_build_object(
  'columns', (
    select json_agg(json_build_object(
      'table', table_name, 'column', column_name, 'type', data_type,
      'udt', udt_name, 'nullable', is_nullable, 'default', column_default)
      order by table_name, ordinal_position)
    from information_schema.columns where table_schema = 'public'),
  'constraints', (
    select json_agg(json_build_object(
      'table', conrelid::regclass::text, 'name', conname,
      'def', pg_get_constraintdef(oid)) order by conrelid::regclass::text, conname)
    from pg_constraint where connamespace = 'public'::regnamespace),
  'rls', (
    select json_agg(json_build_object('table', relname, 'enabled', relrowsecurity,
      'forced', relforcerowsecurity) order by relname)
    from pg_class where relnamespace = 'public'::regnamespace and relkind = 'r'),
  'policies', (
    select json_agg(json_build_object(
      'table', tablename, 'name', policyname, 'cmd', cmd, 'permissive', permissive,
      'roles', roles, 'using', qual, 'check', with_check) order by tablename, policyname)
    from pg_policies where schemaname = 'public'),
  'indexes', (
    select json_agg(indexdef order by indexdef)
    from pg_indexes where schemaname = 'public'),
  'triggers', (
    select json_agg(json_build_object('table', tgrelid::regclass::text, 'name', tgname,
      'def', pg_get_triggerdef(oid)) order by tgname)
    from pg_trigger where not tgisinternal
      and tgrelid::regclass::text not like 'storage.%'
      and tgrelid::regclass::text not like 'auth.%'
      and tgrelid::regclass::text not like 'realtime.%'),
  'auth_triggers', (
    select json_agg(json_build_object('table', tgrelid::regclass::text, 'name', tgname,
      'def', pg_get_triggerdef(oid)) order by tgname)
    from pg_trigger t join pg_class c on c.oid = t.tgrelid
    where not tgisinternal and c.relnamespace = 'auth'::regnamespace
      and pg_get_triggerdef(t.oid) like '%public.%'),
  'functions', (
    select json_agg(json_build_object('name', p.proname,
      'def', pg_get_functiondef(p.oid)) order by p.proname)
    from pg_proc p where p.pronamespace = 'public'::regnamespace),
  'grants', (
    select json_agg(json_build_object('table', table_name, 'grantee', grantee,
      'privilege', privilege_type) order by table_name, grantee, privilege_type)
    from information_schema.role_table_grants
    where table_schema = 'public' and grantee in ('anon', 'authenticated')),
  'buckets', (
    select json_agg(json_build_object('id', id, 'public', public))
    from storage.buckets),
  'row_counts', (
    select json_object_agg(relname, n_live_tup)
    from pg_stat_user_tables where schemaname = 'public')
) as schema;
