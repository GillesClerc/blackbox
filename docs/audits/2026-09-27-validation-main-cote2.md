# Validation schéma — Main + satellite Côté 2 (2026-09-27)

> Contrôle par script de la connectivité (`tools/kicad_netlist.py` + script de validation :
> pins isolées, nets à une pin, nets à plusieurs noms, labels orphelins, no_connect mal posés),
> puis vérification des interfaces entre cartes et des points modifiés le 27/09 contre les
> datasheets. **Ne remplace pas l'ERC de KiCad**, à lancer par Gilles après rechargement.

## Résultat

| Carte | Pins | Nets | Fils mal raccordés | Courts-circuits entre rails | Pins isolées non marquées |
|---|---|---|---|---|---|
| Main | 478 | 121 | 0 | 0 | 0 (après correction ci-dessous) — ERC KiCad : voir « Retour ERC » |
| Côté 2 | 74 | 28 | 0 | 0 | 1 attendue (J2.6 `5V_HOST`, réserve mode hôte) |

## Corrigé pendant la validation (feuille `power`)

- **U10 (FS8205)** : les pins 2 et 5 (drain commun D12) sont superposées dans le symbole, la 5
  étant **cachée**. J'avais retiré le no_connect posé à cet endroit en le croyant superflu :
  l'ERC de KiCad a alors signalé « pin cachée 5 non connectée » (KiCad ne relie pas une pin
  cachée à la pin visible superposée). **Marqueur remis** ; le drain commun n'a besoin
  d'aucune liaison externe [FS8205.md].
- **no_connect ajoutés** sur des pins réellement non connectées selon leur datasheet, qui
  auraient levé des avertissements ERC : U5 (AP2112M SO-8) pins 2, 3, 4 = NC [AP2112.md,
  brochage SO-8] ; U6 (AP2112K SOT-25) pin 4 = NC ; U9 (DW01A) pin 4 TD « non connectée en
  application » [DW01A.md].

## Vérifié conforme

- **Brochage ESP32-S3-WROOM-1** (figure « Pin Layout » de la datasheet) : 10 = IO17 (I2C_SCL),
  13 = IO19 (USB_D−), 14 = IO20 (USB_D+), 16 = IO46 (BTN2), 23 = IO21 (I2C_SDA), 26 = IO45 (BTN1).
  No_connect sur 28-30 (IO35-37, PSRAM octale) et 36-37 (RXD0/TXD0).
- **No_connect justifiés** : PCM5122 pins 13-15, 19 (GPIO à pull-down interne [pcm5122.md]) ;
  LSM6DSOX INT1 (pull-down interne), INT2, OCS_Aux, SDO_Aux (mode 1 [LSM6DSOXTR.md]).
- **BTN1 / BTN2** : J8.5 / J8.6 → GPIO45 / GPIO46, pull-down R23 / R24 10 kΩ, aucun pull-up.
- **Batterie** : J2.3 = BAT_TS (U8 TS, R_TS DNP, TP_TS1), J2.4 = GND (retour NTC sur VSS).
- **USB, Main** : J14.1 5V_USB → U8 IN + C_IN + TP_VBUS1 ; J14.3/4 USB_D∓_RAW → R11/R12
  22 Ω → IO19/IO20 ; J14.6 5V_HOST ← R22 (DNP).
- **USB, Côté 2** : J1 VBUS ×4 → 5V_USB, D+/D− des deux rangées reliés, CC1/CC2 → 5,1 kΩ →
  GND, blindage → GND ; USBLC6 sur les lignes brutes côté connecteur, VBUS pin 5.
- **Liaison J14 (Main) ↔ J2 (Côté 2)** : même brochage des deux côtés (1 5V_USB, 2 GND,
  3 D−, 4 D+, 5 GND, 6 5V_HOST) → câble **broche à broche**.
- **Bus I2C** : 10 pins sur SDA et SCL (ESP32, pull-ups R3/R4 4,7 kΩ, PCM5122, LSM6DSOX,
  J3-J6, J13, pastilles de test) ; pas de pull-up sur Côté 2. **Qwiic J3 de Côté 2** = même
  brochage que J3-J6/J13 (1 GND, 2 3V3_D, 3 SDA, 4 SCL).
- **CAP1298 (Côté 2)** : conforme à la figure 3-1 et au tableau 1-1 (voir `CAP1298.md`).

## Points ouverts

1. **Pastilles UART0 absentes** : TXD0/RXD0 (pins 37/36 du module) sont en no_connect, alors
   que le FSD (décision USB du 27/09) prévoit des pastilles UART pour garder une console en mode
   hôte USB, et que le mode Test duplique son affichage sur l'UART. → proposer deux pastilles
   de test TP_TXD0 / TP_RXD0 sur la Main (pas de GPIO consommé : GPIO43/44 sont réservés à
   l'UART0).
2. **Câble J14 ↔ J2** : vérifier au montage qu'il est bien broche à broche (les câbles JST-PH
   double-extrémité du commerce ne le sont pas tous) ; un câble croisé mettrait VBUS sur
   5V_HOST et D+/D− inversés.
3. **Courant VBUS dans J14 / J2** (~1,1 A par un seul contact JST-PH) : valeur nominale du
   contact à vérifier sur la datasheet JST PH (non présente dans `docs/datasheets/`).
4. **SW3** (marche/arrêt, Côté 2) : pas encore sur le satellite — câblage direct vers J12 de la
   Main ou connecteur sur le satellite, à décider.
5. **Électrodes de Côté 2** : règles de dessin dans `docs/datasheets/AN2934-touch-sensor-design.md`
   (dépendent de l'épaisseur de la paroi, à fixer).
6. **ERC KiCad** à lancer sur les deux projets après rechargement des feuilles.

## Retour ERC de Gilles (2026-09-27)

- « Pin non connectée : U10 pin cachée 5 » → causée par le retrait du no_connect ; corrigé.
- « Item non numéroté : C_AVDD? » → sur le disque la référence est bien `C_AVDD` (feuille
  `audio` inchangée) : le « ? » vient de la session KiCad (réannotation locale) ; remettre la
  référence `C_AVDD` ou recharger la feuille `audio`.

## Vérification des empreintes et des passifs (2026-09-27, demande de Gilles)

**Empreintes** : les 165 composants (Main + Côté 2) ont une empreinte cohérente avec le boîtier
de la BOM et de leur datasheet (CI : SOT-23-5/6, SO-8, SOIC-14/16, TSSOP-28, QFN-16 3 × 3,
LGA-14, module WROOM-1 ; connecteurs JST-SH/PH ; passifs 0402/0603/0805 ; L1 5 × 5 ; D1 SMA).
Aucun écart de boîtier.

**Valeurs** : une seule erreur, **C_EN = 1 µF au schéma mais rangé dans la ligne 100 nF de la
BOM** (le contrôle `check_bom` ne compare pas les lignes groupées) → ligne propre ajoutée,
1 µF (guide Espressif : RC de EN = 10 kΩ + 1 µF).

**Diélectriques** (précisés dans la BOM, aucun n'y figurait sauf C21/C22) :
- **C0G/NP0** : C21/C22 2,2 nF (filtre de sortie du DAC), C_CLK2/C_CLK3 10 pF (DNP).
- **X7R** : découplages 100 nF, 1 µF de l'ampli (HF, entrées, VREF — la datasheet PAM8406 demande
  seulement « céramique faible ESR »).
- **X5R** : réservoirs ≥ 1 µF (LDO, bq24075, MT3608, PCM5122, rails 3,3 V).
- Le schéma de la Main ne porte ni diélectrique ni tension dans les champs des condensateurs :
  **la BOM est la référence** pour la commande.

**Charge pump PCM5122** : C19 entre CAPP et CAPM, C20 de VNEG à GND — conforme (la note
« C20 à corriger » de `pcm5122.md` datait du 23/09, déjà corrigée au schéma).

### Tension et taille : points à trancher

Les condensateurs céramiques X5R/X7R **perdent une partie de leur capacité sous tension continue**,
d'autant plus que le boîtier est petit et que la tension nominale est proche de la tension
de service. Le pourcentage dépend de la référence exacte (courbe « DC bias » du fabricant) :
aucune référence n'est encore choisie, donc pas de chiffre ici. Les cas où la valeur **minimale
exigée par une datasheet** est juste atteinte en nominal :

| Réf. | Actuel | Rail | Exigence datasheet | Proposition |
|---|---|---|---|---|
| C_IN | ~~4,7 µF 10 V 0603~~ → **4,7 µF 25 V X5R 0603, Samsung CL10A475KA8NQNC (C69335), fait le 27/09** | 5V_USB (4,35-6,4 V en service, 28 V destruction) | bq24075 : 1-10 µF | 25 V : marge contre un chargeur défectueux ou une surtension au branchement ; X5R (−55/+85 °C) suffisant, empreinte 0603 conservée |
| C_BAT | 4,7 µF 10 V 0603 | VBAT ≤ 4,2 V | bq24075 : 4,7-47 µF | 10 µF 10 V 0805 (reste ≥ 4,7 µF effectif) |
| C_OUT | 4,7 µF 10 V 0603 | VSYS ≤ 5,5 V | bq24075 : 4,7-47 µF | 10 µF 16 V 0805 |
| C_D1, C_A1 | 1 µF 10 V 0402 | VSYS ≤ 5,5 V | AP2112 : 1 µF céramique min | 1 µF 16 V 0603 (ou 2,2 µF 10 V 0603) |
| C_B1, C_B2 | 22 µF 16 V 0805 | VSYS / 5 V boost | MT3608 : 22 µF | 22 µF 25 V 1206 |
| C_PAM_BULK | 22 µF **10 V** 0805 | 5 V | PAM8406 : ≥ 20 µF | 22 µF 25 V 1206 (ou 2 × 22 µF 16 V 0805) |

Les autres (100 nF, 10 µF sur 3,3 V, 2,2 µF charge pump 25 V, 1 µF de l'ampli en 16 V) ont une
marge suffisante. Changer de boîtier modifie les empreintes au schéma (F8 ensuite). **Décision du 27/09 : seul C_IN
est modifié ; les autres restent en l'état.**

### Résistances

Dissipation maximale : R20 (2,2 kΩ sous 5,5 V) ≈ 14 mW, toutes les autres négligeables — le
calibre usuel d'une 0402 (1/16 W) suffit, à confirmer sur la référence retenue. **R22 (0 Ω,
DNP)** : si un jour on la monte pour le mode hôte USB, elle portera le courant de la clé →
vérifier le courant admissible du 0 Ω 0603 choisi.
