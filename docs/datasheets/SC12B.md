# SC12B (ICMAN) — contrôleur tactile capacitif 12 touches, I2C

> ⚠ **Écarté le 2026-09-27** (choix de Gilles : CAP1298). Synthèse conservée pour référence.

> _Synthèse du « SC12B 规格书 v1.1 » (ICMAN, en chinois ; fichier local [./SC12B.pdf](./SC12B.pdf),
> miroir LCSC de C718973), lu le 2026-09-27._

**Fabricant** : ICMAN Technology (Shenzhen)
**Catégorie** : 12 touches capacitives indépendantes, auto-calibration, sortie I2C ou BCD
**Référence** : SC12B, SSOP-24 (pas 0,635 mm) — LCSC / JLCPCB **C718973** (Extended),
0,51 $ à l'unité, stock JLCPCB **1 289** (27/09/2026)
**Datasheet source** : https://datasheet.lcsc.com/lcsc/2008121206_ICMAN-Tech-SC12B_C718973.pdf

## Brochage (tableau 1-1)

| Pin | Nom | Fonction | Si inutilisée |
|---|---|---|---|
| 1 | GND | Masse | — |
| 2 | CMOD | Condensateur de collecte de charge (valeur fixe, sans effet sur la sensibilité) | — |
| 3 | CDC | Condensateur de sensibilité, **5 à 100 pF** (plus petit = plus sensible) | — |
| 4-15 | CIN0-CIN11 | Électrodes | en l'air |
| 16-19 | BCD3-BCD0 | Sortie code BCD (touche prioritaire) | en l'air |
| 20 | ASEL | Sélection d'adresse I2C | en l'air |
| 21 | SCL | Horloge I2C | GND ou VDD |
| 22 | SDA | Données I2C (pull-up interne faible) | GND, VDD ou en l'air |
| 23 | INT | Sortie « touche » : **haut si une touche est touchée** | — |
| 24 | VDD | Alimentation | — |

## Paramètres électriques (tableau 4-1, 25 °C)

| Param | Valeur | Note |
|---|---|---|
| VDD | 2,5 à 6,5 V (max absolu 6,0 V, §4.1 — incohérence de la datasheet ; 3V3 : aucun problème) | |
| Courant | 0,65 mA à 3,3 V ; 20 µA en veille à 3,3 V | |
| Init. après mise sous tension | **300 ms** (mesure de la capacité à vide) | ne pas toucher pendant ce temps |
| Capacité d'électrode | ≤ 2,5 × C_DC | |
| Variation minimale détectée | 0,2 pF (C_DC = 15 pF) | |
| I2C | **400 kbit/s max** (pull-up 10 kΩ) | |
| Échantillonnage | 12,5 ms par cycle ; appui détecté ≈ 68 ms, relâchement ≈ 44 ms (§2.4) | réglable (RTM[1:0], CTRL0) |
| Température | −40 à +85 °C | |

- C_DC : condensateur **NPO/C0G ou film, ±10 % ou mieux**, placé **au plus près** du circuit (§2.2).
- Schéma d'application (§3.1) : une résistance série de l'ordre de 3 kΩ par électrode ; valeurs de
  CMOD et CDC à relire sur la figure (image) avant de figer le schéma.

## Interface I2C (§3.4)

- **Adresse (7 bits) selon ASEL** (tableau 3-3) : ASEL au **VDD → 0x44**, **en l'air → 0x40**,
  **à GND → 0x42**. Aucune collision avec le bus actuel (0x10, 0x20, 0x35, 0x48, 0x4C, 0x53,
  0x57, 0x6A, 0x76).
- Écriture : adresse, registre, données (auto-incrément). Lecture : écrire le registre puis STOP,
  puis lecture séquentielle.
- **Lecture simplifiée** : le pointeur par défaut est 0x08 → une simple lecture de 2 octets donne
  l'état des touches.

## Registres (tableau 3-4)

| Adr | Nom | Accès | Défaut | Contenu |
|---|---|---|---|---|
| 0x00 | SenSet0 | W | 0111 1001 | Sensibilité canal 0 (SENCH0) |
| 0x01 | SenSetCOM | W | 0111 1001 | Sensibilité commune (SENCOM) |
| 0x02 | CTRL0 | W | 1000 0011 | SLPCYC[2:0], SLPNOW, HOLD, KVF, RTM[1:0] |
| 0x03 | CTRL1 | W | 0000 1000 | CSEL[3:0] (canal échantillonné) |
| 0x08 | Output1 | R | 0 | bit 7 = CIN0 … bit 0 = CIN7 (1 = touché) |
| 0x09 | Output2 | R | 0 | bit 7 = CIN8 … bit 4 = CIN11, bits 3-0 = 0 |
| 0x0A-0x0B | SAMP | R | 0 | CS[3:0] + valeur brute 12 bits |

## Comportements à connaître

- **Multi-touche** : plusieurs touches lues simultanément en I2C (le BCD ne donne que la prioritaire).
- **Auto-calibration** : après ≈ **50 s** de contact continu, la touche est absorbée dans la
  référence et disparaît (§2.3) ; le bit **KVF** (CTRL0) la garde valide indéfiniment.
- **Veille automatique** après ≈ 75 s sans touche **si SDA reste haut** ; tout accès I2C la
  repousse, une touche la réveille (§2.5).

## Notes spécifiques projet

- Candidat pour le **clavier 12 touches de Côté 2** (remplace MTCH2120 / MPR121), évalué le
  2026-09-27. Pas de mode proximité documenté : pour « approche ta main » (Côté 3), la
  sensibilité maximale (C_DC 5 pF) reste à essayer au banc.
- Pilote à écrire (simple : 2 octets lus à 0x08, INT pour éviter le polling).
- Datasheet en chinois uniquement, fabricant peu connu → **valider au banc** avant de figer
  (adaptateur SSOP-24).
