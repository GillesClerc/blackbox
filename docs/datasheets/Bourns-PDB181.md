# Bourns PDB18 (PDB181) — potentiomètre rotatif 17 mm

> _Synthèse de la datasheet Bourns PDB18 Series (fichier local [./Bourns-PDB181.pdf](./Bourns-PDB181.pdf))._

**Fabricant** : Bourns
**Catégorie** : Potentiomètre rotatif carbone 17 mm, simple ou double
**Référence officielle** : série PDB18 (PDB181 = simple section)
**Datasheet source** : https://www.bourns.com (PDB18 Series)

## Vue d'ensemble

Potentiomètre carbone, axe métal (lisse ou moleté 18 dents), crans optionnels (centre ou
multiples), courbes linéaire, audio ou audio inverse.

## Caractéristiques

| Param | Valeur |
|---|---|
| Résistance | 1 kΩ – 1 MΩ |
| Tolérance | ±30 % (≤ 1 kΩ), ±20 % (1 kΩ–1 MΩ), ±30 % (≥ 1 MΩ) |
| Résistance résiduelle | 1 % max |
| Puissance linéaire / audio | 0,2 W / 0,1 W (double : 0,125 / 0,06 W) |
| Tension max linéaire / audio | 200 V / 150 V |
| Bruit de glissement | 47 mV max |
| Angle mécanique | 300° ±5° |
| Couple de rotation / de cran | 10–150 gf·cm / 150–500 g·cm |
| Durée de vie | 15 000 cycles |
| Température | −10 à +50 °C |
| Soudure | 260 °C max, 3 s |
| Fixation | canon M7 × 0,75, écrou + rondelle fournis |

Brochage : 1 – 2 (curseur) – 3, axe représenté en butée anti-horaire.

## Notes spécifiques projet

- Pots rotatifs RV1–RV4 du panneau de contrôle (face Côté 1), lus en ratiométrique par
  l'ADS7830 (REFIN = 3V3) — voir `ADS7830.md`. Courbe **linéaire** pour une lecture ADC directe.
- Durée de vie 15 000 cycles, cohérente avec HW-11 (1000 parties).

## Sources

- Datasheet Bourns PDB18 Series (PDF local)
