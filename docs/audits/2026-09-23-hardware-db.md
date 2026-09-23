# Audit hardware (KiCad Main) + schéma DB — 2026-09-23 · révision 2

> **Révision 2 (même jour)** : chaque constat est désormais **vérifié dans la datasheet**
> (synthèses `docs/datasheets/*.md`, source citée entre crochets). La révision 1 contenait
> des erreurs faute de vérification : H1 (GPIO45/46) déclassé, H6 (niveau WS2812) retiré,
> H11 (courants LEDs) recalculé, découplage SD retiré (non sourcé). Règle ajoutée à
> CLAUDE.md : aucun retour sans vérification datasheet.

## Méthode

- Feuilles KiCad 10 : `blackbox` (racine), `power`, `esp32`, `imu`, `connector`, `audio`.
- Netlist extraite par script (pins, fils, labels, jonctions, transformations de symboles,
  fusion des global labels entre feuilles) : 107 composants. Pas d'ERC/DRC KiCad (pas de
  kicad-cli dans le container), pas de layout.
- Datasheets relues : ESP32-S3, WROOM-1, AP2112, MT3608, SS14, bq24075, DW01A, FS8205,
  USBLC6-2, PCM5122, PAM8406, ICS-43434, WS2812B V5, LSM6DSOX, TF-01A (plan seul) +
  doc ESP-IDF (`sd_pullup_requirements.rst`, `sdspi_share.rst`). FSD et vision relus.
- **Datasheets absentes** (constats correspondants non conclus) : modules GC9A01, carte
  microSD (SD Physical Layer), écran bouche (TBD), MTCH2120, MPR121, BMP280, MLX90614,
  TMAG5273, connecteurs JST.

**Pinout firmware ↔ schéma : cohérent** (I2S0 4/5/6, I2S1 16/18/7, SPI3 38/39/40/14/41/42,
SD 11/15/12/47, I2C 21/17, WS2812 48, LSM6DSOX 0x6A, PCM5122 0x4C). GPIO35-37 libres (PSRAM
octale) ✓ [WROOM-1, tableau des pins, note b].

---

## ✅ Corrigé pendant l'audit

### H2 — J3-J6 passés au brochage Qwiic/STEMMA QT
Avant : 1 = 3V3, 2 = GND (inverse du standard : un breakout Adafruit/SparkFun branché
avec un câble standard recevait une alimentation inversée). Après : **1 = GND, 2 = 3V3,
3 = SDA, 4 = SCL**.
- Patch : `docs/pcb/kicad-patches/connector.kicad_sch` (8 global labels échangés, rien
  d'autre ; vérifié par netlist). À copier dans le projet KiCad.
- Netlist documentée (`J_SAT`, `J_FP`) et FSD §2.2.3 mis à jour : les satellites doivent
  suivre le même brochage.

---

## 🔴 Critique

### A1 — PCM5122 : C20 câblé entre CAPM et VNEG
Netlist : C19 = CAPP–CAPM (condensateur volant ✓), **C20 = CAPM–VNEG ✗**.
La datasheet définit VNEG comme « negative charge pump rail terminal **for decoupling** »
(rail −3,3 V) → condensateur **VNEG → GND** [PCM5122 §6.1 Pin Functions, application
typique : 2 × 2,2 µF]. En l'état, le rail négatif n'est pas découplé → sortie audio
dégradée ou charge pump instable.
→ Déplacer C20 : broche 1 sur VNEG, broche 2 sur GND.

### A2 — Sorties haut-parleur non raccordées
`HP_L+`, `HP_L−`, `HP_R+`, `HP_R−` n'existent que sur la feuille audio (aucun connecteur).
→ Connecteur haut-parleur (face Dessus, FSD §2.2.3) + **ferrite sur chaque fil** : la
datasheet la recommande en classe D sans filtre [PAM8406 Application Note 2].

---

## 🟠 Important

### A3 — Niveau d'entrée du PAM8406 : écrêtage (et risque) à volume élevé
PCM5122 : 2,1 Vrms à 0 dB [PCM5122]. PAM8406 : gain **fixe 24 dB** (× 15,8)
[PAM8406 §Maximum Gain]. En pont sous 5 V, l'excursion max est ≈ 3,5 Vrms, donc
l'entrée utile est ≈ 0,22 Vrms, soit environ −20 dB par rapport à la pleine échelle du DAC.
Or le firmware règle le volume à 0 dB (`hal_audio.c`, reg 0x3D/0x3E = 0x30, 100 % → 0 dB).
La datasheet avertit qu'un signal trop fort écrête et **peut endommager** le composant
[PAM8406 Application Note 4].
→ Plafonner le volume numérique vers −20 dB (0x58) dans `hal_audio_set_volume`, ou
ajouter un atténuateur résistif avant INL/INR. Ce plafond limite aussi la charge du rail 5 V (A5).

### A4 — ICS-43434 sans condensateur de découplage
La datasheet recommande **fortement** 0,1 µF X7R au plus près des pins 5/3, sans via
[ICS-43434 §Power Supply Decoupling]. Aucun sur le schéma (3V3_A n'est découplé qu'au
PCM5122). → Ajouter 100 nF 0402 au pied de U4.

### A5 — Rail 5 V (MT3608 + D1) : limites en crête
Charges sur le rail `5V` [datasheets] :
- 12 × WS2812B : 12 mA par couleur → 36 mA/LED en blanc, soit **432 mA** ; 0,6 mA de
  repos par LED, soit 7,2 mA [WS2812B V5 LED Characteristics] ;
- PAM8406 à 5 V sur 4 Ω : 2 × 2,55 W (THD 1 %), rendement 87 % → ≈ 1,15 A ;
  2 × 3,14 W (THD 10 %) → ≈ 1,44 A [PAM8406 Electrical Characteristics]. Impédance du
  haut-parleur à fixer (en 8 Ω : 1,8 W/canal) ;
- J9 : jusqu'à ~1,9 A de LEDs externes (intention FSD §2.2.3, sans datasheet).

Conséquences :
- **D1 SS14 : I_F(AV) = 1,0 A** [SS14]. En boost, le courant moyen de la diode est égal au
  courant de sortie → dépassé dès LEDs pleines + audio fort (≈ 1,6 A). → **SS34** (3 A,
  même empreinte SMA) ;
- courant batterie ≈ 5,1 V × I_5V / (η × V_BAT) ≈ 1,6 × I_5V à 3,6 V (η supposé ≈ 87 %,
  à mesurer). Pire cas LEDs + audio + 3V3 ≈ 3 A, alors que **la surintensité DW01A
  déclenche vers 1,6-3,2 A** (V_OI1 120-180 mV sur 2 × R_DS(on) 28-37 mΩ)
  [DW01A, FS8205] → coupure de la batterie (reset de la box) possible ;
- le switch du MT3608 (limite 4 A) [MT3608] n'est pas le maillon faible ;
- **J9 à 1,9 A n'est pas tenable** depuis le boost sur batterie (≈ 3,1 A côté batterie à lui seul).

→ SS34 + plafonds firmware (luminosité LEDs, volume, cf. A3) ; revoir l'objectif J9 ;
mesurer le rendement réel du MT3608 au proto.

### H5 — Régulateur 3V3_D (AP2112K SOT-23-5) : le courant passe, la thermique est limite
- Courant : l'ESP32-S3 exige une alimentation **≥ 0,5 A** [ESP32-S3 §5.2] ; pics TX
  WiFi 355 mA, BLE 344 mA [WROOM-1 §6.4] ; l'AP2112 garantit **600 mA** [AP2112]. Il
  reste ≈ 245 mA pour la SD, les 2 GC9A01, l'écran bouche et les satellites : plausible,
  **non concluable sans les datasheets GC9A01 et microSD** (à ajouter).
- Dropout 125 / 200 mV (typ / max) à 300 mA [AP2112] : sur batterie basse, le 3V3 suit
  VBAT − dropout, et l'ESP32 tient jusqu'à 3,0 V [ESP32-S3 §5.2] → **pas de problème de
  dropout** (révision 1 corrigée).
- Thermique : θJA = **184 °C/W** en SOT-23-5 [AP2112]. Sous USB, VSYS ≈ 4,7-5,0 V
  (V_DO(IN-OUT) 300 mV typ à 1 A [bq24075]) : à 250 mA moyens → 0,4 W → +74 °C ; à
  400 mA → 0,64 W → +118 °C (coupure à 160 °C), en boîtier fermé.
→ Correctif minimal : **AP2112 en SOT-89-5** (même famille, θJA 120 °C/W), avec du cuivre
autour. Sinon un buck. Mesurer le courant moyen réel au proto.

### H7 — Carte SD : pull-ups obligatoires absentes
« En mode SPI ou SD 1 bit, CMD et DAT0-DAT3 doivent être tirées au plus par 10 kΩ, y
compris les lignes non reliées à l'hôte » [ESP-IDF `sd_pullup_requirements.rst`]. Aucune
sur J11 (CS/DAT3, CMD, DAT0, DAT1, DAT2).
→ 5 × 10 kΩ vers 3V3_D. Firmware : sur le bus SPI2 partagé, monter la SD avant tout
échange avec l'écran bouche, CS de l'écran maintenu haut [ESP-IDF `sdspi_share.rst`].

### H8 — Ferrite + 10 pF sur les horloges SPI
FB1/FB2 (120 Ω @ 100 MHz) en série et 10 pF à la masse sur SPI2_SCLK et SPI3_SCLK. La doc
Espressif sur le bus SD partagé met en garde contre la charge capacitive des lignes, qui
arrondit l'horloge et viole les timings de la carte [ESP-IDF `sdspi_share.rst` §AC
Loading]. Les yeux tournent à 40 MHz (80 MHz visés au FSD).
→ Résistance série 22-33 Ω à la place de la ferrite (même empreinte 0603) ; 10 pF en DNP
jusqu'au test CEM ; vérifier les fronts à l'oscilloscope au proto.

### H9 — Charge LiPo : timers et NTC désactivés → propositions
État : TMR → GND = « timers désactivés » ; TS = 10 kΩ fixe = pas de surveillance thermique
[bq24075 §6.1]. Batterie 3000 mAh (FSD §2.2.2b), charge 1 A (R_ISET 890 Ω).
Propositions, par ordre de priorité :
1. **Timers** : remplacer TMR → GND par **R_TMR = 56 kΩ 1 %** vers VSS.
   t_MAXCHG = 10 × K_TMR × R_TMR, avec K_TMR = 36-60 s/kΩ → **5,6 h à 9,3 h** ;
   t_PRECHG = 34 à 56 min [bq24075 §9.3.5.6 et tableau Electrical Characteristics].
   Une charge de 3000 mAh à 1 A dure ~4 h (CC + CV), et le timer ralentit quand DPPM réduit
   le courant, donc pas de faux défaut. TMR flottant (4-6 h, valeurs par défaut) serait trop
   juste pour 3000 mAh. Défaut de timer → CHG clignote à 2 Hz [bq24075 §9.3.5.6].
2. **NTC** : batterie 3 fils avec NTC 10 kΩ (courbe Vishay « Type 2 », R25 = 10 kΩ) →
   J2 en JST-PH 3 broches, TS sur la NTC, R_TS supprimée → fenêtre de charge **0-50 °C**
   [bq24075 §8.5 Electrical Characteristics, note (1)]. Si la batterie n'a que 2 fils : NTC 10 kΩ 0603 sur le PCB,
   plaquée contre la batterie (moins précis, mais mieux que rien).
3. **Courant de charge (option)** : 1 A = C/3 pour 3000 mAh, acceptable. Mais au début de
   la charge rapide, le bq24075 dissipe (V_IN − V_BAT) × I ≈ 1,5 W : sa régulation
   thermique réduira le courant à T_J 125 °C [bq24075 note (2)]. C'est sûr, mais ça chauffe
   la boîte. Pour chauffer moins : R_ISET = 1,27 kΩ → 0,70 A (K_ISET 890 AΩ).
4. **Voyant de charge (option)** : CHG et PGOOD sont en pull-up 100 kΩ vers 3V3_D, par
   choix, pour sonder au debug (budget GPIO épuisé, `bq24075.md`) : conforme. Une LED +
   résistance sur CHG (open-drain, V_OL ≤ 0,4 V à 5 mA [bq24075]) rendrait visibles
   l'état de charge et le clignotement à 2 Hz en cas de défaut.

### H4 → TODO — Bouton marche/arrêt (ajouté au FSD Phase 2)
Aujourd'hui SYSOFF → GND et les EN de U5, U6, U7 → VSYS : la box est alimentée dès qu'une
batterie est branchée. Rien que 12 × 0,6 mA de repos WS2812 [WS2812B V5] + I_q PWM du
MT3608 1,6-2,2 mA [MT3608] font ~10 mA, soit ~12 jours pour 3000 mAh.
L'interrupteur est déjà prévu sur la face Côté 2 (FSD §2.2.3). Options :

| Option | Principe | Éteint | Charge USB quand éteint | Consommation éteint |
|---|---|---|---|---|
| **A — SYSOFF** (le plus simple) | Interrupteur SYSOFF haut/bas | FET BAT→OUT ouvert | **Non** : « quand un adaptateur est branché, la charge est aussi désactivée » et OUT reste alimenté par l'USB → la box s'allume si on la branche [bq24075 §6.1 SYSOFF] | I_BAT sommeil 4,3-6,5 µA [bq24075] + DW01A 3-6 µA [DW01A] + pont VBAT 2 µA |
| **B — EN des régulateurs + load switch 5 V** (recommandé) | Interrupteur sur un net EN_SYS → EN de U5/U6 (V_IH 1,5-6 V, pull-down interne 3 MΩ [AP2112]) et EN de U7 (V_IH 1,5 V, arrêt 0,1 µA [MT3608]) + pull-down 100 kΩ, **plus un MOSFET P sur le rail 5 V** | Tous les rails coupés | **Oui** (le bq24075 reste actif) | ~10-15 µA (standby AP2112 ≤ 1 µA ×2, MT3608 ≤ 1 µA, DW01A, pont, fuite du MOSFET) |
| C — Maintien logiciel (plus tard) | Bouton poussoir + GPIO qui maintient EN_SYS | idem B | Oui | idem B, et permet l'extinction auto (FW-11) mais demande une GPIO |

⚠ Pour B, le MOSFET sur le 5 V est indispensable : le MT3608 est un boost asynchrone,
donc EN bas laisse passer VIN → L → D1 → 5 V ≈ VBAT − V_F, et les WS2812 restent
alimentées (conséquence de la topologie [MT3608 pinout/typique]). Avec 3V3_A coupé, les
pull-ups de MUTE/SHDN/MODE tombent à 0 V : le PAM8406 passe en shutdown (< 1 µA) [PAM8406]. OK.

---

## 🟡 Faible / à noter

- **WS2812 — timings firmware hors spec V5** : `hal_leds.c` utilise T0H = 0,4 µs (max 380 ns)
  et T1L = 0,45 µs (min 580 ns) [WS2812B V5 Data Transfer Time]. Valeurs conformes :
  bit0 = 0,3 / 0,9 µs, bit1 = 0,8 / 0,6 µs (voir `WS2812B-B.md`). `hal_leds_init(48, 1)`
  → 12 LEDs au bring-up.
- **GPIO45/46 (BTN1/BTN2)** : flottantes = **conforme** (pull-down internes → VDD_SPI
  3,3 V, boot SPI) [ESP32-S3 §3, WROOM-1 : VDD_SPI 1,8 V seulement sur N16R16VA].
  **Ne jamais ajouter de pull-up externe sur GPIO45** (1 au reset = flash 1,8 V, la box ne
  boote plus) ; bouton seulement actif haut et relâché au reset. Netlist documentée et FSD
  corrigés (ils prévoyaient une pull-up). Option pour libérer GPIO45 :
  `espefuse.py set_flash_voltage 3.3V` (irréversible ; « GPIO45 can be high or low at
  reset », esptool) avec VDD ≤ 3,3 V pendant la programmation [ESP32-S3 §5.2].
- **USBLC6** : brochage conforme (I/O1 = 1/6, I/O2 = 3/4, VBUS = 5) ; au layout, la placer
  au plus près du connecteur [USBLC6-2 §2.3].
- **LSM6DSOX** : INT1/INT2 non routées (pas de réveil sur mouvement sans polling) ; ST
  demande ≥ 100 nF + 10 µF sur VDD, 100 nF sur VDDIO (`LSM6DSOXTR.md`) : seuls C4/C5
  (100 nF) sont présents → ajouter 10 µF sur VDD.
- **Rétroéclairage GC9A01 non pilotable** (J7 pins 9/10 NC) : à confirmer avec la
  datasheet du module (absente).
- **VBAT_SENSE sur J8.5** : nœud ADC haute impédance (1 MΩ / 1 MΩ) sorti dans un câble
  (remarque de conception, pas de datasheet concernée).
- Champ LCSC manquant sur la plupart des passifs et sur U1, U2, U5-U10, U12.
- **E-Switch 100 (SW1/SW2)** : prendre la finition **or** (0,4 VA @ 20 V, faits pour les
  signaux bas niveau) plutôt que l'argent [E-Switch 100 Series].

## ✅ Conforme (vérifié datasheet)

EN : RC 10 kΩ / 1 µF [guidelines Espressif]. bq24075 : EN2 = H, EN1 = L (ILIM),
R_ILIM 1,5 kΩ → 1,07 A, R_ISET 890 Ω → 1 A, CE → GND, SYSOFF non flottant, C_IN/C_OUT/C_BAT
4,7 µF [bq24075]. DW01A : R 100 Ω / C 0,1 µF / R_CS 1 kΩ, TD NC [DW01A application typique].
FS8205 : S1 = VBAT−, G1 = OD, S2 = GND, G2 = OC, D12 commun [FS8205]. MT3608 : 0,6 V ×
(1 + 750 k/100 k) = 5,1 V (5,0-5,2 V), sous le max absolu WS2812 de 5,3 V [MT3608,
WS2812B V5]. AP2112 : EN sur VIN, C_IN/C_OUT ≥ 1 µF. USBLC6 : brochage flow-through
[USBLC6-2]. WS2812 : pilotage 3,3 V (V_IH ≥ 2,7 V) et absence de condensateur par LED
conformes [WS2812B V5]. PCM5122 : MODE1 = L / MODE2 = H (I2C), ADR1/ADR2 = GND (0x4C),
SCK = GND (PLL depuis BCK), VCOM = GND (mode VREF), XSMT vers AVDD, LDOO 0,1 µF, GPIO
flottantes (pull-down internes), découplage 10 µF + 100 nF, filtre 470 Ω / 2,2 nF
[PCM5122]. PAM8406 : MODE haut (classe D, jamais flottante), MUTE/SHDN hauts, VREF 1 µF,
découplage 3 × 1 µF + 22 µF, entrées 1 µF (f_c ≈ 9 Hz) [PAM8406]. ICS-43434 : LR = GND
(gauche), SD 100 kΩ pull-down, VDD 3,3 V [ICS-43434]. LSM6DSOX : CS = VDDIO (I2C),
SA0 = GND (0x6A) (`LSM6DSOXTR.md`). Diviseur VBAT sur ADC1 (GPIO3, strapping JTAG ignoré
avec eFuses vierges) + 100 nF [ESP32-S3 §3.4, guidelines].

---

## Schéma de base de données (diagramme Supabase)

### ✅ Conforme
`devices.box_uid` UNIQUE, `scenarios.slug` UNIQUE, PK composite
`device_scenarios(device_id, scenario_id)` (couvre la requête `/pkg`), PK
`box_challenges.challenge`, `profiles.id` → `auth.users.id`.

### 🟠 Important
- **D1 — Licences liées à la box, pas à l'utilisateur** : `device_scenarios` est le seul
  droit. En SAV ou avec une deuxième box, les achats ne suivent pas ; revendre la box revend
  les achats. À trancher **avant Stripe** (table `purchases` / `user_scenarios`, droits
  par box dérivés du propriétaire).
- **D2 — `firmware_releases.sha256` nullable**, pas d'unicité `(version, channel)` :
  NOT NULL + UNIQUE avant F6, puis signature.

### 🟡 Faible
- `box_challenges.used` nullable (le code filtre `used = false`) → `NOT NULL DEFAULT false`
  ; index sur `expires_at` (purge à chaque `/challenge`).
- `scenarios.active` nullable → `NOT NULL DEFAULT true` ; `price_chf numeric` sans CHECK.
- `devices.owner_id` : index + `ON DELETE SET NULL`.
- `/api/box/register` : un double enregistrement concurrent heurte UNIQUE(box_uid) et renvoie
  500 au lieu de 409 (gérer le code 23505).
- RLS et policies non visibles sur le diagramme → requêtes d'extraction pour la migration
  de référence :

```sql
select table_name, column_name, data_type, is_nullable, column_default
  from information_schema.columns where table_schema = 'public'
  order by table_name, ordinal_position;
select conrelid::regclass as tbl, conname, pg_get_constraintdef(oid)
  from pg_constraint where connamespace = 'public'::regnamespace;
select relname, relrowsecurity from pg_class
  where relnamespace = 'public'::regnamespace and relkind = 'r';
select tablename, policyname, cmd, roles, qual, with_check
  from pg_policies where schemaname = 'public';
select indexdef from pg_indexes where schemaname = 'public';
select tgname, tgrelid::regclass, pg_get_triggerdef(oid)
  from pg_trigger where not tgisinternal;
```
