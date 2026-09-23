# USBLC6-2 (USBLC6-2SC6)

> _Synthèse de la datasheet STMicroelectronics USBLC6-2 (fichier local [./USBLC6-2.pdf](./USBLC6-2.pdf), copie du PDF st.com)._

**Fabricant** : STMicroelectronics
**Catégorie** : Protection ESD très faible capacité pour USB 2.0 (2 lignes + VBUS)
**Référence officielle** : USBLC6-2SC6 (SOT23-6L)
**Datasheet source** : https://www.st.com/resource/en/datasheet/usblc6-2.pdf

## Vue d'ensemble

Réseau de diodes rail-to-rail + diode de clamp sur VBUS, capacité ligne–GND 2,5 pF typ,
conforme IEC 61000-4-2 niveau 4 (15 kV air, 8 kV contact).

## Package & Footprint

- **Package** : SOT23-6L (variante SOT-666 : USBLC6-2P6)
- **Empreinte LCSC** : C7519 (BOM projet)

## Pinout (SOT23-6L, flow-through)

| Pin | Nom | Fonction |
|---|---|---|
| 1 | I/O1 | Ligne 1 (entrée/sortie traversante avec pin 6) |
| 2 | GND | Masse |
| 3 | I/O2 | Ligne 2 (traversante avec pin 4) |
| 4 | I/O2 | Ligne 2 |
| 5 | VBUS | Rail d'alimentation protégé |
| 6 | I/O1 | Ligne 1 |

## Paramètres électriques

| Param | Valeur | Unité | Note |
|---|---|---|---|
| Tenue ESD | 15 kV air / 8 kV contact | — | IEC 61000-4-2 niveau 4 |
| V_BR (VBUS–GND) | 6 | V | I_R = 1 mA |
| V_CL (I/O–GND) | 12 / 17 | V | selon I_PP (1 A / 5 A, 8/20 µs) |
| C I/O–GND | 2,5 typ / 3,5 max | pF | V_R = 1,65 V |
| C I/O–I/O | 1,2 typ / 1,7 max | pF | |

## Notes de layout (datasheet)

Placer le composant **aussi près que possible de la source de perturbation**
(le connecteur), pistes courtes vers GND pour limiter l'inductance parasite.

## Notes spécifiques projet

- **U12** : I/O1 (1, 6) = USB_DM, I/O2 (3, 4) = USB_DP, VBUS (5) = 5V_USB, GND (2) → conforme ✓.
- Placé après les résistances série R11/R12 (côté ESP32) : au layout, le rapprocher du
  connecteur J1 conformément à la recommandation.

## Sources

- Datasheet officiel : https://www.st.com/resource/en/datasheet/usblc6-2.pdf
