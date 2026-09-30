# Staging — copie de test du site et de sa base

Une deuxième copie (site + base Supabase) pour essayer migrations, nouvelles pages et paiements
avant la prod. La prod ne reçoit que ce qui a marché en staging.

| | Production | Staging |
|---|---|---|
| Site | `box.agill.es` | `tbox.agill.es` |
| Base | `supabase.agill.es` | `tsupabase.agill.es` |
| Déploiement | **manuel** (bouton Deploy) | automatique à chaque push sur `master` |
| Données | vraies box, vrais comptes | comptes et box fictifs uniquement |
| Indexation | `SITE_NOINDEX=1` | `SITE_BLOCK_CRAWL=1` + mot de passe |
| Stripe (plus tard) | clés live | clés test |

⚠ Sous Coolify, toute modification de variable exige un **Redeploy** (Restart ne la recharge pas).

## Mise en place (Coolify)

1. **Base** : New Resource → Service → **Supabase**, domaine `tsupabase.agill.es` (service Kong,
   https, port 8000). ⚠ Le modèle Coolify référence `minio/mc`, retiré de Docker Hub (et refusé
   par quay.io) : retirer les services `supabase-minio` et `minio-createbucket`, et passer
   `supabase-storage` en `STORAGE_BACKEND=file` + `FILE_STORAGE_BACKEND_PATH=/var/lib/storage`
   (sans `STORAGE_S3_ENDPOINT`, `STORAGE_S3_FORCE_PATH_STYLE`, `AWS_*`). Montages de fichiers en
   syntaxe courte. L'éditeur de compose ne colle qu'environ 150 lignes à la fois : coller par
   morceaux et vérifier la dernière ligne avant Save.
   Variable `ENABLE_EMAIL_AUTOCONFIRM=true` (modifier l'entrée existante), puis **Redeploy**.
   Relever l'URL, la clé `anon` et la clé `service_role`.
2. **Site** : New Resource → Application → même dépôt GitHub, branche `master`, Nixpacks,
   **Base Directory `web/`**, domaine `tbox.agill.es`, déploiement automatique activé.
3. **Variables du site** (puis **Redeploy**) :
   - `NEXT_PUBLIC_SUPABASE_URL` = `https://tsupabase.agill.es`
   - `NEXT_PUBLIC_SUPABASE_ANON_KEY` = clé `anon` du staging
   - `SUPABASE_SERVICE_ROLE_KEY` = clé `service_role` du staging
   - `BOX_MASTER_SECRET` = nouveau secret (`openssl rand -hex 32`), **différent de la prod**
   - `SITE_BLOCK_CRAWL` = `1`
4. **Prod en manuel** : sur l'application `box.agill.es`, désactiver le déploiement automatique.
   Les mises en prod se font ensuite par le bouton **Deploy**.
5. **Schéma** : dans le SQL Editor de `tsupabase.agill.es`, passer dans l'ordre les fichiers de
   `web/supabase/migrations/` (0001, 0002, puis les suivants), et le noter dans le journal du
   README de ce dossier.

## Mot de passe devant le site

Basic auth via Traefik (le proxy de Coolify). Les menus exacts dépendent de la version de
Coolify ; à faire ensemble si besoin.

- Empreinte du mot de passe : `htpasswd -nbB staging '<mot de passe>'` (paquet apache2-utils).
  Dans les labels Coolify, doubler chaque `$` en `$$`.
- Middleware sur l'application : `traefik.http.middlewares.staging-auth.basicauth.users=staging:<empreinte>`,
  rattaché au routeur HTTPS de l'application (`…routers.<routeur>.middlewares=staging-auth`).
- **Laisser `/api/box/*` sans mot de passe** : routeur dédié `PathPrefix(`/api/box`)`, priorité
  plus haute, sans le middleware (une box ou le simulateur n'envoient pas d'identifiant ; ces
  routes sont protégées par HMAC et JWT).

## Règles de sécurité

- Aucun secret partagé avec la prod (base, `BOX_MASTER_SECRET`, clés Stripe).
- Jamais de vraies données : pas de copie de la base de prod.
- Studio de `tsupabase.agill.es` protégé par mot de passe (vérifier et renforcer les
  identifiants générés par le modèle Supabase).
- Optionnel : arrêter les deux services quand on ne s'en sert pas.

## Vérifications

- `curl -I https://tbox.agill.es` → **401** (mot de passe en place) ; dans le navigateur, avec
  l'identifiant : créer un compte de test, se connecter.
- `curl -s https://tbox.agill.es/robots.txt -u staging:<mot de passe>` → `Disallow: /`.
- `curl https://tbox.agill.es/api/box/challenge?box_uid=ESP32S3-TEST-0001` → 200 (API ouverte).
- `curl https://tsupabase.agill.es/auth/v1/settings -H "apikey: <anon staging>"` →
  `mailer_autoconfirm: true`.

## Box simulée

L'enregistrement normal exige la preuve BLE d'une vraie box. Pour le simulateur :

1. Créer la box à la main dans le SQL Editor du staging :
   `insert into devices (box_uid, owner_id, name) values ('ESP32S3-TEST-0001', '<id du compte de test>', 'Box simulée');`
2. `BOX_MASTER_SECRET=<secret staging> python3 tools/test_box_api.py --base https://tbox.agill.es --box-uid ESP32S3-TEST-0001`
   (challenge → auth → sync).

Une vraie box ne parle qu'à une base à la fois : pour la brancher sur le staging, surcharger
`cloud/api_url` en NVS et la provisionner avec le secret du staging. Inutile pour les étapes web.
