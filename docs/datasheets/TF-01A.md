# TF-01A (slot microSD)

> _Fichier local [./TF-01A.pdf](./TF-01A.pdf) : la « datasheet » LCSC est un **plan mécanique en image** (1 page, texte non extractible). Les informations électriques ci-dessous viennent du symbole LCSC importé dans le schéma et de la documentation ESP-IDF — à vérifier sur le plan avant routage._

**Fabricant** : Korean Hroparts Elec (韩国韩荣)
**Catégorie** : Connecteur microSD (TF) CMS
**Référence officielle** : TF-01A (LCSC C91145)
**Datasheet source** : https://www.lcsc.com/datasheet/lcsc_datasheet_1811082127_Korean-Hroparts-Elec-TF-01A_C91145.pdf

## Package & Footprint

- **Empreinte** : `lcsc_footprints:C91145_TF-SMD_TF-01A`
- Dimensions et type d'éjection : voir le plan PDF (non extrait ici)

## Pinout (symbole LCSC)

| Pin | Nom SD | Nom SPI | Fonction |
|---|---|---|---|
| 1 | DAT2 | RSV | réservée en SPI |
| 2 | CD/DAT3 | CS | chip select |
| 3 | CMD | DI (MOSI) | données entrée carte |
| 4 | VDD | VDD | alimentation 3,3 V |
| 5 | CLK | SCLK | horloge |
| 6 | VSS | VSS | masse |
| 7 | DAT0 | DO (MISO) | données sortie carte |
| 8 | DAT1 | RSV | réservée en SPI |
| 9 | CD | — | contact de détection de carte |
| 10–13 | GND | — | masses mécaniques / blindage |

## Exigences côté hôte (ESP-IDF `sd_pullup_requirements.rst`)

En mode SPI ou SD 1 bit, **CMD et DAT0–DAT3 doivent être tirées au plus par 10 kΩ**, y
compris les lignes non reliées à l'hôte (DAT1, DAT2), pour éviter un mauvais état de la carte.

Bus partagé avec d'autres périphériques SPI (`sdspi_share.rst`) : initialiser la SD en
premier, CS des autres périphériques maintenus hauts ; la charge capacitive sur les lignes
(dont un condensateur sur SCLK) dégrade les timings de la carte.

## Notes spécifiques projet

- **J11** sur SPI2 : CS = GPIO47, MOSI = GPIO11, MISO = GPIO15, SCLK = GPIO12 (via R13
  22 Ω au pied de l'ESP32, C_CLK2 10 pF en DNP), VDD = 3V3_D. CD non connectée.
- ✅ Pull-ups 10 kΩ R15-R19 sur CS, CMD, DAT0, DAT1 et DAT2 (ajoutées le 2026-09-27).
- ✅ **Découplage au slot** (2026-11-15, audit A4) : **C_SD2 100 nF 0402 + C_SD1 10 µF 0805**
  entre VDD (4) et VSS (6). Pics de « plusieurs centaines de mA » pendant ~1 ms à l'accès flash,
  à tamponner par des condensateurs (note Swissbit « SD Memory Card Design-in », sans valeur
  chiffrée) ; 100 mA en moyenne sur 1 s en SPI (spécification SD v4.10). La valeur
  10 µF + 100 nF est un choix d'usage, pas une exigence fabricant.

## Sources

- Plan LCSC : https://www.lcsc.com/datasheet/lcsc_datasheet_1811082127_Korean-Hroparts-Elec-TF-01A_C91145.pdf
- ESP-IDF : `docs/en/api-reference/peripherals/sd_pullup_requirements.rst`, `sdspi_share.rst`
