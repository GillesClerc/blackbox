# Guide de layout — Main (4 couches) + 5 satellites (2 couches)

> Aide-mémoire pour router sous KiCad sans rouvrir les datasheets. Chaque règle porte sa
> provenance : **[DS]** datasheet du composant (synthèse dans `docs/datasheets/`), **[AN]** note
> d'application ou site spécialisé (URL en fin de document), **[calc]** calcul, **[projet]**
> décision du projet. Quand deux sources se contredisent, c'est signalé par ⚖.
> Rédigé le 2026-09-29 d'après les schémas du 27-28/09.

---

## 1. Réglages KiCad (fabrication JLCPCB standard, sans surcoût)

### 1.1 Empilements

| Carte | Couches | Épaisseur | Empilement (Board Setup → Physical Stackup) |
|---|---|---|---|
| **Main** | 4 | 1,6 mm | **JLC04161H-7628** (celui par défaut) : L1 cuivre 35 µm · préimprégné 7628 **0,2104 mm, εr 4,4** · L2 cuivre 15,2 µm · âme 1,065 mm · L3 15,2 µm · préimprégné 0,2104 mm · L4 35 µm |
| **Satellites** | 2 | 1,6 mm | FR-4, 1 oz (35 µm) des deux côtés |

Attribution des couches de la Main **[AN : TI SLYT512, SLAA896 ; Espressif]** :
- **L1 (F.Cu)** : composants et signaux ; remplissage GND autour.
- **L2 (In1.Cu)** : **GND plein, jamais coupé**. C'est la référence de tout ce qui court sur L1.
- **L3 (In2.Cu)** : zones d'alimentation (VSYS, 5V, 3V3_D, 3V3_A) + quelques signaux lents.
- **L4 (B.Cu)** : signaux lents / croisements, LEDs du halo (face Dessous) ; remplissage GND.

### 1.2 Board Setup → Design Rules → Constraints

Capacités JLCPCB standard : piste/espace mini 0,10/0,10 mm ; via mini 0,15/0,25 mm ; « perçage de
0,2-0,25 mm avec pastille < 0,45 mm » et tout perçage de 0,1-0,15 mm = **surcoût** ; cuivre-bord
≥ 0,2 mm ; via-via 0,2 mm ; pastille-pastille (trous) 0,45 mm ; sérigraphie trait ≥ 0,15 mm, texte
≥ 1,0 mm ; vernis 1:1 **[AN : JLCPCB capabilities]**. On garde de la marge :

| Paramètre KiCad | Valeur | Remarque |
|---|---|---|
| Minimum clearance | **0,15 mm** | fab 0,10 |
| Minimum track width | **0,15 mm** | fab 0,10 |
| Minimum connection width | 0,15 mm | |
| Minimum annular width | 0,10 mm | via 0,3/0,6 → 0,15 |
| Minimum via diameter / hole | **0,5 / 0,3 mm** | 0,3/0,5 reste sans surcoût (pastille ≥ 0,45) |
| Minimum through hole | 0,3 mm | |
| Hole to hole clearance | 0,5 mm | |
| Copper to edge clearance | **0,3 mm** | fab 0,2 ; 0,4 mm si V-cut |
| Minimum text height / thickness | **1,0 / 0,15 mm** | |
| Solder mask expansion / min web | 0 / 0 | JLCPCB applique 1:1 |

**Tailles prédéfinies** : pistes 0,2 · 0,3 · 0,4 · 0,6 · 1,0 · 1,5 mm ; vias **0,3/0,6** (défaut),
0,4/0,8 (puissance).

### 1.3 Classes de nets (Board Setup → Net Classes)

Largeurs pour **ΔT = 10 °C, 1 oz externe** (IPC-2221) : 0,5 A → 0,12 mm · 1 A → 0,30 mm · 2 A →
0,78 mm · 3 A → 1,37 mm. En **0,5 oz interne** il faut ≈ 5× plus large (1 A → 1,5 mm) : sur L3,
utiliser des **zones**, pas des pistes **[calc, AN : IPC-2221 / advancedpcb]**. ⚖ IPC-2152 donne des
valeurs proches à 1 A (0,29 mm) mais plus larges à 3 A (2,1 mm) : prendre le max des deux.

| Classe | Nets | Largeur | Clearance | Pourquoi |
|---|---|---|---|---|
| Default | tout le reste | 0,20 | 0,20 | |
| PWR_BAT | VBAT, VBAT-, net FS8205 (drain commun), GND près de J2/U10 | **1,5** (ou zone) | 0,25 | jusqu'à ~3 A avant déclenchement DW01A (seuil ≈ 1,6-3,2 A) [projet, FSD §2.2.2b] |
| PWR_1A | 5V_USB, VSYS | **0,8** | 0,25 | entrée 1,07 A (R_ILIM), charge 1 A [DS bq24075] |
| PWR_BOOST | SW (L1/U7.1/D1), 5V_BOOST, 5V | **1,0** (ou zone) | 0,25 | crête inductance ≈ 1,9 A [DS SWPA5040S, projet] |
| PWR_3V3 | 3V3_D, 3V3_A, EN_SYS | 0,4 | 0,2 | ≤ 600 mA [DS AP2112] |
| AUDIO_HP | HP_L±, HP_R±, SPK_L±, SPK_R± | **0,6** (1,25 aux pastilles des ferrites) | 0,25 | crête ≈ 0,6 A sur 8 Ω [calc] ; 1,25 mm conseillé aux pads BLM21 [AN : Murata] |
| USB | USB_DP/DM(_RAW) | voir §2.3 | | paire |

**Vias de puissance** : ≥ 1 via 0,3 mm par ampère **[AN : TI SLVA773]** (un via 0,3 mm tient
≈ 1,4 A en externe [calc] ; ⚖ TI SLAA896 annonce 2 A pour un via rempli de 0,25 mm) → **2 vias
par transition** pour les nets ≥ 1 A.

### 1.4 Zones (remplissages)

- Clearance zone 0,25 mm, largeur mini 0,2 mm ; **reliefs thermiques** sur les pastilles
  (branches 0,3 mm), **connexion pleine** sur les vias et les pads thermiques des CI.
- **Vias de couture GND** : le long des bords (≈ tous les 5-10 mm), autour du module ESP32, autour
  des zones audio et du boost, à côté de chaque via de signal qui change de couche **[AN : TI
  SCAA082, SLVA680 ; Espressif]**.

---

## 2. Main — plan de placement

### 2.1 Zonage (à fixer avant de poser un seul composant)

```
 ┌──────────────── bord avec l'antenne (dépasse ou encoche) ──────────────┐
 │  [ESP32-S3-WROOM-1]   ← rien sous/à côté de l'antenne, 15 mm dans le    │
 │                          boîtier                                        │
 │  NUMÉRIQUE : SD, IMU, connecteurs satellites (JST-SH), LEDs (L4)        │
 │                                                                         │
 │  PUISSANCE : J2 batt · DW01A/FS8205 · bq24075 · boost · LDO · J14 · J12 │
 │                                       │  AUDIO (coin) : PCM5122 → RC →  │
 │                                       │  PAM8406 → ferrites → J10 ; mic │
 └─────────────────────────────────────────────────────────────────────────┘
```

- **Un seul plan GND (L2), non fendu, mais zoné** : l'audio dans un coin, le boost et le
  chargeur dans une autre zone, le numérique à part. Aucun signal ne passe d'une zone à l'autre
  au-dessus d'une coupure ; aucun signal numérique ne traverse la zone audio, sur **aucune**
  couche **[AN : TI SLYT512, SCAA082 ; DS PCM5122 §11 : masse commune AGND/DGND]**.
  ⚖ Certains convertisseurs demandent AGND/DGND séparés : **pas le PCM5122** (§11).
- **Antenne loin** du boost (L1, nœud SW), des horloges rapides, de l'USB, de l'UART et des points
  de test **[AN : Espressif]**.
- Connecteurs **sur les bords**, orientés vers la face qu'ils desservent (câbles courts) :
  J7/J7b/J9 → Devant ; J10 → Dessus (HP) ; J8 → Côté 1 ; J14 + J12 → Côté 2 ; un Qwiic par face.
- La Main est sur la **face Dessous** : LEDs du halo et trou du micro côté extérieur (voir §2.5).

### 2.2 ESP32-S3-WROOM-1 (U1) — zone numérique

| Règle | Source |
|---|---|
| **Antenne au bord de la carte**, point d'alimentation côté bord ; idéalement l'antenne **dépasse** la carte. Sinon, **encoche** de la carte sous et de part et d'autre de l'antenne. **Jamais** le module au centre avec un évidement sur 4 côtés. | [DS guide Espressif] |
| **Aucun cuivre, aucune piste, aucun via** sous la zone d'antenne (« Keepout Zone » du land pattern), **sur les 4 couches** (Rule Area KiCad : no tracks / vias / pours). | [DS WROOM-1, land pattern] |
| **≥ 15 mm** de dégagement autour de l'antenne dans le boîtier (pas de batterie, HP, vis, métal). | [DS guide Espressif] |
| Beaucoup de cuivre GND et des vias de couture denses sur la carte autour du module (hors zone antenne). | [AN : Espressif PCB layout] |
| Pad central (EPAD, pin 41 GND) : soudure **facultative** ; si on la soude, vias vers GND et pas trop de pâte (le module se soulève). | [DS WROOM-1] |
| **C2 100 nF** au plus près de la broche 3V3, **C1 10 µF** juste à côté. | [DS guide Espressif] |
| **R6 10 k / C_EN 1 µF** près de la broche EN, **piste EN courte** (sensible au bruit). | [DS guide Espressif] |
| **R13/R14 (22 Ω)** sur SPI2/SPI3_SCLK **au pied du module** (côté source) ; C_CLK2/3 (DNP) juste après. | [AN : Diodes AN022, R série à la source] |
| R3/R4 (pull-ups I2C 4,7 k) près du module ; I2C lent, routage libre mais loin du boost et de l'audio. | [projet] |
| TP_TXD0/TP_RXD0/TP_EN/TP_GPIO0 accessibles, **loin de l'antenne**. | [AN : Espressif] |

### 2.3 USB (R11/R12 → J14)

- **R11/R12 22 Ω au plus près de l'ESP32** (GPIO19/20) **[DS guide Espressif ; AN : NXP AN11392]**.
- Paire D+/D− **parallèle, même longueur, sans angle droit, sur L1 au-dessus de L2 continu**,
  peu de vias (si via : une paire de vias GND à côté), GND de part et d'autre **[AN : Espressif]**.
- ⚖ **Impédance** : Espressif demande 90 Ω ±10 % différentiel ; NXP juge l'impédance « non critique
  en Full-Speed » (12 Mbit/s) avec des pistes courtes **[AN]**. → Garder la paire **courte et
  appariée** ; si tu veux les 90 Ω, calcule largeur/espace avec le calculateur d'impédance JLCPCB
  (empilement 7628, référence L2) — je n'ai pas de valeur vérifiée à te donner.
- Pas de ferrite sur D+/D−, pas de condensateur > 50 pF **[AN : NXP AN11392]**.
- **J14 broche 1 (VBUS 1,1 A) et 2/5 (GND)** en classe PWR_1A.

### 2.4 Puissance

**Chemin batterie (J2 → DW01A/FS8205 → GND)** — attention : **VBAT− ≠ GND**. J2.2 = VBAT− (négatif
cellule) → U10.1 ; U10.3 = GND carte. Le DW01A (U9.6) et C_U9 sont référencés à **VBAT−** [projet,
netlist].
| Règle | Source |
|---|---|
| U10 (FS8205) **collé à J2**, pistes VBAT/VBAT−/GND courtes et larges (classe PWR_BAT, zones si possible). | [projet] |
| Le DW01A mesure la chute **dans les deux Rds(on) du FS8205** (seuil 150 ± 30 mV) : aucune piste de puissance supplémentaire entre U10 et le point de mesure, sinon le seuil de surintensité dérive. U9, R_CS (1 k) et C_U9 au plus près de U10. | [DS DW01A ; AN : TI bq2970 §9.4.1] |
| R_VCC (100 Ω) + C_U9 (100 nF) au plus près de U9.5 (VCC) ; C_U9 retourne sur **VBAT−**, pas sur GND. | [DS DW01A, projet] |
| J2.4 (retour NTC) sur GND, J2.3 (BAT_TS) en piste fine vers U8.1, loin du boost. | [projet] |

**bq24075 (U8, QFN-16 3×3)**
| Règle | Source |
|---|---|
| **C_IN (4,7 µF 25 V) sur IN (13)** et **C_OUT (4,7 µF) sur OUT (10/11)** au plus près, pistes courtes vers le pad thermique. C_BAT sur BAT (2/3). | [DS bq24075 §12.1] |
| **Pad thermique → GND** avec vias ; **VSS (8) aussi relié à GND** (⚖ la datasheet dit à la fois « le pad n'est pas la masse principale » et « le pad est la masse principale » : câbler **les deux**). | [DS bq24075] |
| Vias du pad : **4 vias 0,3/0,5 mm** (pad 1,68 mm, pas ≈ 1 mm) ; TI dessine 0,2 mm « filled, plugged or tented » — chez JLCPCB, un 0,2 mm à pastille < 0,45 mm coûte plus : 0,3/0,5 reste gratuit. **Tenter les vias côté L4** (vernis) pour limiter la remontée de soudure. | [DS bq24075 RGT0016C ; AN : TI SLUA271 §3.4.1 ; calc] |
| Masses faible courant (R_ISET, R_ILIM, R_TMR, TS) séparées du chemin de charge, rejoignant GND en un point près du pad. | [DS bq24075 §12.1] |
| IN/OUT/BAT dimensionnés pour 1,07 A / 1 A (PWR_1A) — les deux broches OUT (10, 11) et BAT (2, 3) reliées par du cuivre, pas une seule. | [DS bq24075] |

**Boost MT3608 (U7) + L1 + D1 + C_B1/C_B2**
| Règle | Source |
|---|---|
| **Boucle chaude = U7.1 (SW) → D1 → C_B2 → GND → U7.2** : la plus petite possible, **C_B2 placé en premier**, tout sur L1 sans via. | [DS MT3608 ; AN : TI SLVA773 ; calc] |
| **Nœud SW (U7.1, L1.2, D1 anode) : surface minimale** — cuivre juste suffisant pour 1,9 A, pas de zone. | [DS MT3608] |
| C_B1 (22 µF) sur VSYS au plus près de U7.5, C_B2 (22 µF) près de D1/U7.2 ; retours GND vers L2 par **plusieurs vias** directement aux pastilles. | [DS MT3608 ; AN : SLVA773, 1 via/A] |
| **R_FB_H / R_FB_L au plus près de U7.3 (FB)**, piste FB courte, **jamais parallèle** au nœud SW ni sous L1 ; prise de tension sur C_B2 (après D1). | [DS MT3608 ; AN : SLVA773] |
| Rien de sensible sous L1 / le nœud SW sur L4 (L2 GND fait écran, mais pas d'audio ni d'I2C juste dessous). Loin de l'antenne. | [AN : SLAA896 §2.4, Espressif] |
| Q1 (AO3401A) après C_B2 : 5V_BOOST → Q1 → 5V en PWR_BOOST ; Q2 et R21 près de Q1. | [projet] |

**LDO AP2112 (U5 SO-8 = 3V3_D, U6 SOT-23-5 = 3V3_A)**
| Règle | Source |
|---|---|
| **C_D1/C_A1 (1 µF) sur VIN et C_D2/C_A2 (10 µF) sur VOUT au plus près** (1 µF céramique mini). | [DS AP2112] |
| **U5 : broches 6 ET 7 = GND**, les deux reliées ; SO-8 choisi pour la thermique (θJA 114 °C/W) → **zone cuivre GND généreuse** sous/autour des broches 6-7 avec vias vers L2. | [DS AP2112 ; projet] |
| U6 (3V3_A) **près de la zone audio**, sa sortie filée vers le PCM5122 et le micro sans passer par la zone numérique. | [projet] |

### 2.5 Audio (coin dédié)

**PCM5122 (U2, TSSOP-28)**
| Règle | Source |
|---|---|
| **C_AVDD, C_DVDD, C_CPVDD, C_LDOO (100 nF)** au plus près de leurs broches (8, 28, 1, 26) ; **C15-C17 (10 µF)** juste derrière. | [DS PCM5122 §11] |
| **Charge pump : C19 entre CAPP (2) et CAPM (4), C20 de VNEG (5) à GND** — collés au CI, boucle minimale. | [DS PCM5122 §11] |
| **Coulée GND sur L1 autour du DAC**, reliée à L2 par plusieurs vias. | [DS PCM5122 §11.2] |
| **Horloges I2S (BCK 21, LRCK 23, DIN 22) loin des sorties OUTL (6)/OUTR (7)** ; masse entre OUTL et OUTR. | [DS PCM5122 §11] |
| Filtre de sortie **R1/R2 (470 Ω) + C21/C22 (2,2 nF C0G)** au pied de OUTL/OUTR, puis pistes analogiques courtes vers C_PAM_INL/INR. | [DS PCM5122, projet] |
| I2S depuis l'ESP32 : pistes groupées au-dessus de L2 continu ; pas de consigne de longueur chez TI. | [AN : recherche, aucune trouvée] |

**PAM8406 (U3, SOP-16, classe D filterless)**
| Règle | Source |
|---|---|
| **C_PAM_HF1-3 (1 µF) au plus près de PVDDL (4), PVDDR (13), VDD (6)** — le plus petit à < 1 mm de sa broche ; **C_PAM_BULK (22 µF, ≥ 20 µF exigés)** près de l'ampli. | [DS PAM8406 ; AN : TI SLAA896 §2.3] |
| **PGNDL (2), PGNDR (15), GND (11)** : vias vers L2 **dans/au ras des pastilles**. | [AN : TI SLAA896 §2.2] |
| **Ferrites FB3-FB6 au plus près des sorties** (1, 3, 14, 16), puis J10 ; pistes de sortie **courtes**, sur L1 au-dessus de L2 (jamais de signal sur la couche adjacente sans masse entre les deux). | [DS PAM8406 note 2 ; AN : TI SLOA216 §3.1, SLAA896 §2.4] |
| Paires L+/L− et R+/R− **routées côte à côte** (sorties en pont, **jamais à GND**), 0,6 mm, 1,25 mm aux ferrites. | [DS PAM8406, projet ; AN : Murata] |
| Les fils du HP (15-20 cm) sont en quart d'onde vers 375-500 MHz : les torsader, les garder loin de l'antenne. | [calc ; AN : TI SNAA050] |
| C_PAM_INL/INR et C_PAM_VREF près de leurs broches (7, 10, 8) ; R_MODE_D, R_MUTE, R_SHDN à côté. | [DS PAM8406] |

**Micro ICS-43434 (U4, bottom port)**
| Règle | Source |
|---|---|
| **Trou acoustique dans le PCB ≥ 0,5 mm et plus grand que le port** du micro (AN : typ. 0,5-1 mm), centré sur le port ; **aucune pâte** sur le trou (masquer dans la couche paste). | [DS ICS-43434 ; AN : ADI AN-1003] |
| Pastilles 1:1 avec celles du boîtier. | [DS ICS-43434] |
| **C3 (100 nF X7R) au plus près des broches VDD (5) et GND (3), sur la même couche, sans via** entre le condensateur et le micro ; le raccord au plan se fait de l'autre côté du condensateur. | [DS ICS-43434] |
| Le trou doit déboucher côté **extérieur** (face Dessous) avec un passage dans le boîtier ; joint le plus fin possible (sinon résonateur de Helmholtz). | [AN : ADI AN-1003 ; projet] |
| Aucune source chiffrée sur la distance à l'ampli/HP : l'éloigner du PAM8406 et de J10 autant que possible. | [AN : non trouvé] |

### 2.6 IMU LSM6DSOX (U11, LGA-14)

- **Aucune piste, aucun via, aucune structure sous le boîtier** ; pistes d'accès symétriques
  **[AN : ST TN0018 §1-2]**.
- Pastilles = pad + 0,1 mm ; ouverture vernis = pastille + 0,1 mm **[AN : TN0018 §2.1]** (vérifier
  l'empreinte LCSC C481766 contre ces valeurs).
- **> 2 mm des trous de vis**, loin des points chauds (boost, LDO, bq24075), des connecteurs et des
  zones qui fléchissent **[AN : TN0018]**. C4/C5 au plus près de VDD (8) / VDDIO (5).
- **Sérigraphier les axes** X/Y/Z d'après la figure 4 de la datasheet **[AN : DS LSM6DSOX §3]**.

### 2.7 Carte SD (J11 TF-01A) et SPI

- J11 au bord (insertion), pull-ups R15-R19 près de J11 ; SPI2 depuis l'ESP32 au-dessus de L2
  continu **[projet]**.
- Horloges SPI rapides (écrans 40-80 MHz) : R série à la source (R13/R14 déjà au pied de l'ESP32),
  **vias GND à côté de chaque via de signal**, jamais de fente sous les pistes **[AN : Diodes AN022,
  TI SCAA082]**. Aucune longueur de câble chiffrée trouvée chez un fabricant.

### 2.8 LEDs WS2812B (LED2-LED13, face L4) et connecteurs

- ⚖ **Condensateurs** : la datasheet V5 dit « aucun composant externe, pas même de condensateur »
  **[DS]** ; Adafruit conseille un gros condensateur sur l'alim de la bande **[AN]**. Le schéma suit
  la datasheet ; C_B2 (22 µF) est en tête du rail 5V.
- R8 (330 Ω) au départ de la chaîne, **près de l'ESP32 (GPIO48)** [AN : AN022 R à la source ;
  ⚖ Adafruit la place côté LED].
- Rail 5V des 12 LEDs (≈ 0,44 A datasheet, 0,72 A selon Adafruit) : **0,4 mm** suffit en 1 oz
  **[calc]**. DATA en chaîne courte LED à LED.
- J3-J6/J13 (Qwiic), J7/J7b/J8 : broches GND bien reliées au plan (retour des signaux du câble) ;
  une piste par signal, pas de stub.

### 2.9 Points de test

Tous les TP_* (Ø 1,5 mm) sur **une seule face** (L1 ou L4) pour sonder carte posée, **loin de
l'antenne**, TP_GND1-3 répartis près des zones puissance / numérique / audio [projet,
`03-validation-qualite.md`].

---

## 3. Satellites (2 couches) — règles communes

| Règle | Source |
|---|---|
| Composants en **top**, **plan GND plein en bottom** (sauf sous les électrodes et l'antenne NFC, voir plus bas) ; remplissage GND en top relié par vias de couture. | [AN : Espressif ; TI SLVA680] |
| Connecteur Qwiic (et liaisons de la Main) **au bord**, GND du connecteur relié au plan par un via adjacent. | [AN : TI SLVA680 §2.4] |
| I2C : 100 kHz, t_r ≤ 1000 ns, C_bus ≤ 400 pF ; avec 4,7 kΩ le bus tient **≈ 250 pF** au total (5 câbles + 10 CI) → **mesurer t_r au proto**, passer à 2,2 kΩ sur la Main si besoin. | [AN : TI SLVA689 ; calc] |
| ⚖ Au-delà de 10 cm, l'I2C recommande l'ordre SDA-VDD-VSS-SCL ; le Qwiic met SDA/SCL côte à côte → câbles courts. | [AN : NXP UM10204 §7.5] |
| **Trous de fixation** (dégagement ISO 273, série moyenne) : M2 → **2,4 mm**, M2,5 → 2,9 mm, M3 → **3,4 mm** ; **3 points de fixation**, loin des capteurs MEMS. | [AN : mechcodex ; Bosch BMP280 HS §6.1.7] |
| Sérigraphie : repère broche 1, polarité, nom des connecteurs (« → J7 Main »), axes des capteurs. | [projet] |

### 3.1 Dessus — antenne NFC ST25DV (U1 + ANT1)

| Règle | Source |
|---|---|
| **Cible L ≈ 4,8 µH** (C_TUN interne 28,5 pF, 26,5-30,5). Viser **un peu en dessous** (accord ≈ 13,6-13,7 MHz) : C3 (DNP) ne peut que **baisser** la fréquence. | [DS ST25DV ; AN : ST AN2972 §1, §5 ; communauté ST] |
| Géométries de départ : **carrée 35 mm, 9 spires, piste/espace 0,3/0,3 mm ≈ 4,85 µH** ; carrée 40 mm, 8 spires 0,35/0,35 ≈ 4,6 µH ; ronde Ø 33/27 mm, 9 spires 0,35/0,35 ≈ 4,7 µH (exemple ST). Affiner avec **ST eDesignSuite** (tient compte de la capacité parasite). | [AN : AN2972 §2.2-3.3 ; calc] |
| **U1 à quelques mm de l'antenne**, pistes AC0/AC1 **courtes et symétriques** (toute longueur ajoutée décale l'accord). | [AN : AN2972 §3.4.1] |
| **Aucun cuivre au-dessus, en dessous ni à l'intérieur de la boucle**, sur les 2 couches ; aucune boucle de masse fermée qui l'entoure (Rule Area KiCad). Le pont de retour de la spire intérieure passe en bottom par 2 vias. | [AN : AN2972 §3.4.2] |
| Métal proche (charnières, vis, HP !) → L baisse, f monte : l'éloigner, ou compenser (spires / C3). | [AN : AN2972 §3.4.3] |
| **C3 (0402 C0G, DNP)** entre AC0 et AC1, au pied de U1. C1 (10 nF) + C2 (100 nF) au plus près de VCC (8)/VSS (4). | [DS ST25DV ; AN : AN2972 §5] |
| Prototyper 6-10 variantes par pas de 5 % de L et mesurer **dans le boîtier final**. | [AN : AN2972 §5] |
| **Empreinte ANT1 à créer** (bobine en cuivre dans le footprint, 2 pads aux extrémités). | [projet, question 7] |

### 3.2 Devant — visage

| Règle | Source |
|---|---|
| **BMP280 (U1)** : trou d'évent (sur le couvercle) **tourné vers l'ouverture de la bouche**, ≥ 0,1 mm d'air au-dessus ; **pas de lumière directe** sur le trou (WS2812B !) ; loin des bords, des points chauds et des fixations. | [AN : Bosch BMP280 HS §1.1, §6.1 ; DS §7.6] |
| BMP280 : pas de vernis/coating/colle ; pastilles selon la figure 19 de la datasheet ; pas de pistes/vias sous le boîtier (règle ST pour les LGA, Bosch ne dit rien). | [AN : Bosch HS §3.1, §6.1 ; ST TN0018] |
| Souffle = air chaud : > 3 °C/s crée des erreurs → **petit volume mort** entre la bouche et le capteur (chambre/canal court), ouverture assez large. C1/C2 (100 nF) sur VDD (8)/VDDIO (6). | [AN : Bosch HS §6.1.9 ; DS §6.3] |
| **VEML7700 (U2)** : fenêtre centrée sur la **zone sensible (décentrée dans le boîtier)**, largeur **w = 0,5 mm + 2·d·tan α** : d = 1 mm → 3,4 mm (±55°) ; d = 2 mm → 6,2 mm ; d = 3 mm → 9,1 mm. Capteur **au plus près** de la fenêtre. C3 (100 nF) sur VDD (2). | [AN : Vishay « Designing the VEML7700 »] |
| VEML7700 : **cloison opaque / mousse** entre le halo et le capteur (Vishay ne traite pas la fuite des LEDs ; mesurer LEDs éteintes). | [suggestion, pas de source] |
| Halo LED1-LED8 : rail 5V 0,4 mm, DATA en chaîne, loin du BMP280/VEML7700. | [calc ; projet] |
| SPI3/SPI2 (J2/J3 → J4/J5/J6) : pistes courtes, sur plan GND continu, GND des connecteurs bien reliés. | [AN : SCAA082] |

### 3.3 Côté 1 — panneau de contrôle

| Règle | Source |
|---|---|
| **ADS7830 (U1)** : C1 (100 nF) au plus près de VDD (16), C2 (1 µF) juste à côté ; **C3 (1 µF) au plus près de REFIN (10)** ; GND (9) vers un point de masse propre, loin du retour numérique. | [DS ADS7830 layout, Typical Connection] |
| Pistes des curseurs (FADER1-4, POT1-4) : fines, loin des signaux SW/BTN et du connecteur J2 ; pas de consigne de longueur. | [projet] |
| Option (non au schéma) : **condensateur sur chaque entrée ≥ 20 × 25 pF**, ex. 10 nF (τ ≤ 25 µs avec 10 kΩ) — utile si les lectures bruitent. | [AN : TI E2E « first rule of thumb when driving ADC inputs » ; calc] |
| **Pattes 4-7 des faders (cadre) → GND** : relier au plan par des vias. Grosses pastilles THT : reliefs thermiques pour la soudure. | [projet] |
| Organes de façade (SW1/SW2/BTN1/BTN2 par J4-J7) : si ESD, **TVS contre le connecteur, sans stub** (pas au schéma, question 20). | [AN : TI SLVA680 §2.1] |

### 3.4 Côté 2 — USB-C + tactile

**USB-C (J1) + USBLC6 (U1)**
| Règle | Source |
|---|---|
| **USBLC6 contre J1** : 10 mm × 0,5 mm de piste ≈ 6 nH = **+144 V** sur le clamp. Pistes courtes de J1 → U1 (I/O), VBUS → pin 5, GND → pin 2 → plan par via direct. | [DS USBLC6 §2.2-2.3] |
| TVS **plus près du connecteur que de tout le reste**, sans stub ni via entre J1 et U1, angles à 45°, aucune piste non protégée en parallèle. | [AN : TI SLVA680 §2.1-2.3] |
| **R1/R2 (5,1 k) au pied de CC1/CC2**, une par broche. | [projet ; AN : hackaday USB-C] |
| D+/D− : paire parallèle, même longueur, GND autour ; les deux rangées A6/B6 et A7/B7 reliées au plus près de J1. | [AN : Espressif ; Microchip AVR1017] |
| VBUS/GND vers J2 (1,1 A) en **0,8 mm**. | [calc] |
| ⚖ **Blindage de J1** : relié directement à GND (schéma actuel, TI SLVA680) vs RC 1 MΩ // 4,7 nF (Microchip AVR1017). | [AN] |

**CAP1298 (U2) + électrodes E1-E8** — dépend de l'**épaisseur de paroi t** (à fixer)
| Règle | Source |
|---|---|
| **Budget : ≤ 50 pF par entrée** (électrode + piste) ; la garde SG pilote 20-200 pF. | [DS CAP1298] |
| Touches E1-E6 : **≥ 8 mm + 2·t** (t = 3 mm → 14 mm), coins arrondis. | [AN : Microchip AN2934 §1.2-1.3] |
| ⚖ Espacement entre touches : **4 mm + t** (Microchip) vs t/2 (TI) → prendre Microchip. | [AN : AN2934 ; TI CapTIvate] |
| **Électrodes en top, pistes en bottom** (via), **≤ 0,2 mm**, courtes ; rien ne passe derrière ni entre les électrodes ; ≥ t (ou 5 mm) de toute autre piste. **CAP1298 au plus près** des électrodes. | [AN : Azoteq AZD125 §3.2 ; TI CapTIvate] |
| **Numérique (I2C, USB) à ≥ 4 mm** des pistes capacitives, croisements à 90°. | [AN : AZD125 ; CapTIvate] |
| **Pas de plan plein derrière les électrodes** : GND **hachuré 25 %** (piste 0,2 mm / espace 1,4 mm) ou découpé. | [AN : TI CapTIvate ; AN2934 §1.4] |
| ⚖ Masse coplanaire autour : ~2 mm (Microchip) / ≥ t/2 (TI) / ≥ 5 mm (Azoteq). | [AN] |
| **E8 (garde) : anneau à 1-3 mm de E7, ouvert** (ne pas le fermer). | [AN : AN2934 §1.4] |
| **Coller le PCB à la paroi** (1 mm d'air ≈ 8 mm de verre perdus) : adhésif type 3M 467MP/468MP. | [AN : AZD125 §3.1 ; CapTIvate] |
| C1 (100 nF) + C2 (1 µF) au plus près de VDD (7). J2 (vers J14) et l'USB loin des électrodes. | [DS CAP1298] |

### 3.5 Côté 3 — TMAG5273 (U1)

| Règle | Source |
|---|---|
| Le champ traverse PCB, plastique, bois, aluminium : **U1 centré sous la zone de pose de l'aimant**, aimant centré sur les éléments Hall (fig. 6-2). | [DS TMAG5273 §7.5.1, §6.3] |
| **Sérigraphier les axes X/Y/Z et le sens** (pôle nord → codes positifs, fig. 6-1). | [DS TMAG5273 §6.3.1-6.3.2] |
| **C1 (100 nF, ≥ 10 nF) collé à VCC (4)** ; TEST (3) et INT (5) à GND. | [DS §7.4] |
| Pas de vis acier, inductance, HP près du capteur (TI ne chiffre pas) ; calibrer l'offset (±300 µT typ.). Une piste de 100 mA à 1 mm ≈ 20 µT : négligeable devant l'aimant. | [DS §5.7 ; calc] |

---

## 4. Checklist avant DRC / commande

- [ ] Empilement et classes de nets saisis (§1) ; DRC sans erreur.
- [ ] Rule Areas : antenne ESP32 (4 couches), antenne NFC (2 couches).
- [ ] L2 de la Main **non fendu** (vérifier avec seule L2 visible) ; aucune piste sur la zone audio
      venant du numérique.
- [ ] Boucle du boost minimale ; FB loin de SW ; 2 vias par transition ≥ 1 A.
- [ ] Découplages : chaque petit condensateur à < 1-2 mm de sa broche, retour GND direct.
- [ ] Trou du micro percé, sans pâte ; rien sous l'IMU ni sous le BMP280.
- [ ] Vias du pad bq24075 tentés côté L4 ; EPAD ESP32 : décision (soudé ou non).
- [ ] Axes sérigraphiés (IMU, TMAG) ; broche 1 et polarités lisibles ; texte ≥ 1 mm.
- [ ] Vue 3D : collisions, hauteur des connecteurs vs boîtier, dégagement antenne 15 mm.
- [ ] Checklist JLCPCB du FSD §3.2 (Gerbers, BOM, CPL, go/no-go).

---

## Sources (notes d'application et sites spécialisés)

- TI SLYT512 (partitionnement de masse) — https://www.ti.com/lit/pdf/SLYT512
- TI SCAA082 (plans fendus, vias de retour) — https://www.ti.com/lit/an/scaa082a/scaa082a.pdf
- TI SLAA896 (layout classe D) — https://www.ti.com/lit/an/slaa896/slaa896.pdf
- TI SLOA216 (EMI classe D) — https://www.ti.com/lit/an/sloa216/sloa216.pdf
- TI SLAA581 (ferrites classe D) — https://www.ti.com/lit/an/slaa581/slaa581.pdf
- TI SNAA050 (pistes et antennes) — https://www.ti.com/lit/pdf/snaa050
- TI SLVA773 (layout boost) — https://www.ti.com/lit/pdf/slva773
- TI SLUA271 (QFN, vias thermiques) — https://www.ti.com/lit/an/slua271c/slua271c.pdf
- TI bq2970 §9.4.1 (protection batterie) — https://www.ti.com/lit/ds/symlink/bq2970.pdf
- TI SLVA680 (layout ESD/TVS) — https://www.ti.com/lit/an/slva680/slva680.pdf
- TI SLVA689 (pull-ups I2C) — https://www.ti.com/lit/an/slva689/slva689.pdf
- TI CapTIvate design guide — https://software-dl.ti.com/msp430/msp430_public_sw/mcu/msp430/CapTIvate_Design_Center/latest/exports/docs/users_guide/html/CapTIvate_Technology_Guide_html/markdown/ch_design_guide.html
- TI E2E, entrées ADC — https://e2e.ti.com/blogs_/archives/b/precisionhub/posts/first-rule-of-thumb-when-driving-adc-inputs
- Diodes AN022 (terminaison série) — https://www.diodes.com/assets/App-Note-Files/AN022-P.pdf
- Murata BLM21 (ENFA0005) — https://www.murata.com/products/productdata/8796740845598/ENFA0005.pdf
- ADI AN-1003 (micros MEMS, miroir) — http://www.t-es-t.hu/download/analog/an1003.pdf
- InvenSense AN-100 (manipulation micros) — https://www.cdiweb.com/datasheets/invensense/an-100-00-mems-microphone-handling-and-assembly-guide.pdf
- Espressif, PCB layout ESP32-S3 — https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html
- NXP AN11392 (USB) — https://www.nxp.com/docs/en/application-note/AN11392.pdf
- NXP UM10204 (I2C) — https://www.nxp.com/docs/en/user-guide/UM10204.pdf
- Microchip AVR1017 (USB) — https://ww1.microchip.com/downloads/en/Appnotes/doc8388.pdf
- Microchip AN2934 (électrodes) — synthèse `docs/datasheets/AN2934-touch-sensor-design.md`
- Azoteq AZD125 (capacitif) — https://www.azoteq.com/images/stories/pdf/azd125_capacitive_sensing_design_guide_v1.0.pdf
- ST AN2972 (antennes NFC dynamiques) — https://www.st.com/resource/en/application_note/an2972-how-to-design-an-antenna-for-dynamic-nfc-tags-stmicroelectronics.pdf
- ST TN0018 (MEMS LGA) — https://www.st.com/resource/en/technical_note/tn0018-surface-mounting-guidelines-for-mems-sensors-in-an-lga-package-stmicroelectronics.pdf
- Bosch BMP280 HS000 (montage) — https://www.bosch-sensortec.com/media/boschsensortec/downloads/handling_soldering_mounting_instructions/bst-bmp280-hs000.pdf
- Vishay « Designing the VEML7700 » — https://www.vishay.com/docs/84323/designingveml7700.pdf
- Adafruit NeoPixel best practices — https://learn.adafruit.com/adafruit-neopixel-uberguide/best-practices
- JLCPCB capacités / empilements — https://jlcpcb.com/capabilities/pcb-capabilities · https://jlcpcb.com/impedance
- Largeur de piste IPC-2221 / IPC-2152 — https://www.advancedpcb.com/en-us/tools/trace-width-calculator/ · https://www.smps.us/pcb-calculator.html
- Trous de passage ISO 273 — https://mechcodex.com/reference/metric-clearance-hole-sizes
