# Reste à faire — synthèse des audits

> Document vivant, consolidé à partir de `2026-09-23-audit.md` (logiciel) et
> `2026-09-23-hardware-db.md` (hardware rév. 2.1 + DB). Cocher ici au fil de l'eau ;
> le détail, les sources datasheet et les calculs sont dans les rapports.
> Dernière mise à jour : 2026-09-23.
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

### 🔴 Bloquant
- [ ] 🧑 **C20 (PCM5122)** : le déplacer de CAPM–VNEG vers **VNEG–GND** (rail −3,3 V non découplé).
- [ ] 🧑 **Connecteur haut-parleur** (HP_L±, HP_R± reliés à rien) + **ferrite sur chaque fil**.
- [ ] 🧑 **Reporter le patch J3-J6** dans le projet KiCad.

### 🟠 Important
- [ ] 🧑 **Bouton marche/arrêt** (face Côté 2) — option recommandée : net EN_SYS vers les EN
  de U5/U6/U7 + pull-down 100 kΩ + **MOSFET P sur le rail 5 V** (le boost laisse passer VBAT
  même désactivé). Alternative simple : SYSOFF (mais pas de charge quand la box est éteinte).
- [ ] 🧑 **Charge LiPo** : TMR → **R 56 kΩ** (timers 5,6-9,3 h) ; **batterie 3 fils avec NTC 10 kΩ**
  sur TS (J2 en 3 broches, retirer R_TS). Option : R_ISET 1,27 kΩ (0,7 A) pour chauffer moins.
- [ ] 🧑 **D1 SS14 → SS34** (1 A insuffisant avec LEDs + audio) ; revoir l'objectif de 1,9 A sur J9.
- [ ] 🧑 **3V3_D** : AP2112 en **SOT-89-5** (θJA 120 au lieu de 184 °C/W) + cuivre autour.
- [ ] 🧑 **Carte SD** : 5 × **10 kΩ** vers 3V3_D (CS, CMD, DAT0, DAT1, DAT2).
- [ ] 🧑 **ICS-43434** : **100 nF** au pied de VDD.
- [ ] 🧑 **Horloges SPI** : FB1/FB2 → **22-33 Ω**, 10 pF en DNP.

### 🟡 Faible
- [ ] 🧑 LSM6DSOX : ajouter **10 µF** sur VDD.
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
- [ ] **Volume** plafonné vers −20 dB (gain fixe 24 dB du PAM8406).
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
- [ ] 🧑 Écran bouche et haut-parleur (pas encore choisis) ; carte microSD (consommation).

## 7. Divers

- [ ] 🧑 Réassigner la box de test du compte `qwe@qwe.com` à ton vrai compte (SQL dans Studio,
  ou supprimer la ligne `devices` puis refaire l'appairage BLE depuis `/devices/add`).
