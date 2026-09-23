# ESP32-S3 (SoC) — datasheet

> _Synthèse de l'« ESP32-S3 Series Datasheet » v2.2 (fichier local [./ESP32-S3-datasheet.pdf](./ESP32-S3-datasheet.pdf)). Ne couvre que ce qui concerne notre carte ; module : [./ESP32-S3-WROOM-1-datasheet.md](./ESP32-S3-WROOM-1-datasheet.md) ; intégration : [./ESP32-S3-WROOM-1-hardware-design-guidelines.md](./ESP32-S3-WROOM-1-hardware-design-guidelines.md)._

**Fabricant** : Espressif Systems
**Catégorie** : SoC WiFi 2,4 GHz + Bluetooth LE, double cœur Xtensa LX7 240 MHz
**Référence** : puce ESP32-S3R8 (dans le module WROOM-1-N16R8)
**Datasheet source** : https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf

## Strapping pins (§3 Boot Configurations)

Lues au reset (latchées), puis GPIO normales. Valeur par défaut = pull interne si la broche
n'est reliée à rien (ou à un circuit haute impédance).

| Pin | Rôle | Défaut | Bit |
|---|---|---|---|
| GPIO0 | Mode de boot (avec GPIO46) | pull-up faible | 1 |
| GPIO3 | Source JTAG (seulement si EFUSE_STRAP_JTAG_SEL = 1) | **flottante, pas de pull interne** | — |
| GPIO45 | Tension VDD_SPI | pull-down faible | 0 |
| GPIO46 | Mode de boot, affichage messages ROM | pull-down faible | 0 |

**Mode de boot** : GPIO0 = 1 → boot SPI (GPIO46 indifférent) ; GPIO0 = 0 **et** GPIO46 = 0 →
download (USB-Serial-JTAG, USB-OTG, UART).

**VDD_SPI** (eFuse vierges : EFUSE_VDD_SPI_FORCE = 0) : GPIO45 = 0 → **3,3 V** (défaut) ;
GPIO45 = 1 → 1,8 V. Avec EFUSE_VDD_SPI_FORCE = 1, GPIO45 est ignorée et la tension suit
EFUSE_VDD_SPI_TIEH (1 → 3,3 V).

**GPIO3 / JTAG** : ignorée tant que les eFuses JTAG sont vierges (défaut).

**Timing** : t_SU ≥ 0 ms, t_H ≥ 3 ms (strapping stable 3 ms après la montée de CHIP_PU).

## Caractéristiques électriques (§5)

| Param | Min | Typ | Max | Unité | Note |
|---|---|---|---|---|---|
| Alimentation (abs max) | −0,3 | — | 3,6 | V | |
| VDD3P3 / VDDA / RTC / CPU | 3,0 | 3,3 | 3,6 | V | |
| Courant fourni par l'alimentation | 0,5 | — | — | A | « 500 mA ou plus recommandé » |
| Courant IO cumulé | — | — | 1500 | mA | abs max |
| V_IH | 0,75 × VDD | — | VDD + 0,3 | V | |
| V_IL | −0,3 | — | 0,25 × VDD | V | |
| V_OH / V_OL | 0,8 × VDD / — | — | — / 0,1 × VDD | V | charge haute impédance |
| I_OH (VDD 3,3 V, drive 3) | — | 40 | — | mA | |
| I_OL (VDD 3,3 V, drive 3) | — | 28 | — | mA | |
| Pull-up / pull-down internes | — | 45 / 45 | — | kΩ | |
| CHIP_PU : V_IH_nRST / V_IL_nRST | 0,75 × VDD / — | — | — / 0,25 × VDD | V | |

⚠ Lors d'une **programmation d'eFuse**, VDD3P3_CPU ne doit pas dépasser 3,3 V.

## Notes spécifiques projet

- GPIO45 (BTN1) et GPIO46 (BTN2) **laissées flottantes = conforme** (pull-down internes →
  VDD_SPI 3,3 V et boot SPI). **Ne jamais ajouter de pull-up externe sur GPIO45.** Un bouton
  est possible s'il est actif haut (vers 3V3) et relâché au reset.
- Option pour libérer GPIO45 : programmer EFUSE_VDD_SPI_FORCE = 1 + EFUSE_VDD_SPI_TIEH = 1
  (`espefuse.py set_flash_voltage 3.3V`, irréversible, alimentation ≤ 3,3 V pendant l'écriture).
- GPIO3 (VBAT_SENSE) : strapping JTAG ignoré (eFuses vierges) ✓.
- Les entrées ESP32 exigent 0,75 × 3,3 V ≈ 2,5 V au niveau haut.

## Sources

- ESP32-S3 Series Datasheet v2.2 : https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
