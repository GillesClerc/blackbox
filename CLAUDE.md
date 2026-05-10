# Blackbox — EscapeBox Firmware

## Contexte projet
Box physique d'escape game électronique. ESP32 (Lolin) pour dev, cible finale ESP32-S3.
Voir docs/escapebox-vision.md et docs/escapebox-fsd.md pour le détail complet.

## Environnement
- ESP-IDF v6.1
- ESP32 Lolin (dev) sur /dev/ttyUSB0
- Cible finale : ESP32-S3-WROOM-1-N16R8
- Container Docker : escapebox-dev (voir Dockerfile + start.sh)
- idf.py build/flash depuis /workspaces/blackbox/firmware/

## Commandes
- Build : `idf.py build` (depuis firmware/)
- Flash : `idf.py -p /dev/ttyUSB0 flash`
- Monitor : lancer depuis un terminal WSL2 (pas dispo dans Claude Code)
- Logs via pyserial : `python3 -c "import serial,time; s=serial.Serial('/dev/ttyUSB0',115200,timeout=0.5); time.sleep(2); print(s.read(4096).decode('utf-8','replace'))"`

## Notes USB / Docker
- Le device /dev/ttyUSB0 est exposé via usbipd (WSL2 → container)
- Le container tourne en --privileged
- Si ttyUSB0 non accessible : `docker exec -u root $(docker ps -q --filter ancestor=escapebox-dev) chmod 666 /dev/ttyUSB0`
- SPI à 40MHz nécessite le flag `SPI_DEVICE_NO_DUMMY` en ESP-IDF v6 (sinon ESP_ERR_NOT_SUPPORTED)

## Git
- Remote : git@github.com:GillesClerc/blackbox.git
- Commiter souvent avec des messages clairs
- Pusher après chaque feature stable

## Architecture firmware (firmware/)
```
firmware/
  main/           → app_main, point d'entrée
  components/
    display/      → ST7735 1.8" (SPI2, 40MHz, DMA) ✅ validé
    audio/        → MAX98357A I2S (à faire)
    nfc/          → PN532 I2C (à faire)
    servo/        → SG90 MCPWM (à faire)
    leds/         → WS2812 RMT (à faire)
    sensors/      → Bus I2C complet (à faire)
    scenario/     → Moteur YAML state machine (à faire — priorité)
    storage/      → SD card filesystem (à faire)
```

## Statut Phase 1 (FSD §3.1)
- [x] Display ST7735 1.8" — driver SPI DMA 40MHz, plasma demo
- [ ] Moteur de scénario YAML (state machine) ← priorité firmware
- [ ] Audio MAX98357A + I2S
- [ ] NFC PN532 I2C
- [ ] Servo SG90 MCPWM
- [ ] LEDs WS2812 RMT
- [ ] Bus I2C complet (MPR121, MPU6050, BMP280, AS5600, VEML7700)
- [ ] Filesystem SD

## Conventions code
- C pur ESP-IDF natif, zéro lib externe sauf LVGL (plus tard)
- Un composant = une responsabilité, un dossier dans components/
- Logs via ESP_LOGI/LOGW/LOGE avec TAG propre par composant
- Pas de malloc() direct → utiliser les heap ESP-IDF si nécessaire
- Toujours ESP_ERROR_CHECK() sur les retours de drivers critiques
