# Blackbox — EscapeBox

Box physique d'escape game. Specs completes dans :
- `docs/escapebox-fsd.md` — hardware, phases, composants, architecture
- `docs/escapebox-vision.md` — vision produit
- `docs/plans/web-implementation.md` — plan web platform

## Environnement
- ESP-IDF v6.1
- ESP32-S3-WROOM-1-N16R8 (DevKitC-1) sur /dev/ttyACM0
- Flash 16 MB physique, configuree a 8 MB (partition custom 7.9 MB app)
- Container Docker : escapebox-dev (voir Dockerfile + start.sh)

## Commandes
- Build : `idf.py build` (depuis firmware/)
- Flash : `python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before default-reset --after hard-reset write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x10000 build/blackbox.bin`
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
- Pas de malloc() direct → utiliser les heap ESP-IDF si necessaire
- `ESP_ERROR_CHECK()` uniquement dans les fonctions `_init()` — partout ailleurs, verifier explicitement et retourner l'erreur (un timeout legitime appellerait `abort()`)
