# DW01A

> _Synthèse de la datasheet H&M Semiconductor DW01A (fichier local [./DW01A.pdf](./DW01A.pdf))._

**Fabricant** : Shenzhen H&M Semiconductor (seconde source de nombreux fabricants)
**Catégorie** : Circuit de protection batterie Li-ion/LiPo 1 cellule
**Référence officielle** : DW01A (SOT-23-6)
**Datasheet source** : https://hmsemi.com/downfile/DW01A.PDF

## Vue d'ensemble

Protège une cellule Li-ion/LiPo contre surcharge, décharge profonde, surintensité et
court-circuit en pilotant deux MOSFET N (ex. FS8205) sur le négatif du pack. Délais
internes (aucun condensateur de temporisation), Iq 3 µA.

## Package & Footprint

- **Package** : SOT-23-6

## Pinout

| Pin | Nom | Fonction |
|---|---|---|
| 1 | OD | Grille du MOSFET de contrôle de décharge |
| 2 | CS | Entrée de mesure de courant / détection chargeur |
| 3 | OC | Grille du MOSFET de contrôle de charge |
| 4 | TD | Pin de test (réduction des délais) — non connectée en application |
| 5 | VCC | Alimentation, via une résistance R1 |
| 6 | GND | Masse (négatif cellule) |

## Paramètres électriques (Ta = 25 °C)

| Param | Min | Typ | Max | Unité |
|---|---|---|---|---|
| Courant d'alimentation (VCC 3,6 V) | — | 3,0 | 6,0 | µA |
| Courant en power-down (VCC 1,8 V) | — | — | 4 | µA |
| Seuil de surcharge V_OCP | 4,25 | 4,30 | 4,35 | V |
| Relâchement surcharge V_OCR | 4,05 | 4,10 | 4,15 | V |
| Seuil de décharge profonde V_ODP | 2,30 | 2,40 | 2,50 | V |
| Relâchement décharge V_ODR | 2,90 | 3,00 | 3,10 | V |
| Seuil de surintensité V_OI1 | 120 | 150 | 180 | mV |
| Seuil de court-circuit V_OI2 (VCC 3,0 V) | 1,0 | — | 1,4 | V |
| Délai surcharge T_OC | — | 80 | 200 | ms |
| Délai décharge T_OD | — | 40 | 200 | ms |
| Délai surintensité T_OI1 | — | 10 | 20 | ms |
| Délai court-circuit T_OI2 | — | 5 | 50 | µs |
| Abs max VDD–VSS | −0,3 | — | 10 | V |

V_OI1 et V_OI2 sont mesurés sur CS, donc aux bornes des deux MOSFET en série :
`I_déclenchement ≈ V_OI1 / (2 × R_DS(on))`.

## Application typique (datasheet p. 3)

R1 = 100 Ω (VCC depuis BATT+), C1 = 0,1 µF (VCC–GND), R2 = 1 kΩ (CS), M1/M2 = double
MOSFET N en série sur BATT−, TD non connectée.

## Notes spécifiques projet

- **U9** + **U10** (FS8205) : R_VCC = 100 Ω, C_U9 = 0,1 µF, R_CS = 1 kΩ, TD NC —
  conforme à l'application typique ✓.
- Avec le FS8205 (R_DS(on) ≤ 28 mΩ à V_GS 4,5 V, ≤ 37 mΩ à 2,5 V) : déclenchement de
  surintensité ≈ 1,6–3,2 A selon la dispersion et la tension cellule → **limite le courant
  crête total du système** (boost 5 V + LDO).
- Surcharge 4,30 V > fin de charge bq24075 4,20 V ✓ (protection de secours uniquement).

## Sources

- Datasheet : https://hmsemi.com/downfile/DW01A.PDF
