# Blackbox — EscapeBox

Box physique d'escape game. Specs completes dans :
- `docs/escapebox-fsd.md` — hardware, phases, composants, architecture
- `docs/escapebox-vision.md` — vision produit
- `docs/plans/web-implementation.md` — plan web platform
- `docs/plans/firmware-cloud-client.md` — plan client cloud firmware (auth/sync/packages d'assets faits ; reste mixer audio, BLE, OTA)
- `docs/audits/` — audits datés (constats, plan de correction, points restants). Derniers : `2026-09-23-audit.md` (logiciel), `2026-09-23-hardware-db.md` (KiCad Main + DB)
- `docs/datasheets/` — PDF officiels + synthese markdown par composant (pinout, registres, sequence d'init, exemple ESP-IDF, drivers existants). Skill `/component-research <REF>` pour en ajouter un.

## Regle datasheets (obligatoire)
- **Avant tout travail sur un composant** (code driver/HAL, schema KiCad, audit, revue, conseil, reponse a une question), **lire sa synthese `docs/datasheets/<composant>.md`**, et le PDF a cote pour tout point absent ou douteux de la synthese.
- **Ne jamais donner un retour, une recommandation ou un chiffre sans l'avoir verifie dans la datasheet** (ni de memoire, ni sur une datasheet d'une autre variante — ex. WS2812B V5 ≠ WS2812B ancienne). Citer la source (section/table) dans le retour.
- Composant sans synthese : telecharger le PDF officiel dans `docs/datasheets/` et creer le `.md` (template du skill component-research) **avant** de conclure. Toute valeur relevee dans un PDF et utile au projet est reportee dans la synthese.
- Lire aussi `docs/escapebox-fsd.md` (spec, pinout, decisions) et `docs/escapebox-vision.md` pour le contexte produit.

## Environnement
- ESP-IDF v6.1
- ESP32-S3-WROOM-1-N16R8 (DevKitC-1) sur /dev/ttyACM0
- Flash 16 MB — partitions OTA (factory + ota_0 + ota_1 de 3 MB, storage LittleFS 6.9 MB), rollback active ; `box_nvs` (0x12000, 24 Ko) = identite box isolee de la NVS applicative (jamais effacee)
- PSRAM octal 8 MB activee (CONFIG_SPIRAM_MODE_OCT)
- Carte SD sur SPI2 (CS=47, module 5V), montee FAT sur /sdcard — scenario + ambient.mp3 charges depuis SD, fallback embarque
- Container Docker : escapebox-dev (voir Dockerfile + start.sh)

## Commandes
- Build : `idf.py build` (depuis firmware/)
- Flash complet (premiere fois ou si table de partitions change) : `python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before default-reset --after hard-reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0xf000 build/ota_data_initial.bin 0x20000 build/blackbox.bin`
- Reflash app seule (cas courant) : meme commande avec uniquement `0x20000 build/blackbox.bin`
- Tests host (sans cible) : `firmware/test_host/run.sh` (validateur de scenario, ASan/UBSan), `python3 tools/test_box_crypto.py` (vecteur crypto aligne sur le serveur)
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
- **Livraison des scenarios (dette soldee, F3.2)** : packages dans `web/scenario-packages/` (hors `public/`), servis par `GET /api/box/pkg/[slug]/[...path]` — JWT box + droit `device_scenarios` + path assaini. Publier un scenario : `python3 tools/package_scenario.py <src_dir> --slug <slug>` (genere le manifest sha256/version) puis commit + bump `scenarios.version` en DB.
- **Enregistrement box (dette soldee, F5)** : `/api/box/register` exige desormais la **preuve de possession** (option B) — challenge + HMAC signe par la box via BLE pendant l'appairage (`/devices/add`, Web Bluetooth). Plus de squat d'UID possible. Provisioning BLE : composant `ble_prov` (service GATT e5c40001-…), declencheurs = boot sans `wifi_creds` ou item « Appairage » du menu de boot (fenetre 5 min).

## Auth box (provisioning + firmware)
- Secret par box : `box_secret = HKDF-SHA256(BOX_MASTER_SECRET, info="escapebox:<box_uid>", 32)`. Le `BOX_MASTER_SECRET` reste cote serveur (jamais embarque ni commite). Crypto partagee host↔serveur dans `tools/box_crypto.py`.
- `box_uid` derive de la MAC eFuse : `ESP32S3-XXXX-XXXX`.
- Provisioning : `BOX_MASTER_SECRET=<hex> python3 tools/provision_box.py --port /dev/ttyACM0 [--flash]`. Dry-run par defaut ; `--flash` n'ecrit que la partition `box_nvs` (namespace `box_creds`) — la NVS applicative est preservee (`--wifi-ssid` ecrit en plus une image nvs, qui l'efface). Prerequis : table de partitions avec `box_nvs` sur la box (flash complet une fois). Appairer ensuite depuis `/devices/add`.
- Firmware `hal_box_auth` : lit `box_creds` dans `box_nvs` (migre au boot l'ancien emplacement de la NVS applicative ; ancienne table → lecture legacy + warning). `hal_box_auth_sign(purpose, challenge, ...)` signe `"<purpose>:<box_uid>:<challenge>"` en HMAC-SHA256 (PSA crypto — l'API HMAC de `mbedtls/md.h` est privee dans mbedTLS 4 / ESP-IDF v6.1).
- **Separation de domaine des signatures** : `purpose` = `auth` (`/api/box/auth`, par `cloud_client`) ou `register` (`/api/box/register`, preuve BLE). Le canal BLE n'est pas authentifie → `ble_prov` ne signe QUE `register` ; une preuve d'appairage ne donne jamais de JWT. Garder firmware / `web/lib/box-auth.ts` (`verifyBoxHmac(purpose, ...)`) / `tools/box_crypto.py` alignes — toute evolution du format = rupture a deployer des deux cotes ensemble.
- Flux reseau box↔cloud : `cloud_client` (challenge→auth→sync HTTPS) **valide sur cible** : packages d'assets versionnes installes sur SD (manifest sha256/fichier, install incrementale avec reprise, download authentifie) — jalons F1-F3 du plan `docs/plans/firmware-cloud-client.md`. API URL via `CONFIG_ESCAPEBOX_API_URL`, surcharge NVS `cloud/api_url`. Reste : BLE + option B (F5), OTA (F6) ; scores optionnels. Audio : mixer 4 voix dans `hal_audio` (task `audio_mixer` seule ecrivaine I2S) — `"play": "<nom>"` d'un scenario joue `audio/<nom>.mp3` du package en one-shot ducke par-dessus le bg, fallback tons builtin.

## Conventions code
- C pur ESP-IDF natif. Libs externes : minimp3 (decodeur MP3), esp_lcd_gc9a01 (driver yeux), assets Uncanny Eyes Adafruit MIT (components/ui_manager/data/defaultEye.h). LVGL non utilise actuellement (dependance conservee pour usage futur).
- Un composant = une responsabilite, un dossier dans components/
- Logs via ESP_LOGI/LOGW/LOGE avec TAG propre par composant
- Pas de malloc() direct → utiliser les heap ESP-IDF si necessaire. Pour les buffers chauds (audio, rendu), allouer une fois en static au boot, ne jamais malloc dans le chemin run-time.
- `ESP_ERROR_CHECK()` uniquement dans les fonctions `_init()` **appelees au boot depuis `app_main`**. Si un `_init()` est appele depuis un task runtime (hot-plug, retry I2C, etc.), verifier explicitement et retourner l'erreur — `ESP_ERROR_CHECK` y appellerait `abort()` et crasherait le systeme entier.
- Pas de `portMAX_DELAY` sur les `xSemaphoreTake` / `xQueueReceive` dans les chemins de run-time : utiliser un timeout en ticks (typiquement `pdMS_TO_TICKS(200-500)`) et logger les timeouts. `portMAX_DELAY` n'est pas detecte par le TWDT et peut figer un task indefiniment.
- Peripheriques optionnels au boot : un capteur/afficheur/DAC absent ne doit jamais faire aborter `app_main` (reboot en boucle) — tester le retour, logger, continuer sans (cf. ecran et audio dans `main.c`).
- Scenario JSON : toute nouvelle cle lue par le moteur doit etre ajoutee a `scenario_validate.c` (+ cas dans `firmware/test_host/`) et a `tools/yaml2json.py`.
- Donnees partagees entre tasks sur dual-core : `volatile` ne suffit pas pour la coherence memoire SMP. Utiliser `<stdatomic.h>` (`atomic_store`/`atomic_load`) ou une section critique (`portENTER_CRITICAL`).
