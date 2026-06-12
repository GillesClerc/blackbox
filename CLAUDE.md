# Blackbox — EscapeBox

Box physique d'escape game. Specs completes dans :
- `docs/escapebox-fsd.md` — hardware, phases, composants, architecture
- `docs/escapebox-vision.md` — vision produit
- `docs/plans/web-implementation.md` — plan web platform
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

## Conventions code
- C pur ESP-IDF natif. Libs externes : minimp3 (decodeur MP3), esp_lcd_gc9a01 (driver yeux), assets Uncanny Eyes Adafruit MIT (components/ui_manager/data/defaultEye.h). LVGL non utilise actuellement (dependance conservee pour usage futur).
- Un composant = une responsabilite, un dossier dans components/
- Logs via ESP_LOGI/LOGW/LOGE avec TAG propre par composant
- Pas de malloc() direct → utiliser les heap ESP-IDF si necessaire. Pour les buffers chauds (audio, rendu), allouer une fois en static au boot, ne jamais malloc dans le chemin run-time.
- `ESP_ERROR_CHECK()` uniquement dans les fonctions `_init()` **appelees au boot depuis `app_main`**. Si un `_init()` est appele depuis un task runtime (hot-plug, retry I2C, etc.), verifier explicitement et retourner l'erreur — `ESP_ERROR_CHECK` y appellerait `abort()` et crasherait le systeme entier.
- Pas de `portMAX_DELAY` sur les `xSemaphoreTake` / `xQueueReceive` dans les chemins de run-time : utiliser un timeout en ticks (typiquement `pdMS_TO_TICKS(200-500)`) et logger les timeouts. `portMAX_DELAY` n'est pas detecte par le TWDT et peut figer un task indefiniment.
- Donnees partagees entre tasks sur dual-core : `volatile` ne suffit pas pour la coherence memoire SMP. Utiliser `<stdatomic.h>` (`atomic_store`/`atomic_load`) ou une section critique (`portENTER_CRITICAL`).
