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
   - `SITE_BLOCK_CRAWL` = `1` (`robots.txt` interdit tout, voir « Ne pas être référencé »)
   Puis **Redeploy** (les variables ne sont lues qu'au démarrage).
4. **Prod en manuel** : dans l'application `box.agill.es`, désactiver le déploiement automatique
   (Auto Deploy / webhook). Les mises en prod se feront par le bouton **Deploy**.
5. **Schéma** : dans le SQL Editor de la base de staging, passer les migrations de
   `web/supabase/migrations/` dans l'ordre (je fournis chaque fichier).

## Ne pas être référencé, ne pas être visité

Deux couches, de la plus simple à la plus sûre :

1. **Deux variables** (code du 2026-09-30), lues à chaque requête : un changement de valeur
   demande un **Restart**, pas un nouveau build.
   - **`SITE_NOINDEX=1` → prod** (`box.agill.es`) tant que la marque n'est pas choisie : toutes
     les pages renvoient `X-Robots-Tag: noindex, nofollow, noarchive`, **`robots.txt` reste
     ouvert**. Le robot doit pouvoir charger la page pour lire le `noindex` : un `Disallow` l'en
     empêcherait, et une adresse liée ailleurs pourrait alors sortir dans les résultats sans
     description. Le site reste accessible aux proches (FFF) par lien direct. Réversible sans
     séquelle : on retire la variable le jour du lancement.
   - **`SITE_BLOCK_CRAWL=1` → staging** : `robots.txt` répond `Disallow: /`. Sans risque ici,
     puisque le mot de passe empêche de toute façon le robot d'entrer. Ne pas combiner les deux
     en prod.
2. **Mot de passe devant le staging** (basic auth, via Traefik, le proxy de Coolify) : plus
   personne n'y entre sans identifiant, robots compris. Principe (le libellé exact des menus
   dépend de la version de Coolify — à faire ensemble si besoin) :
   - générer l'empreinte : `htpasswd -nbB staging '<mot de passe>'` (paquet apache2-utils) ;
     dans les labels Docker de Coolify, doubler chaque `$` en `$$` ;
   - ajouter un middleware sur l'application de staging :
     `traefik.http.middlewares.staging-auth.basicauth.users=staging:<empreinte>`
     et le rattacher au routeur HTTPS de l'application (`…routers.<routeur>.middlewares=staging-auth`) ;
   - **laisser `/api/box/*` sans mot de passe** (routeur dédié `PathPrefix(`/api/box`)`, priorité
     plus haute, sans le middleware) : ni une box ni le simulateur n'envoient d'identifiant, et ces
     routes sont déjà protégées par le HMAC et le JWT.
   - Vérifier : `curl -I https://staging.box.agill.es` → **401** ;
     `curl https://staging.box.agill.es/api/box/challenge?box_uid=ESP32S3-TEST-0001` → 200.

## Règles de sécurité du staging

- **Aucun secret partagé avec la prod** : base, `BOX_MASTER_SECRET`, plus tard clés Stripe
  **test** (elles ne peuvent pas déplacer d'argent réel). Une fuite du staging n'ouvre rien en prod.
- **Jamais de vraies données** : pas de copie de la base de prod, seulement des comptes et des box
  fictifs.
- **Studio de la base de staging protégé par mot de passe**, comme celui de la prod (vérifier les
  identifiants générés par le modèle Supabase de Coolify, les changer s'ils sont faibles).
- Optionnel : **arrêter** les deux services de staging dans Coolify quand on ne s'en sert pas.

## Vérifications

- `curl -I https://staging.box.agill.es` → 401 si le mot de passe est en place (sinon 200) ;
  dans le navigateur, avec l'identifiant : créer un compte de test, se connecter.
- `curl -s https://staging.box.agill.es/robots.txt -u staging:<mot de passe>` → `Disallow: /`.
- Prod : `curl -sI https://box.agill.es | grep -i x-robots` → `noindex, nofollow, noarchive` ;
  `curl -s https://box.agill.es/robots.txt` → `Allow: /` (volontairement ouvert).
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
