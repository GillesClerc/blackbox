# Bourns PTA (PTA6043) — potentiomètre à glissière

> _Synthèse de la datasheet Bourns PTA Series (fichier local [./Bourns-PTA6043.pdf](./Bourns-PTA6043.pdf))._

**Fabricant** : Bourns
**Catégorie** : Potentiomètre à glissière (fader) carbone, boîtier métal, bas profil
**Référence officielle** : série PTA — PTA6043 = course **60 mm**, sans cache-poussière, **simple piste**
**Datasheet source** : https://www.bourns.com (PTA Series)

## Codification (« How To Order »)

`PTA` + course (15/20/30/45/**60** mm) + cache-poussière (**4** = sans) + pistes
(**3** = simple, 4 = double) + style de pins + cran central + longueur de levier +
style de levier + courbe (A = audio, **B = linéaire**) + code de résistance.

## Caractéristiques

| Param | Valeur |
|---|---|
| Résistance | 1 kΩ – 1 MΩ |
| Tolérance | ±30 % (≤ 1 kΩ), ±20 % (1 kΩ–1 MΩ), ±30 % (≥ 1 MΩ) |
| Résistance résiduelle | 500 Ω ou 1 % max |
| Puissance linéaire 60 mm | 0,25 W (double : 0,125 W) |
| Tension max linéaire 20–60 mm | 200 V DC |
| Bruit de glissement | 100 mV max |
| Force de manœuvre | 30–250 gf |
| Durée de vie | 15 000 cycles |
| Température | −10 à +50 °C |
| Soudure manuelle | 350 °C max, 3 ± 0,5 s |

## Notes spécifiques projet

- Faders SL1–SL4 du panneau de contrôle (face Côté 1), lus en ratiométrique par l'ADS7830
  — voir `ADS7830.md`. Choisir la courbe **B (linéaire)**.
- Course de 60 mm : vérifier l'encombrement sur une face de cube de 120–150 mm.

## Sources

- Datasheet Bourns PTA Series (PDF local)
