# Reste à faire — synthèse des audits

> Document vivant, consolidé à partir de `2026-09-23-audit.md` (logiciel) et
> `2026-09-23-hardware-db.md` (hardware rév. 2.1 + DB). Cocher ici au fil de l'eau ;
> le détail, les sources datasheet et les calculs sont dans les rapports.
> Dernière mise à jour : 2026-09-27.
>
> **Qui** : 🧑 Gilles (KiCad, décision, action sur un service) · 🤖 Claude (code / doc,
> sur validation) · 🤝 à trancher ensemble.

## ✅ Déjà fait (pour mémoire)

- Logiciel : 5 critiques corrigés, **validés sur cible** (box ESP32S3-8FF7-D684) :
  signatures `auth`/`register`, partition `box_nvs`, boot sans DAC/écran, validateur de
  scénario, preuve d'appairage juste avant `/register`.
- Hardware : J3-J6 passés en brochage Qwiic (patch `docs/pcb/kicad-patches/connector.kicad_sch`).
- Docs : 32 datasheets + synthèses `.md`, règle « datasheet d'abord » dans CLAUDE.md, FSD à jour.

---

## 1. Hardware — PCB Main (avant le layout)

### 🔴 Bloquant — ✅ soldé le 2026-09-27
- [x] 🧑 **C20 (PCM5122)** : déplacé sur **VNEG–GND** (vérifié par netlist : C19 reste le
  condensateur volant CAPP–CAPM).
- [x] 🤖 **Connecteur haut-parleur** : **J10** (JST PH 4 broches) + **FB3-FB6**
  (BLM21PG221SN1D, 220 Ω @ 100 MHz, 2 A — `docs/datasheets/BLM21PG.md`) sur la feuille
  `audio`. Chaîne vérifiée : `U3.OUT → HP_x → FBn → SPK_x → J10.n`.
  Haut-parleur retenu : **PUI AS04008PO-2-R** 8 Ω (`docs/datasheets/AS04008PO.md`).
  ⚠ Reste à faire côté KiCad : **ouvrir la feuille `audio` et lancer l'ERC** pour valider
  le rendu et le placement du bloc (inséré par script, électriquement juste).
- [x] 🤖 **Patch J3-J6 reporté** dans le projet KiCad (pin 1 = GND, 2 = 3V3_D, vérifié).

### 🟠 Important
- [x] 🤖 **Carte SD : 5 × 10 kΩ vers 3V3_D** — R15-R19 sur un rail commun, feuille
  `connector` (2026-09-27). DAT1 et DAT2 n'avaient aucun fil : nets nommés `SD_DAT1` /
  `SD_DAT2` au passage. Source : ESP-IDF `sd_pullup_requirements.rst`.
- [x] 🤖 **Horloges SPI** : FB1/FB2 remplacées par **R13/R14 = 22 Ω** 0603 (mêmes pins,
  fils inchangés) + **C6/C7 10 pF en DNP** vers GND sur SPI2_SCLK / SPI3_SCLK (2026-09-27).
- [x] 🤖 **Charge LiPo — timers** : **R_TMR 56 kΩ** 1 % entre TMR et GND, feuille `power`
  (2026-09-27) → t_MAXCHG 5,6-9,3 h au lieu de timers désactivés.
- [x] ⚠️ **ICS-43434 100 nF : constat A4 infondé** — le découplage existait déjà (`C3`,
  100 nF 0402 sur 3V3_A, à côté de U4), vérifié par netlist sur le fichier du 23/09.
  Reste une **contrainte de layout** : au plus près des broches 5 et 3, sans via.
- [x] 🤖 **Bouton marche/arrêt — option B appliquée** (2026-09-27, feuille `power`) :
  net `EN_SYS` commun aux EN de U5/U6/U7 (ils étaient câblés sur VSYS), **R20 100 kΩ** en
  pull-down, **J12** (JST-SH 2 broches) vers l'interrupteur **SW3** (E-Switch 100, contacts
  or), et load switch **Q1 = AO3401A** sur le rail 5 V commandé par **Q2 = 2N7002** +
  **R21 100 kΩ**. Le rail est coupé en `5V_BOOST` (D1, C_B2, feedback R_FB_H) et `5V`
  (12 WS2812, PAM8406, J9). Fiches : `AO3401A.md`, `2N7002.md`.
  ⚠ À vérifier au proto : la consommation réelle box éteinte (attendu ~10-15 µA).
- [x] 🤖 **Charge — NTC** (2026-09-27) : **J2 passé en JST-PH 3 broches** (BAT+, BAT−, NTC),
  net `BAT_TS`, `R_TS` conservée en **DNP**. Batterie retenue : **cellule 18650 3400-3500 mAh**
  avec NTC collée. R_ISET (890 Ω → 1 A) et R_TMR (56 kΩ) restent valables tels quels pour
  cette capacité — une seconde cellule aurait imposé de les revoir (charge en 8 h).
  🧑 Reste : choisir la cellule (LG MJ1 / Samsung 35E / Panasonic NCR18650B) et récupérer
  sa datasheet ; décider support à ressorts (proto) ou languettes soudées (série).
- [x] 🤖 **D1 SS14 → SS34** (2026-09-27) : symbole `Diode:SS34` + LCSC **C8678**
  (Basic part JLCPCB), même empreinte SMA, BOM mise à jour (`docs/datasheets/SS34.md`).
  ⚠ Le PDF local est celui de Vishay (famille SS32-SS36) : LCSC bloque le téléchargement
  de la fiche MDD, et les tableaux du PDF Vishay ne sont pas extractibles en texte → pour
  un chiffre critique, ouvrir la fiche MDD depuis la page LCSC.
- [ ] 🧑 **Objectif de 1,9 A sur J9 à revoir** (indépendant de la diode) : sur batterie,
  cela ferait ~3,1 A côté cellule, au-delà du seuil de coupure DW01A (1,6-3,2 A).
- [x] 🤖 **3V3_D** : U5 passé en **SO-8** (`AP2112M-3.3TRG1`, LCSC C5290219, θJA 114 °C/W)
  le 2026-09-27 — meilleur que le SOT-89-5 envisagé, et disponible chez LCSC. U6 reste en
  SOT-25. Symbole créé dans la bibliothèque projet (absent de la bibliothèque KiCad).
  🧑 Reste à faire au layout : **plan de cuivre généreux** sous et autour de U5.

### 🟡 Faible
- [x] ❌ **LSM6DSOX 10 µF : constat erroné** (2026-09-27) — la datasheet ST ne demande que
  100 nF sur VDD et 100 nF sur VDDIO (figure 24), déjà présents (C4/C5). Le « 10 µF » venait
  d'une affirmation non sourcée de notre synthèse. Rien à faire.
- [ ] 🧑 USBLC6 au plus près de J1 (layout).
- [ ] 🧑 Champs LCSC manquants (passifs, U1, U2, U5-U10, U12).
- [ ] 🧑 E-Switch SW1/SW2 en finition **or**.
- [ ] 🤝 VBAT_SENSE sur J8.5 : le garder ou le retirer du connecteur.
- [ ] 🧑 Mettre à jour la section « power » de `docs/pcb/01-netlist.txt` (en retard sur le KiCad) ;
  lancer l'**ERC KiCad**.

## 2. Hardware — satellites (à la conception)

- [ ] 🧑 **MLX90614 : variante 3 V (Bxx)** — vérifier la référence LCSC C58661.
- [ ] 🧑 **TMAG5273 : variante A1** (adresse 0x35) ; INT → GND.
- [ ] 🧑 **BMP280** : CSB **directement** sur VDDIO, SDO → GND, 100 nF sur VDD et VDDIO.
- [ ] 🧑 Satellites en **brochage Qwiic** (1 = GND, 2 = 3V3, 3 = SDA, 4 = SCL).
- [ ] 🤝 **Énigme « clé USB »** : le mode host exige de fournir le VBUS → à concevoir ou à abandonner
  avant le routage de Côté 2.
- [ ] 🧑 Au proto : **mesurer la capacité / le temps de montée du bus I2C** (≤ 1000 ns à 100 kHz,
  soit ≤ ~250 pF avec 4,7 kΩ) ; passer en 2,2 kΩ si besoin.

## 3. Firmware (🤖, sur validation)

- [ ] **Driver MTCH2120** : adresse **0x20** + adressage mémoire 16 bits (DEVID 0x0000, BTNSTA 0x0102)
  — confirmer d'abord l'ordre des octets (figure 3-5 / driver Microchip).
- [ ] **WS2812** : timings V5 (bit0 0,3/0,9 µs, bit1 0,8/0,6 µs) + 12 LEDs au lieu de 1.
- [ ] **Volume** plafonné vers **−22 dB** (gain fixe 24 dB du PAM8406 + nominal 1 W du
  haut-parleur 8 Ω retenu — calcul dans `docs/datasheets/PAM8406.md`).
- [ ] **Extinction sur batterie basse** (VBAT_SENSE, ~3,4-3,5 V : MTCH2120 et ESP32 ≥ 3,0 V).
- [ ] **WiFi** : reconnexion après 5 échecs + **sync périodique**.
- [ ] SPI2 partagé : monter la SD avant l'écran bouche ; TMAG5273 MASK_INTB ; MLX90614 avec PEC.
- [ ] Raccourcis debug (touches maintenues 9/10/11) derrière le mode dev ; borne sur `count` (flash LED).
- [ ] BLE : LE Secure Connections (`sm_sc = 1`) + fermer la fenêtre après `wifi_ok`.
- [ ] OTA (F6) : `esp_ota_mark_app_valid_cancel_rollback()`, vérification sha256/signature,
  retirer le MP3 embarqué (1 Mo, déjà sur SD).
- [ ] Hygiène : licences cJSON/NimBLE dans `THIRD_PARTY_LICENSES`, commiter `dependencies.lock`,
  code mort (LVGL, hal_imu/nfc/light, `/components` vide).

## 4. Web & base de données

- [ ] 🤝 **Licences liées à l'utilisateur plutôt qu'à la box** — à trancher **avant Stripe**.
- [ ] 🤖 **Migrations SQL versionnées** (`supabase/migrations/`) — 🧑 Gilles passe les requêtes
  d'extraction du rapport hardware-db dans Studio et colle le résultat.
- [ ] 🤖 `firmware_releases.sha256` NOT NULL + UNIQUE(version, channel) ; `used`/`active` NOT NULL ;
  index `devices.owner_id` ; `/register` : 409 au lieu de 500 sur doublon.
- [ ] 🤖 Version de scénario en un seul endroit (le serveur lit le manifest).
- [ ] 🤖 Clé JWT dérivée (`HKDF(master, "escapebox:jwt")`) + `iss`/`aud`.
- [ ] 🤖 Rate limiting `/api/box/*` (FSD WB-11 : 10 req/min par box).
- [ ] 🤖 `shadcn` en devDependencies + `npm audit fix` ; en-têtes de sécurité (CSP, HSTS).
- [ ] 🤖 CI : tests host, crypto, `tsc`/`lint`, validation des scénarios.

## 5. Sécurité produit (avant la vente)

- [ ] 🤝 Secure Boot + flash encryption (secret de la box en clair aujourd'hui).
- [ ] 🤝 Chiffrement / liaison des packages de scénario à la box (FSD R-04) — choix business.

## 6. Datasheets encore manquantes

- [ ] 🧑 Module écran rond GC9A01 retenu (rétroéclairage, régulateur éventuel).
- [ ] 🧑 Batterie 3000 mAh retenue (NTC, courant max, protection intégrée).
- [x] ✅ Haut-parleur : **PUI AS04008PO-2-R** (`docs/datasheets/AS04008PO.md`).
- [x] ✅ Ferrites de sortie audio : **Murata BLM21PG** (`docs/datasheets/BLM21PG.md`).
- [ ] 🧑 Écran bouche (pas encore choisi) ; carte microSD (consommation).
- [x] ✅ SS34 (`SS34.md`), AO3401A (`AO3401A.md`), 2N7002 (`2N7002.md`) — récupérées.

## 6b. Validation et contrôle qualité (nouveau, 2026-09-27)

Document : **`docs/pcb/03-validation-qualite.md`** (squelette).

- [ ] 🤝 **Valider la liste des points de test** — ils doivent être posés **avant le
  routage**, on ne les ajoute pas après coup. 8 obligatoires (rails + masses), 6 de
  diagnostic, 6 de signaux, 4 déjà au schéma.
- [ ] 🤖 Ajouter les symboles `TestPoint` retenus au schéma, une fois la liste validée.
- [ ] 🧑 Au routage : sérigraphier le nom de chaque pastille, les grouper face Dessous,
  prévoir au moins une masse acceptant une pince.
- [ ] 🤖 **Auto-test firmware** (scan I2C, SD, écrans, audio en boucle acoustique, LEDs,
  IMU, ADC) — remplace l'essentiel des mesures manuelles de la phase 4.
- [ ] 🧑 Relever les valeurs de référence sur la première carte saine.

## 7. Divers

- [ ] 🧑 Réassigner la box de test du compte `qwe@qwe.com` à ton vrai compte (SQL dans Studio,
  ou supprimer la ligne `devices` puis refaire l'appairage BLE depuis `/devices/add`).
