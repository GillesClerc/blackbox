# Reste à faire

> Document vivant, **recréé le 2026-09-27**, complété le 2026-11-15 (`2026-11-14-audit.md`) à partir de l'audit complet du jour
> (`2026-09-27-audit.md`). Les numéros (H1, F2…) renvoient à ce rapport ; le détail des
> points hérités est dans `2026-09-23-audit.md` et `2026-09-23-hardware-db.md`.
> Cocher ici au fil de l'eau.
>
> **Qui** : 🧑 Gilles (KiCad, choix, service externe) · 🤖 Claude (code / doc, sur
> validation) · 🤝 à trancher ensemble.

---

## Vérification du 2026-11-15 — schémas et guide de layout

Détail dans `2026-11-15-verification-schemas-layout.md` (numéros A/B/C/G de ce rapport).
Traitée point par point avec Gilles.

- [x] 🤖 **A1 (schéma)** — démarrage progressif de Q1 (R_SS / C_SS) posé, puis **retiré avec
  le load switch en A3** : le TPS63020 démarre à courant limité (400 mA, §7.4.1).
- [x] 🤖 **TP_GATE1 n'était relié à rien dans KiCad** (trouvé en traitant A1) : label **local**
  `GATE_5V` sur la grille, label **global** du même nom sur TP_GATE1 — KiCad ne les relie pas.
  La grille porte maintenant un label global. `kicad_netlist.py` corrigé : labels locaux limités
  à leur feuille, et `--check` signale tout label local homonyme d'un net global.
- [x] 🤖 **A2** — empreinte du bq24075 conforme au land pattern TI : 5 vias thermiques
  renumérotés 17 (ils n'avaient aucun net), Ø 0,2 / 0,45 mm bouchés côté L4, pâte 85 %.
- [x] 🤖 **B3** — `BQ24075RGTR`, `FS8205`, `MT3608` déclarées dans les tables **du projet** Main :
  la table globale du poste pointait vers une copie hors dépôt (`C:/Users/Gilles/dev/Blackbox/…`).
- [x] 🤖 **A4a** — C_SD2 (100 nF) + C_SD1 (10 µF) au pied de J11, feuille `connector` ; BOM,
  guide §2.7, `TF-01A.md` à jour.
- [ ] 🧑 **A4b** — choisir le module des yeux GC9A01 (référence, ordre des broches, courant) :
  J4/J5 du satellite Devant restent provisoires, **bloquant pour le layout de Devant** et pour
  boucler le budget 3V3_D (H1).
- [x] 🤖 **A3** — rail 5 V : **MT3608 → buck-boost TPS63020** (C15483) + L1 **Sunlord WPN4020H1R5MT**
  1,5 µH (C98360), 3 × 22 µF en sortie, consigne **5,0 V** (1,62 MΩ / 180 kΩ), R_VINA + C_VINA ;
  **retirés** : D1, Q1, Q2, R21, R_SS, C_SS, TP_5VB1, TP_FB1 (B6), TP_GATE1, PWR_FLAG du 5 V.
  Régule aussi sous USB (**H9 soldé** : plus de 5,2-5,3 V sur les WS2812), charge déconnectée à
  l'arrêt (**H4 sans objet**). Symbole/empreinte/3D générés par easyeda2kicad (C15483, C98360),
  types de broches corrigés. Synthèses `TPS63020.md`, `WPN4020H.md` ; BOM, guide §1.3/§2.4,
  validation, FSD à jour. Netlist comparée : seul le bloc change.
- [ ] 🧑 **A3 — deux vérifications à l'œil sur le PDF `TPS63020.pdf`** (figures non lisibles ici) :
  (1) figure 7 p. 13 : comment VINA est alimentée (la rév. G a « removed the connection from VINA
  to VIN ») — le schéma la met sur VSYS par R_VINA 0 Ω ; (2) dessin DSJ0014A (mécanique) contre
  l'empreinte LCSC `C15483_VSON-14…` : pad central en peigne jusqu'à ±2,2 mm. Contrôler aussi
  l'empreinte de L1 contre la Fig. 5 du catalogue Sunlord (a 1,9 / b 1,1 / c 3,7 mm).
- [ ] 🧑 **A3** — stock JLCPCB à revérifier à la commande : TPS63020DSJR 638, **WPN4020H1R5MT 200**.
- [ ] 🧑 **Recharger les feuilles `power` et `connector`** dans KiCad (Fichier → Revenir) avant toute sauvegarde ;
  au premier F8, vérifier que U8 prend bien l'empreinte corrigée (5 vias « 17 » dans le pad), et
  que U7 (TPS63020) et L1 prennent les nouvelles empreintes `lcsc_footprints:C15483_…` / `C98360_…`.
- [ ] 🤖 **A1 (firmware)** — plafond global de courant **~1,5 A côté batterie** (LEDs + audio,
  en plus du plafond audio à −22 dB) : le bq24075 coupe VSYS dès 2,0 A garantis (V_O(SC2)).
- [ ] 🧑 **A1 (proto)** — mesurer VSYS à l'oscilloscope à l'allumage et en charge crête (coupure
  du bq24075 si VBAT − VSYS > 200 mV pendant 250 µs).

## Audit du 2026-11-14 — points nouveaux

Détail dans `2026-11-14-audit.md` (numéros H/G/F/W de ce rapport).

Avant de router :
- [ ] 🤝 **H1** — budget de courant du rail 3V3_D (AP2112 600 mA) : fiche du module des yeux
  (rétroéclairages sur 3V3_D par R1 = 0 Ω) et écran bouche.
- [x] 🤖 **H2** — empreintes optionnelles posées le 14/11 : `R_SHDN_DAC` (0 Ω, DNP, SHDN# ←
  GPIO6 du PCM5122, net `AMP_SHDN_CTRL`) et `R_SHDN_PD` (10 kΩ, DNP). `R_SHDN` reste montée.
  Ne jamais monter `R_SHDN` et `R_SHDN_PD` ensemble.
- [ ] 🧑 **Recharger la feuille `audio`** dans KiCad (Fichier → Revenir) avant toute sauvegarde.
- [x] 🤝 **H3** — LEDs de la Main sur L4 : assemblage double face assumé (décision du 14/11),
  à chiffrer au devis.
- [ ] 🧑 **H6** — brochage du fader PTA6043 à confirmer sur le PDF, puis dans la synthèse.
- [ ] 🧑 **H8** — relancer l'ERC Main (`C_AVDD?` sur la racine) et Côté 1 (rapport périmé).
- [ ] 🤝 **H5** — empreinte optionnelle : condensateur 5 V sur Devant. (H4, démarrage progressif de
  Q1 : sans objet depuis le TPS63020, A3.)
- [x] 🤖 **G1 / G2 / G3 / G4 / G6** — guide de layout complété le 15/11.
- [ ] 🧑 Trois PDF illisibles par le groupe (`BMP280.pdf`, `MLX90614.pdf`, `TMAG5273.pdf` en
  `rw--w----`) : `sudo chmod g+r` dessus, sinon git les voit modifiés.

Firmware (correctifs du 15/11 : build sans avertissement, tests host 36/36 sous ASan/UBSan,
**flashés et vérifiés sur la DevKitC** — boot, son de fond + one-shot, auth et sync cloud) :
- [x] 🤖 **F1** — mixer : une voix en boucle sans trame décodable est arrêtée.
- [x] 🤖 **F2** — cycles de `branch` refusés par les validateurs C et TS (6 cas partagés ajoutés,
  36/36 côté TS) + un tick de délai dans le moteur.
- [x] 🤖 **F3** — MP3 hors 44 100 Hz refusé par le mixer ; erreur au back-office et dans
  `package_scenario.py` (W3).
- [x] 🤖 **F4** — fenêtre BLE ouverte aussi quand le WiFi échoue au boot. Relevé sur cible : le
  DHCP a pris **17 s** au premier boot (1-5 s ensuite) ; à 15 s la sync de boot n'était jamais
  lancée. `hal_wifi_connect` distingue désormais refus (`ESP_FAIL`) et délai (`ESP_ERR_TIMEOUT`),
  attente portée à 45 s au boot et pendant l'appairage BLE (page `/devices/add` : 50 s).
  **Cause probable de l'échec d'appairage BLE du 30/09** (§9) — à revalider dans Chrome.
- [x] 🤖 `cloud_client` : tampon d'en-têtes HTTP 1024 o (le JWT dépassait les 512 o par défaut,
  erreur `HTTP_HEADER` à chaque requête authentifiée — relevé sur cible).
- [x] 🤖 **F5** — backend MTCH2120 retiré (0x28 = adresse du CAP1298).
- [x] 🤖 **F6 / F7** — SSID de 32 octets ; `CJSON_NESTING_LIMIT=32`, pile de `main` à 8 Ko.
- [ ] 🧑 **Reste à valider sur cible** : appairage BLE complet depuis Chrome ; fenêtre BLE après
  un vrai refus WiFi (mauvais mot de passe) ; MP3 corrompu ou à 48 kHz sur la SD (F1/F3).
- [ ] 🤖 **F8-F12** — course des voix MP3, installation en cours de partie, `-Os`, console USB,
  `CONFIG_ESP_TASK_WDT_PANIC`.

Web :
- [x] 🤖 **W2** — zod sur `/api/box/auth` et `/register` (Vitest 84/84, lint et typecheck OK).
- [x] 🤖 **W3** — MP3 sans trame ou hors 44 100 Hz : erreur à la publication.
- [ ] 🤖 **W4** — plafond de taille décompressée du zip.

---

## 0. Avant de router la carte Main — maintenant

- [x] 🤖 **L1 = Sunlord SWPA5040S6R8MT, C36411** (H1, validé le 27/09) — valeur `6.8u`,
  champs MPN/LCSC, empreinte **importée par LCSC manager**
  (`C36411_IND-SMD_L5_0-W5_0_SWPA5040S`, avec modèle 3D ; pastilles identiques au catalogue
  Sunlord, 1,4 × 4,2 mm à ±1,85 mm). ⚠ Sa zone d'encombrement colle au corps (5 × 5 mm) :
  garder un peu d'air au placement.
- [x] 🤖 **C_B1 (entrée du MT3608) : 10 µF → 22 µF 16 V 0805** (27/09) — recommandé par la
  datasheet (« Capacitor Selection »).
- [x] 🤖 **J2 en JST-PH 4 broches** (H2, 27/09) — 1 BAT+, 2 BAT−, 3 NTC, **4 retour NTC sur
  GND**. Harnais de cellule à 4 fils.
- [x] ✅ **FS8205 (U10) : brochage confirmé** (H3, 27/09) sur la datasheet §4 (capture de
  Gilles) — le schéma est conforme.
- [x] 🤖 **J13 : 5ᵉ connecteur I2C** (H5, 27/09), JST-SH 4 en Qwiic, feuille `connector`.
- [x] 🤖 **Deux bouts de fil pendants supprimés** de la feuille `power` (+ leurs jonctions
  devenues inutiles) — `kicad_netlist.py --check` : aucun fil mal raccordé.
- [x] 🤖 **BTN1/BTN2 routés** (H4, décision du 27/09) — J8.5/J8.6 vers Côté 1, pull-down
  **R23/R24 10 kΩ**, boutons actifs hauts, jamais de pull-up sur GPIO45.
- [x] 🤖 **USB-C sur le satellite Côté 2** (H6, décision du 27/09) — J1, U12 (USBLC6) et
  R9/R10 (CC) retirés de la Main (BOM : « Satellite Cote 2 ») ; **J14** JST-PH 6 broches
  (VBUS, GND, D−, D+, GND, 5V_HOST) ; R11/R12 22 Ω restent au pied de l'ESP32 ;
  **R22 0 Ω DNP** réserve le 5 V pour un futur mode hôte.
- [ ] 🧑 **Dans KiCad** : recharger les feuilles `power` et `connector` (Fichier → Revenir)
  **avant toute sauvegarde**, relancer l'ERC, puis « Mettre à jour le PCB depuis le
  schéma » (F8) : L1 (5 × 5), J2 (4 broches), J13, J14, R22-R24 nouveaux ; J1 et U12
  disparaissent du PCB Main. Ensuite, commiter la sauvegarde KiCad du 27/09.
- [x] 🤖 **Validation schéma Main + Côté 2** (27/09, `2026-09-27-validation-main-cote2.md`) :
  0 fil mal raccordé, 0 court-circuit entre rails ; no_connect ajoutés sur `power` (U5 2-4, U6.4, U9 TD ;
  celui de U10 D12 retiré à tort puis remis) → **recharger `power`** avant l'ERC.
- [x] 🤖 **Pastilles UART0** TP_TXD0 / TP_RXD0 (nets UART0_TX / UART0_RX, pins 37/36 du
  module) posées le 27/09 → recharger `esp32` puis F8.
- [x] 🤝 **SW3 = E-Switch RR111C1921** (27/09 : bascule ronde de façade Ø 20 mm, non lumineuse),
  câblée en direct sur J12 par cosses faston 4,8 mm.
- [x] 🤝 **SW3 : contacts argent** → **R20 = 2,2 kΩ** (27/09, au lieu de 100 kΩ) : ~1,5-2,5 mA
  dans le contact box allumée, rien éteinte → recharger `power` puis F8.
- [ ] 🧑 Câble J14 ↔ Côté 2 J2 **broche à broche** (vérifier au montage) ; courant nominal
  d'un contact JST-PH (~1,1 A sur VBUS) à vérifier sur la datasheet JST.
- [ ] 🧑 Placement : **antenne en bord de carte**, opposée à la cellule, sans cuivre dessous ;
  15 mm de dégagement en boîtier (H7).

## 1. Carte Main — pendant le layout

- [ ] 🧑 U5 (AP2112M SO-8) : plan de cuivre généreux sous et autour.
- [ ] 🧑 bq24075 : pad exposé sur GND avec vias thermiques, VSS (broche 8) raccordée aussi.
- [ ] 🧑 Buck-boost TPS63020 : C_B1 (entrée) et C_B2-C_B4 (sortie) + L1 au plus près, deux boucles
  minimales, PGND / GND commande réunis en un point (guide §2.4).
- [x] ~~USBLC6 (U12) au plus près de J1~~ — déplacés sur le satellite Côté 2 le 27/09 (voir §3).
- [ ] 🧑 ICS-43434 : C3 au plus près des broches 5 et 3, sans via ; trou du micro à
  prévoir avec le boîtier (bottom-port en face Dessous, H13).
- [ ] 🧑 Audio dans un coin, pas de piste numérique dessous, **GND continu (pas de split)**,
  I2S court et groupé.
- [ ] 🧑 Piste EN (RC 10 k / 1 µF) courte.
- [ ] 🧑 Points de test : nom sérigraphié, groupés face Dessous, une masse qui accepte une pince.
- [ ] 🤝 Options peu coûteuses (H11, H12) : pastilles **TXD0/RXD0**, pull-up externe sur
  GPIO0, résistance série 1-10 kΩ sur la broche VSYS de J12.
- [ ] 🧑 DRC (règles JLCPCB), Gerbers, CPL, revue 3D — checklists du FSD §3.2.

## 2. BOM et documents hardware

- [x] 🤖 **BOM de référence corrigée** (H8, 2026-09-27) : U11 = LSM6DSOXTR, doublon U21
  retiré, R1/R2 470 Ω, C21/C22 2,2 nF C0G, TMAG5273A1 (C3716049), U10 SOT-23-6, bouche
  « à choisir », notes J7b/J8/J9/U3/R_PG, LCSC de D1/Q1/Q2 dans la bonne colonne ; ajoutés :
  CAP1298 (COTE2-U2), ST25DV (U27, absent), haut-parleur (LS1) ; découplage satellites 5 × 100 nF.
  Lignes satellites refaites le 27/09 d'après les schémas (préfixes DESSUS-/DEVANT-/COTE1-/COTE3-).
- [x] 🤖 **Une seule BOM** : `docs/pcb/02-bom-lcsc.csv` supprimée (2026-09-27).
- [ ] 🧑 **FS8205 (U10) : C32254 à 0 en stock chez JLCPCB** (27/09). Équivalents SOT-23-6 en
  stock : FS8205A TECH PUBLIC C2830320 (55 501), FS8205A FUXINSEMI C908265 (22 508), 8205A
  JSMSEMI C2762931… — **décision Gilles (27/09) : garder C32254 et attendre le retour du
  stock** ; revérifier au moment de la commande.
- [ ] 🤖 `check_bom.py` : comparer aussi la **MPN** et les valeurs des **lignes à
  plusieurs références** (il n'a vu ni U11 ni le filtre audio).
- [ ] 🤖 `docs/pcb/01-netlist.txt` : régénérer depuis `kicad_netlist.py` (ou l'archiver) —
  en retard sur KiCad (power, J8.5, J10/J12/Q1/Q2).
- [ ] 🤖 `docs/pcb/03-validation-qualite.md` : noms des TP → `TP_xxx1`.
- [ ] 🤝 Archiver `docs/schematics/*.txt` (schémas ASCII d'avant KiCad : ILI9488, PN532, AS5600…).
- [ ] 🧑 Champs LCSC manquants (~100 références) — **au moment de la commande**, décision du
  27/09 (appariement automatique JLCPCB des passifs courants).
- [ ] 🧑 E-Switch SW1/SW2 en finition **or** : confirmer la lettre du code de commande
  (illisible à l'extraction du PDF ; `100SP1T1B4M1QE` de la BOM non vérifié).

## 3. Satellites (à la conception)

Projets KiCad créés le 2026-09-27 : `hardware/devant/`, `dessus/`, `cote1/`, `cote2/`, `cote3/`
(bibliothèques partagées avec la Main, cartouche de spécification sur chaque feuille).
- [x] 🤖 **Côté 2 — partie USB dessinée** : J1 USB-C, U1 USBLC6 (sur les lignes brutes, côté
  connecteur), R1/R2 CC 5,1 kΩ, J2 vers J14 de la Main — netlist vérifiée.
- [x] 🤖 **Schémas Dessus, Devant, Côté 1, Côté 3 dessinés** (27/09) — netlists vérifiées par
  script (0 pin isolée, 0 net à plusieurs noms), BOM passée en références préfixées par face
  (`DESSUS-U1`, `DEVANT-LED1`, `COTE1-SL1`, `COTE3-U1`…) et recoupée avec les schémas.
  **Questions ouvertes : `docs/audits/2026-09-27-questions-satellites.md`** (modules des yeux,
  écran bouche, nombre de LED du halo, montage des toggles/boutons, antenne NFC…).
- [ ] 🧑 Recharger les 4 feuilles dans KiCad, mise en page, ERC, F8.
- [x] 🤖 Brochages corrigés dans les synthèses (27/09) : **ST25DV** 1 = V_EH, 2 = AC0 (l'inverse
  était écrit, figure 2 relue) ; **VEML7700** 2 = VDD, 4 = SDA. Les symboles LCSC étaient justes.
- [x] 🧑 Symboles MLX90614 (C2837265) et MTCH2120 (C45606479) importés par LCSC manager
  (2026-09-27) — brochages conformes, mais les deux composants sont remis en cause (ci-dessous).
- [x] 🤝 **MLX90614 retiré du produit** (2026-09-27) : trop cher (variante 3 V 6,6-8,9 $),
  bridait le bus à 100 kHz. Côté 3 = TMAG5273 + électrode de proximité capacitive.
- [x] 🤝 **Contrôleur tactile Côté 2 = CAP1298** (2026-09-27, SOIC-14, C2652072) : moins cher
  et disponible. Écartés : MTCH2120 (0 stock), MPR121 (fin de vie), CAP1214, SC12B, deux
  CAP12xx (0x28 fixe commune), MCU dédié (second firmware). Comparatif : synthèses
  CAP1298/CAP1296/SC12B/AT42QT2120/ATtiny1616 dans `docs/datasheets/`.
- [x] 🧑 CAP1298-1-SL-TR (C2652072) importé ; imports MLX90614 / MTCH2120 supprimés (27/09).
- [x] 🤝 **« Approche ta main » sur Côté 2** (27/09) : CAP1298 = 6 touches (CS1-4, CS6, CS7) +
  proximité CS8 gardée par SG (CS5) ; **Côté 3 = TMAG5273 seul**.
- [x] 🤖 **Côté 2 — partie tactile dessinée** (27/09) : U2 CAP1298, C1/C2 (100 nF + 1 µF),
  J3 Qwiic, électrodes E1-E8 (symboles TestPoint provisoires, hors BOM) — netlist vérifiée.
- [ ] 🧑 Côté 2 au layout : dessiner les électrodes en cuivre (6 touches, grande électrode de
  proximité E7 entourée de l'anneau de garde E8) — règles de dessin à prendre dans le guide
  Microchip des capteurs capacitifs (à télécharger et synthétiser avant le layout).
- [ ] 🤝 Modèles 3D des satellites : les empreintes LCSC pointent vers
  `${KIPRJMOD}/libs/lcsc/3dmodels`, donc introuvables depuis `hardware/<face>/` → déplacer
  les bibliothèques dans `hardware/libs/` (variable de chemin commune) le jour où l'on veut
  la 3D des satellites.

- [x] 🤖 **TMAG5273A1** (0x35) ; INT → GND (+ MASK_INTB côté firmware) ; 100 nF sur VCC — au schéma Côté 3.
- [x] 🤖 **BMP280** : CSB **directement** sur VDDIO, SDO → GND (0x76), 100 nF sur VDD et VDDIO — au schéma Devant.
- [x] 🤖 Brochage **Qwiic** sur chaque satellite (1 = GND, 2 = 3V3, 3 = SDA, 4 = SCL) — 5 satellites.
- [ ] 🧑 **Satellite Côté 2** : J1 USB-C + USBLC6 **au plus près du port** + CC 5,1 kΩ,
  connecteur vers J14 (JST-PH 6, fils de section adaptée à ~1,1 A sur VBUS, D+/D− torsadés),
  CAP1298 dessiné ; SW3 câblé en direct sur J12 (hors satellite).
- [ ] 🤝 **Énigme « clé USB » (mode hôte) — place réservée, pas câblée** : sur le satellite,
  commutateur de VBUS limité en courant alimenté par `5V_HOST` (R22 à monter), protection
  contre la réinjection dans le chargeur, CC côté source, commande par expander I2C (plus de
  GPIO libre). Côté firmware : bascule USB-Serial-JTAG ↔ OTG hôte (console USB perdue en
  mode hôte).
- [ ] 🧑 Satellite Dessus : dessiner l'antenne NFC ANT1 au layout (boucle ~4,8 µH, zone de garde ; empreinte à créer).
- [ ] 🧑 Au proto : **temps de montée du bus I2C** ≤ 1000 ns à 100 kHz (≤ ~250 pF avec
  4,7 kΩ) ; passer en 2,2 kΩ si besoin.

## 4. Firmware (🤖, sur validation)

Adaptation au hardware actuel :
- [ ] **LEDs** (F1) : 12 LEDs + chaîne J9, timings V5 (0,3/0,9 et 0,8/0,6 µs),
  **plafond global de luminosité** (budget DW01A), mutex autour de `hal_leds_show`.
- [ ] **Audio** (F2) : plafond de volume vers **−22 dB** (ou gain analogique −6 dB +
  plafond réduit), courbe de volume utilisable, **mixage mono sur L** (un seul haut-parleur).
- [ ] **Batterie basse** (F3) : lecture VBAT_SENSE (ADC1_CH2), extinction propre vers
  3,4-3,5 V.
- [ ] **Bus I2C** (F4) : tout à 100 kHz (`hal_imu` est à 400 kHz) ; aucun
  `ESP_ERROR_CHECK` sur un accès I2C dans `hal_imu` et `hal_light` (satellite absent ⇒
  reboot en boucle aujourd'hui).
- [ ] **Pilote CAP1298** (F5 ; le pilote MTCH2120 a été retiré le 15/11) : adresse 0x28,
  registres 8 bits, scrutation (ALERT# non remontée) — garder l'API commune de `hal_touch`.
- [ ] **Drivers à écrire** : LSM6DSOX sur la Main (brancher `hal_imu`, fin de l'inclinaison
  simulée), TMAG5273 (MASK_INTB), BMP280, ADS7830, ST25DV
  (remplace `hal_nfc` PN532), micro ICS-43434, écran bouche (quand il sera choisi).
- [ ] **Boutons GPIO45/46** (F6) — lire BTN1/BTN2 (actifs hauts, pull-down externe) ; ne
  jamais exiger d'appui au démarrage (strapping).
- [ ] **PCM5122 : lire le verrouillage PLL au registre 4, bit 4 (0 = verrouillé)** (F7) —
  le code lit le registre réservé 0x05 ; compléter `pcm5122-registers.md`.
- [ ] `yaml2json.py` : ajouter `eye_blink`/`eye_emotion`/`eye_look`, retirer `servo` (F8).
- [ ] SPI2 partagé : monter la SD avant tout échange avec l'écran bouche (CS écran haut).

Hérités du 23/09 :
- [ ] **WiFi** : reconnexion après 5 échecs + **sync périodique**.
- [ ] **BLE** : LE Secure Connections (`sm_sc = 1`) + fermer la fenêtre après `wifi_ok`.
- [ ] **F5 E2E** : parcours navigateur complet (Chrome → `/devices/add`) à valider.
- [ ] **OTA (F6)** : `esp_ota_mark_app_valid_cancel_rollback()`, vérification
  sha256/signature de l'URL, retrait du MP3 embarqué (1 Mo ; binaire à 2,63 Mo pour 3 Mo).
- [ ] Raccourcis debug (touches 9/10/11 maintenues) derrière le mode dev ; borne sur
  `count` (`action_flash`) ; `voice_stop_wait` qui peut expirer (buffer réalloué).
- [ ] Hygiène : licences cJSON/NimBLE dans `THIRD_PARTY_LICENSES`, commiter
  `dependencies.lock`, code mort (LVGL, `hal_nfc`, `/components` vide), coredump (option).
- [ ] **Auto-test firmware** (scan I2C, SD, écrans, audio en boucle acoustique, LEDs,
  IMU, ADC) — remplace l'essentiel des mesures manuelles de la validation.

## 5. Web et base de données

- [x] 🤝 **Licences liées au compte** (E1, table `licenses`) — vérifié le 14/11.
- [x] 🤖 **Migrations SQL versionnées** (0001-0004) — vérifié le 14/11.
- [x] 🤖 `used`/`active` NOT NULL ; index `devices.owner_id` ; `/register` : 409 sur doublon.
- [ ] 🤖 `firmware_releases` : UNIQUE(version, channel) ; `sha256` NOT NULL (appliqué seulement
  si aucune ligne n'est vide).
- [x] 🤖 Version de scénario en un seul endroit (`scenario_versions`, manifest recalculé à l'envoi).
- [x] 🤖 Clé JWT dérivée + `iss`/`aud` ; révocation effective (propriétaire et licence relus à
  chaque requête).
- [ ] 🤖 **Rate limiting `/api/box/*`** (WB-11) — `/challenge` écrit en base sans authentification
  (W1 de l'audit du 14/11) : le point le plus exposé.
- [x] 🤖 En-têtes de sécurité (CSP, HSTS). `shadcn` en devDependencies + `npm audit` : non revérifié.
- [x] 🤖 CI : web, crypto, tests host.

## 6. Sécurité produit (avant la vente)

- [ ] 🤝 Secure Boot + flash encryption (secret de la box en clair aujourd'hui).
- [ ] 🤝 Signature des scénarios et du firmware (FW-02, WB-05) ; chiffrement/liaison des
  packages à la box (R-04) — choix business.

## 7. Datasheets manquantes

- [ ] 🧑 **Cellule 18650** retenue (LG MJ1 / Samsung 35E / Panasonic NCR18650B) : courant
  max, **température de charge max** (souvent 45 °C, contre 50 °C pour la fenêtre du
  bq24075, H14) ; décider support à ressorts (proto) ou languettes (série).
- [ ] 🧑 **NTC** collée sur la cellule (courbe B, R25).
- [ ] 🧑 **Module écran rond GC9A01** (rétroéclairage, régulateur éventuel, courant sur 3V3_D).
- [ ] 🧑 Écran bouche (pas encore choisi) ; carte microSD (consommation, découplage au slot).

## 8. Validation et contrôle qualité

Document : `docs/pcb/03-validation-qualite.md`.
- [ ] 🧑 Relever les valeurs de référence sur la première carte saine.
- [ ] 🧑 Mesures ciblées par l'audit : rendement réel du TPS63020 (batterie 3,0-4,2 V et USB) ; **charge crête** (LEDs
  blanches + audio + WiFi, batterie basse) sans coupure DW01A ; rail 5 V ≤ 5,2 V à vide (TPS63020 en
  power save, H9 normalement soldé) ; charge sur un port PC 500 mA (H10) ; consommation éteinte
  ≤ 20 µA ; thermique de U5 et du bq24075 sous USB en boîtier fermé.

## 9. Divers

- [ ] 🤝 **Provisioning Wi-Fi par BLE (`/devices/add`) : ne fonctionnait pas** au dernier essai
  de Gilles (souvenir, 2026-09-30) — à contrôler (logs `ble_prov`, Chrome, fenêtre 5 min) quand
  le firmware reprend.

- [ ] 🧑 Réassigner la box de test du compte `qwe@qwe.com` à ton vrai compte (SQL dans Studio,
  ou supprimer la ligne `devices` puis refaire l'appairage BLE depuis `/devices/add`).
- [ ] 🧑 Replanifier les jalons M2/M3 du FSD §3.0 (dates dépassées).

---

## ✅ Soldé récemment (pour mémoire)

- **Hardware Main (27/09)** : C20 sur VNEG–GND, J10 + FB3-FB6, J3-J6 en Qwiic, pull-ups SD,
  R13/R14 22 Ω sur les horloges SPI, R_TMR 56 k, NTC sur TS (R_TS en DNP), marche/arrêt
  (EN_SYS + load switch Q1/Q2), D1 SS34, U5 en SO-8, VBAT_SENSE retiré de J8.5,
  26 points de test, 53 erreurs ERC traitées. Constats A4 (ICS-43434) et LSM6DSOX 10 µF
  reconnus erronés. **Tous vérifiés sur la netlist le 27/09.**
- **Logiciel (23/09)** : signatures `auth`/`register`, partition `box_nvs`, boot sans
  DAC/écran, validateur de scénario (26 cas), preuve d'appairage — validés sur cible.
- **Docs (27/09)** : FSD nettoyé (v0.3), synthèse `LSM6DSOXTR.md` corrigée (brochage),
  CLAUDE.md pointé vers l'audit du 27/09.
