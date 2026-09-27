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

- [ ] 🧑 **L1 : inductance du boost** (H1) — proposition sourcée : **Sunlord
  SWPA5040S6R8MT, C36411** (6,8 µH ±20 %, Isat 2,9 A, 5 × 5 × 4 mm), synthèse
  `docs/datasheets/SWPA5040S.md`. À valider, puis remplacer le 1210 dans KiCad.
- [ ] 🧑 **C_B1 (entrée du MT3608) : 10 µF → 22 µF** — la datasheet recommande 22 µF
  céramique en entrée **et** en sortie (« Capacitor Selection ») ; C_B2 fait déjà 22 µF.
- [ ] 🧑 **J2 en 4 broches** (H2) — BAT+, BAT−, NTC, **retour NTC sur GND** : aujourd'hui la
  NTC revient sur `VBAT-`, ce qui décale la coupure en surchauffe d'environ +7 °C à 1 A et
  la supprime pendant la récupération d'une décharge profonde.
- [ ] 🧑 **FS8205 (U10) : confirmer le brochage SOT-23-6 à l'œil** dans le PDF (H3) — il a
  été déduit, jamais lu. Vérifier aussi sur LCSC que C32254 est bien en SOT-23-6.
- [ ] 🤝 **BTN1/BTN2 (GPIO45/46)** (H4) — les router vers J8.5/J8.6 (Côté 1 : boutons
  actifs hauts, pull-down externe autorisée, **jamais de pull-up sur GPIO45**) ou les
  laisser en réserve avec une pastille.
- [ ] 🤝 **5ᵉ connecteur I2C ou chaînage** (H5) — 5 faces satellites en I2C pour 4
  connecteurs J3-J6.
- [ ] 🤝 **Emplacement du port USB-C** (H6) — le FSD le place sur Côté 2, mais J1
  (vertical) est sur la Main, en face Dessous.
- [ ] 🧑 Placement : **antenne en bord de carte**, opposée à la cellule, sans cuivre dessous ;
  15 mm de dégagement en boîtier (H7).
- [ ] 🧑 Nettoyer les **deux bouts de fil pendants** de la feuille `power` (sur `C_U9` et
  sur le corps de `R_CS`), relancer l'ERC, puis **commiter la sauvegarde KiCad** du
  27/09 (annotation des TP en `TP_xxx1`).

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

- [ ] 🤖 **Corriger la BOM de référence** `hardware/main/BOM/02-bom-lcsc.csv` (H8) :
  **U11 = LSM6DSOXTR (pas un PN532)**, doublon U21 à retirer, **R1/R2 = 470 Ω, C21/C22 =
  2,2 nF NP0/C0G**, TMAG5273**A1**, U10 en SOT-23-6, bouche « non choisie » (pas SSD1680),
  notes J7b/J8/J9/U3/L1/R_PG/R_CHG, LCSC de D1/Q1/Q2 dans la bonne colonne, TP en
  `TP_xxx1`, haut-parleur ajouté.
- [ ] 🤖 **Une seule BOM** : supprimer `docs/pcb/02-bom-lcsc.csv` (périmée) ou la générer.
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

- [ ] 🧑 **MLX90614 variante Bxx (3 V)** — vérifier la référence LCSC C58661 ; 100 nF sur VDD.
- [ ] 🧑 **TMAG5273A1** (0x35) ; INT → GND (+ MASK_INTB côté firmware) ; ≥ 10 nF sur VCC.
- [ ] 🧑 **BMP280** : CSB **directement** sur VDDIO, SDO → GND (0x76), 100 nF sur VDD et VDDIO.
- [ ] 🧑 Brochage **Qwiic** sur chaque satellite (1 = GND, 2 = 3V3, 3 = SDA, 4 = SCL).
- [ ] 🤝 **Énigme « clé USB »** : le mode host exige de fournir le VBUS → à concevoir ou
  abandonner, lié à H6.
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
- [ ] **MTCH2120** (F5) : adresse 0x20, adressage mémoire 16 bits (DEVID 0x0000, BTNSTA
  0x0102) — confirmer d'abord l'ordre des octets (figure 3-5 / driver Microchip).
- [ ] **Drivers à écrire** : LSM6DSOX sur la Main (brancher `hal_imu`, fin de l'inclinaison
  simulée), TMAG5273 (MASK_INTB), MLX90614 (SMBus + PEC), BMP280, ADS7830, ST25DV
  (remplace `hal_nfc` PN532), micro ICS-43434, écran bouche (quand il sera choisi).
- [ ] Boutons GPIO45/46 (F6) — selon la décision H4.
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

- [ ] 🧑 **Inductance L1** (dès le choix, H1).
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
