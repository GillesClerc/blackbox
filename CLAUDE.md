# Blackbox — EscapeBox

Box physique d'escape game. Specs completes dans :
- `docs/escapebox-fsd.md` — hardware, phases, composants, architecture
- `docs/escapebox-vision.md` — vision produit
- `docs/plans/web-implementation.md` — plan web platform
- `docs/plans/firmware-cloud-client.md` — plan client cloud firmware (auth/sync/scores, BLE provisioning, OTA)
- `docs/datasheets/` — PDF officiels + synthese markdown par composant (pinout, registres, sequence d'init, exemple ESP-IDF, drivers existants). Consulter avant de coder un nouveau peripherique. Skill `/component-research <REF>` pour en ajouter un.

## Environnement
- ESP-IDF v6.1
- ESP32-S3-WROOM-1-N16R8 (DevKitC-1) sur /dev/ttyACM0
- Flash 16 MB — partitions OTA (factory + ota_0 + ota_1 de 3 MB, storage LittleFS 6.9 MB), rollback active
- PSRAM octal 8 MB activee (CONFIG_SPIRAM_MODE_OCT)
- Carte SD sur SPI2 (CS=47, module 5V), montee FAT sur /sdcard — scenario + ambient.mp3 charges depuis SD, fallback embarque
- Container Docker : escapebox-dev (voir Dockerfile + start.sh)

## Commandes
- Build : `idf.py build` (depuis firmware/)
- Flash complet (premiere fois ou si table de partitions change) : `python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before default-reset --after hard-reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0xf000 build/ota_data_initial.bin 0x20000 build/blackbox.bin`
- Reflash app seule (cas courant) : meme commande avec uniquement `0x20000 build/blackbox.bin`
- Monitor : lancer depuis un terminal WSL2 (pas dispo dans Claude Code)
- Logs : `python3 -c "import serial,time; s=serial.Serial('/dev/ttyACM0',115200,timeout=0.5); time.sleep(2); print(s.read(4096).decode('utf-8','replace'))"`

## Notes USB / Docker
- /dev/ttyACM0 expose via usbipd (WSL2 → container --privileged)
- Si non accessible : `docker exec -u root $(docker ps -q --filter ancestor=escapebox-dev) chmod 666 /dev/ttyACM0`

## Git
- Remote : git@github.com:GillesClerc/blackbox.git
- Commiter souvent avec des messages clairs, sans co-authored by claude
- Pusher apres chaque feature stable

## Web platform (web/)
- Next.js 16 + Supabase self-hosted (Coolify). Landing + auth + API box. Plan : `docs/plans/web-implementation.md`.
- API box (`web/app/api/box/`) : challenge → auth (HMAC HKDF par box_uid → JWT 2h) → sync. Secret par box derive du `BOX_MASTER_SECRET` serveur (jamais embarque) ; routes box via client service_role (`lib/supabase/admin.ts`), la box n'est pas un user Supabase.
- ⚠ **DETTE — livraison des scenarios** : les JSON sont servis en statique public depuis `web/public/scenarios/` (aucune auth — quiconque devine l'URL telecharge). OK pour le FFF (scenario unique, gratuit de fait). **AVANT la mise en vente (Phase 4 Stripe)**, passer a une livraison controlee : route API qui verifie le JWT box + le droit `device_scenarios` avant de streamer, ou Supabase Storage avec URLs signees courtes renvoyees par le sync.
- ⚠ **DETTE — enregistrement box (`/api/box/register`)** : on est parti sur **l'option A (claim simple)** — un utilisateur connecte revendique un `box_uid`, sans preuve que la box est en sa possession. Suffisant pour le FFF (Gilles seul enregistre ses propres box). **AVANT toute inscription multi-utilisateur publique**, passer a **l'option B (claim + preuve HMAC)** : la box signe un challenge pendant le provisioning BLE pour prouver qu'elle detient son secret derive. Sinon squat possible (revendiquer un UID qu'on ne possede pas → scores/sync detournes). L'option B necessitera de cabler `hal_box_auth_sign` dans le flux de provisioning BLE.

## Auth box (provisioning + firmware)
- Secret par box : `box_secret = HKDF-SHA256(BOX_MASTER_SECRET, info="escapebox:<box_uid>", 32)`. Le `BOX_MASTER_SECRET` reste cote serveur (jamais embarque ni commite). Crypto partagee host↔serveur dans `tools/box_crypto.py`.
- `box_uid` derive de la MAC eFuse : `ESP32S3-XXXX-XXXX`.
- Provisioning : `BOX_MASTER_SECRET=<hex> python3 tools/provision_box.py --port /dev/ttyACM0 [--flash]`. Dry-run par defaut ; `--flash` ecrit la NVS `box_creds` (efface les autres namespaces nvs → faire au premier provisioning). Enregistrer ensuite le `box_uid` via `POST /api/box/register`.
- Firmware `hal_box_auth` : lit `box_creds` en NVS, `hal_box_auth_sign(challenge, ...)` signe `"<box_uid>:<challenge>"` en HMAC-SHA256 (PSA crypto — l'API HMAC de `mbedtls/md.h` est privee dans mbedTLS 4 / ESP-IDF v6.1).
- Flux reseau box↔cloud : `cloud_client` (challenge→auth→sync HTTPS) **valide sur cible**, scenarios telecharges sur SD + manifest (jalons F1+F2 du plan `docs/plans/firmware-cloud-client.md`). API URL via `CONFIG_ESCAPEBOX_API_URL`, surcharge NVS `cloud/api_url`. Reste : scores (F3), BLE + option B (F4), OTA (F5).

## Conventions code
- C pur ESP-IDF natif. Libs externes : minimp3 (decodeur MP3), esp_lcd_gc9a01 (driver yeux), assets Uncanny Eyes Adafruit MIT (components/ui_manager/data/defaultEye.h). LVGL non utilise actuellement (dependance conservee pour usage futur).
- Un composant = une responsabilite, un dossier dans components/
- Logs via ESP_LOGI/LOGW/LOGE avec TAG propre par composant
- Pas de malloc() direct → utiliser les heap ESP-IDF si necessaire. Pour les buffers chauds (audio, rendu), allouer une fois en static au boot, ne jamais malloc dans le chemin run-time.
- `ESP_ERROR_CHECK()` uniquement dans les fonctions `_init()` **appelees au boot depuis `app_main`**. Si un `_init()` est appele depuis un task runtime (hot-plug, retry I2C, etc.), verifier explicitement et retourner l'erreur — `ESP_ERROR_CHECK` y appellerait `abort()` et crasherait le systeme entier.
- Pas de `portMAX_DELAY` sur les `xSemaphoreTake` / `xQueueReceive` dans les chemins de run-time : utiliser un timeout en ticks (typiquement `pdMS_TO_TICKS(200-500)`) et logger les timeouts. `portMAX_DELAY` n'est pas detecte par le TWDT et peut figer un task indefiniment.
- Donnees partagees entre tasks sur dual-core : `volatile` ne suffit pas pour la coherence memoire SMP. Utiliser `<stdatomic.h>` (`atomic_store`/`atomic_load`) ou une section critique (`portENTER_CRITICAL`).
