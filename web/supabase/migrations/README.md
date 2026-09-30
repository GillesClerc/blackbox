# Migrations SQL (Supabase self-hosted)

Pas de CLI Supabase : chaque fichier se colle dans **Studio → SQL Editor** et se lance en
entier. On ne modifie **jamais** le schéma à la main sans écrire la migration correspondante.
Relevé de contrôle : `../snapshot-schema.sql` (une cellule JSON).

| Fichier | Contenu | Staging | Production |
|---|---|---|---|
| `0001_baseline.sql` | schéma de la prod au 2026-09-30, à l'identique | à passer (base vierge) | **ne pas passer** (déjà dans cet état) |
| `0002_security.sql` | RLS `box_challenges`, droits clients retirés, `devices`/`profiles` en lecture seule, `waitlist` insertion seule, `handle_new_user` durcie, NOT NULL | à passer | à passer |

## Journal des passages

| Fichier | Staging | Production |
|---|---|---|
| 0001_baseline | — | (état d'origine) |
| 0002_security | — | — |

Après chaque passage : relancer `snapshot-schema.sql` et vérifier le résultat attendu décrit en
tête du fichier de migration.
