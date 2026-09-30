# Migrations SQL (Supabase self-hosted)

Pas de CLI Supabase : en prod, chaque fichier se colle dans **Studio → SQL Editor** et se lance en
entier. En staging : `python3 tools/staging_sql.py -f <fichier>` (clé dans `web/.env.staging.local`),
puis `python3 tools/staging_sql.py --gen-types` pour régénérer `web/lib/database.types.ts`.
Test automatique : `web/supabase/migrations.test.ts` rejoue toutes les migrations dans PGlite. On ne modifie **jamais** le schéma à la main sans écrire la migration correspondante.
Relevé de contrôle : `../snapshot-schema.sql` (une cellule JSON).

| Fichier | Contenu | Staging | Production |
|---|---|---|---|
| `0001_baseline.sql` | schéma de la prod au 2026-09-30, à l'identique | à passer (base vierge) | **ne pas passer** (déjà dans cet état) |
| `0002_security.sql` | RLS `box_challenges`, droits clients retirés, `devices`/`profiles` en lecture seule, `waitlist` insertion seule, `handle_new_user` durcie, NOT NULL | à passer | à passer |
| `0003_inventory.sql` | E1 : `scenario_versions`, `licenses` (droit au compte), fiche catalogue (`status`, thème, ambiance…), `profiles.role`, reprise des droits par box en licences ; additive | à passer | à passer **avant** de déployer le code E1 |

## Journal des passages

| Fichier | Staging | Production |
|---|---|---|
| 0001_baseline | ✅ 2026-09-30 (base vierge `tsupabase.agill.es`) | (état d'origine) |
| 0002_security | ✅ 2026-09-30 (relevé identique à la prod) | ✅ 2026-09-30 (vérifié : relevé du schéma + test clé publique → `permission denied`) |
| 0003_inventory | ✅ 2026-09-30 (`tools/staging_sql.py`) | — |

Après chaque passage : relancer `snapshot-schema.sql` et me renvoyer la cellule pour contrôle
(RLS, policies et droits attendus).
