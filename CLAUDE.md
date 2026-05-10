# Blackbox — EscapeBox Firmware

## Contexte projet
Box physique d'escape game électronique. ESP32 standard pour dev, cible finale ESP32-S3.
Voir docs/vision.md et docs/fsd.md pour le détail complet.

## Environnement
- ESP-IDF v6.1
- ESP32 dev sur /dev/ttyUSB0
- Cible finale : ESP32-S3

## Commandes
- Build : idf.py build
- Flash : idf.py -p /dev/ttyUSB0 flash
- Monitor : idf.py -p /dev/ttyUSB0 monitor
- Flash + monitor : idf.py -p /dev/ttyUSB0 flash monitor

## Git
- Remote : git@github.com:gilles/blackbox.git
- Commiter souvent avec des messages clairs
- Pusher après chaque feature stable

## Architecture firmware
- /components → un dossier par composant (nfc, servo, audio, display...)
- /firmware → main app
- /docs → vision.md, fsd.md

## Conventions code
- C pur ESP-IDF natif
- Un composant = une responsabilité
- Logs via ESP_LOGI/LOGW/LOGE
