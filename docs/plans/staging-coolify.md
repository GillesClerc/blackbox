# Staging — une copie de test du site et de sa base

> **Pourquoi** : aujourd'hui, un `git push` sur `master` met directement en ligne
> `box.agill.es`, branché sur la seule base Supabase. Tester une migration de base ou un paiement
> Stripe s'y fait donc « en vrai ». Le staging est une **deuxième copie** (site + base) où l'on
> essaie tout d'abord ; la prod ne reçoit que ce qui a marché. Décision du 2026-09-30.

## Ce qu'on met en place

| | Production (existant) | Staging (nouveau) |
|---|---|---|
| Site | `box.agill.es` | `staging.box.agill.es` (proposition) |
| Base | `supabase.agill.es` | `supabase-staging.agill.es` (proposition) |
| Déploiement | **manuel** (bouton Deploy de Coolify) | **automatique** à chaque push sur `master` |
| Données | vraies box, vrais comptes | comptes et box de test |
| Stripe (plus tard) | clés live | clés **test** |

Le point clé : **la prod cesse de se redéployer seule**. On pousse, le staging se met à jour, on
vérifie, puis on clique « Deploy » sur la prod.

## Étapes dans Coolify (Gilles)

1. **Base de staging** : New Resource → Service → **Supabase** (même modèle que la prod).
   Domaine `supabase-staging.agill.es`. Dans Environment Variables, comme pour la prod :
   `ENABLE_EMAIL_AUTOCONFIRM=true` (modifier l'entrée existante, puis **Redeploy**).
   Relever l'URL, la clé `anon` et la clé `service_role`.
2. **Site de staging** : New Resource → Application → même dépôt GitHub, branche `master`,
   Nixpacks, **Base Directory `web/`**, domaine `staging.box.agill.es`, déploiement automatique
   (webhook) activé.
3. **Variables du site de staging** (onglet Environment Variables) :
   - `NEXT_PUBLIC_SUPABASE_URL` = URL de la base de staging
   - `NEXT_PUBLIC_SUPABASE_ANON_KEY` = clé `anon` de staging
   - `SUPABASE_SERVICE_ROLE_KEY` = clé `service_role` de staging
   - `BOX_MASTER_SECRET` = **nouveau** secret (`openssl rand -hex 32`), différent de la prod
   Puis **Redeploy** (les variables ne sont lues qu'au démarrage).
4. **Prod en manuel** : dans l'application `box.agill.es`, désactiver le déploiement automatique
   (Auto Deploy / webhook). Les mises en prod se feront par le bouton **Deploy**.
5. **Schéma** : dans le SQL Editor de la base de staging, passer les migrations de
   `web/supabase/migrations/` dans l'ordre (je fournis chaque fichier).

## Vérifications

- `curl https://staging.box.agill.es` → 200 ; créer un compte de test, se connecter.
- `curl https://supabase-staging.agill.es/auth/v1/settings -H "apikey: <anon staging>"` →
  `mailer_autoconfirm: true`.
- `BOX_MASTER_SECRET=<secret staging> python3 tools/test_box_api.py --base https://staging.box.agill.es --box-uid <uid de test>`
  (flux challenge → auth → sync). L'enregistrement normal exige la preuve BLE d'une vraie box :
  pour une box simulée, créer la ligne à la main dans le SQL Editor de staging —
  `insert into devices (box_uid, owner_id, name) values ('ESP32S3-TEST-0001', '<id de ton compte de test>', 'Box simulée');`

## Box de test sur le staging

Une box ne parle qu'à une seule base à la fois (URL `CONFIG_ESCAPEBOX_API_URL`, surcharge NVS
`cloud/api_url`). Pour tester une box contre le staging : surcharger `cloud/api_url` et la
provisionner avec le secret de staging. Pas nécessaire pour les étapes web E0-E2 : le simulateur
`tools/test_box_api.py` suffit.
