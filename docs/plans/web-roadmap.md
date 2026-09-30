# Web — roadmap vers le site de présentation et l'inventaire des histoires

> Proposition du 2026-09-30, à arbitrer par Gilles avant tout code. Objectif : **un beau site
> de présentation** + **un backend complet pour l'inventaire des histoires** (catalogue,
> versions et publication des packages, droits, achat, synchronisation vers la box).
> Sources : FSD (§2.3.3, §6.1, §6.3, FR-WEB, §8.1.3, §9.3), vision, `web-implementation.md`,
> `firmware-cloud-client.md`, `RESTE-A-FAIRE.md` §5, et relevé du code `web/` au 30/09.
> Cocher au fil de l'eau ; les décisions ouvertes sont regroupées au §4.
>
> **Arbitrages de Gilles (2026-09-30)** : licence **liée au compte** · assets dans **Supabase
> Storage** · **staging** : oui · nom de marque / domaine : en réflexion, **le site reste hors
> ligne** (pas de mise en prod publique de la vitrine) · prix : **étude à faire** · histoire
> révoquée déjà installée : **la box la garde** pour l'instant. → **E0 lancé.**

---

## 1. État des lieux

### 1.1 Ce qui existe

| Domaine | Statut | Détail (fichiers) |
|---|---|---|
| Landing « Ouvrez l'œil » + waitlist | ✅ fait | `app/page.tsx`, `components/box-face.tsx`, `waitlist-form.tsx`, server action `app/actions.ts` (pas de route `api/waitlist`) |
| Auth e-mail / mot de passe | ✅ fait | `app/(auth)/*`, `components/auth-form.tsx`, `proxy.ts` (refresh session + garde des préfixes) |
| Auth Google | ❌ absent | annoncé « en service » par le FSD, aucun code |
| Mes box (liste) | ✅ fait | `app/(app)/devices/page.tsx` : box_uid, firmware, dernière synchro (RLS) |
| Appairage BLE (option B) | ✅ fait, E2E navigateur à valider | `app/(app)/devices/add/page.tsx` (Web Bluetooth + preuve `register`) |
| Compte | 🟡 partiel | `app/(app)/account/page.tsx` : e-mail + date, bloc « bientôt ici » |
| API box challenge / auth / register / sync | ✅ fait | `app/api/box/*`, `lib/box-auth.ts` (HKDF par box, JWT 2 h) ; sync écrit `last_sync_at` + `firmware_version` |
| Livraison des packages | ✅ fait, solide | `app/api/box/pkg/[slug]/[...path]` (JWT + droit + anti path-traversal), `web/scenario-packages/capitaine_verdier/` (v4) |
| Publication d'une histoire | 🟡 manuel | `tools/package_scenario.py` → commit → redéploiement → bump `scenarios.version` à la main dans Studio |
| Catalogue, fiche, bibliothèque, scores | ❌ absent | aucune page `/shop`, `/library`, `/scores` |
| Achat (Stripe) | ❌ absent | `stripe` et `@stripe/stripe-js` installés, rien de câblé |
| Back-office | ❌ absent | tout se fait dans Supabase Studio (service_role) ; aucun rôle admin |
| Studio / simulateur / page `/v/` / activation QR | ❌ absent | hors périmètre de cette roadmap (§5) |
| Schéma versionné | ❌ absent | aucune migration SQL dans le dépôt, pas de types générés |
| Tests / CI | ❌ absent | seuls `tools/test_box_api.py` et `tools/test_box_crypto.py` (manuels) ; pas de `.github/` |
| Sécurité | 🟡 | pas de rate limiting `/api/box/*` (WB-11), clé JWT = `BOX_MASTER_SECRET` brut (pas de HKDF dédiée, ni `iss`/`aud`), pas de révocation (WB-12), `/register` renvoie 500 au lieu de 409 sur doublon concurrent, pas d'en-têtes CSP/HSTS |
| UI kit | 🟡 | shadcn initialisé mais **aucun composant** (`components/ui/` absent), `shadcn` en `dependencies` |
| i18n | ❌ absent | tout en français en dur |

**Tables réelles** (Supabase self-hosted, créées à la main) : `profiles`, `devices`, `scenarios`
(`slug`, `version`, `package_path`, `active`, `price_chf`…), `device_scenarios` (**seul droit
d'accès, lié à la box**), `box_challenges`, `firmware_releases`, `waitlist`. Pas de `licenses`.

### 1.2 Où la documentation n'est plus à jour

- **FSD §2.3.3** : « Supabase Auth (email + Google) » et « Tailwind/shadcn » donnés **en service** :
  Google n'existe pas, shadcn n'est pas utilisé.
- **FSD §2.3.3, schéma cible** : `licenses` + `device_scenarios(installed_at)` — rien de tout cela
  n'est tranché ; la « décision ouverte avant Stripe » (licence à l'utilisateur) conditionne tout
  le modèle d'inventaire.
- **FSD §6.3** : `proxy.ts` protège `/shop`, `/library`, `/scores`, `/checkout`, `/studio`, qui
  n'existent pas (correct comme cible, trompeur comme état).
- **FSD §8.1.3** : tests Google, ECDSA, Cloudflare R2 = cibles, rien d'implémenté.
- **FSD, flux QR** : domaine `escapebox.ch` supposé acquis (nom de marque non tranché).
- **`web-implementation.md`** : SQL et arborescence obsolètes (`scenario_path`, `web/public/scenarios`,
  `api/waitlist`, pas de `firmware_releases`) ; les phases 2, 4 et 5 ne sont pas faites alors que
  le plan les présente comme une suite continue. → le remplacer par ce document + migrations.
- **Mémoire de statut web** (12/06-04/07) : juste, mais antérieure à l'audit du 27/09.

---

## 2. Choix structurants proposés (à valider, voir §4)

1. **Licence liée à l'utilisateur**, pas à la box. Une box reçoit les histoires des licences de
   son propriétaire. `device_scenarios` devient le **suivi d'installation** (version installée,
   date), plus la source du droit. Permet l'achat avant d'avoir une box, le changement de box, le
   cadeau, la révocation propre.
2. **Une histoire = une fiche + des versions.** `scenarios` porte le catalogue (titre, pitch,
   visuels, prix, difficulté, joueurs, durée, langue, statut) ; `scenario_versions` porte chaque
   package (numéro, manifest, taille, statut brouillon/publié/retiré, date). La version publiée est
   **une seule donnée en base** : fin de la double source manifest + `scenarios.version`.
3. **Assets hors du dépôt Git** : bucket privé **Supabase Storage** (déjà auto-hébergé, sans
   nouveau fournisseur) ; la route `/api/box/pkg` reste le point d'entrée des box et lit dans le
   bucket → **aucun changement firmware**. R2 seulement si la bande passante l'exige plus tard.
4. **Back-office dans la même app** (`/admin`), protégé par un rôle en base (`profiles.role`) +
   RLS, pas d'outil séparé.
5. **Contrat de l'API box gelé** : les champs de `/sync` et le format des packages restent
   compatibles ; toute évolution est additive.

---

## 3. Étapes

Chaque étape est livrable et testable seule. Ordre recommandé : **E0 → E1 → E2** (le cœur :
l'inventaire), **E3 en parallèle** dès E0 (front seul), puis **E4 → E5 → E6**.
Taille indicative : S ≈ 1 session, M ≈ 2-3, L ≈ 4+.

### E0 — Fondations (S-M)

**Objectif** : un socle reproductible et vérifié avant d'ajouter des tables et de l'argent.

- [x] Relever le schéma réel (`web/supabase/snapshot-schema.sql`, fait le 2026-09-30) et l'écrire en
      **migration initiale** `web/supabase/migrations/0001_initial.sql` (tables, RLS, trigger
      `handle_new_user`, index manquants `devices.owner_id`, contraintes `firmware_releases`).
      → `0001_baseline.sql` (état exact de la prod) + **`0002_security.sql`** (🧑 à passer en prod) :
      le relevé a révélé deux failles — `box_challenges` sans RLS et ouverte à la clé publique
      (déni de service de l'auth box), et la policy `devices` FOR ALL qui laissait un utilisateur
      connecté insérer une box sans la preuve BLE (contournement de l'option B).
      ✅ **0002 passée en prod le 2026-09-30**, vérifiée (relevé + test avec la clé publique).
- [x] Désindexation : `SITE_NOINDEX=1` actif en prod (en-tête vérifié), `SITE_BLOCK_CRAWL`
      prévu pour le staging (`docs/plans/staging-coolify.md`).
- [ ] Types TypeScript générés depuis le schéma (`lib/database.types.ts`) — après la migration initiale.
- [x] Dettes API box : clé JWT dérivée `HKDF(master, "escapebox:jwt")` + `iss`/`aud` ;
      `/register` → 409 sur 23505 (2026-09-30, tests `lib/box-auth.test.ts`).
- [x] `shadcn` en devDependencies, en-têtes de sécurité (CSP, HSTS, X-Frame-Options,
      Permissions-Policy avec `bluetooth=(self)`) dans `next.config.ts` (2026-09-30).
- [ ] `npm audit` (à relancer, voir le rapport de la session).
- [x] CI GitHub Actions `.github/workflows/ci.yml` : typecheck, lint, Vitest, build sans
      secret, `tools/test_box_crypto.py`, tests host firmware (2026-09-30).
- [x] Environnement **staging** : guide `docs/plans/staging-coolify.md`. Base `tsupabase.agill.es`
      en ligne (2026-09-30, MinIO retiré : stockage sur disque, `STORAGE_BACKEND=file`), migrations
      0001-0002 passées, schéma identique à la prod. Site `tbox.agill.es` en ligne (vérifié : 200,
      `robots.txt` Disallow, `/api/box/challenge` écrit en base de staging). Prod passée en déploiement
      manuel (2026-09-30). Mot de passe devant `tbox` : reporté (décision Gilles).

**Fichiers / tables** : `web/supabase/migrations/`, `web/lib/`, `web/next.config.ts`,
`web/package.json`, `.github/workflows/`, `lib/box-auth.ts`, `api/box/register`.
**Fini quand** : la migration rejouée sur une base vierge (staging) redonne le schéma de prod ;
CI verte ; `tools/test_box_api.py` passe en staging et en prod après déploiement.
**À trancher** : staging oui/non (coût Coolify) ; qui passe le SQL en prod (toi dans Studio, comme
aujourd'hui).

### E1 — Modèle d'inventaire (M) — ✅ FAIT (staging + prod, 2026-09-30)

**Objectif** : la base qui porte tout le reste : histoires, versions, licences, installations.

- [x] Tables : `scenarios` (métadonnées catalogue complètes, statut `draft/published/archived`),
      `scenario_versions` (numéro, manifest JSON, taille, chemin de stockage, statut, dates,
      `published_by`), `licenses` (`user_id`, `scenario_id`, `source` = achat / cadeau / admin /
      code, référence Stripe, `revoked_at`), `device_scenarios` recyclée en suivi d'installation
      (`installed_version`, `installed_at`, `last_seen_at`).
- [x] `profiles.role` (`user` / `admin`, éventuellement `editor`) + policies RLS.
- [x] Route `/sync` : droit = licences non révoquées du propriétaire de la box × versions
      publiées ; **même format de réponse** qu'aujourd'hui.
- [x] Route `/pkg` : droit via licence, lecture du manifest et des fichiers de la **version
      publiée** (encore depuis `web/scenario-packages/` à cette étape).
- [x] Migration des données : Capitaine Verdier v4 → une ligne `scenario_versions` + une licence
      pour ton compte (et réassignation de la box de test, `RESTE-A-FAIRE` §9).

Fiche : champs `theme` et `ambiance` ajoutés (demande de Gilles). Migration **`0003_inventory.sql`**,
logique `lib/entitlements.ts`, types générés `lib/database.types.ts`, tests `lib/entitlements.test.ts`
+ `supabase/migrations.test.ts` (PGlite), parcours box `tools/test_box_e2e.py` (vert sur `tbox`).
- [x] **Prod** : `0003_inventory.sql` passée, code déployé, relevé conforme, box réelle `ESP32S3-8FF7-D684`
      synchronisée avec le nouveau code (21:31 UTC). Rôle admin donné au compte de Gilles.
- [ ] Migration ultérieure : supprimer `scenarios.active/version/package_path` une fois E1 stable.

**Fichiers / tables** : migrations `0003_inventory.sql`, `api/box/sync`, `api/box/pkg`,
`lib/database.types.ts`.
**Fini quand** : la box réelle synchronise et installe Capitaine Verdier sans aucun changement
firmware ; révoquer la licence en base fait disparaître l'histoire du `/sync` ; policies RLS
testées (un utilisateur ne voit que ses licences et ses box).
**À trancher** : licence à l'utilisateur (recommandé) ; nombre de box par compte (3, WB-04) ;
que devient une histoire révoquée déjà installée (le firmware la garde tant qu'il n'a pas de
nettoyage, voir §4.2).

### E2 — Back-office d'inventaire `/admin` (L) — ✅ FAIT (staging + prod, 2026-10-01)

Fait : validateur de scénario porté en TypeScript avec **cas partagés avec le firmware**
(`firmware/test_host/scenario_cases.json`) ; règles de package portées (manifest identique à
`package_scenario.py`) ; migration `0004_storage_publish.sql` ; routes `/api/admin/*` et pages `/admin` ;
`/api/box/pkg` lit aussi Storage. Validé par `tools/test_admin_e2e.py` sur `tbox` (29 vérifications :
accès, fiche, dépôt, publication, retour arrière, licences, et réception côté box à chaque étape).
- [x] **Prod** : `0004_storage_publish.sql` passée, code déployé (routes `/api/admin/*` → 401 sans compte,
      `/admin` → connexion).
- [ ] Capitaine Verdier reste servi depuis le dépôt (v4, stockage `repo`) : la déposer en v5 via `/admin`
      la fera passer dans Storage.


**Objectif** : publier une histoire sans terminal, sans commit, sans Studio.

- [ ] Liste des histoires (statut, version publiée, nb de licences, nb de box à jour).
- [ ] Créer / éditer la fiche (titre, pitch, visuels, prix, difficulté, joueurs, durée, langue).
- [ ] **Nouvelle version** : upload d'un zip (ou des fichiers) → validation serveur
      (règles de `package_scenario.py` : profondeur, noms, `scenario.json` obligatoire, 32 Mo/fichier,
      128 fichiers, MP3 44,1 kHz) + **validation du `scenario.json`** (mêmes règles que
      `scenario_validate.c`) → manifest généré (sha256, tailles) → stockage **Supabase Storage** →
      version en brouillon.
- [ ] **Publier** (bascule atomique de la version courante), **dépublier / revenir** à la version
      précédente.
- [ ] Licences à la main : offrir une histoire à un compte, révoquer.
- [ ] `/pkg` lit désormais dans le bucket (proxy, pas d'URL signée exposée à la box).
- [ ] `tools/package_scenario.py` gardé pour le dev hors ligne, ou retiré (à décider).

**Fichiers / tables** : `app/admin/**`, `lib/packaging.ts` (port TS des règles),
`lib/scenario-validate.*`, bucket `scenario-packages`, `api/box/pkg`.
**Fini quand** : Capitaine Verdier v5 publiée depuis `/admin` ; la box ne télécharge que les
fichiers modifiés (install incrémentale F3) ; retour à v4 en un clic, la box se réaligne ;
un compte non admin reçoit 403 sur `/admin`.
**À trancher** : format d'entrée (zip de package prêt, ou YAML source compilé côté serveur comme
`tools/yaml2json.py`) ; validateur de scénario **unique** (port TS, ou `scenario_validate.c`
compilé en WebAssembly pour garder une seule implémentation — recommandé) ; visuels du catalogue
dans le même bucket ou un bucket public.

### E3 — Site de présentation (M, en parallèle de E1-E2) — 🟡 refonte faite le 2026-10-01, en revue sur `tbox`

Direction « Ardoise » (arbitrage Gilles : garder l'esprit « Ouvrez l'œil », tout le reste repensé ;
visuels dessinés en code ; même identité sur l'espace compte) : site clair aux matières de la box
(craie, ardoise, noyer, laiton, lueur iris réservée aux écrans), Young Serif / Atkinson Hyperlegible
Next / Martian Mono, cube vivant en ouverture (yeux qui suivent, bouche qui écrit), patron déplié
des six faces. Pages : accueil, `/la-box`, `/histoires`, `/histoires/[slug]` (lues depuis
l'inventaire), `/faq`, `/confidentialite`, `/mentions-legales` (mentions « à compléter » visibles).


**Objectif** : un site vitrine complet dans la direction « Ouvrez l'œil » : ton espiègle et
mystérieux, teasing plutôt que fiche technique, public familial.

- [ ] Arborescence publique `(marketing)` : accueil (landing actuelle retravaillée), **« La box »**
      (le personnage, les faces, sans tout dévoiler), **« Les histoires »** (catalogue public en
      teaser, alimenté par E1), **fiche histoire** publique (pitch, ambiance, joueurs, durée, âge),
      FAQ, **pages légales** (mentions, confidentialité nLPD/RGPD ; CGV avant E5).
- [ ] Composants shadcn réellement utilisés + système de design documenté (palette nuit
      d'atelier / iris ambre, Unbounded / Public Sans / Space Mono).
- [ ] SEO : métadonnées par page, images Open Graph, sitemap, robots ; performance (Lighthouse
      mobile ≥ 90) ; accessibilité (contrastes, focus, `prefers-reduced-motion`).
- [ ] Waitlist conservée, avec source (page d'origine) pour mesurer l'intérêt FFF.

**Fichiers / tables** : `app/(marketing)/**`, `components/ui/**`, `components/marketing/**`,
`app/sitemap.ts`, `app/robots.ts`, `app/opengraph-image.tsx`.
**Fini quand** : toutes les pages en ligne sur staging puis prod, Lighthouse mobile ≥ 90, les
fiches publiques lisent le catalogue en base (aucun texte d'histoire en dur).
**À trancher** : **nom de marque et domaine** ; langues au lancement (FR seul, ou FR/DE/EN — WB-09) ;
visuels (photos du proto, illustrations, rendu 3D) ; montrer un prix ou « bientôt ».

### E4 — Compte, « Mes box », bibliothèque (M)

**Objectif** : l'espace joueur complet.

- [ ] **Bibliothèque** `/library` : mes histoires (licences), sur quelles box elles sont
      installées et à quelle version.
- [ ] **Mes box** `/devices` : renommer, dernière synchro, firmware, histoires installées vs
      disponibles (« à jour » / « en attente de synchro »), retirer une box du compte
      (**révocation**, WB-12 : liste noire de `jti` ou compteur de génération par box).
- [ ] Compte : changer de mot de passe, **mot de passe oublié** (exige un envoi d'e-mails),
      supprimer son compte (nLPD/RGPD).
- [ ] Auth Google (si retenue).
- [ ] Suivi d'installation : le serveur enregistre chaque manifest téléchargé (`device_scenarios.
      installed_version`) — **sans changement firmware** ; plus tard, un compte rendu explicite
      envoyé par la box (§4.2).

**Fichiers / tables** : `app/(app)/library`, `app/(app)/devices/[id]`, `app/(app)/account`,
`api/box/pkg` (journal), `api/box/auth` (révocation), table `device_revocations` ou colonne
`devices.token_generation`.
**Fini quand** : un compte voit ses histoires et l'état de chaque box ; retirer une box → son
prochain appel API renvoie 401 et elle n'obtient plus de JWT ; mot de passe oublié fonctionne
de bout en bout.
**À trancher** : fournisseur d'e-mails (Resend, prévu au FSD ; aujourd'hui aucun SMTP, autoconfirm
activé) ; Google oui/non ; Apple (FSD) plus tard.

### E5 — Achat (Stripe) (M)

**Objectif** : acheter une histoire et la voir arriver sur la box.

- [ ] Stripe Checkout (paiement unique) depuis la fiche histoire ; pages `checkout/success` et
      `checkout/cancel`.
- [ ] Webhook `api/webhooks/stripe` (signature vérifiée, idempotent) → création de la licence.
- [ ] Reçus (Stripe) ; remboursement → révocation de la licence.
- [ ] Codes cadeau / cartes QR physiques (`activate/[token]`, flux FSD §2.3.3) — ici ou en E6.
- [ ] CGV, TVA, moyens de paiement (carte, TWINT pour la Suisse).

**Fichiers / tables** : `lib/stripe.ts`, `app/api/checkout`, `app/api/webhooks/stripe`,
`app/(app)/checkout/**`, `licenses` (référence Stripe), table `activation_codes` si QR.
**Fini quand** : achat test (carte 4242…) en staging → licence en base en < 5 s → histoire dans
le `/sync` de la box en < 10 s (WB-02, WB-03) ; webhook rejoué deux fois = une seule licence ;
remboursement = licence révoquée.
**À trancher** : **prix** (19-29 CHF par histoire, vision) ; histoire offerte avec la box ; TVA
(seuil d'assujettissement) ; TWINT ; codes cadeau au lancement ou non.

### E6 — Suivi des box et exploitation (M)

**Objectif** : savoir ce qui se passe sur le parc et intervenir.

- [ ] Journal des synchros (`box_syncs` : date, firmware, histoires servies, erreurs) et vue
      admin du parc (dernière synchro, version firmware, box en retard).
- [ ] Gestion des **releases firmware** depuis `/admin` (upload du `.bin` dans le bucket, sha256,
      canal, activation) — prérequis de l'OTA F6.
- [ ] **Rate limiting** `/api/box/*` (WB-11 : 10 req/min par box).
- [ ] Sauvegardes Supabase (base + bucket) vérifiées par une restauration ; supervision
      (erreurs serveur, disponibilité) ; rotation des secrets documentée.
- [ ] Tests E2E (Playwright) des parcours clés : inscription, appairage simulé, achat test,
      publication admin.

**Fichiers / tables** : `box_syncs`, `app/admin/devices`, `app/admin/firmware`,
`firmware_releases`, `lib/rate-limit.ts`, `web/e2e/`.
**Fini quand** : une release firmware publiée depuis `/admin` apparaît dans le `firmware_update`
du `/sync` ; une box qui boucle est limitée (429) sans bloquer les autres ; restauration de
sauvegarde testée une fois.
**À trancher** : outil de supervision (logs Coolify seuls, ou service externe) ; stockage du
limiteur (table Postgres ou mémoire, une seule instance aujourd'hui).

### Transverse (à chaque étape)

Tests des routes touchées ; migration SQL versionnée à chaque changement de schéma (jamais de
modification à la main sans migration) ; RLS relue ; déploiement staging → prod ; FSD §2.3.3/§6.x
et ce fichier mis à jour.

---

## 4. Décisions ouvertes

### 4.1 Pour toi

| Décision | Options | Recommandation | Bloque |
|---|---|---|---|
| Licence liée à… | utilisateur / box | **utilisateur** | E1 |
| Stockage des assets | dépôt Git (actuel) / Supabase Storage / R2 | **Supabase Storage** (déjà hébergé) ; R2 si besoin | E2 |
| Rôles admin | un flag admin / admin + éditeur | **admin seul** au début | E1 |
| Format d'upload | zip de package / YAML source | zip d'abord, YAML ensuite | E2 |
| Validateur de scénario | port TS / C en WebAssembly / Python en CI | **WebAssembly** (une seule implémentation) | E2 |
| Nom de marque, domaine | EscapeBox / autre ; escapebox.ch ? | à travailler avant E3 prod | E3 |
| Langues | FR seul / FR-DE-EN | FR au lancement, structure i18n prête | E3 |
| E-mails | aucun / Resend / SMTP | **Resend** (prévu au FSD) | E4 |
| Google / Apple | oui / non | Google oui, Apple plus tard | E4 |
| Prix | 19-29 CHF/histoire ; histoire incluse avec la box | à tester en FFF | E5 |
| Paiement | carte seule / + TWINT ; TVA | carte + TWINT | E5 |
| Staging | oui / non | **oui** avant Stripe | E0 |

### 4.2 Dépendances avec le firmware

| Point | Impact firmware | Quand |
|---|---|---|
| Contrat `/sync` et format des packages inchangés (E1, E2) | **aucun** si on garde les champs actuels et le proxy `/pkg` | E1-E2 |
| Clé JWT dérivée + `iss`/`aud` (E0) | aucun : les JWT en cours deviennent invalides, la box se ré-authentifie (déjà géré sur 401) | E0 |
| Révocation (E4) | aucun : 401 → ré-auth → 403 si box retirée ; vérifier que le firmware ne boucle pas | E4 |
| Rate limiting 10 req/min (E6) | vérifier la cadence de `cloud_client` (sync + téléchargement de N fichiers) : le quota doit viser les appels d'auth/sync, pas chaque fichier | E6 |
| Histoire révoquée déjà installée | le firmware ne nettoie pas les histoires retirées (hors périmètre du plan firmware) : décider si elle reste jouable hors ligne | E1 |
| Versions installées remontées par la box | optionnel : le serveur les déduit des téléchargements ; un paramètre `installed=` sur `/sync` serait plus fiable (petit changement firmware) | E4 |
| Releases firmware (E6) | OTA F6 à faire côté firmware (après les cartes) | E6 |
| Signature ECDSA des packages (WB-05) | vérification côté firmware obligatoire ; à planifier avec Secure Boot | après E6 |
| Scores / page `/v/` | `POST /api/box/session` + file hors ligne (plan firmware, optionnel) | hors périmètre |

---

## 5. Hors périmètre de cette roadmap

Studio / éditeur de scénarios (React Flow, Monaco), simulateur, marketplace et reversements
(Stripe Connect), abonnements B2B, scores et leaderboard `/v/`, i18n complète, signature ECDSA et
chiffrement des packages par box. À replanifier après E6 et après les premières box FFF.
