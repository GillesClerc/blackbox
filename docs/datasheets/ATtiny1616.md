# ATtiny1616 / ATtiny3216 (Microchip, tinyAVR 1-series) — MCU avec contrôleur tactile PTC

> _Synthèse de la datasheet DS40002001 (2019, fichier local [./ATtiny1616.pdf](./ATtiny1616.pdf),
> miroir LCSC de C507118), lue le 2026-09-27. Évalué comme « contrôleur tactile programmable »._

**Catégorie** : microcontrôleur AVR 8 bits, 16 Ko (1616) / 32 Ko (3216) de flash, 20 MHz
**Boîtiers** : SOIC-20, **VQFN-20 3 × 3 mm, pas 0,40 mm** (dessin du boîtier)
**LCSC / JLCPCB (27/09/2026)** : ATTINY1616-MNR (VQFN-20) **C507118**, 1,04 $, stock **11 316** ;
ATTINY1616-SN (SOIC-20) C614136, 2,89 $, stock 33 ; ATTINY3216-SNR (SOIC-20) C609661, 3,10 $, stock 75

## Points vérifiés (vue d'ensemble, tableau de synthèse des périphériques)

- **Peripheral Touch Controller (PTC)** : boutons, curseurs, molettes, surfaces 2D ; réveil sur
  toucher ; **blindage actif** (driven shield) contre l'humidité et le bruit.
- **Jusqu'à 12 canaux en capacité propre, 36 en capacité mutuelle** (matrice X/Y : chaque
  ligne PTC peut être X ou Y) → un **clavier 3 × 4 = 12 touches ne demande que 7 broches** en mutuel.
- Le PTC **prend le contrôle de l'ADC0** pendant son utilisation (note 1).
- **TWI (I2C) maître et esclave**, avec double correspondance d'adresse : l'adresse esclave
  est **libre** (registre TWIn.SADDR) → pas de conflit avec les CAP12xx (0x28).
- Alimentation **1,8 à 5,5 V** (0-5 MHz à 1,8 V).
- Programmation et débogage par **UPDI, une seule broche**.

## Notes projet

- Le tactile se programme avec la **bibliothèque QTouch de Microchip** (générée par MPLAB) :
  vérifier sa licence avant de l'adopter (règle « licences » du projet).
- Suppose un **second firmware** (sur le satellite), à flasher en production (pastilles
  UPDI), sans mise à jour à distance sauf si l'on écrit un chargeur par I2C.
