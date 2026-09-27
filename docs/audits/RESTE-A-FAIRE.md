# Reste à faire

> Document vivant, **recréé le 2026-09-27** à partir de l'audit complet du jour
> (`2026-09-27-audit.md`). Les numéros (H1, F2…) renvoient à ce rapport ; le détail des
> points hérités est dans `2026-09-23-audit.md` et `2026-09-23-hardware-db.md`.
> Cocher ici au fil de l'eau.
>
> **Qui** : 🧑 Gilles (KiCad, choix, service externe) · 🤖 Claude (code / doc, sur
> validation) · 🤝 à trancher ensemble.

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
- [ ] 🤝 **SW3 : choisir un interrupteur plus haut de gamme** (à accrochage, contacts bas niveau),
  câblé en direct sur J12.
- [ ] 🧑 Câble J14 ↔ Côté 2 J2 **broche à broche** (vérifier au montage) ; courant nominal
  d'un contact JST-PH (~1,1 A sur VBUS) à vérifier sur la datasheet JST.
- [ ] 🧑 Placement : **antenne en bord de carte**, opposée à la cellule, sans cuivre dessous ;
  15 mm de dégagement en boîtier (H7).

## 1. Carte Main — pendant le layout

- [ ] 🧑 U5 (AP2112M SO-8) : plan de cuivre généreux sous et autour.
- [ ] 🧑 bq24075 : pad exposé sur GND avec vias thermiques, VSS (broche 8) raccordée aussi.
- [ ] 🧑 Boost : boucle SW → L1 → D1 → C_B2 → GND la plus courte ; C_B1 au pied de IN.
- [ ] 🧑 USBLC6 (U12) au plus près de J1.
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
- [ ] 🧑 E-Switch SW1/SW2/SW3 en finition **or**.

## 3. Satellites (à la conception)

Projets KiCad créés le 2026-09-27 : `hardware/devant/`, `dessus/`, `cote1/`, `cote2/`, `cote3/`
(bibliothèques partagées avec la Main, cartouche de spécification sur chaque feuille).
- [x] 🤖 **Côté 2 — partie USB dessinée** : J1 USB-C, U1 USBLC6 (sur les lignes brutes, côté
  connecteur), R1/R2 CC 5,1 kΩ, J2 vers J14 de la Main — netlist vérifiée.
- [ ] 🤝 Dessin face par face (🤖 schéma + vérification netlist, 🧑 mise en page + ERC) :
  Côté 2 (USB + tactile dessinés ; SW3 en câble direct sur J12), Côté 3 (TMAG5273 seul), Côté 1 (ADS7830, faders, pots,
  toggles, boutons), Dessus (ST25DV + antenne, base `hardware/main/.history/nfc.kicad_sch`),
  Devant (écrans, BMP280, VEML7700 — bloqué par la fiche du module GC9A01 et le choix de
  l'écran bouche).
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

- [ ] 🧑 **TMAG5273A1** (0x35) ; INT → GND (+ MASK_INTB côté firmware) ; ≥ 10 nF sur VCC.
- [ ] 🧑 **BMP280** : CSB **directement** sur VDDIO, SDO → GND (0x76), 100 nF sur VDD et VDDIO.
- [ ] 🧑 Brochage **Qwiic** sur chaque satellite (1 = GND, 2 = 3V3, 3 = SDA, 4 = SCL).
- [ ] 🧑 **Satellite Côté 2** : J1 USB-C + USBLC6 **au plus près du port** + CC 5,1 kΩ,
  connecteur vers J14 (JST-PH 6, fils de section adaptée à ~1,1 A sur VBUS, D+/D− torsadés),
  CAP1298 dessiné ; SW3 câblé en direct sur J12 (hors satellite).
- [ ] 🤝 **Énigme « clé USB » (mode hôte) — place réservée, pas câblée** : sur le satellite,
  commutateur de VBUS limité en courant alimenté par `5V_HOST` (R22 à monter), protection
  contre la réinjection dans le chargeur, CC côté source, commande par expander I2C (plus de
  GPIO libre). Côté firmware : bascule USB-Serial-JTAG ↔ OTG hôte (console USB perdue en
  mode hôte).
- [ ] 🧑 Satellite Dessus : antenne NFC ST25DV (boucle, zone de garde) ; reprendre `nfc.kicad_sch`.
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
- [ ] **Pilote CAP1298** (F5, remplace le pilote MTCH2120 devenu obsolète) : adresse 0x28,
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

- [ ] 🤝 **Licences liées à l'utilisateur plutôt qu'à la box** — à trancher **avant Stripe**.
- [ ] 🤖 **Migrations SQL versionnées** (`supabase/migrations/`) — 🧑 passer dans Studio
  les requêtes d'extraction du rapport `2026-09-23-hardware-db.md` et coller le résultat.
- [ ] 🤖 `firmware_releases.sha256` NOT NULL + UNIQUE(version, channel) ; `used`/`active`
  NOT NULL ; index `devices.owner_id` ; `/register` : 409 au lieu de 500 sur doublon (23505).
- [ ] 🤖 Version de scénario en un seul endroit (le serveur lit le manifest).
- [ ] 🤖 Clé JWT dérivée (`HKDF(master, "escapebox:jwt")`) + `iss`/`aud` ; révocation (WB-12).
- [ ] 🤖 Rate limiting `/api/box/*` (WB-11 : 10 req/min par box).
- [ ] 🤖 `shadcn` en devDependencies + `npm audit fix` ; en-têtes de sécurité (CSP, HSTS).
- [ ] 🤖 CI : tests host, crypto, `tsc`/`lint`, validation des scénarios.

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
- [ ] 🧑 Mesures ciblées par l'audit : rendement réel du MT3608 ; **charge crête** (LEDs
  blanches + audio + WiFi, batterie basse) sans coupure DW01A ; rail 5 V ≤ 5,3 V avec
  plusieurs chargeurs (H9) ; charge sur un port PC 500 mA (H10) ; consommation éteinte
  ≤ 20 µA ; thermique de U5 et du bq24075 sous USB en boîtier fermé.

## 9. Divers

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
