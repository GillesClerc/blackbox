# ESP32-S3-WROOM-1 (module) — datasheet

> _Synthèse de l'« ESP32-S3-WROOM-1 & WROOM-1U Datasheet » v1.8 (fichier local [./ESP32-S3-WROOM-1-datasheet.pdf](./ESP32-S3-WROOM-1-datasheet.pdf)). SoC : [./ESP32-S3-datasheet.md](./ESP32-S3-datasheet.md) ; intégration PCB : [./ESP32-S3-WROOM-1-hardware-design-guidelines.md](./ESP32-S3-WROOM-1-hardware-design-guidelines.md)._

**Fabricant** : Espressif Systems
**Catégorie** : Module WiFi + BLE avec flash et PSRAM intégrées, antenne PCB
**Référence officielle** : ESP32-S3-WROOM-1-N16R8 (16 MB flash Quad SPI, 8 MB PSRAM Octal SPI)
**Datasheet source** : https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf

## Variante N16R8

- Flash 16 MB **Quad SPI**, PSRAM 8 MB **Octal SPI**, −40 à 65 °C ambiant.
- VDD_SPI **3,3 V** (seules les variantes N16R16VA sont en 1,8 V).
- **IO35, IO36, IO37 reliées à la PSRAM octale → indisponibles.**
- Strapping identique au SoC (GPIO0, GPIO3, GPIO45, GPIO46) — voir la synthèse SoC.

## Alimentation

| Param | Min | Typ | Max | Unité |
|---|---|---|---|---|
| VDD33 | 3,0 | 3,3 | 3,6 | V |
| Courant fourni par l'alimentation externe | 0,5 | — | — | A |

## Consommation (3,3 V, 25 °C, TX à 100 % de duty)

| Mode | Pic (mA) |
|---|---|
| TX 802.11b 1 Mbps @ 20,5 dBm | 355 |
| TX 802.11g 54 Mbps @ 18 dBm | 297 |
| TX 802.11n HT20 MCS7 @ 17,5 dBm | 286 |
| RX 802.11b/g/n HT20 | 95 |
| TX BLE @ 20 dBm / 9 dBm / 0 dBm | 344 / 202 / 187 |
| RX BLE | 93 |

La PSRAM intégrée peut augmenter ces valeurs (note de la datasheet sur les modes basse conso).

## Pinout utile (module 41 pads)

Pad 1/40/41 (EPAD) = GND, pad 2 = 3V3, pad 3 = EN. GPIO disponibles sur N16R8 : 0–21,
38–48 (19/20 = USB, 43/44 = UART0 par défaut, 0/3/45/46 = strapping).

## Notes spécifiques projet

- **U1** : C1 10 µF + C2 100 nF sur 3V3, EN 10 kΩ + 1 µF ✓ (voir guidelines).
- L'alimentation 3V3_D (AP2112K, 600 mA min) doit couvrir ≥ 0,5 A pour le module seul,
  plus les autres charges du rail — voir `AP2112.md`.

## Sources

- Datasheet officiel v1.8 : https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf
