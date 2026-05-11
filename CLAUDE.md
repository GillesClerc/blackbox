# Blackbox — EscapeBox Firmware

## Contexte projet
Box physique d'escape game électronique. ESP32 (Lolin) pour dev, cible finale ESP32-S3-WROOM-1-N16R8 (commandé AliExpress).
Voir docs/escapebox-vision.md et docs/escapebox-fsd.md pour le détail complet.

## Boîtier Phase 1 — Cube 6 faces actives (120×120×120mm)
| Face | Rôle | Composants clés |
|:-----|:-----|:----------------|
| Avant | Écran narratif principal | ILI9488 4" SPI, WS2812 border |
| Droite | Keypad capacitif | MPR121 (proto) / MTCH2120 (série), WS2812 |
| Gauche | Zone NFC | PN532 (antenne derrière bois fin), WS2812 anneau |
| Haut | Capteurs ambiance | VEML7700, BMP280 (souffle), LSM6DSOTR, Hall A3144E |
| Dos | Face révélation (victoire) | GC9A01 1.3" round (QR leaderboard), WS2812 celebration |
| Dessous | Technique | USB-C, SD card, interrupteur ON/OFF |

**Servo** : pas de servo en Phase 1 (scénarios digitaux — pas d'objets physiques).
La "face dos" s'active à la victoire : WS2812 + GC9A01 affiche QR → leaderboard.
Le servo est réintroduit en Phase 2 pour les scénarios avec pack physique (`hardware_required: [servo_main]` dans le YAML). Le driver MCPWM est déjà écrit.

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
    audio/        → PCM5122PW I2S DAC + I2C config ✅ (driver écrit, validé au branchement)
    nfc/          → PN532 I2C ✅ (driver écrit)
    servo/        → SG90 MCPWM ✅ (driver écrit)
    leds/         → WS2812B RMT ✅ (driver écrit, validé au branchement)
    sensors/      → Bus I2C + LSM6DSOTR, AS5600, VEML7700, MPR121 (proto) / MTCH2120 (série) ✅ (drivers écrits)
    scenario/     → Moteur JSON state machine ✅ (hints, variables, branch, do_fail)
    storage/      → SD card SPI + FAT ✅ (driver écrit)
```

## Statut Phase 1 (FSD §3.1)
- [x] Display ST7735 1.8" — driver SPI DMA 40MHz, plasma demo, boot screen animé
- [x] Moteur de scénario JSON — state machine FreeRTOS, hints temporisés, variables, branch, do_fail
- [x] Scénario "Capitaine Verdier" — YAML + JSON, simulateur d'events, 3 énigmes
- [x] Audio PCM5122PW — driver I2S + I2C config (validé au branchement du chip)
- [x] LEDs WS2812B — driver RMT (validé au branchement)
- [x] Capteurs I2C — LSM6DSOTR (IMU 6 axes), AS5600 (angle magn.), VEML7700 (lumière), MPR121 (touch proto) + MTCH2120 (touch série)
- [x] Outil YAML→JSON — tools/yaml2json.py, validation du format scénario
- [x] NFC PN532 I2C — driver écrit (à valider sur hardware)
- [x] Servo SG90 MCPWM — driver écrit (à valider sur hardware)
- [x] Filesystem SD — driver SPI+FAT écrit (à valider sur hardware)

## Capteurs I2C (backbone JST, bus I2C_NUM_0, SDA=21, SCL=22, 400kHz)
| Composant              | Adresse | Rôle                                      |
|:-----------------------|:--------|:------------------------------------------|
| MPR121 (Phase 1 proto) | 0x5A    | Tactile capacitif 12 canaux (breakout)    |
| MTCH2120 (Phase 2 PCB) | 0x28    | Tactile capacitif 12 canaux (sur PCB)     |
| AS5600                 | 0x36    | Encodeur magnétique 12 bits               |
| LSM6DSOTR              | 0x6A    | IMU 6 axes (accel + gyro)                 |
| VEML7700               | 0x10    | Lumière ambiante                          |
| PCM5122PW              | 0x4C    | DAC audio stéréo (I2C ctrl)               |

## Conventions code
- C pur ESP-IDF natif, zéro lib externe sauf LVGL (plus tard)
- Un composant = une responsabilité, un dossier dans components/
- Logs via ESP_LOGI/LOGW/LOGE avec TAG propre par composant
- Pas de malloc() direct → utiliser les heap ESP-IDF si nécessaire
- `ESP_ERROR_CHECK()` uniquement dans les fonctions `_init()` — partout ailleurs, vérifier explicitement et retourner l'erreur. Raison : un timeout légitime (ex. pas de tag NFC) appellerait `abort()` et crasherait le firmware.
- Les GPIO des servos (GPIO1/GPIO2) sont ceux de la cible ESP32-S3 (FSD §2.2.2). Sur le dev board ESP32 Lolin, GPIO1=UART0 TX et GPIO2=Display DC — ne pas tester les servos sur le Lolin, uniquement sur l'ESP32-S3.
