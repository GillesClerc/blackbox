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
| 0001_baseline | ✅ 2026-09-30 (base vierge `tsupabase.agill.es`) | (état d'origine) |
| 0002_security | ✅ 2026-09-30 (relevé identique à la prod) | ✅ 2026-09-30 (vérifié : relevé du schéma + test clé publique → `permission denied`) |

Après chaque passage : relancer `snapshot-schema.sql` et me renvoyer la cellule pour contrôle
(RLS, policies et droits attendus).
