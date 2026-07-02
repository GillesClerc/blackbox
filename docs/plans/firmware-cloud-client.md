# Firmware — Client cloud + provisioning BLE

*Guide opérationnel. Spec fonctionnelle dans `docs/escapebox-fsd.md` (§ API box, § architecture). Côté serveur : `docs/plans/web-implementation.md` (Phase 3).*

**Périmètre** : le flux réseau complet box ↔ cloud — auth (challenge/HMAC → JWT), sync et installation de **scénarios avec assets propres** (packages versionnés, hash par fichier), mixage audio pour jouer ces assets, provisioning Wi-Fi BLE + register option B, OTA. L'upload des scores est **optionnel** (annexe), à activer quand un scénario en aura l'usage (leaderboard/QR).

**Fait** :
- **F1 — Auth** (commit c4a4bf7, validé sur cible) : composant `cloud_client`, task core 0 prio 3, challenge → `hal_box_auth_sign` → JWT 2 h en RAM (ré-auth à 110 min ou sur 401), horloge réglée via `server_time`, API URL `CONFIG_ESCAPEBOX_API_URL` + surcharge NVS `cloud/api_url`. `PROJECT_VER` rapporté au sync.
- **F2 — Sync scenario.json** (commit 48d2fc8, validé sur cible) : download streaming `.tmp`+rename vers `/sdcard/scenarios/<slug>/scenario.json`, manifest global `/sdcard/scenarios/manifest.json`. **Sera remplacé par F3** (manifest par scénario, versionné) — le manifest global disparaît.

---

## Jalons

| Jalon | Contenu | Livrable vérifiable |
|---|---|---|
| ~~F1~~ | ✅ Auth challenge/HMAC → JWT | fait, validé sur cible |
| ~~F2~~ | ✅ Sync + scenario.json sur SD | fait, remplacé par F3 |
| **F3** | **Packages d'assets** : outillage + serveur versionné + install incrémentale (sha256/fichier) | Scénario avec MP3 propres publié → tout arrive sur SD ; bump de version → seuls les fichiers modifiés re-téléchargés |
| **F4** | **Mixage audio** : voix/SFX MP3 par-dessus la musique de fond + résolution `"play"` → asset SD | Une voix off jouée pendant l'ambiance, sans glitch ; fallback tons si asset absent |
| **F5** | Provisioning BLE (NimBLE) + register **option B** (preuve HMAC) | Box neuve appairée depuis `/devices/add` (Web Bluetooth) : Wi-Fi + enregistrement prouvé |
| **F6** | OTA : `esp_https_ota` depuis le bloc `firmware_update` du sync | Release en DB → mise à jour + rollback si boot invalide |
| *Opt.* | *Scores : `POST /api/box/session` + envoi fin de partie (file offline SD)* | *À activer avec le premier scénario à leaderboard/QR* |

> Chantier connexe (hors de ce plan, UI) : **menu de démarrage** (choix du scénario, Wi-Fi, paramètres). F3 prépare le terrain : clé NVS `active_scenario` lue par le chargeur + liste des scénarios installés.

---

## Architecture firmware

```
components/
  cloud_client/     # F1-F3-F6 : auth, sync, packages, OTA (code "froid")
  hal_audio/        # F4 : mixer multi-voies (extension du composant existant)
  ble_prov/         # F5 : GATT provisioning (NimBLE)
```

Rappels valables partout : task réseau core 0 prio 3 (jamais au-dessus du jeu), buffers alloués une fois au boot (PSRAM pour le gros), timeouts partout, `ESP_ERROR_CHECK` interdit hors `_init()` boot, dégradation propre sans Wi-Fi/SD/provisioning.

---

## F3 — Packages d'assets

### Format de package (serveur, statique)

```
web/public/scenarios/<slug>/
  manifest.json          ← généré, JAMAIS écrit à la main
  scenario.json
  ambient.mp3            ← musique de fond du scénario
  audio/<nom>.mp3        ← voix/SFX résolus par "play": "<nom>" (F4)
  (autres : images e-ink, etc. — la livraison est agnostique au type)
```

`manifest.json` :
```json
{
  "slug": "capitaine_verdier",
  "version": 2,
  "total_bytes": 4823456,
  "files": [
    { "path": "scenario.json",       "bytes": 9516,    "sha256": "…" },
    { "path": "ambient.mp3",         "bytes": 2400000, "sha256": "…" },
    { "path": "audio/hint_code.mp3", "bytes": 180224,  "sha256": "…" }
  ]
}
```

- **`tools/package_scenario.py <src_dir> --slug <slug> [--version N] --out web/public/scenarios/`** : copie les fichiers, calcule tailles + sha256, écrit le manifest. Refuse les chemins non sûrs. Sans `--version` : incrémente celle du manifest de destination existant.
- Contrainte assets audio (pour F4) : MP3 44 100 Hz, mono ou stéréo, CBR recommandé — documentée ici, vérifiée par l'outil (warning).

### Côté web/DB

- `alter table scenarios rename column scenario_path to package_path;` puis valeurs → `/scenarios/<slug>` (dossier, plus un fichier). Ajouter `version int not null default 1`.
- Route sync : renvoyer `package_path` + `version` par scénario. Rien d'autre ne change (la livraison reste statique publique — dette Stripe inchangée, cf. CLAUDE.md).
- Publier = `package_scenario.py` + commit `web/public/scenarios/<slug>/` + bump `scenarios.version` en DB.

### Côté firmware (`cloud_client`)

- **Install incrémentale** : pour chaque scénario du sync,
  1. manifest local `/sdcard/scenarios/<slug>/manifest.json` présent **et** `version` identique → installé, skip.
  2. sinon : télécharger `{package_path}/manifest.json` (≤ 64 Ko), valider chaque `path` (relatif, pas de `..`, profondeur ≤ 2, charset `[A-Za-z0-9._/-]`, ≤ 32 Mo/fichier), vérifier l'espace SD (`statvfs` vs `total_bytes`).
  3. par fichier : si le fichier local existe avec la bonne taille **et** que son hash (ancien manifest local, sinon re-hash du fichier) correspond → skip ; sinon download streaming `.tmp` avec **sha256 au fil de l'eau** (PSA `psa_hash_*`, déjà utilisé dans `hal_box_auth`), vérif avant rename.
  4. tous les fichiers OK → écrire le manifest local en dernier (= **marqueur de commit**). Échec partiel → pas de manifest, les fichiers complets restent, reprise au prochain sync.
- **Migration F2→F3** : l'ancien manifest global `/sdcard/scenarios/manifest.json` est supprimé s'il existe ; un dossier sans manifest local est re-vérifié/complété au sync.
- **Orphelins** : dossier SD absent de la liste sync → `LOGI "orphelin (conservé)"`, aucune suppression (le GC viendra avec la révocation de licences).
- **Sélection** : clé NVS `cloud/active_scenario` (slug) lue par le chargeur de `main.c` ; absente ou invalide → premier dossier valide (comportement actuel). Exposer `cloud_client` n'y touche pas — c'est le futur menu qui l'écrira.
- **Contention SD/audio** : le download partage la SD avec le streaming `ambient.mp3` (FATFS réentrant → sûr, mais débit partagé). Mesurer ; si glitchs au boot, retarder le lancement du bg audio jusqu'à la fin du premier sync (ordre de boot à ajuster dans `main.c`).
- Vérif : publier un package avec 2 MP3 → sync → fichiers + manifest sur SD ; bump version en ne modifiant que `scenario.json` → seul ce fichier est re-téléchargé ; couper le Wi-Fi en plein download → reprise propre au boot suivant.

## F4 — Mixage audio (`hal_audio`)

- **Mixer logiciel N voies** (bg musique + 1 voie one-shot voix/SFX pour commencer) : un task de rendu unique décode frame par frame chaque voie active (minimp3, buffers PSRAM), additionne en 32 bits avec saturation → PCM 16 bits → I2S. Remplace l'actuel `audio_bg_mp3` (le task tones `audio_bg` fallback reste).
- API nouvelle : `hal_audio_play_oneshot(path)` (coupe le one-shot précédent), `hal_audio_set_bg_volume()` / ducking simple (bg à ~40 % pendant une voix — à valider à l'oreille).
- `play_audio()` de `main.c` : `"play": "xxx"` → si `<scenario_dir>/audio/xxx.mp3` existe → `hal_audio_play_oneshot`, sinon tons builtin actuels (fallback permanent : un scénario sans assets reste jouable).
- Contrainte format : 44 100 Hz (pas de resampling dans un premier temps — vérifié par `package_scenario.py`). Deux décodages minimp3 simultanés sur core 0 : budget CPU à mesurer (`uxTaskGetStackHighWaterMark` + charge), stack du task de rendu à dimensionner (~32 Ko comme l'actuel).
- Charger `references/audio-i2s.md` du skill avant d'attaquer.
- Vérif : voix off par-dessus l'ambiance sans glitch ni sur les yeux ni sur l'audio ; asset manquant → tons ; deux `play` rapprochés → le second coupe le premier.

## F5 — Provisioning BLE + register option B

**Firmware `ble_prov`** (NimBLE, prévu au FSD) :
- Service GATT custom, activé **sur demande** (entrée menu « appairage », fenêtre 5 min, LED témoin) et automatiquement au boot si NVS `wifi_creds` absente (box neuve). Jamais actif en jeu.
- Caractéristiques :
  - `box_uid` — read
  - `wifi_ssid`, `wifi_pass` — write only (jamais lisibles)
  - `status` — notify : `idle / connecting / wifi_ok(ip) / wifi_fail / registered`
  - `auth_challenge` — write ; `auth_response` — notify (signature `hal_box_auth_sign`) → **preuve de possession**
- À réception ssid+pass : écrire NVS `wifi_creds`, reconnecter `hal_wifi`, notifier le résultat. BLE coupé à la fin de la fenêtre.

**Client : page `/devices/add`** (Web Bluetooth — Chrome desktop/Android ; ⚠ pas iOS Safari, acceptable FFF/beta, l'app mobile viendra après) :
1. Connexion BLE → lit `box_uid`
2. `GET /api/box/challenge?box_uid=…` → écrit le challenge en BLE → reçoit `auth_response`
3. Saisie Wi-Fi → écrit ssid/pass → attend `wifi_ok`
4. `POST /api/box/register { box_uid, name, challenge, challenge_response }`

**Serveur — register passe en option B** : `register/route.ts` exige `challenge + challenge_response`, vérifie via `verifyBoxHmac` + table `box_challenges` (non utilisé, non expiré → `used=true`, même mécanique que `/api/box/auth`). La dette « squat d'UID » du CLAUDE.md est soldée — mettre à jour CLAUDE.md à ce jalon.

- Vérif : effacer `wifi_creds` en NVS → parcours complet depuis un navigateur → box en Wi-Fi + ligne `devices` en DB ; rejouer le register avec un faux HMAC → 401.

## F6 — OTA

- Sync renvoie `firmware_update { version, url, sha256 }` → confirmation utilisateur via le menu (pas d'update silencieuse en pleine partie), puis `esp_https_ota` en streaming vers la partition passive.
- Vérifier le `sha256` annoncé sur l'image reçue avant `esp_ota_set_boot_partition`.
- Rollback déjà actif : au boot suivant, `cloud_client` marque l'app valide (`esp_ota_mark_app_valid_cancel_rollback`) **seulement après** un sync réussi — un firmware qui ne sait plus parler au serveur est rollback automatiquement.
- Hébergement des `.bin` : à trancher au moment de F6 (Supabase Storage bucket vs `web/public/firmware/`) ; la table `firmware_releases` existe déjà, insertion manuelle via Studio pour commencer.
- Vérif : flasher une version N, publier N+1 en DB, sync → update → reboot en N+1 marqué valide ; publier une image corrompue → rollback vers N.

## Optionnel — Scores (quand un scénario l'exigera)

Côté web : `web/app/api/box/session/route.ts` — `POST` Bearer JWT box, body `{ scenario_id, score, duration_seconds, hints_used, completed }` (zod), insert `game_sessions` via client admin, renvoie `{ ok, session_token }` (pour le QR `/v/`).
Côté firmware : event `SESSION_UPLOAD` en fin de partie, **file offline** `/sdcard/pending_sessions.jsonl` rejouée à chaque sync — une partie sans réseau n'est jamais perdue.

---

## Points d'attention

- **RAM/flash** : NimBLE + Wi-Fi coexistent sur ESP32-S3 mais surveiller la heap interne (buffers TLS en interne, download + décodage MP3 en PSRAM). Mesurer `esp_get_free_heap_size()` avant/après F4 et F5.
- **Priorités de tasks** : audio (rendu mixer) et yeux sont les chemins temps réel — `cloud_client` et l'host NimBLE en dessous. Vérifier l'absence de glitch audio pendant un download (cf. contention SD, F3).
- **Secrets** : `wifi_pass` write-only en BLE, jamais loggé ; JWT jamais persisté ; `BOX_MASTER_SECRET` strictement côté serveur/provisioning.
- **`provision_box.py` reste le chemin usine** (secret box) ; le BLE ne provisionne que le Wi-Fi utilisateur + l'enregistrement. NVS `box_creds` et `wifi_creds` indépendants.
- **Livraison statique publique** (dette CLAUDE.md) : les packages restent téléchargeables sans auth — à passer en livraison contrôlée avant Stripe.

## Hors périmètre (plans ultérieurs)

- Menu de démarrage (choix scénario/Wi-Fi/paramètres — chantier UI dédié), SNTP propre, GC des scénarios révoqués, resampling audio, signature des packages (au-delà du sha256 d'intégrité), rate-limiting serveur (WB-11), app mobile (remplace Web Bluetooth pour iOS), canal OTA `beta`, page publique `/v/` et QR (plan web Phase 3).
