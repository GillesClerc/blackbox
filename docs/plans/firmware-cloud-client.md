# Firmware — Client cloud + provisioning BLE

*Guide opérationnel. Spec fonctionnelle dans `docs/escapebox-fsd.md` (§ API box, § architecture). Côté serveur : `docs/plans/web-implementation.md` (Phase 3).*

**Périmètre** : le flux réseau complet box ↔ cloud — auth (challenge/HMAC → JWT), sync + téléchargement des scénarios sur SD, upload des scores (y c. la route web manquante), provisioning Wi-Fi via BLE avec passage du register en **option B** (preuve HMAC), et OTA en jalon final.

**Déjà en place** (ne pas refaire) :
- `hal_wifi` — STA, credentials NVS `wifi_creds` (ssid/pass), smoke-test HTTPS validé sur cible.
- `hal_box_auth` — `box_uid` + secret dérivé en NVS `box_creds`, `hal_box_auth_sign()` (HMAC-SHA256 via PSA).
- Serveur : `/api/box/challenge`, `/api/box/auth` (JWT 2h), `/api/box/register` (option A), `/api/box/sync` (scénarios installés + bloc `firmware_update`), table `box_challenges` anti-replay.
- Partitions OTA (factory + ota_0/ota_1 3 MB) + rollback activé ; SD montée FAT sur `/sdcard`.

---

## Jalons

| Jalon | Contenu | Livrable vérifiable |
|---|---|---|
| **F1** | `cloud_client` : challenge → auth → JWT, gestion expiration | Sur cible : log `sync 200` avec Bearer obtenu par la box elle-même |
| **F2** | Sync + téléchargement scénarios → SD + manifest | Scénario ajouté dans `device_scenarios` en DB → présent sur `/sdcard` et jouable après sync |
| **F3** | Scores : route web `POST /api/box/session` + envoi fin de partie (file offline) | Partie jouée → ligne dans `game_sessions` ; hors-ligne → score livré au sync suivant |
| **F4** | Provisioning BLE (NimBLE) + register **option B** (preuve HMAC) | Box neuve appairée depuis `/devices/add` (Web Bluetooth) : Wi-Fi configuré + box enregistrée avec preuve |
| **F5** | OTA : `esp_https_ota` depuis le bloc `firmware_update` du sync | Release en DB → box se met à jour, rollback si boot invalide |

> Ordre choisi : F1-F3 ferment la boucle de valeur (scénarios + scores) avec le Wi-Fi provisionné par `tools/provision_box.py` — pas besoin du BLE pour le FFF chez Gilles. F4 débloque l'inscription par un tiers, F5 la maintenance à distance.

---

## Architecture firmware

Un seul composant nouveau par jalon, une responsabilité chacun :

```
components/
  cloud_client/     # F1-F3-F5 : auth, sync, download, session, OTA (code "froid")
  ble_prov/         # F4 : GATT provisioning (NimBLE)
```

**`cloud_client`** :
- Task dédiée basse priorité (le réseau ne doit jamais préempter audio/rendu). Déclencheurs via queue : `SYNC_BOOT` (auto au boot si `hal_wifi_is_connected()`), `SYNC_MANUAL` (menu), `SESSION_UPLOAD` (fin de partie), plus tard `OTA_APPLY`.
- `esp_http_client` + `esp_crt_bundle` (déjà validé par le smoke-test), JSON via **cJSON** (inclus dans ESP-IDF).
- Base URL : `CONFIG_ESCAPEBOX_API_URL` (Kconfig, défaut `https://box.agill.es`), surchargeable en NVS pour tester contre un serveur local.
- JWT 2h en buffer **static** (pas de malloc runtime) ; sur 401 → re-auth une fois puis abandon avec log. Jamais persisté.
- Dégradation propre : non provisionné (`hal_box_auth_is_provisioned()` false) ou pas de Wi-Fi → la box fonctionne 100 % offline, le sync est juste indisponible (log WARN, pas d'erreur bloquante).
- Conventions maison : timeouts partout (`pdMS_TO_TICKS`, pas de `portMAX_DELAY`), buffers HTTP alloués une fois au boot (PSRAM ok pour le download), `ESP_ERROR_CHECK` interdit hors `_init()` boot.

### F1 — Auth

```
GET  {base}/api/box/challenge?box_uid={hal_box_auth_uid()}   → { challenge, expires_in }
     hal_box_auth_sign(challenge, hex, 65)
POST {base}/api/box/auth { box_uid, challenge, challenge_response } → { token, server_time }
```
- `server_time` → régler l'horloge (`settimeofday`) : pas de RTC ni SNTP pour l'instant, et un TLS strict sur les dates de certs en a besoin. Suffisant en attendant SNTP.
- Vérif : `idf.py build`, flash, log de la séquence complète + un `GET /api/box/sync` à 200.

### F2 — Sync + scénarios sur SD

- `GET /api/box/sync?firmware_version=x.y.z` (version depuis `esp_app_get_description()->version`, alignée `PROJECT_VER`).
- Pour chaque scénario reçu absent du manifest local : download `{base}{scenario_path}` → `/sdcard/scenarios/<slug>.json` (écriture dans un `.tmp` puis rename — jamais de fichier tronqué).
- Manifest `/sdcard/scenarios/manifest.json` : `[ { slug, id, installed_at } ]`. Re-download si absent ; la gestion de versions de scénario viendra quand l'API en exposera une (le FSD prévoit `version` + `signature` — hors périmètre, intégrité couverte par TLS).
- Intégration : le chargeur de scénario existant (composant `scenario` / `hal_storage`) doit pouvoir lister/choisir parmi `/sdcard/scenarios/` — à raccorder au menu (aujourd'hui : un scénario unique). Garder le fallback embarqué.
- Bloc `firmware_update` : loggé et ignoré jusqu'à F5.
- Pas de SD montée → sync des scénarios sauté avec WARN (le reste du sync — last_sync_at, version — passe quand même).

### F3 — Scores (web + firmware)

Côté web (manque au plan Phase 3) — `web/app/api/box/session/route.ts` :
- `POST` avec Bearer JWT box, body `{ scenario_id, score, duration_seconds, hints_used, completed }` (validation zod), insert `game_sessions` via client admin, renvoie `{ ok, session_token }` (pour le QR `/v/` plus tard).

Côté firmware :
- En fin de partie, le moteur de scénario pousse un event `SESSION_UPLOAD` avec les stats.
- **File offline** : append JSON-lines dans `/sdcard/pending_sessions.jsonl` ; chaque sync (ou upload direct si connecté) rejoue la file puis tronque les lignes acquittées. Une partie jouée sans réseau n'est jamais perdue.
- Vérif : partie complète → ligne en DB avec bon `device_id` ; partie Wi-Fi coupé → livrée au sync suivant.

### F4 — Provisioning BLE + register option B

**Firmware `ble_prov`** (NimBLE, prévu au FSD) :
- Service GATT custom, activé **sur demande** (entrée menu « appairage », fenêtre 5 min, LED témoin) et automatiquement au boot si NVS `wifi_creds` absente (box neuve). Jamais actif en jeu.
- Caractéristiques :
  - `box_uid` — read
  - `wifi_ssid`, `wifi_pass` — write only (jamais lisibles)
  - `status` — notify : `idle / connecting / wifi_ok(ip) / wifi_fail / registered`
  - `auth_challenge` — write ; `auth_response` — notify (la box signe via `hal_box_auth_sign`) → **preuve de possession**
- À réception ssid+pass : écrire NVS `wifi_creds`, reconnecter `hal_wifi`, notifier le résultat. BLE coupé à la fin de la fenêtre.

**Client : page `/devices/add`** (Web Bluetooth — Chrome desktop/Android ; ⚠ pas iOS Safari, acceptable FFF/beta, l'app mobile viendra après) :
1. Connexion BLE → lit `box_uid`
2. `GET /api/box/challenge?box_uid=…` → écrit le challenge en BLE → reçoit `auth_response`
3. Saisie Wi-Fi → écrit ssid/pass → attend `wifi_ok`
4. `POST /api/box/register { box_uid, name, challenge, challenge_response }`

**Serveur — register passe en option B** : `register/route.ts` exige `challenge + challenge_response`, vérifie via `verifyBoxHmac` + table `box_challenges` (non utilisé, non expiré → `used=true`, même mécanique que `/api/box/auth`). La dette « squat d'UID » du CLAUDE.md est soldée — mettre à jour CLAUDE.md à ce jalon.

- Vérif : effacer `wifi_creds` en NVS → parcours complet depuis un navigateur → box en Wi-Fi + ligne `devices` en DB ; rejouer le register avec un faux HMAC → 401.

### F5 — OTA

- Sync renvoie `firmware_update { version, url, sha256 }` → confirmation utilisateur via le menu (pas d'update silencieuse en pleine partie), puis `esp_https_ota` en streaming vers la partition passive.
- Vérifier le `sha256` annoncé sur l'image reçue avant `esp_ota_set_boot_partition`.
- Rollback déjà actif : au boot suivant, `cloud_client` marque l'app valide (`esp_ota_mark_app_valid_cancel_rollback`) **seulement après** un sync réussi — un firmware qui ne sait plus parler au serveur est rollback automatiquement.
- Hébergement des `.bin` : à trancher au moment de F5 (Supabase Storage bucket vs `web/public/firmware/`) ; la table `firmware_releases` existe déjà, insertion manuelle via Studio pour commencer.
- Vérif : flasher une version N, publier N+1 en DB, sync → update → reboot en N+1 marqué valide ; publier une image corrompue → rollback vers N.

---

## Points d'attention

- **RAM/flash** : NimBLE + Wi-Fi coexistent sur ESP32-S3 mais surveiller la heap interne (les buffers TLS doivent rester en interne, le download peut aller en PSRAM). Mesurer `esp_get_free_heap_size()` avant/après F4.
- **Priorités de tasks** : audio (I2S) et rendu yeux sont les chemins temps réel — la task `cloud_client` et l'host NimBLE en dessous. Vérifier l'absence de glitch audio pendant un download.
- **Secrets** : `wifi_pass` write-only en BLE, jamais loggé ; le JWT jamais persisté ; `BOX_MASTER_SECRET` toujours strictement côté serveur/provisioning.
- **`provision_box.py` reste le chemin usine** (secret box) ; le BLE ne provisionne que le Wi-Fi utilisateur + l'enregistrement. Les deux NVS namespaces (`box_creds`, `wifi_creds`) sont indépendants.

## Hors périmètre (plans ultérieurs)

- SNTP propre (F1 se contente de `server_time`), signature des scénarios, rate-limiting serveur (WB-11), app mobile (remplace Web Bluetooth pour iOS), canal OTA `beta`, page publique `/v/` et QR (plan web Phase 3).
